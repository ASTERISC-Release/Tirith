#!/bin/bash -e

sudo bpftrace -e 'kprobe:drm_clflush_virt_range { @[comm]++; } interval:s:5 { print(@); clear(@); }'
