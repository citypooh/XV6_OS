#include "uthreads_private.h"

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
  if (t == 0)
    return 0;
  *head = t->wait_next;
  if (*head == 0)
    *tail = 0;
  t->wait_next = 0;
  return t;
}

/*
 * Mutex
 */

void
mutex_init(mutex_t *m) {
  m->locked = 0;
  m->owner_tid = 0;
  m->wait_head = 0;
  m->wait_tail = 0;
}

void
mutex_lock(mutex_t *m) {
  int mytid = thread_self();

  for (;;) {
    if (m->locked == 0) {
      m->locked = 1;
      m->owner_tid = mytid;
      return;
    }

    if (m->owner_tid == mytid) {
      return;  // simple re-entrant ignore
    }

    wait_enqueue(&m->wait_head, &m->wait_tail, current_thread);
    current_thread->state = T_SLEEPING;
    thread_schedule();
    // when we wake, try again
  }
}

void
mutex_unlock(mutex_t *m) {
  int mytid = thread_self();
  if (!m->locked || m->owner_tid != mytid) {
    return;
  }

  struct thread *t = wait_dequeue(&m->wait_head, &m->wait_tail);

  if (t == 0) {
    m->locked = 0;
    m->owner_tid = 0;
    return;
  }

  m->locked = 0;
  m->owner_tid = 0;

  if (t->state == T_SLEEPING) {
    t->state = T_RUNNABLE;
  }
}

/*
 * Condition variables
 */

void
cond_init(cond_t *c) {
  c->wait_head = 0;
  c->wait_tail = 0;
}

void
cond_wait(cond_t *c, mutex_t *m) {
  wait_enqueue(&c->wait_head, &c->wait_tail, current_thread);

  mutex_unlock(m);

  current_thread->state = T_SLEEPING;
  thread_schedule();

  mutex_lock(m);
}

void
cond_signal(cond_t *c) {
  struct thread *t = wait_dequeue(&c->wait_head, &c->wait_tail);
  if (t && t->state == T_SLEEPING) {
    t->state = T_RUNNABLE;
  }
}

void
cond_broadcast(cond_t *c) {
  struct thread *t;
  while ((t = wait_dequeue(&c->wait_head, &c->wait_tail)) != 0) {
    if (t->state == T_SLEEPING) {
      t->state = T_RUNNABLE;
    }
  }
}
