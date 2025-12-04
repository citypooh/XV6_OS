#ifndef UTHREADS_PRIVATE_H
#define UTHREADS_PRIVATE_H

#include "types.h"
#include "user.h"
#include "uthreads.h"

#define MAX_THREADS 64       // max threads in one process
#define STACK_SIZE 8192      // stack size per thread in bytes

enum thread_state {
  T_UNUSED = 0,
  T_RUNNABLE,
  T_RUNNING,
  T_SLEEPING,
  T_ZOMBIE
};

/*
 * Core thread structure
 */
struct thread {
  uint sp;                      // saved stack pointer (for thread_switch)
  char *stack;                  // malloc'ed stack base
  int tid;                      // thread id
  enum thread_state state;      // T_*

  void *(*start_routine)(void *); // user function
  void *arg;                    // argument to start_routine
  void *retval;                 // return value for thread_join

  struct thread *joiner;        // thread blocked in join(tid)
  struct thread *wait_next;     // link field for wait queues
};

/*
 * Channel internal structure
 */

struct channel {
  void **buf;
  int capacity;
  int count;
  int rpos;
  int wpos;
  int closed;       // 0 = open, 1 = closed

  mutex_t lock;
  cond_t not_empty;
  cond_t not_full;
};

/*
 * Global thread table
 */

extern struct thread threads[MAX_THREADS];
extern struct thread *current_thread;
extern int next_tid;

/*
 * Internal functions
 */

void thread_schedule(void);
void thread_switch(struct thread *old, struct thread *next);

#endif
