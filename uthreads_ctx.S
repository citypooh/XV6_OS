# uthreads_ctx.S
# low-level context switch for user threads

.globl thread_switch
thread_switch:
  # prologue
  pushl %ebp
  movl %esp, %ebp

  # save callee-saved registers
  pushl %ebx
  pushl %esi
  pushl %edi

  # save current esp into old->sp
  movl 8(%ebp), %eax      # eax = old
  movl %esp, (%eax)       # old->sp = esp

  # load next->sp into esp
  movl 12(%ebp), %edx     # edx = next
  movl (%edx), %esp       # esp = next->sp

  # restore callee-saved registers of next
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp

  ret
