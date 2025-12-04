#include "uthreads_private.h"

/*
 * Generic wait queue helpers (linked list of struct thread *)
 */

// add thread t at end of wait list
static void
wait_enqueue(struct thread **head, struct thread **tail, struct thread *t) {
  t->wait_next = 0;
  if (*tail == 0) {
    *head = t;
    *tail = t;
  } else {
    (*tail)->wait_next = t;
    *tail = t;
  }
}

// remove and return first thread in wait list
static struct thread *
wait_dequeue(struct thread **head, struct thread **tail) {
  struct thread *t = *head;
  if (t) {
    *head = t->wait_next;
    if (*head == 0) {
      *tail = 0;
    }
    t->wait_next = 0;
  }
  return t;
}

/*
 * Mutex
 */

void
mutex_init(mutex_t *m) {
  if (m == 0) return;
  m->locked = 0;
  m->owner = 0;
  m->wait_head = 0;
  m->wait_tail = 0;
}

void
mutex_lock(mutex_t *m) {
  if (m == 0) return;

  for (;;) {
    if (m->locked == 0) {
      // acquire lock
      m->locked = 1;
      m->owner = current_thread;
      return;
    }

    // already locked by someone else → block
    if (m->owner == current_thread) {
      // no re-entrant lock; just spin (or you can panic)
      // 여기선 단순히 deadlock 피하려고 return 안하고 계속 대기하게 두면 됨.
    }

    current_thread->state = T_SLEEPING;
    wait_enqueue(&m->wait_head, &m->wait_tail, current_thread);
    thread_schedule();
    // when woken, retry loop
  }
}

void
mutex_unlock(mutex_t *m) {
  if (m == 0) return;
  if (m->owner != current_thread) {
    // not owner; ignore (or could error)
    return;
  }

  struct thread *t = wait_dequeue(&m->wait_head, &m->wait_tail);
  if (t) {
    // pass ownership directly to next waiter
    m->owner = t;
    if (t->state == T_SLEEPING) {
      t->state = T_RUNNABLE;
    }
    // lock stays held; new owner will run and exit critical section later
  } else {
    m->locked = 0;
    m->owner = 0;
  }
}

/*
 * Condition variables
 */

void
cond_init(cond_t *c) {
  if (c == 0) return;
  c->wait_head = 0;
  c->wait_tail = 0;
}

void
cond_wait(cond_t *c, mutex_t *m) {
  if (c == 0 || m == 0) return;

  // add current thread to cond queue
  wait_enqueue(&c->wait_head, &c->wait_tail, current_thread);

  // mark as sleeping before yielding
  current_thread->state = T_SLEEPING;

  // release mutex
  mutex_unlock(m);

  // schedule another thread; no preemption between unlock and schedule
  thread_schedule();

  // when woken, re-acquire mutex before returning
  mutex_lock(m);
}

void
cond_signal(cond_t *c) {
  if (c == 0) return;

  struct thread *t = wait_dequeue(&c->wait_head, &c->wait_tail);
  if (t && t->state == T_SLEEPING) {
    t->state = T_RUNNABLE;
  }
}

void
cond_broadcast(cond_t *c) {
  if (c == 0) return;

  struct thread *t;
  while ((t = wait_dequeue(&c->wait_head, &c->wait_tail)) != 0) {
    if (t->state == T_SLEEPING) {
      t->state = T_RUNNABLE;
    }
  }
}

/*
 * Semaphores (new)
 */

void
sem_init(sem_t *s, int value) {
  if (s == 0) return;
  s->value = value;
  s->wait_head = 0;
  s->wait_tail = 0;
}

void
sem_wait(sem_t *s) {
  if (s == 0) return;

  s->value--;
  if (s->value < 0) {
    // need to block
    current_thread->state = T_SLEEPING;
    wait_enqueue(&s->wait_head, &s->wait_tail, current_thread);
    thread_schedule();
    // when woken, continue
  }
}

void
sem_post(sem_t *s) {
  if (s == 0) return;

  s->value++;
  if (s->value <= 0) {
    // there are waiting threads
    struct thread *t = wait_dequeue(&s->wait_head, &s->wait_tail);
    if (t && t->state == T_SLEEPING) {
      t->state = T_RUNNABLE;
    }
  }
}
