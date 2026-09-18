#!/usr/bin/env python3
"""
generate_with_libclang.py — final version

✔ Generates LD_PRELOAD-style wrappers using libclang.
✔ Handles function-pointer and array parameters correctly.
✔ Skips complex return types safely.
✔ Adds per-function logging only when env var X11WRAP_LOG is set.
"""

from __future__ import annotations
import sys, os, argparse, shlex, re
from typing import List

try:
    from clang import cindex
except Exception as e:
    print("ERROR: clang.cindex not importable. Install python clang bindings and ensure libclang is available.", file=sys.stderr)
    print(" e.g. pip install clang  OR apt install python3-clang libclang-dev clang", file=sys.stderr)
    raise


# --- Helpers --------------------------------------------------------------

_funcptr_pattern = re.compile(r'^(?P<ret>.+?)\s*\(\s*\*\s*\)\s*(?P<rest>\(.*\))\s*$')
_array_trailing_pattern = re.compile(r'^(?P<base>.+?)(?P<dims>(?:\s*\[[^\]]+\])+)\s*$')

def type_contains_pointer(tsp: str) -> bool:
    return "*" in tsp

def is_exact_void(tsp: str) -> bool:
    return tsp.strip() == "void"

def escape_string(s: str) -> str:
    return s.replace('"', '\\"')

def param_needs_review(param_type_spelling: str) -> bool:
    return ("(*)" in param_type_spelling) or ("[" in param_type_spelling)

def return_type_is_complex(ret_spelling: str) -> bool:
    s = ret_spelling.strip()
    if "(*" in s or ("(" in s and ")" in s and "*" in s):
        return True
    if "(" in s or ")" in s:
        return True
    return False

def render_type_spelling(tsp: str) -> str:
    return tsp

def format_param_decl(param_type_spelling: str, param_name: str) -> str:
    """Formats parameters with correct placement for function-pointer and array types."""
    s = param_type_spelling.strip()
    m_fp = _funcptr_pattern.match(s)
    if m_fp:
        ret = m_fp.group("ret").strip()
        rest = m_fp.group("rest").strip()
        return f"{ret} (*{param_name}){rest}"

    if "(*" in s and "*)" in s:
        return s.replace("(*)", f"(*{param_name})").replace("(* )", f"(*{param_name})")

    m_arr = _array_trailing_pattern.match(s)
    if m_arr:
        base = m_arr.group("base").strip()
        dims = m_arr.group("dims").replace(" ", "")
        return f"{base} {param_name}{dims}"

    return f"{s} {param_name}"


# --- Clang AST traversal --------------------------------------------------

def gather_functions(tu):
    funcs = []
    def walk(node):
        if node.kind == cindex.CursorKind.FUNCTION_DECL:
            funcs.append(node)
        for ch in node.get_children():
            walk(ch)
    walk(tu.cursor)
    return funcs


# --- Code generation ------------------------------------------------------

def generate_wrapper_for_fn(fn_cursor: cindex.Cursor) -> str:
    name = fn_cursor.spelling
    ret_type = fn_cursor.result_type.spelling
    if fn_cursor.type.is_function_variadic():
        return f"/* SKIPPED: {name} -- variadic function (requires manual wrapper) */\n\n"
    if return_type_is_complex(ret_type):
        return f"/* SKIPPED: {name} -- complex return type '{ret_type}' */\n\n"

    params = [(p.type.spelling, p.spelling or f"arg{i}") for i, p in enumerate(fn_cursor.get_arguments())]
    typedef_name = f"orig_{name}_t"

    typedef_params = ", ".join(format_param_decl(render_type_spelling(pt), pn) for pt, pn in params) or "void"
    typedef_line = f"typedef {render_type_spelling(ret_type)} (*{typedef_name})({typedef_params});\n"

    wrapper_params_sig = ", ".join(format_param_decl(render_type_spelling(pt), pn) for pt, pn in params) or "void"
    call_param_names = ", ".join(pn for _, pn in params)

    lines = [typedef_line, "\n"]
    lines.append(f"{render_type_spelling(ret_type)} {name}({wrapper_params_sig}) {{\n")
    lines.append(f"    static {typedef_name} orig = NULL;\n")
    lines.append(f"    if (!orig) orig = ({typedef_name})dlsym(RTLD_NEXT, \"{escape_string(name)}\");\n")
    lines.append(f"    /* conditional logging */\n")
    lines.append(f"    if (x11wrap_log_enabled) safe_log(\"[wrap] {name} called\");\n")
    
    for pt, pn in params:
        if param_needs_review(pt):
            lines.append(f"    /* REVIEW: parameter '{pn}' has complex type '{pt}' */\n")
            break

    lines.append("    if (!orig) {\n")
    if is_exact_void(ret_type):
        lines.append("        return;\n")
    elif type_contains_pointer(ret_type):
        lines.append(f"        return ({ret_type})NULL;\n")
    else:
        lines.append(f"        return ({ret_type})0;\n")
    lines.append("    }\n\n")

    if is_exact_void(ret_type):
        lines.append(f"    orig({call_param_names});\n" if call_param_names else "    orig();\n")
        lines.append("    return;\n")
    else:
        lines.append(f"    return orig({call_param_names});\n" if call_param_names else "    return orig();\n")
    lines.append("}\n\n")
    return "".join(lines)


# --- CLI / main -----------------------------------------------------------

def normalize_clang_args(arglist: List[str], args_str: str | None) -> List[str]:
    out = []
    for it in arglist:
        if isinstance(it, (list, tuple)):
            out.extend(filter(None, it))
        elif it:
            out.append(it)
    if args_str:
        out.extend(shlex.split(args_str))
    return [a for a in out if a.strip()]


def main(argv: List[str]):
    ap = argparse.ArgumentParser(description="Generate LD_PRELOAD wrappers using libclang.")
    ap.add_argument("header", help="Header to parse, e.g. /usr/include/X11/Xlib.h")
    ap.add_argument("--out", "-o", default="generated_wrappers.c")
    ap.add_argument("--clang-arg", action="append", nargs=1, default=[])
    ap.add_argument("--clang-args", type=str, default=None)
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args(argv[1:])

    if not os.path.exists(args.header):
        sys.exit(f"Header not found: {args.header}")

    clang_args = normalize_clang_args(args.clang_arg, args.clang_args)
    if not any(a.startswith("-I") for a in clang_args):
        clang_args.append("-I/usr/include")
    if not any(a.startswith("-D_XOPEN_SOURCE") for a in clang_args):
        clang_args.append("-D_XOPEN_SOURCE=700")

    if args.verbose:
        print("Using clang args:", clang_args, file=sys.stderr)

    index = cindex.Index.create()
    tu = index.parse(args.header, args=clang_args)
    for d in tu.diagnostics:
        print("clang diag:", d.spelling, file=sys.stderr)
    funcs = gather_functions(tu)
    if args.verbose:
        print(f"Found {len(funcs)} functions", file=sys.stderr)

    out_lines = [
        "/* AUTO-GENERATED by generate_with_libclang.py */\n",
        "#define _GNU_SOURCE\n",
        "#include <stdio.h>\n#include <dlfcn.h>\n#include <stdlib.h>\n#include <stdarg.h>\n#include <stddef.h>\n",
        f"#include <{os.path.basename(args.header)}>\n\n",
        "static int x11wrap_log_enabled = 0;\n\n"
        "static void safe_log(const char *fmt, ...) {\n",
        "    if(getenv(\"X11WRAP_LOG\")) {\n",
        "       va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); fprintf(stderr, \"\\n\"); va_end(ap);\n",
        "    }\n"
        "}\n\n",
        "/* Wrappers follow */\n\n"
    ]

    for fn in funcs:
        try:
            out_lines.append(generate_wrapper_for_fn(fn))
        except Exception as e:
            out_lines.append(f"/* ERROR generating wrapper for {fn.spelling}: {e} */\n\n")

    # out_lines.append(
    #     "__attribute__((constructor)) static void _wrap_init(void) { if(getenv(\"X11WRAP_LOG\")) safe_log(\"[wrap] wrapper library loaded\"); }\n"
    #     "__attribute__((destructor)) static void _wrap_fini(void) { if(getenv(\"X11WRAP_LOG\")) safe_log(\"[wrap] wrapper library unloaded\"); }\n"
    # )

    out_lines.append("__attribute__((constructor)) static void _wrap_init(void) {\n")
    out_lines.append("    char *e = getenv(\"X11WRAP_LOG\");\n")
    out_lines.append("    x11wrap_log_enabled = (e && !(e[0]=='0' && e[1]=='\\0'));\n")
    out_lines.append("    if (x11wrap_log_enabled) safe_log(\"[wrap] wrapper library loaded\");\n")
    out_lines.append("}\n\n")
    out_lines.append("__attribute__((destructor)) static void _wrap_fini(void) {\n")
    out_lines.append("    if (x11wrap_log_enabled) safe_log(\"[wrap] wrapper library unloaded\");\n")
    out_lines.append("}\n\n")

    with open(args.out, "w") as f:
        f.write("".join(out_lines))

    print(f"Wrote {args.out}", file=sys.stderr)
    print("Compile:\n  gcc -shared -fPIC -o libx11wrap.so {out} -ldl -I/usr/include -I/usr/include/X11".format(out=args.out), file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv)

