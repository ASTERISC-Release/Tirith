import os
import re
import subprocess
import argparse


def run_opt_and_extract_accesses(opt_path, plugin_path, bitcode_path, start_fn):
    # Construct the command
    cmd = [
        opt_path,
        f"-load-pass-plugin={plugin_path}",
        f"-passes=gl-access-tracker",
        f"-start-fn={start_fn}",
        
        bitcode_path
    ]

    # Run the command
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,errors='ignore')  # <== this line ignores invalid bytes)

    accesses = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if line.startswith("==> accesses "):
            accessed = line.split("==> accesses ", 1)[1]
            accesses.append(accessed)

    return accesses

# Example usage
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run gl-access-tracker pass and extract accessed functions.")
    parser.add_argument("--start-fn", required=True, help="Start function name (e.g., _mesa_DrawElements)")
    
    args = parser.parse_args()
    accessed_funcs = run_opt_and_extract_accesses(
        opt_path="opt",  # or the full path to your `opt` binary
        plugin_path="./libGlAccessTracker.so",
        bitcode_path="../build/linked.ll",
        start_fn=args.start_fn
    )


    # Regex template
    assignment_re = re.compile(r'\b\w+->(?P<func>\w+)\s*=\s*(?P<target>\w+)\s*;')

    # Set directory to scan
    directory = "../src/gallium/drivers/iris/"  # Replace with your actual source directory

    # Store results
    results = {}

    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(".c"):
                path = os.path.join(root, file)
                with open(path, 'r', errors='ignore') as f:
                    for line in f:
                        match = assignment_re.search(line)
                        if match:
                            func = match.group('func')
                            target = match.group('target')
                            if func in accessed_funcs:
                                results.setdefault(func, set()).add(target)

    # Print results


    for func, targets in sorted(results.items()):
        for t in sorted(targets):
            cmd = [
                "opt",
                "-load-pass-plugin", "./libGlIoctlAccessTracker.so",
                "-passes=gl-access-tracker",
                f"-gl-start-fn={t}",
                "../build/linked.ll"
            ]
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, check=True, encoding='utf-8', errors='ignore')

                output = result.stdout + result.stderr
                if "calls ioctl" in output:
                    print(f"{func} -> {t}")
            except subprocess.CalledProcessError as e:
                print(f"Error running command for {func}: {e}")

