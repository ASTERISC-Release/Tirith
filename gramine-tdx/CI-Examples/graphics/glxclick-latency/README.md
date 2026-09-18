# Shared-input click latency

This experiment measures click-to-action latency for native execution and a
Gramine VM. A `uinput` virtual mouse generates left-button clicks. The measured
action increments application state in the button handler.

The native application is pinned to one CPU. The Gramine VM uses one vCPU.

## Measurements

Native latency is split into:

- input injection to `XNextEvent` returning the button event;
- event delivery to the application action.

Gramine latency is split into:

- input injection to `xcb_wait_for_event` returning in QEMU;
- QEMU event processing;
- shared-memory delivery from QEMU to Gramine;
- Gramine event delivery to the application action.

QEMU timestamps the host side with `CLOCK_MONOTONIC_RAW` and the TSC. Host TSC
values are translated into the guest TSC domain with KVM's per-vCPU offset and
frequency.

The report includes pooled end-to-end statistics, a 1%-trimmed average,
outliers above 1 ms, native and Gramine stage breakdowns, and VM-specific input
transport. Repeated runs also report per-round medians and paired deltas.

Rendering, presentation, scanout, physical mouse, and USB latency are not
measured.

## Run

Build QEMU after changing its latency instrumentation:

```sh
cd /root/Tirith/qemu
./qemu-install.sh
```

Start the host X11-to-VSOCK proxy, then run the experiment inside the Podman
environment:

```sh
cd /root/Tirith/gramine-tdx/CI-Examples/graphics/glxclick-latency
make

./run-experiment.sh
./run-experiment.sh 1000
./run-experiment.sh -r 5 1000
./run-experiment.sh -csv -r 5 1000
```

`click-count` defaults to 100 clicks per runtime per round. `-r N` runs N
rounds, alternating native-first and Gramine-first order. `-csv` writes
per-click data to `results/<timestamp>/latency.csv`. The statistical report is
printed to stdout.

Clicks are 50 ms apart. `CLICK_LATENCY_INTERVAL_MS` changes the interval, and
`CLICK_LATENCY_CPU` selects the native application's host CPU.

`run-experiment.sh` sets `SG_INPUT_LATENCY=1` for the Gramine run. This enables
the QEMU and Gramine timing fields and waits for the application's action
acknowledgment before recording each sample.
