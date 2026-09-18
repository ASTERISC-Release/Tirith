#!/bin/bash -e

sudo bpftrace -e '
tracepoint:syscalls:sys_enter_ioctl
/1/
{
  @start[tid] = nsecs;
  @cmd[tid] = args->cmd;
  @fd[tid] = args->fd;
}

tracepoint:syscalls:sys_exit_ioctl
/@start[tid]/
{
  $dur = (nsecs - @start[tid]) / 1000;    // microseconds
  $cmd = @cmd[tid];
  $fd  = @fd[tid];

  // tune threshold (us) as needed -- set to 50us here
  if ($dur > 220) {
    printf("IOCTL slow: pid=%d tid=%d fd=%d cmd=0x%x dur=%d us\n", pid, tid, $fd, $cmd, $dur);

    // print kernel/user stacks if available
    printf("=== KERNEL STACK ===\n");
    kstack();
    printf("=== USER STACK ===\n");
    ustack();
    printf("--------------------\n");
  }

  delete(@start[tid]);
  delete(@cmd[tid]);
  delete(@fd[tid]);
}
'
