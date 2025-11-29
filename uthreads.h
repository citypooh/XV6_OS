#ifndef UTHREADS_H
#define UTHREADS_H

#include "types.h"

struct thread;       // internal thread struct
struct channel;      // internal channel struct

void thread_init(void);  // must be called before using threads
int thread_create(void (*start_routine)(void *), void *arg);
void thread_exit(void *retval);  // end current thread and save retval
void *thread_join(int tid);      // wait for thread and get retval
int thread_self(void);
void thread_yield(void);

/*
 * Mutex API
 */

typedef struct mutex {
  int locked;                 // 0 free, 1 locked
  int owner_tid;              // tid of owner
  struct thread *wait_head;   // first waiting thread
  struct thread *wait_tail;   // last waiting thread
} mutex_t;
void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

/*
 * Condition Variable API
 */

typedef struct cond {
  struct thread *wait_head;
  struct thread *wait_tail;
} cond_t;
void cond_init(cond_t *c);
void cond_wait(cond_t *c, mutex_t *m);  // sleep on cond and release m
void cond_signal(cond_t *c);
void cond_broadcast(cond_t *c);

/*
 * Channel API
 */

typedef struct channel channel_t;
channel_t *channel_create(int capacity);    // buffer holds at most capacity items
int channel_send(channel_t *ch, void *data);  // return 0 or -1 if closed
int channel_recv(channel_t *ch, void **data); // return 0 or -1 if closed and empty
void channel_close(channel_t *ch);            // mark channel closed and wake waiters

#endif // UTHREADS_H