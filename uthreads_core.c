#include "uthreads_private.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

static int current_index = 0;      // index of current_thread

static void thread_trampoline(void);  // first code run in new thread

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
  for (int i = 0; i < MAX_THREADS; i++) {
    threads[i].state = T_UNUSED;
    threads[i].tid = 0;
    threads[i].sp = 0;
    threads[i].start_routine = 0;
    threads[i].arg = 0;
    threads[i].retval = 0;
    threads[i].joiner = 0;
    threads[i].wait_next = 0;
  }

  struct thread *t = &threads[0];
  t->tid = next_tid++;
  t->state = T_RUNNING;
  current_thread = t;
  current_index = 0;
}

int
thread_self(void) {
  if (current_thread == 0)
    return -1;
  return current_thread->tid;
}

static int
alloc_thread_slot(void) {
  for (int i = 0; i < MAX_THREADS; i++) {
    if (threads[i].state == T_UNUSED) {
      return i;
    }
  }
  return -1;
}

static void
setup_new_thread_stack(struct thread *t) {
  char *sp = t->stack_mem + STACK_SIZE;  // stack grows down

  sp -= 4;
  *(uint*)sp = (uint)thread_trampoline;  // fake return eip

  sp -= 4;
  *(uint*)sp = 0;  // saved ebp

  sp -= 4;
  *(uint*)sp = 0;  // saved ebx

  sp -= 4;
  *(uint*)sp = 0;  // saved esi

  sp -= 4;
  *(uint*)sp = 0;  // saved edi

  t->sp = sp;
}

int
thread_create(void (*start_routine)(void *), void *arg) {
  int idx = alloc_thread_slot();
  if (idx < 0)
    return -1;

  struct thread *t = &threads[idx];
  t->tid = next_tid++;
  t->state = T_RUNNABLE;
  t->start_routine = start_routine;
  t->arg = arg;
  t->retval = 0;
  t->joiner = 0;
  t->wait_next = 0;

  setup_new_thread_stack(t);
  return t->tid;
}

void
thread_yield(void) {
  if (current_thread == 0)
    return;

  if (current_thread->state == T_RUNNING)
    current_thread->state = T_RUNNABLE;

  thread_schedule();
}

void
thread_exit(void *retval) {
  struct thread *t = current_thread;
  if (t == 0)
    return;

  t->retval = retval;
  t->state = T_ZOMBIE;

  if (t->joiner && t->joiner->state == T_SLEEPING) {
    t->joiner->state = T_RUNNABLE;
  }
  t->joiner = 0;

  thread_schedule();

  for (;;)
    ;
}

void *
thread_join(int tid) {
  if (tid <= 0)
    return 0;

  struct thread *target = find_thread_by_tid(tid);
  if (target == 0)
    return 0;

  if (target == current_thread)
    return 0;

  for (;;) {
    if (target->state == T_ZOMBIE) {
      void *ret = target->retval;

      target->state = T_UNUSED;
      target->tid = 0;
      target->sp = 0;
      target->start_routine = 0;
      target->arg = 0;
      target->retval = 0;
      target->joiner = 0;
      target->wait_next = 0;

      return ret;
    }

    target->joiner = current_thread;
    current_thread->state = T_SLEEPING;
    thread_schedule();
  }
}

void
thread_schedule(void) {
  if (current_thread == 0)
    return;

  int start = current_index;
  int next_index = -1;

  for (int i = 0; i < MAX_THREADS; i++) {
    int idx = (start + 1 + i) % MAX_THREADS;
    if (threads[idx].state == T_RUNNABLE) {
      next_index = idx;
      break;
    }
  }

  if (next_index < 0) {
    return;
  }

  struct thread *old = current_thread;
  struct thread *next = &threads[next_index];

  if (old == next) {
    old->state = T_RUNNING;
    return;
  }

  if (old->state == T_RUNNING)
    old->state = T_RUNNABLE;

  next->state = T_RUNNING;
  current_thread = next;
  current_index = next_index;

  thread_switch(old, next);
}

static void
thread_trampoline(void) {
  struct thread *t = current_thread;
  if (t && t->start_routine) {
    t->start_routine(t->arg);
  }
  thread_exit(0);
}
