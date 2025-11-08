# Homework 3

**NetID of first partner :** sj4771  
**NetID of second partner :** sh8348  
**Full Name of first partner :** Seongjae Jung  
**Full Name of second partner :** David Hong  
**Github Account Name of first partner :** seongjaeny  
**Github Account Name of second partner :** citypooh  


---

## Overview

This project extends the xv6 operating system with:

1. A `nice` system call and user utility.
2. A 5-level priority-based Round Robin scheduler.
3. Priority Inheritance to resolve the priority inversion problem through new `lock()` / `release()` system calls.

---

## Implementation Summary

### 1. Nice System Call

- Added a system call to change a process’s nice value.
- `nice` values range 0–4, where 0 is the highest priority.
- Default new process `nice = 2`.
- Returns the old nice value for the given PID.

**Files Modified**
- `sysproc.c`: added `sys_nice()`
- `syscall.c`, `syscall.h`, `user.h`, `usys.S`: registered syscall
- `proc.c`, `proc.h`: added `nice` and `orig_nice` fields

**Usage**
```bash
$ nice <pid> <value>
$ nice <value>      # applies to current process
```

**Example**
```bash
$ nice 1 4
1 3    # pid=1, old nice=3
```

---

### 2. Priority-Based Round Robin Scheduler

- Controlled via `param.h`:
  ```c
  #define SCHED_PRIORITY 1   // enable priority-based RR
  ```
- Scheduler picks lowest nice (highest priority) first.
- Among equal nice levels → Round Robin.
- Default policy remains Round Robin if flag = 0.

**Files Modified**
- `proc.c`: scheduler section under `#if SCHED_PRIORITY`
- `param.h`: added NICE constants and scheduler flag

---

### 3. Priority Inheritance (Extra Credit)

Implements real-time-safe locking to prevent priority inversion.

**System Calls**
```c
int lock(int id);
int release(int id);
```

- Valid lock IDs: 1–7
- Invalid IDs (0 or >7) return -1
- Locks are globally shared via `struct lock_t locks[MAX_LOCKS]`

**Mechanism**
When a high-priority process waits on a lock held by a low-priority process:
- The owner temporarily inherits the higher priority.
- Once the lock is released, the owner’s priority is restored (or recomputed if holding multiple locks).

**Core Fixes**
| Issue | Fix |
|-------|-----|
| Deadlock during release | Used `wakeup1()` (since `ptable.lock` already held) |
| Lock ID ambiguity | Only IDs 1–7 valid, others return -1 |
| Priority restoration | Scan all locks owned by process and recompute effective nice |
| Exit safety | Release all held locks and wake up waiters |

**Files Modified**
- `proc.h`: added `lock_t` struct and prototypes
- `proc.c`: implemented `k_lock_acquire()`, `k_lock_release()`, `inherit_priority()`, `restore_priority()`
- `sysproc.c`: added syscall wrappers `sys_lock()` and `sys_release()`

---

## Test Programs

Each test prints:
- Start and end markers with ticks (`uptime()`),
- PASS/FAIL results,
- Logs showing scheduling and locking behavior.

---

### Scheduler Tests

| File | Purpose | Expected Result |
|------|----------|-----------------|
| `test1.c` | Static priorities (nice 0–4) | Lower nice → more primes printed |
| `test2.c` | Dynamic nice change | Boosted process prints more |
| `test3.c` | Extreme priorities | High priority dominates CPU time |

---

### Lock & Priority Inheritance Tests

| File | Purpose | PASS Condition |
|------|----------|----------------|
| `test_lock1.c` | Basic Lock/Unlock + invalid ID | Invalid IDs handled; child acquires after parent release |
| `test_lock2.c` | Priority Inversion demo | H waits for L; timestamps show inversion |
| `test_lock3.c` | Priority Inheritance verification | M’s workload decreases when inheritance active |

---

## Sample Output

```bash
$ test_lock1
[RESULT] LOCK1: PASS

$ test_lock2
[RESULT] LOCK2: PASS (H waited for L, see timestamps)

$ test_lock3
[PI] M baseline count=54903, with_inheritance=36385
[RESULT] LOCK3: PASS
```

---

## Modified Files

| File | Change | Description |
|------|---------|-------------|
| `proc.h` | Added fields & prototypes | Added `nice`, `orig_nice`, `lock_t`, functions |
| `proc.c` | Scheduler + Locks | Implemented priority RR + priority inheritance |
| `sysproc.c` | Added syscalls | `sys_nice`, `sys_lock`, `sys_release` |
| `syscall.c / .h / user.h / usys.S` | Syscall registration | Registered new calls |
| `param.h` | Config flag | `SCHED_PRIORITY` toggle |
| `nice.c` | User CLI | For `nice` syscall |
| `test1.c–test3.c` | Scheduler tests | Priority correctness |
| `test_lock1.c–test_lock3.c` | Lock tests | Priority inversion & inheritance |

---

## How to Build and Run

```bash
$ make clean && make qemu-nox
# In xv6 shell:
$ test1
$ test2
$ test3
$ test_lock1
$ test_lock2
$ test_lock3
```

---

## Final Results

All tests pass under `SCHED_PRIORITY = 1`:

```
LOCK1: PASS
LOCK2: PASS
LOCK3: PASS
```

---
