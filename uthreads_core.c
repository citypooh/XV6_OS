#include "uthreads_private.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

static int current_index = 0;      // index of current_thread

static void thread_trampoline(void);  // first code run in new thread

// find thread by tid in global table
static struct thread *
find_thread_by_tid(int tid) {
  for (int i = 0; i < MAX_THREADS; i++) {
    if (threads[i].state != T_UNUSED && threads[i].tid == tid) {
      return &threads[i];
    }
  }
  return 0;
}

void
thread_init(void) {
  // initialize thread table
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i].sp = 0;
    threads[i].stack = 0;
    threads[i].tid = 0;
    threads[i].state = T_UNUSED;
    threads[i].start_routine = 0;
    threads[i].arg = 0;
    threads[i].retval = 0;
    threads[i].joiner = 0;
    threads[i].wait_next = 0;
  }

  // set up main thread (slot 0)
  struct thread *t = &threads[0];
  t->sp = 0;
  t->stack = 0;           // main stack is process stack, not managed here
  t->tid = next_tid++;
  t->state = T_RUNNING;
  t->start_routine = 0;
  t->arg = 0;
  t->retval = 0;
  t->joiner = 0;
  t->wait_next = 0;

  current_thread = t;
  current_index = 0;
}

// helper: allocate a new thread slot
static struct thread *
alloc_thread_slot(void) {
  for (int i = 0; i < MAX_THREADS; i++) {
    if (threads[i].state == T_UNUSED) {
      return &threads[i];
    }
  }
  return 0;
}

int
thread_create(void *(*start_routine)(void *), void *arg) {
  struct thread *t = alloc_thread_slot();
  if (t == 0) {
    return -1;
  }

  char *stack = (char *)malloc(STACK_SIZE);
  if (stack == 0) {
    return -1;
  }

  // initialize thread struct
  t->stack = stack;
  t->tid = next_tid++;
  t->state = T_RUNNABLE;
  t->start_routine = start_routine;
  t->arg = arg;
  t->retval = 0;
  t->joiner = 0;
  t->wait_next = 0;

  // set up initial stack frame for thread_switch
  uint *sp = (uint *)(stack + STACK_SIZE);

  // prepare stack so that thread_switch will pop registers and ret to thread_trampoline
  // pop edi, pop esi, pop ebx, pop ebp, ret
  *--sp = (uint)thread_trampoline;  // return address for ret
  *--sp = 0;                        // fake ebp
  *--sp = 0;                        // fake ebx
  *--sp = 0;                        // fake esi
  *--sp = 0;                        // fake edi

  t->sp = (uint)sp;

  return t->tid;
}

void
thread_exit(void *retval) {
  struct thread *t = current_thread;

  t->retval = retval;
  t->state = T_ZOMBIE;

  // wake joiner if any
  if (t->joiner && t->joiner->state == T_SLEEPING) {
    t->joiner->state = T_RUNNABLE;
  }

  // schedule next runnable thread (should not return)
  thread_schedule();

  // if we ever come back here, something is wrong; just spin
  for (;;) {
  }
}

void *
thread_join(int tid) {
  struct thread *target = find_thread_by_tid(tid);
  if (target == 0) {
    return 0;
  }

  // disallow self-join
  if (target == current_thread) {
    return 0;
  }

  // allow only a single joiner
  if (target->joiner != 0 && target->joiner != current_thread) {
    return 0;
  }

  target->joiner = current_thread;

  for (;;) {
    if (target->state == T_ZOMBIE) {
      void *retval = target->retval;

      // clean up target's resources
      if (target->stack) {
        free(target->stack);
        target->stack = 0;
      }
      target->sp = 0;
      target->tid = 0;
      target->state = T_UNUSED;
      target->start_routine = 0;
      target->arg = 0;
      target->retval = 0;
      target->joiner = 0;
      target->wait_next = 0;

      return retval;
    }

    // block current thread until target exits
    current_thread->state = T_SLEEPING;
    thread_schedule();
    // when we wake up, loop and re-check state
  }
}

int
thread_self(void) {
  if (current_thread == 0) {
    return -1;
  }
  return current_thread->tid;
}

void
thread_yield(void) {
  if (current_thread == 0) {
    return;
  }

  // mark current as runnable and schedule someone else
  if (current_thread->state == T_RUNNING) {
    current_thread->state = T_RUNNABLE;
  }
  thread_schedule();
}

void
thread_schedule(void) {
  struct thread *old = current_thread;
  int old_index = current_index;

  int next_index = -1;

  if (old == 0) {
    // no current thread yet → pick first runnable
    for (int i = 0; i < MAX_THREADS; i++) {
      if (threads[i].state == T_RUNNABLE) {
        next_index = i;
        break;
      }
    }
  } else {
    // round-robin: search from next slot
    for (int offset = 1; offset <= MAX_THREADS; offset++) {
      int idx = (old_index + offset) % MAX_THREADS;
      if (threads[idx].state == T_RUNNABLE) {
        next_index = idx;
        break;
      }
    }
  }

  if (next_index < 0) {
    // no other runnable threads; keep running old if possible
    return;
  }

  struct thread *next = &threads[next_index];

  if (old && old->state == T_RUNNING) {
    old->state = T_RUNNABLE;
  }

  next->state = T_RUNNING;
  current_thread = next;
  current_index = next_index;

  if (old != next) {
    thread_switch(old, next);
  }
}

static void
thread_trampoline(void) {
  struct thread *t = current_thread;
  void *retval = 0;

  if (t && t->start_routine) {
    retval = t->start_routine(t->arg);
  }

  thread_exit(retval);
}
