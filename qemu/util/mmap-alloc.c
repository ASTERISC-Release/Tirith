/*
 * Support for RAM backed by mmaped host memory.
 *
 * Copyright (c) 2015 Red Hat, Inc.
 *
 * Authors:
 *  Michael S. Tsirkin <mst@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#ifdef CONFIG_LINUX
#include <linux/mman.h>
#else  /* !CONFIG_LINUX */
#define MAP_SYNC              0x0
#define MAP_SHARED_VALIDATE   0x0
#endif /* CONFIG_LINUX */

#include "qemu/osdep.h"
#include "qemu/mmap-alloc.h"
#include "qemu/host-utils.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qemu/sg.h"

#define HUGETLBFS_MAGIC       0x958458f6

#ifdef CONFIG_LINUX
#include <sys/vfs.h>
#include <linux/magic.h>
#endif

// static const size_t LOW_OFFSET_INTO_MEMORY = 0x100000ULL;           // 1MB
// static const void *VIRTUAL_ADDRESS_LOW = (void*)0x100000ULL;        // 1MB
static const void *VIRTUAL_ADDRESS_LOW = (void*)0x200000ULL;        // 2MB
static const void *VIRTUAL_ADDRESS_MED = (void*)0x100000000ULL;        // 4GB
static const void *VIRTUAL_ADDRESS_HIGH = (void*)0x180000000ULL;    // 6GB

static const size_t LOW_OFFSET_INTO_MEMORY = 0x40000000ULL;           // 1MB
// static const void *VIRTUAL_ADDRESS_LOW = (void*)0x40000000ULL;        // 1MB
static const size_t HIGH_OFFSET_INTO_MEMORY = 0x80000000ULL;        // 2GB
// static const void *VIRTUAL_ADDRESS_HIGH = (void*)0x100000000ULL;    // 4GB
// static const void *VIRTUAL_ADDRESS_HIGH = (void*)0x140000000ULL;    // 5GB
void *global_ram_address = NULL;
void *global_ram1_address = NULL;
void *global_ram2_address = NULL;
void *global_ram3_address = NULL;

static pthread_t mmap_listen_thrs[NUM_COMM_PAGES];
static int mmap_listen_thr_started = 0;



QemuFsType qemu_fd_getfs(int fd)
{
#ifdef CONFIG_LINUX
    struct statfs fs;
    int ret;

    if (fd < 0) {
        return QEMU_FS_TYPE_UNKNOWN;
    }

    do {
        ret = fstatfs(fd, &fs);
    } while (ret != 0 && errno == EINTR);

    switch (fs.f_type) {
    case TMPFS_MAGIC:
        return QEMU_FS_TYPE_TMPFS;
    case HUGETLBFS_MAGIC:
        return QEMU_FS_TYPE_HUGETLBFS;
    default:
        return QEMU_FS_TYPE_UNKNOWN;
    }
#else
    return QEMU_FS_TYPE_UNKNOWN;
#endif
}

size_t qemu_fd_getpagesize(int fd)
{
#ifdef CONFIG_LINUX
    struct statfs fs;
    int ret;

    if (fd != -1) {
        do {
            ret = fstatfs(fd, &fs);
        } while (ret != 0 && errno == EINTR);

        if (ret == 0 && fs.f_type == HUGETLBFS_MAGIC) {
            return fs.f_bsize;
        }
    }
#ifdef __sparc__
    /* SPARC Linux needs greater alignment than the pagesize */
    return QEMU_VMALLOC_ALIGN;
#endif
#endif

    return qemu_real_host_page_size();
}

#define OVERCOMMIT_MEMORY_PATH "/proc/sys/vm/overcommit_memory"
static bool map_noreserve_effective(int fd, uint32_t qemu_map_flags)
{
#if defined(__linux__)
    const bool readonly = qemu_map_flags & QEMU_MAP_READONLY;
    const bool shared = qemu_map_flags & QEMU_MAP_SHARED;
    gchar *content = NULL;
    const char *endptr;
    unsigned int tmp;

    /*
     * hugeltb accounting is different than ordinary swap reservation:
     * a) Hugetlb pages from the pool are reserved for both private and
     *    shared mappings. For shared mappings, all mappers have to specify
     *    MAP_NORESERVE.
     * b) MAP_NORESERVE is not affected by /proc/sys/vm/overcommit_memory.
     */
    if (qemu_fd_getpagesize(fd) != qemu_real_host_page_size()) {
        return true;
    }

    /*
     * Accountable mappings in the kernel that can be affected by MAP_NORESEVE
     * are private writable mappings (see mm/mmap.c:accountable_mapping() in
     * Linux). For all shared or readonly mappings, MAP_NORESERVE is always
     * implicitly active -- no reservation; this includes shmem. The only
     * exception is shared anonymous memory, it is accounted like private
     * anonymous memory.
     */
    if (readonly || (shared && fd >= 0)) {
        return true;
    }

    /*
     * MAP_NORESERVE is globally ignored for applicable !hugetlb mappings when
     * memory overcommit is set to "never". Sparse memory regions aren't really
     * possible in this system configuration.
     *
     * Bail out now instead of silently committing way more memory than
     * currently desired by the user.
     */
    if (g_file_get_contents(OVERCOMMIT_MEMORY_PATH, &content, NULL, NULL) &&
        !qemu_strtoui(content, &endptr, 0, &tmp) &&
        (!endptr || *endptr == '\n')) {
        if (tmp == 2) {
            error_report("Skipping reservation of swap space is not supported:"
                         " \"" OVERCOMMIT_MEMORY_PATH "\" is \"2\"");
            return false;
        }
        return true;
    }
    /* this interface has been around since Linux 2.6 */
    error_report("Skipping reservation of swap space is not supported:"
                 " Could not read: \"" OVERCOMMIT_MEMORY_PATH "\"");
    return false;
#endif
    /*
     * E.g., FreeBSD used to define MAP_NORESERVE, never implemented it,
     * and removed it a while ago.
     */
    error_report("Skipping reservation of swap space is not supported");
    return false;
}

/*
 * Reserve a new memory region of the requested size to be used for mapping
 * from the given fd (if any).
 */
static void *mmap_reserve(size_t size, int fd)
{
    int flags = MAP_PRIVATE;

#if defined(__powerpc64__) && defined(__linux__)
    /*
     * On ppc64 mappings in the same segment (aka slice) must share the same
     * page size. Since we will be re-allocating part of this segment
     * from the supplied fd, we should make sure to use the same page size, to
     * this end we mmap the supplied fd.  In this case, set MAP_NORESERVE to
     * avoid allocating backing store memory.
     * We do this unless we are using the system page size, in which case
     * anonymous memory is OK.
     */
    if (fd == -1 || qemu_fd_getpagesize(fd) == qemu_real_host_page_size()) {
        fd = -1;
        flags |= MAP_ANONYMOUS;
    } else {
        flags |= MAP_NORESERVE;
    }
#else
    fd = -1;
    flags |= MAP_ANONYMOUS;
#endif

    return mmap(0, size, PROT_NONE, flags, fd, 0);
}

/*
 * Reserve a new memory region at a desired address.
 *
 * Without MAP_FIXED_NOREPLACE, @want is only a hint. If the kernel places the
 * mapping elsewhere, treat it as a failure and unmap the unexpected mapping.
 */
static void *mmap_reserve_at(void *want, size_t size, int fd)
{
    int flags = MAP_PRIVATE;
    void *ret;

#if defined(__powerpc64__) && defined(__linux__)
    if (fd == -1 || qemu_fd_getpagesize(fd) == qemu_real_host_page_size()) {
        fd = -1;
        flags |= MAP_ANONYMOUS;
    } else {
        flags |= MAP_NORESERVE;
    }
#else
    fd = -1;
    flags |= MAP_ANONYMOUS;
#endif

#ifdef MAP_FIXED_NOREPLACE
    ret = mmap(want, size, PROT_NONE, flags | MAP_FIXED_NOREPLACE, fd, 0);
    if (ret != MAP_FAILED) {
        return ret;
    }
#endif

    ret = mmap(want, size, PROT_NONE, flags, fd, 0);
    if (ret == want) {
        return ret;
    }
    if (ret != MAP_FAILED) {
        munmap(ret, size);
    }
    return MAP_FAILED;
}

/* Really bad heuristic, but it will do for now. */
int dimm_counter = 0;

/* Track whether we kept a guard page for a given RAM mapping. */
static GHashTable *guard_len_by_ptr;

static void guard_len_record(void *ptr, size_t guard_len)
{
    if (!guard_len_by_ptr) {
        guard_len_by_ptr = g_hash_table_new(g_direct_hash, g_direct_equal);
    }
    /* Store guard_len+1 so that 0 is distinguishable from NULL (not found). */
    g_hash_table_insert(guard_len_by_ptr, ptr,
                        (gpointer)(uintptr_t)(guard_len + 1));
}

static size_t guard_len_lookup(void *ptr, size_t default_guard_len)
{
    gpointer v;

    if (!guard_len_by_ptr) {
        return default_guard_len;
    }
    v = g_hash_table_lookup(guard_len_by_ptr, ptr);
    if (!v) {
        return default_guard_len;
    }
    return (size_t)(uintptr_t)v - 1;
}

/*
 * Activate memory in a reserved region from the given fd (if any), to make
 * it accessible.
 */
static void *mmap_activate(void *ptr, size_t size, int fd,
                           uint32_t qemu_map_flags, off_t map_offset)
{
    const bool noreserve = qemu_map_flags & QEMU_MAP_NORESERVE;
    const bool readonly = qemu_map_flags & QEMU_MAP_READONLY;
    const bool shared = qemu_map_flags & QEMU_MAP_SHARED;
    const bool sync = qemu_map_flags & QEMU_MAP_SYNC;
    const int prot = PROT_READ | (readonly ? 0 : PROT_WRITE);
    int map_sync_flags = 0;
    int flags = MAP_FIXED;
    void *activated_ptr;

    if (noreserve && !map_noreserve_effective(fd, qemu_map_flags)) {
        return MAP_FAILED;
    }

    flags |= fd == -1 ? MAP_ANONYMOUS : 0;
    flags |= shared ? MAP_SHARED : MAP_PRIVATE;
    flags |= noreserve ? MAP_NORESERVE : 0;
    if (shared && sync) {
        map_sync_flags = MAP_SYNC | MAP_SHARED_VALIDATE;
    }

    // printf("mmap_activate: ptr=%p; size=%ld; FD=%d; offset=%ld; flags=0x%x\n",
    //        ptr, size, fd, map_offset, flags | map_sync_flags);

    activated_ptr = mmap(ptr, size, prot, flags | map_sync_flags, fd,
                         map_offset);
    char *proc_link = g_strdup_printf("/proc/self/fd/%d", fd);
    char *file_name = g_malloc0(PATH_MAX);
    int len = readlink(proc_link, file_name, PATH_MAX - 1);

    if (len < 0) {
        len = 0;
    }
    file_name[len] = '\0';
    if (activated_ptr == MAP_FAILED && map_sync_flags) {
        if (errno == ENOTSUP) {
            proc_link = g_strdup_printf("/proc/self/fd/%d", fd);
            file_name = g_malloc0(PATH_MAX);
            int len = readlink(proc_link, file_name, PATH_MAX - 1);

            if (len < 0) {
                len = 0;
            }
            file_name[len] = '\0';
            fprintf(stderr, "Warning: requesting persistence across crashes "
                    "for backend file %s failed. Proceeding without "
                    "persistence, data might become corrupted in case of host "
                    "crash.\n", file_name);
            g_free(proc_link);
            g_free(file_name);
            warn_report("Using non DAX backing file with 'pmem=on' option"
                        " is deprecated");
        }
        /*
         * If mmap failed with MAP_SHARED_VALIDATE | MAP_SYNC, we will try
         * again without these flags to handle backwards compatibility.
         */
        activated_ptr = mmap(ptr, size, prot, flags, fd, map_offset);
    }

#if 1
    /* TODO: This hardcodes the three DIMM logic. */
    if (dimm_counter == 0) {
        log_always("\n");
        log_always("========BACKEND-SETUP\n");
        log_always("========DIMM1========\n");
        if(qemu_map_flags & QEMU_MAP_SHARED){
            size_t length = 2*1024*1024*1024ULL - 2*1024*1024; // 2GB - 2MB
            log_always("[native] %p -- %p\n", 
            activated_ptr, 
            (void *)((uintptr_t)activated_ptr + length));

            off_t offset = map_offset + (off_t)2*1024*1024;
            void *want = (void *) VIRTUAL_ADDRESS_LOW;
            log_always("[identity] %p -- %p)\n", want, (void *)((uintptr_t)want + length));

            void *lowShadow = mmap(want, length, prot, (MAP_SHARED | MAP_FIXED), fd, offset);
            if (lowShadow == MAP_FAILED){
                perror("WARNING 1:1 MAPPINGS NOT PRESENT -- mmap LOW FAILED!\n");
            }
        }
        global_ram1_address = activated_ptr;
        dimm_counter++;
        
    } else if (dimm_counter == 1) {
        log_always("========DIMM2========\n");
        if(qemu_map_flags & QEMU_MAP_SHARED){
            // QEMU does not map 2GB-4GB region (MMIO/PCIe space.)
            size_t length = 2*1024*1024*1024ULL; // 4GB - 2GB
            log_always("[native] %p -- %p\n", 
            activated_ptr, 
            (void *)((uintptr_t)activated_ptr + length));

            off_t offset = map_offset;
            void *want = (void *) VIRTUAL_ADDRESS_MED;
            log_always("[identity] %p -- %p)\n", want, (void *)((uintptr_t)want + length));

            if (activated_ptr != want){
                log_always("activated != want! -- Host allocation isn't identity mapped.");
                assert(false);
            }
        }

        // This should be the global ram address (used by sg-listener)
        global_ram_address = activated_ptr;
        global_ram2_address = activated_ptr;
        dimm_counter++;
    } else if (dimm_counter == 2) {
        log_always("========DIMM3========\n");
        if(qemu_map_flags & QEMU_MAP_SHARED){
            size_t length = size; 
            log_always("[native] %p -- %p\n", 
            activated_ptr, 
            (void *)((uintptr_t)activated_ptr + length));

            off_t offset = map_offset;
            void *want = (void *) VIRTUAL_ADDRESS_HIGH;
            log_always("[identity] %p -- %p)\n", want, (void *)((uintptr_t)want + length));

            if (activated_ptr != want){
                log_always("activated != want! -- Host allocation isn't identity mapped.");
                assert(false);
            }
        }

        // After ram is mapped, spawn mmap listener thread
        log_always("[*] Spawing mmap listener thread \n");
        if (!mmap_listen_thr_started) {
            mmap_listen_thr_started = 1;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            for (long i = 0; i < NUM_COMM_PAGES; i++) {
                pthread_create(&mmap_listen_thrs[i], &attr, mmap_listener, (void*)i);
            }
            pthread_attr_destroy(&attr);
        }
        global_ram3_address = activated_ptr;

        log_always("========BACKEND-SETUP\n\n");
        dimm_counter++;
    }
#endif

    g_free(proc_link);
    g_free(file_name);
    return activated_ptr;
}

static inline size_t mmap_guard_pagesize(int fd)
{
#if defined(__powerpc64__) && defined(__linux__)
    /* Mappings in the same segment must share the same page size */
    return qemu_fd_getpagesize(fd);
#else
    return qemu_real_host_page_size();
#endif
}

void *qemu_ram_mmap(int fd,
                    size_t size,
                    size_t align,
                    uint32_t qemu_map_flags,
                    off_t map_offset)
{
    const size_t guard_pagesize = mmap_guard_pagesize(fd);
    // printf("qemu_ram_mmap: page size (%lx)\n", qemu_fd_getpagesize(fd));
    size_t offset, total;
    void *ptr, *guardptr;
    const int dimm_idx = dimm_counter;
    const bool try_identity_native = (qemu_map_flags & QEMU_MAP_SHARED) &&
                                    (dimm_idx == 1 || dimm_idx == 2);
    void *identity_want = NULL;

    /*
     * Note: this always allocates at least one extra page of virtual address
     * space, even if size is already aligned.
     */
    total = size + align;

    if (try_identity_native) {
        identity_want = (void *)(dimm_idx == 1 ? VIRTUAL_ADDRESS_MED : VIRTUAL_ADDRESS_HIGH);

        /* If we can't make ptr==want, we'll fall back to the old 2-mapping setup. */
        if (align && is_power_of_2(align)) {
            assert(((uintptr_t)identity_want & (align - 1)) == 0);
        }

        guardptr = mmap_reserve_at(identity_want, total, fd);
        if (guardptr == MAP_FAILED) {
            guardptr = mmap_reserve(total, fd);
        }
    } else {
        guardptr = mmap_reserve(total, fd);
    }
    if (guardptr == MAP_FAILED) {
        return MAP_FAILED;
    }

    assert(is_power_of_2(align));
    /* Always align to host page size */
    assert(align >= guard_pagesize);

    offset = QEMU_ALIGN_UP((uintptr_t)guardptr, align) - (uintptr_t)guardptr;

    ptr = mmap_activate(guardptr + offset, size, fd, qemu_map_flags,
                        map_offset);
    if (ptr == MAP_FAILED) {
        munmap(guardptr, total);
        return MAP_FAILED;
    }

    if (offset > 0) {
        munmap(guardptr, offset);
    }

    /*
     * Leave a single PROT_NONE page allocated after the RAM block, to serve as
     * a guard page guarding against potential buffer overflows.
     */
    total -= offset;
    if (total > size + guard_pagesize) {
        munmap(ptr + size + guard_pagesize, total - size - guard_pagesize);
    }

    /*
     * For identity-native DIMM2 at 0x100000000, drop its guard page so DIMM3
     * can start exactly at 0x180000000.
     */
    if (try_identity_native && dimm_idx == 1 && ptr == identity_want) {
        munmap(ptr + size, guard_pagesize);
        guard_len_record(ptr, 0);
    } else {
        guard_len_record(ptr, guard_pagesize);
    }

    return ptr;
}

void qemu_ram_munmap(int fd, void *ptr, size_t size)
{
    if (ptr) {
        const size_t default_guard = mmap_guard_pagesize(fd);
        const size_t guard_len = guard_len_lookup(ptr, default_guard);
        /* Unmap the RAM block and (optionally) the guard page */
        munmap(ptr, size + guard_len);
    }
}
