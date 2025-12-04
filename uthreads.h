#ifndef UTHREADS_H
#define UTHREADS_H

#include "types.h"

/*
 * Forward declarations
 */
struct thread;
struct channel;

/*
 * Thread API
 */

void thread_init(void);  // must be called before using threads

// start_routine returns void* (so join can collect it)
int thread_create(void *(*start_routine)(void *), void *arg);

void thread_exit(void *retval);   // end current thread and save retval
void *thread_join(int tid);       // wait for thread and get retval
int thread_self(void);            // return current thread id
void thread_yield(void);          // cooperative yield

/*
 * Mutex API
 *
 * NOTE: struct definition is public so user code can declare
 * mutex_t variables as globals/statics/local variables.
 */

typedef struct mutex {
  int locked;                 // 0 = unlocked, 1 = locked
  struct thread *owner;       // current owner
  struct thread *wait_head;   // wait queue head
  struct thread *wait_tail;   // wait queue tail
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

/*
 * Condition Variable API
 */

typedef struct cond {
  struct thread *wait_head;   // wait queue head
  struct thread *wait_tail;   // wait queue tail
} cond_t;

void cond_init(cond_t *c);
void cond_wait(cond_t *c, mutex_t *m);  // sleep on cond and release m
void cond_signal(cond_t *c);
void cond_broadcast(cond_t *c);

/*
 * Semaphore API (extra credit + Part 3)
 */

typedef struct sem {
  int value;                  // semaphore count
  struct thread *wait_head;   // wait queue head
  struct thread *wait_tail;   // wait queue tail
} sem_t;

void sem_init(sem_t *s, int value);
void sem_wait(sem_t *s);
void sem_post(sem_t *s);

/*
 * Channel API
 */

typedef struct channel channel_t;

channel_t *channel_create(int capacity);      // buffer holds at most capacity items
int channel_send(channel_t *ch, void *data);  // return 0 or -1 if closed
int channel_recv(channel_t *ch, void **data); // return 0 or -1 if closed and empty
void channel_close(channel_t *ch);            // mark channel closed and wake waiters

#endif // UTHREADS_H
