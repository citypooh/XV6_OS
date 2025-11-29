#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NTHREADS 3

static void
worker(void *arg) {
  int id = (int)arg;
  for (int i = 0; i < 5; i++) {
    printf(1, "thread %d: iteration %d\n", id, i);
    thread_yield();
  }
  thread_exit((void *)(id * 10 + 1));
}

int
main(int argc, char *argv[]) {
  int tids[NTHREADS];
  void *ret;

  printf(1, "test_final_part1: start\n");

  thread_init();

  for (int i = 0; i < NTHREADS; i++) {
    int tid = thread_create(worker, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create thread %d\n", i + 1);
      exit();
    }
    tids[i] = tid;
    printf(1, "created thread tid=%d\n", tid);
  }

  for (int i = 0; i < NTHREADS; i++) {
    ret = thread_join(tids[i]);
    printf(1, "joined tid=%d retval=%d\n", tids[i], (int)ret);
  }

  printf(1, "test_final_part1: done\n");
  exit();
}