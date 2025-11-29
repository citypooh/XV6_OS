#ifndef UTHREADS_PRIVATE_H
#define UTHREADS_PRIVATE_H

#include "types.h"
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

struct thread {
  void *sp;                        // saved stack pointer for this thread
  int tid;
  int state;
  void (*start_routine)(void *);   // entry function
  void *arg;                       // argument for entry function
  void *retval;                    // value passed to thread_exit
  struct thread *joiner;           // thread waiting in join
  struct thread *wait_next;        // link for wait queues
  char stack_mem[STACK_SIZE];      // stack storage for this thread
};

extern struct thread threads[MAX_THREADS];
extern struct thread *current_thread;
extern int next_tid;

void thread_schedule(void);                      // pick next runnable thread
void thread_switch(struct thread *old,
  struct thread *next);         // low-level context switch

struct channel {
  void **buf;
  int capacity;
  int count;
  int rpos;
  int wpos;
  int closed;
  mutex_t lock;
  cond_t not_empty;
  cond_t not_full;
};

#endif