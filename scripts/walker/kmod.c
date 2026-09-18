// gpu_map_inspect.c
// Walk PTEs by hand (pgd/p4d/pud/pmd/pte) - compatibility guards + fixed vma naming.

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/dcache.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/pgtable.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Assistant");
MODULE_DESCRIPTION("Inspect process VMAs and PTEs mapped to /dev/dri (GPU/DRM nodes) - manual walker with compat");

static int target_pid = -1;
module_param(target_pid, int, 0444);
MODULE_PARM_DESC(target_pid, "PID to inspect if process_name not found");

static char *process_name = NULL;
module_param(process_name, charp, 0444);
MODULE_PARM_DESC(process_name, "Process name (task->comm) to inspect");

static unsigned int max_pages_per_vma = 64;
module_param(max_pages_per_vma, uint, 0644);
MODULE_PARM_DESC(max_pages_per_vma, "Max PTEs to print per VMA (prevents log spam)");

struct inspect_context {
    struct vm_area_struct *vma; /* fixed name */
    unsigned int printed;
    unsigned int max_print;
};

/* Compatibility wrappers for pte map/unmap helpers:
 * Newer kernels provide pte_offset_map_lock / pte_unmap_unlock. Older kernels
 * expose pte_offset_map / pte_unmap. We abstract both.
 */
#if defined(pte_offset_map_lock) && defined(pte_unmap_unlock)
static pte_t *my_pte_map_lock(pmd_t *pmd, unsigned long addr, spinlock_t **ptl)
{
    /* some kernel variants take mm as first arg; try NULL which worked on many trees */
#ifdef pte_offset_map_lock
    return pte_offset_map_lock(NULL, pmd, addr, ptl);
#else
    return NULL;
#endif
}
static void my_pte_unmap_unlock(pte_t *ptep, spinlock_t *ptl)
{
    pte_unmap_unlock(ptep, ptl);
}
#elif defined(pte_offset_map) && defined(pte_unmap)
static pte_t *my_pte_map_lock(pmd_t *pmd, unsigned long addr, spinlock_t **ptl)
{
    *ptl = NULL;
    return pte_offset_map(pmd, addr);
}
static void my_pte_unmap_unlock(pte_t *ptep, spinlock_t *ptl)
{
    pte_unmap(ptep);
    if (ptl)
        spin_unlock(ptl);
}
#else
/* Fallback best-effort */
static pte_t *my_pte_map_lock(pmd_t *pmd, unsigned long addr, spinlock_t **ptl)
{
    *ptl = NULL;
#ifdef pte_offset_map_lock
    return pte_offset_map_lock(NULL, pmd, addr, ptl);
#else
    pr_info("No compatible pte map helper available on this kernel version\n");
    return NULL;
#endif
}
static void my_pte_unmap_unlock(pte_t *ptep, spinlock_t *ptl)
{
#ifdef pte_unmap_unlock
    pte_unmap_unlock(ptep, ptl);
#endif 
}
#endif

/* cache_mode_helpers.c - add to your module source */

/* Try to decode the cache mode for a VMA and for a PTE (best-effort). */
static const char *cache_mode_from_pgprot(pgprot_t prot)
{
#if defined(pgprot_noncached) && defined(pgprot_writecombine) \
  && defined(pgprot_writeback) && defined(pgprot_writethrough)
    if (pgprot_noncached(prot))
        return "UC";     /* uncached */
    if (pgprot_writecombine(prot))
        return "WC";     /* write-combining */
    if (pgprot_writethrough(prot))
        return "WT";     /* write-through */
    if (pgprot_writeback(prot))
        return "WB";     /* write-back */
    return "WB?";       /* assume WB if none matched */
#elif defined(pgprot_val)
    /* Fallback: print raw value and try basic guesses using common bits
     * (x86-like: PWT, PCD, PAT). This is best-effort and mainly useful for debugging.
     */
    unsigned long v = pgprot_val(prot);
    /* On x86, _PAGE_PWT and _PAGE_PCD indicate PWT/PCD. If PAT is present we cannot
     * decode reliably here without architecture-specific shifts, so just print raw.
     */
    if (v & (_PAGE_PCD))
        return "PCD?"; /* likely uncached/UC or WC depending on PAT */
    if (v & (_PAGE_PWT))
        return "PWT?"; /* possible WT/WC */
    return "WB?";
#else
    return "unknown";
#endif
}

/* Given a VMA, print its cache mode. */
static void print_vma_cache_mode(struct vm_area_struct *vma)
{
    const char *mode = cache_mode_from_pgprot(vma->vm_page_prot);
    pr_info("  vma cache mode: %s (raw pgprot=0x%lx)\n",
            mode, (unsigned long) pgprot_val(vma->vm_page_prot));
}

/* Given a PTE (and optional vma), try to resolve cache mode for that PTE.
 * We prefer decoding from pte if possible; else fall back to vma->vm_page_prot.
 */
static void print_pte_cache_info(struct inspect_context *ctx, pte_t p, unsigned long addr)
{
    printk(KERN_INFO "    Inspecting PTE cache for addr 0x%lx\n", addr);

#if defined(pte_val) && (defined(_PAGE_PWT) || defined(_PAGE_PCD) || defined(_PAGE_PAT))
    unsigned long pv = pte_val(p);
    /* best-effort x86 style decoding: look at PWT/PCD/PAT/PAT-index if available */
    bool pwt = !!(pv & _PAGE_PWT);
    bool pcd = !!(pv & _PAGE_PCD);
#ifdef _PAGE_PAT
    bool pat = !!(pv & _PAGE_PAT);
#else
    bool pat = false;
#endif

    /* simple heuristics (x86-ish) */
    if (pcd && !pwt)
        pr_info("    PTE cache: UC (PCD set) pte=0x%lx\n", pv);
    else if (pwt && !pcd)
        pr_info("    PTE cache: WT/possibly WC (PWT set) pte=0x%lx\n", pv);
    else if (pat)
        pr_info("    PTE cache: PAT-influenced (PAT bit set) pte=0x%lx\n", pv);
    else
        pr_info("    PTE cache: default (likely WB) pte=0x%lx\n", pv);
#else
    /* Fallback: print the vma-level cache mode and raw pte value */
    if (ctx && ctx->vma)
        print_vma_cache_mode(ctx->vma);
    pr_info("    PTE raw val: 0x%lx\n", (unsigned long) pte_val(p));
#endif
}

/* Helper: print a single PTE (address must be page-aligned) */
static void inspect_pte_at(struct mm_struct *mm, unsigned long addr,
                           struct inspect_context *ctx)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    spinlock_t *ptl = NULL;
    pte_t p;

    pgd = pgd_offset(mm, addr);
    if (!pgd || pgd_none(*pgd))
        return;

    p4d = p4d_offset(pgd, addr);
    if (!p4d || p4d_none(*p4d))
        return;

    pud = pud_offset(p4d, addr);
    if (!pud || pud_none(*pud))
        return;

    pmd = pmd_offset(pud, addr);
    if (!pmd || pmd_none(*pmd))
        return;

    /* huge PMD? use available macros safely */
#if defined(pmd_huge)
    if (pmd_trans_huge(*pmd) || pmd_huge(*pmd)) {
#else
    if (pmd_trans_huge(*pmd)) {
#endif
        pr_info("  huge PMD mapping at 0x%lx (pmd flags: 0x%lx)\n", addr,
                (unsigned long)pmd_val(*pmd));
        ctx->printed++;
        return;
    }

    /* map the pte safely and take the pte-table lock if necessary */
    // pte = my_pte_map_lock(pmd, addr, &ptl);
    // if (!pte) {
    //     pr_info("  failed to map PTE table for 0x%lx\n", addr);
    //     return;
    // }

    pte = pte_offset_kernel(pmd, addr);
    if (!pte || pte_none(*pte)) {
        pr_info("  no PTE for 0x%lx\n", addr);
        // my_pte_unmap_unlock(pte, ptl);
        return;
    }

    p = *pte;
    if (!pte_present(p)) {
        pr_info("  PTE @ 0x%lx: not present (possibly swapped)\n", addr);
    } else {
        unsigned long pfn = pte_pfn(p);
        pr_info("  PTE @ 0x%lx: PFN 0x%lx flags: %c%c%c\n",
                addr, pfn,
                pte_write(p) ? 'W' : '-',
                pte_dirty(p) ? 'D' : '-',
                pte_young(p) ? 'A' : '-');

        print_pte_cache_info(ctx, p, addr);
    }

    // my_pte_unmap_unlock(pte, ptl);

    ctx->printed++;
    return;
}

/* Inspect all pages in a VMA up to ctx->max_print. Uses _addr_end helpers to skip holes. */
static void inspect_vma_pages(struct mm_struct *mm, struct vm_area_struct *vma,
                              struct inspect_context *ctx)
{
    unsigned long addr = vma->vm_start;
    unsigned long end = vma->vm_end;

    printk(KERN_INFO "  Inspecting VMA pages from 0x%lx to 0x%lx\n", addr, end);

    while (addr < end && ctx->printed < ctx->max_print) {
        pgd_t *pgd = pgd_offset(mm, addr);

        pgd = pgd_offset(mm, addr);
        if (!pgd || pgd_none(*pgd)) {
            pr_info("  no pgd for 0x%lx; stopping walk\n", addr);
            return;
        }

        p4d_t *p4d = p4d_offset(pgd, addr);
        p4d = p4d_offset(pgd, addr);
        if (!p4d || p4d_none(*p4d))
            continue;

        pud_t *pud = pud_offset(p4d, addr);
        pud = pud_offset(p4d, addr);
        if (!pud || pud_none(*pud))
            continue;

        pmd_t *pmd = pmd_offset(pud, addr);
        pmd = pmd_offset(pud, addr);
        if (!pmd || pmd_none(*pmd)) {
            pr_info("  no pmd for 0x%lx; stopping walk\n", addr);
            return;
        }

        /* huge pmd mapping - skip the whole pmd span */
#if defined(pmd_huge)
        if (pmd_trans_huge(*pmd) || pmd_huge(*pmd)) {
#else
        if (pmd_trans_huge(*pmd)) {
#endif
            pr_info("  huge PMD mapping at 0x%lx - skipping pmd range\n", addr);
            ctx->printed++;
            addr = pmd_addr_end(addr, end);
            continue;
        }

        /* Otherwise inspect the PTE for this page */
        pr_info("  walking PTE for 0x%lx\n", addr);
        inspect_pte_at(mm, addr, ctx);
        return; /* for demo, inspect only the first page; remove to continue walking */

        addr += PAGE_SIZE;
    }
}

static void print_vma_flags(struct vm_area_struct *vma)
{
    unsigned long f = vma->vm_flags;

    pr_info("  vm_flags raw: 0x%lx\n", f);
    pr_info("  decoded flags:");

#define PRINT_FLAG(flag) \
    do { \
        if (f & flag) \
            pr_cont(" " #flag); \
    } while (0)

#ifdef VM_READ
    PRINT_FLAG(VM_READ);
#endif
#ifdef VM_WRITE
    PRINT_FLAG(VM_WRITE);
#endif
#ifdef VM_EXEC
    PRINT_FLAG(VM_EXEC);
#endif
#ifdef VM_SHARED
    PRINT_FLAG(VM_SHARED);
#endif
#ifdef VM_MAYREAD
    PRINT_FLAG(VM_MAYREAD);
#endif
#ifdef VM_MAYWRITE
    PRINT_FLAG(VM_MAYWRITE);
#endif
#ifdef VM_MAYEXEC
    PRINT_FLAG(VM_MAYEXEC);
#endif
#ifdef VM_MAYSHARE
    PRINT_FLAG(VM_MAYSHARE);
#endif
#ifdef VM_GROWSDOWN
    PRINT_FLAG(VM_GROWSDOWN);
#endif
#ifdef VM_GROWSUP
    PRINT_FLAG(VM_GROWSUP);
#endif
#ifdef VM_IO
    PRINT_FLAG(VM_IO);
#endif
#ifdef VM_PFNMAP
    PRINT_FLAG(VM_PFNMAP);
#endif
#ifdef VM_DONTEXPAND
    PRINT_FLAG(VM_DONTEXPAND);
#endif
#ifdef VM_DONTDUMP
    PRINT_FLAG(VM_DONTDUMP);
#endif
#ifdef VM_LOCKED
    PRINT_FLAG(VM_LOCKED);
#endif
#ifdef VM_HUGETLB
    PRINT_FLAG(VM_HUGETLB);
#endif
#ifdef VM_MIXEDMAP
    PRINT_FLAG(VM_MIXEDMAP);
#endif
#ifdef VM_DONTCOPY
    PRINT_FLAG(VM_DONTCOPY);
#endif
#ifdef VM_ACCOUNT
    PRINT_FLAG(VM_ACCOUNT);
#endif
#ifdef VM_NORESERVE
    PRINT_FLAG(VM_NORESERVE);
#endif
#ifdef VM_UFFD_MISSING
    PRINT_FLAG(VM_UFFD_MISSING);
#endif
#ifdef VM_UFFD_WP
    PRINT_FLAG(VM_UFFD_WP);
#endif
#ifdef VM_SOFTDIRTY
    PRINT_FLAG(VM_SOFTDIRTY);
#endif
#ifdef VM_SPECIAL
    PRINT_FLAG(VM_SPECIAL);
#endif
#ifdef VM_MERGEABLE
    PRINT_FLAG(VM_MERGEABLE);
#endif
#ifdef VM_SEQ_READ
    PRINT_FLAG(VM_SEQ_READ);
#endif
#ifdef VM_RAND_READ
    PRINT_FLAG(VM_RAND_READ);
#endif
#ifdef VM_DROPPABLE
    PRINT_FLAG(VM_DROPPABLE);
#endif

    pr_cont("\n");

#undef PRINT_FLAG
}

static void inspect_task_mm(struct task_struct *task)
{
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    unsigned long addr = 0;
    char *kbuf;

    mm = get_task_mm(task);
    if (!mm) {
        pr_info("No mm for pid %d (%s)\n", task->pid, task->comm);
        return;
    }

    kbuf = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!kbuf) {
        pr_warn("kmalloc PATH_MAX failed\n");
        mmput(mm);
        return;
    }

    mmap_read_lock(mm);

    while ((vma = find_vma(mm, addr)) != NULL) {
        struct file *f = vma->vm_file;

        /* prepare next search address to avoid infinite loop (covers zero-length VMAs) */
        addr = vma->vm_end ? vma->vm_end : vma->vm_start + 1;

        if (!f)
            continue;

        /* quick path check using d_path */
        {
            char *p = d_path(&f->f_path, kbuf, PATH_MAX);
            if (IS_ERR(p))
                continue;
            if (strncmp(p, "anon_inode:i915.gem", 19) != 0)
                continue;

            pr_info("=== PID %d (%s) VMA %lx-%lx mapped file=%s ===\n",
                    task->pid, task->comm, vma->vm_start, vma->vm_end, p);

            print_vma_flags(vma);
            pr_info("  vm_pgoff: 0x%llx\n",
                    (unsigned long long)vma->vm_pgoff);

            {
                struct inspect_context ctx = {
                    .vma = vma,
                    .printed = 0,
                    .max_print = max_pages_per_vma,
                };
                inspect_vma_pages(mm, vma, &ctx);

                if (ctx.printed >= ctx.max_print)
                    pr_info("  (printed first %u pages; increase max_pages_per_vma to see more)\n",
                            ctx.max_print);
            }
        }
    }

    mmap_read_unlock(mm);

    kfree(kbuf);
    mmput(mm);
}

#include <linux/pid.h>
static int __init gpu_map_inspect_init(void)
{
    struct task_struct *task;
    int found = 0;

    if (!process_name && target_pid < 0) {
        pr_err("Specify process_name=\"...\" and/or target_pid=<pid>\n");
        return -EINVAL;
    }

    pr_info("gpu_map_inspect: starting (name=%s pid=%d)\n",
            process_name ? process_name : "NULL",
            target_pid);

    /* First: try name-based search if provided */
    if (process_name) {
        rcu_read_lock();
        for_each_process(task) {
            if (strncmp(task->comm, process_name, TASK_COMM_LEN) == 0) {
                found++;
                pr_info("Found PID=%d comm=%s — inspecting mm\n",
                        task->pid, task->comm);
                inspect_task_mm(task);
            }
        }
        rcu_read_unlock();
    }

    /* If no name match and pid provided → fallback to pid */
    if (!found && target_pid >= 0) {
        struct pid *pid_struct = find_get_pid(target_pid);
        if (!pid_struct) {
            pr_err("PID %d not found (no pid_struct)\n", target_pid);
            return -ESRCH;
        }

        /* pid_task must be used under RCU; acquire task pointer then take a reference */
        rcu_read_lock();
        task = pid_task(pid_struct, PIDTYPE_PID);
        if (task)
            get_task_struct(task); /* take a stable ref for use outside RCU */
        rcu_read_unlock();

        /* drop the pid_struct reference from find_get_pid */
        put_pid(pid_struct);

        if (!task) {
            pr_err("PID %d not found (no task)\n", target_pid);
            return -ESRCH;
        }

        pr_info("Using PID fallback: PID=%d comm=%s\n", task->pid, task->comm);

        inspect_task_mm(task);

        put_task_struct(task);
        return 0;
    }

    if (!found && process_name)
        pr_info("No process with name \"%s\" found.\n", process_name);

    return 0;
}

static void __exit gpu_map_inspect_exit(void)
{
    pr_info("gpu_map_inspect: module unloaded\n");
}

module_init(gpu_map_inspect_init);
module_exit(gpu_map_inspect_exit);