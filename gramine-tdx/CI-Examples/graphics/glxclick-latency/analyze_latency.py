#!/usr/bin/env python3

import argparse
import csv
import math
import re
import statistics
import sys
from pathlib import Path

INJECT_RE = re.compile(r"click_inject_left_press_ns=(\d+) index=(\d+)")
NATIVE_TIMING_RE = re.compile(
    r"native_input_timing delivery_ns=(\d+) action_ns=(\d+) index=(\d+)"
)
GRAMINE_TIMING_RE = re.compile(
    r"gramine_input_timing index=(\d+) receive_ns=(\d+) receive_tsc=(\d+) "
    r"publish_tsc=(\d+) delivery_tsc=(\d+) action_tsc=(\d+) "
    r"delivered_sequence=(\d+) tsc_khz=(\d+)"
)


def read_samples(path: Path, pattern: re.Pattern[str]) -> dict[int, int]:
    samples: dict[int, int] = {}
    for line in path.read_text(errors="replace").splitlines():
        match = pattern.search(line)
        if not match:
            continue
        timestamp = int(match.group(1))
        index = int(match.group(2))
        if index in samples:
            raise ValueError(f"duplicate sample {index} in {path}")
        samples[index] = timestamp
    return samples


def read_gramine_timings(path: Path) -> dict[int, tuple[int, ...]]:
    samples: dict[int, tuple[int, ...]] = {}
    for line in path.read_text(errors="replace").splitlines():
        match = GRAMINE_TIMING_RE.search(line)
        if not match:
            continue
        index = int(match.group(1))
        if index in samples:
            raise ValueError(f"duplicate Gramine timing {index} in {path}")
        samples[index] = tuple(int(value) for value in match.groups()[1:])
    return samples


def read_native_timings(path: Path) -> dict[int, tuple[int, int]]:
    samples: dict[int, tuple[int, int]] = {}
    for line in path.read_text(errors="replace").splitlines():
        match = NATIVE_TIMING_RE.search(line)
        if not match:
            continue
        delivery_ns, action_ns, index = (int(value) for value in match.groups())
        if index in samples:
            raise ValueError(f"duplicate native timing {index} in {path}")
        samples[index] = (delivery_ns, action_ns)
    return samples


def validate_indices(expected_count: int, streams: dict[str, dict[int, object]]) -> bool:
    expected = set(range(expected_count))
    valid = True
    for name, samples in streams.items():
        missing = sorted(expected - samples.keys())
        unexpected = sorted(samples.keys() - expected)
        if missing or unexpected:
            print(f"error: {name} stream does not match", file=sys.stderr)
            print(f"  missing: {missing}", file=sys.stderr)
            print(f"  unexpected: {unexpected}", file=sys.stderr)
            valid = False
    return valid


def percentile(sorted_values: list[float], fraction: float) -> float:
    # Nearest-rank is stable and easy to compare across experiment runs.
    rank = max(1, math.ceil(fraction * len(sorted_values)))
    return sorted_values[rank - 1]


def summarize(values: list[float]) -> dict[str, float]:
    ordered = sorted(values)
    trim_count = len(ordered) // 100
    trimmed = ordered[trim_count:-trim_count] if trim_count else ordered
    return {
        "Average": statistics.fmean(ordered),
        "Trimmed avg": statistics.fmean(trimmed),
        "Median": statistics.median(ordered),
        "P95": percentile(ordered, 0.95),
        "P99": percentile(ordered, 0.99),
        "Min": ordered[0],
        "Max": ordered[-1],
        "Stddev": statistics.pstdev(ordered),
    }


def cycles_to_ns(cycles: int, frequency_khz: int) -> float:
    return cycles * 1_000_000.0 / frequency_khz


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze native and Gramine click-to-action latency"
    )
    parser.add_argument("expected_count", type=int)
    parser.add_argument(
        "--run",
        action="append",
        nargs=4,
        type=Path,
        required=True,
        metavar=("NATIVE_INJECT", "NATIVE_APP", "GRAMINE_INJECT", "GRAMINE_APP"),
        help="add one paired native/Gramine round",
    )
    parser.add_argument("--csv-output", type=Path)
    args = parser.parse_args()

    if args.expected_count <= 0:
        parser.error("expected_count must be positive")

    rows = []
    for round_index, run_paths in enumerate(args.run, start=1):
        (native_inject_log, native_log,
         gramine_inject_log, gramine_log) = run_paths
        try:
            native_injected = read_samples(native_inject_log, INJECT_RE)
            native_timings = read_native_timings(native_log)
            gramine_injected = read_samples(gramine_inject_log, INJECT_RE)
            gramine_timings = read_gramine_timings(gramine_log)
        except (OSError, ValueError) as error:
            print(f"error: round {round_index}: {error}", file=sys.stderr)
            return 1

        streams = {
            f"round {round_index} native injection": native_injected,
            f"round {round_index} native timing": native_timings,
            f"round {round_index} Gramine injection": gramine_injected,
            f"round {round_index} Gramine timing": gramine_timings,
        }
        if not validate_indices(args.expected_count, streams):
            return 1

        run_order = "native-first" if round_index % 2 else "Gramine-first"
        for index in range(args.expected_count):
            native_delivery_ns, native_action_ns = native_timings[index]
            native_window_ns = native_delivery_ns - native_injected[index]
            native_game_ns = native_action_ns - native_delivery_ns
            native_ns = native_action_ns - native_injected[index]
            if native_window_ns < 0 or native_game_ns < 0:
                print(
                    f"error: invalid native timestamps for round {round_index} "
                    f"click {index}",
                    file=sys.stderr,
                )
                return 1

            (receive_ns, receive_tsc, publish_tsc, delivery_tsc, action_tsc,
             delivered_sequence, frequency_khz) = gramine_timings[index]
            if delivered_sequence != index + 1:
                print(
                    f"error: round {round_index} click {index} has delivery "
                    f"sequence {delivered_sequence}",
                    file=sys.stderr,
                )
                return 1
            if frequency_khz == 0 or not (
                receive_tsc <= publish_tsc <= delivery_tsc <= action_tsc
            ):
                print(
                    f"error: invalid Gramine timestamps for round {round_index} "
                    f"click {index}",
                    file=sys.stderr,
                )
                return 1

            window_qemu_ns = receive_ns - gramine_injected[index]
            if window_qemu_ns < 0:
                print(
                    f"error: negative window-to-QEMU latency for round "
                    f"{round_index} click {index}",
                    file=sys.stderr,
                )
                return 1
            qemu_processing_ns = cycles_to_ns(
                publish_tsc - receive_tsc, frequency_khz
            )
            qemu_gramine_ns = cycles_to_ns(
                delivery_tsc - publish_tsc, frequency_khz
            )
            gramine_game_ns = cycles_to_ns(
                action_tsc - delivery_tsc, frequency_khz
            )
            gramine_ns = (
                window_qemu_ns + qemu_processing_ns +
                qemu_gramine_ns + gramine_game_ns
            )
            gramine_action_ns = receive_ns + cycles_to_ns(
                action_tsc - receive_tsc, frequency_khz
            )
            rows.append({
                "round": round_index,
                "run_order": run_order,
                "index": index,
                "native_inject_ns": native_injected[index],
                "native_delivery_ns": native_delivery_ns,
                "native_action_ns": native_action_ns,
                "native_ns": float(native_ns),
                "native_window_ns": float(native_window_ns),
                "native_game_ns": float(native_game_ns),
                "gramine_inject_ns": gramine_injected[index],
                "gramine_action_ns": gramine_action_ns,
                "gramine_ns": gramine_ns,
                "window_qemu_ns": float(window_qemu_ns),
                "qemu_processing_ns": qemu_processing_ns,
                "qemu_gramine_ns": qemu_gramine_ns,
                "gramine_game_ns": gramine_game_ns,
                "receive_tsc": receive_tsc,
                "publish_tsc": publish_tsc,
                "delivery_tsc": delivery_tsc,
                "action_tsc": action_tsc,
                "tsc_khz": frequency_khz,
            })

    native_stats = summarize([row["native_ns"] for row in rows])
    gramine_stats = summarize([row["gramine_ns"] for row in rows])
    print("END-TO-END CLICK-TO-ACTION LATENCY")
    print(
        f"Samples: {len(rows)} per runtime across {len(args.run)} round(s) "
        f"({args.expected_count} per runtime per round)"
    )
    print("The observed delta compares separate runs and may be negative from runtime variance.")
    print(f"{'Metric':<12} {'Native':>12} {'Gramine':>12} {'Observed delta':>16}")
    for metric in native_stats:
        native_ms = native_stats[metric] / 1_000_000.0
        gramine_ms = gramine_stats[metric] / 1_000_000.0
        print(
            f"{metric:<12} {native_ms:>9.3f} ms {gramine_ms:>9.3f} ms "
            f"{gramine_ms - native_ms:>+9.3f} ms"
        )

    outlier_threshold_ns = 1_000_000.0
    native_outliers = sum(row["native_ns"] > outlier_threshold_ns for row in rows)
    gramine_outliers = sum(row["gramine_ns"] > outlier_threshold_ns for row in rows)
    print(
        f"Outliers > 1 ms: native {native_outliers}/{len(rows)}, "
        f"Gramine {gramine_outliers}/{len(rows)}"
    )

    if len(args.run) > 1:
        round_native_medians = []
        round_gramine_medians = []
        round_deltas = []
        print("\nPER-ROUND MEDIAN LATENCY")
        print(
            f"{'Round':>5} {'Order':<14} {'Native':>12} "
            f"{'Gramine':>12} {'Observed delta':>16}"
        )
        for round_index in range(1, len(args.run) + 1):
            round_rows = [row for row in rows if row["round"] == round_index]
            native_median = statistics.median(
                row["native_ns"] for row in round_rows
            )
            gramine_median = statistics.median(
                row["gramine_ns"] for row in round_rows
            )
            delta = gramine_median - native_median
            round_native_medians.append(native_median)
            round_gramine_medians.append(gramine_median)
            round_deltas.append(delta)
            print(
                f"{round_index:>5} {round_rows[0]['run_order']:<14} "
                f"{native_median / 1_000_000:>9.3f} ms "
                f"{gramine_median / 1_000_000:>9.3f} ms "
                f"{delta / 1_000_000:>+9.3f} ms"
            )
        print(
            f"Median of round medians: native "
            f"{statistics.median(round_native_medians) / 1_000_000:.3f} ms, "
            f"Gramine "
            f"{statistics.median(round_gramine_medians) / 1_000_000:.3f} ms; "
            f"median paired delta "
            f"{statistics.median(round_deltas) / 1_000_000:+.3f} ms"
        )

    native_segments = {
        "Window -> native app": [row["native_window_ns"] for row in rows],
        "Native app -> action": [row["native_game_ns"] for row in rows],
    }
    print("\nNATIVE END-TO-END BREAKDOWN")
    print(
        f"{'Stage':<22} {'Average':>10} {'Median':>10} {'P95':>10} "
        f"{'P99':>10} {'Min':>10} {'Max':>10}"
    )
    for name, values in native_segments.items():
        stats = summarize(values)
        print(
            f"{name:<22} {stats['Average'] / 1000:>8.3f} us "
            f"{stats['Median'] / 1000:>8.3f} us "
            f"{stats['P95'] / 1000:>8.3f} us "
            f"{stats['P99'] / 1000:>8.3f} us "
            f"{stats['Min'] / 1000:>8.3f} us "
            f"{stats['Max'] / 1000:>8.3f} us"
        )

    segments = {
        "Window -> QEMU": [row["window_qemu_ns"] for row in rows],
        "QEMU processing": [row["qemu_processing_ns"] for row in rows],
        "QEMU -> Gramine": [row["qemu_gramine_ns"] for row in rows],
        "Gramine -> game": [row["gramine_game_ns"] for row in rows],
    }
    print("\nGRAMINE END-TO-END BREAKDOWN")
    print(
        f"{'Stage':<20} {'Average':>10} {'Median':>10} {'P95':>10} "
        f"{'P99':>10} {'Min':>10} {'Max':>10}"
    )
    for name, values in segments.items():
        stats = summarize(values)
        print(
            f"{name:<20} {stats['Average'] / 1000:>8.3f} us "
            f"{stats['Median'] / 1000:>8.3f} us "
            f"{stats['P95'] / 1000:>8.3f} us "
            f"{stats['P99'] / 1000:>8.3f} us "
            f"{stats['Min'] / 1000:>8.3f} us "
            f"{stats['Max'] / 1000:>8.3f} us"
        )

    vm_transport = [
        row["qemu_processing_ns"] + row["qemu_gramine_ns"] for row in rows
    ]
    transport_stats = summarize(vm_transport)
    print("\nVM-SPECIFIC INPUT TRANSPORT")
    print("QEMU processing + shared-memory delivery into Gramine")
    print(
        f"Average {transport_stats['Average'] / 1000:.3f} us, "
        f"median {transport_stats['Median'] / 1000:.3f} us, "
        f"P95 {transport_stats['P95'] / 1000:.3f} us, "
        f"P99 {transport_stats['P99'] / 1000:.3f} us"
    )

    if args.csv_output:
        args.csv_output.parent.mkdir(parents=True, exist_ok=True)
        fields = list(rows[0].keys())
        with args.csv_output.open("w", newline="") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)
        print(f"\nCSV: {args.csv_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
