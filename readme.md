# xv6 User-Level Threading Library

**Team Members**
- Seongjae Jung (sj4771)
- David Hong (sh8348)

---

## Overview

This is an user-level threading library for xv6.

For detailed documentation with diagrams and screenshots, see the PDF submitted to Brightspace.

---

## Files

**Core Library**
- `uthreads.h` — Public API (thread, mutex, cond, semaphore, channel)
- `uthreads_private.h` — Internal structures and constants
- `uthreads_core.c` — Thread lifecycle and scheduler
- `uthreads_sync.c` — Mutex, condition variable, semaphore
- `uthreads_channel.c` — Channel implementation
- `uthreads_ctx.S` — Context switch (x86 assembly)

**Extra Credit: Thread-Safe I/O**
- `safeio.h`, `safeio.c`, `safeio_private.h`

**Test Programs**
- `test_final_part1.c` — Basic threading
- `test_final_part2.c` — Mutex, condition variable, channel
- `test_final_part3_producer_consumer.c` — Producer-consumer with channels
- `test_final_part3_producer_consumer_sem.c` — Producer-consumer with semaphores
- `test_final_part3_reader_writer.c` — Reader-writer lock
- `test_final_extra.c` — Thread-safe file I/O demo

---

## Thread Structure

Each thread has: saved stack pointer, allocated stack, thread ID, state, start function and argument, return value, a joiner pointer (for join), and a wait queue link.

We used 64 max threads and 8KB stack per thread.

---

## Thread States

Five states: UNUSED (slot available), RUNNABLE (ready to run), RUNNING (currently executing), SLEEPING (blocked on something), ZOMBIE (finished but not joined).

Transitions: create sets RUNNABLE, scheduler picks one to be RUNNING, yield goes back to RUNNABLE, blocking primitives set SLEEPING, exit sets ZOMBIE, join cleans up to UNUSED.

---

## Context Switch

Written in x86 assembly. Saves callee-saved registers (ebx, esi, edi, ebp) and the stack pointer to the old thread's structure, loads the new thread's stack pointer, restores its registers, and returns. New threads have their stack set up so the first return jumps to a trampoline that calls the start function.

---

## Scheduler

Simple round-robin. Scans from the current thread's position, finds the next RUNNABLE thread, switches to it. If nothing else is runnable, keeps running the current thread. Threads cooperate by calling yield or blocking on synchronization primitives.

---

## Mutex

Tracks locked state, owner, and a wait queue. Lock checks if free — if so, grab it. If not, add self to wait queue, set state to SLEEPING, and schedule another thread. Unlock passes ownership directly to the next waiter if any, otherwise just releases.

---

## Shared Counter Test

We test with and without mutex. Four threads each increment a counter 1000 times. Without mutex, the final count is often less than 4000 due to lost updates (read-modify-write race). With mutex, it's always exactly 4000.

---

## Condition Variables

Just a wait queue. Wait adds the thread to the queue, releases the mutex, sleeps, then re-acquires the mutex when woken. Signal wakes one waiter. Broadcast wakes all.

---

## Semaphores

Tracks a value and a wait queue. Wait decrements value, blocks if negative. Post increments value, wakes one waiter if value was negative.

---

## Channels

Bounded buffer with mutex and two condition variables (not_empty, not_full). Send blocks if full, recv blocks if empty. Close wakes all waiters so they can exit cleanly.

---

## Producer-Consumer (Semaphores)

Three producers, two consumers, buffer size 5. Uses empty_slots and full_slots semaphores plus a buffer mutex. Producers wait on empty_slots, consumers wait on full_slots. Sentinel values signal consumers to exit after all items are done.

---

## Producer-Consumer (Channels)

Same setup but using a channel. Much simpler — the channel handles blocking internally. Close the channel when producers are done, consumers exit when recv returns -1.

---

## Reader-Writer Lock

Uses mutex and two condition variables. Tracks active readers, waiting writers, and whether a writer is active. Writer priority: readers won't start if writers are waiting. This prevents writer starvation.

---

## Thread-Safe File I/O

Problem: xv6 read/write block the whole process. Solution: a dedicated I/O worker thread. User threads send requests through a channel and wait on a condition variable. The worker does the actual syscall and signals completion. This lets other threads run while I/O is pending.

Demo forks two processes — one writes to a file, one reads. Shows interleaved output.

---

## Building and Running

    make clean && make qemu-nox

In xv6 shell:

    t_part1
    t_part2
    t_part3_pc
    t_part3_pc_sem
    t_part3_rw
    t_extra

---

## Summary

Implemented: thread states/structure, context switching, round-robin scheduler, mutex, condition variables, semaphores, channels, producer-consumer (both versions), reader-writer lock with writer priority, thread-safe file I/O.

---
