# Hello World

This directory contains a Makefile and a manifest template for running a simple
"Hello World" program in Gramine. It can be used as a sanity test for your
Gramine installation.

# Building

## Building for Linux

Run `make` (non-debug) or `make DEBUG=1` (debug) in the directory.

## Building for SGX

Run `make SGX=1` (non-debug) or `make SGX=1 DEBUG=1` (debug) in the directory.

# Run Hello World with Gramine

Without SGX:
```sh
gramine-direct helloworld
```

With SGX:
```sh
gramine-sgx helloworld
```

## VM startup stress test

With the host Xwayland proxy already running, generate the manifests and run ten
fresh VM instances:

```sh
make
./startup-stress.sh
```

Each instance uses a one-second automatic profile race and exits after STK
prints its FPS result. A run is considered successful only if at least one race
frame was measured. Per-attempt logs are retained under
`startup-stress-logs/`; crashes and startup timeouts are summarized on stderr.

Pass a different attempt count as the first argument. Heap perturbation and
shader-cache disabling are opt-in so the default run matches the normal STK
environment:

```sh
./startup-stress.sh 20
STK_MALLOC_DIAGNOSTICS=1 ./startup-stress.sh 10
STK_DISABLE_SHADER_CACHE=1 ./startup-stress.sh 10
```
