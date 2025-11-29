#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NTHREADS 4
#define NLOOPS   1000
#define NITEMS 20

static mutex_t counter_lock;
static int counter = 0;

static mutex_t cond_lock;
static cond_t cond_var;
static int ready = 0;

static channel_t *test_ch;

static void
counter_worker(void *arg) {
  int id = (int)arg;
  for (int i = 0; i < NLOOPS; i++) {
    mutex_lock(&counter_lock);
    counter++;
    mutex_unlock(&counter_lock);

    if ((i % 200) == 0) {
      printf(1, "counter thread %d: i=%d\n", id, i);
      thread_yield();
    }
  }
  thread_exit(0);
}

static void
cond_waiter(void *arg) {
  (void)arg;

  mutex_lock(&cond_lock);
  while (!ready) {
    printf(1, "cond_waiter: waiting\n");
    cond_wait(&cond_var, &cond_lock);
  }
  printf(1, "cond_waiter: got signal\n");
  mutex_unlock(&cond_lock);

  thread_exit(0);
}

static void
cond_signaler(void *arg) {
  (void)arg;

  for (int i = 0; i < 5; i++) {
    printf(1, "cond_signaler: work %d\n", i);
    thread_yield();
  }

  mutex_lock(&cond_lock);
  ready = 1;
  cond_signal(&cond_var);
  mutex_unlock(&cond_lock);

  thread_exit(0);
}

static void
channel_producer(void *arg) {
  int i;
  (void)arg;

  printf(1, "channel_producer: start\n");

  for (i = 0; i < NITEMS; i++) {
    int *p = (int *)malloc(sizeof(int));
    if (!p) {
      printf(1, "channel_producer: malloc failed\n");
      break;
    }
    *p = i;
    if (channel_send(test_ch, p) < 0) {
      printf(1, "channel_producer: send failed at %d\n", i);
      free(p);
      break;
    }
    if ((i % 5) == 0) {
      printf(1, "channel_producer: sent %d\n", i);
      thread_yield();
    }
  }

  channel_close(test_ch);

  printf(1, "channel_producer: done\n");
  thread_exit(0);
}

static void
channel_consumer(void *arg) {
  (void)arg;
  void *data;
  int count = 0;

  printf(1, "channel_consumer: start\n");

  while (1) {
    int r = channel_recv(test_ch, &data);
    if (r < 0) {
      break;
    }
    int *p = (int *)data;
    printf(1, "channel_consumer: got %d\n", *p);
    count++;
    free(p);
    if ((count % 5) == 0) {
      thread_yield();
    }
  }

  printf(1, "channel_consumer: received %d items\n", count);
  thread_exit(0);
}

int
main(int argc, char *argv[]) {
  int tids[NTHREADS];
  void *ret;

  printf(1, "test_final_part2: start\n");

  thread_init();

  mutex_init(&counter_lock);
  mutex_init(&cond_lock);
  cond_init(&cond_var);

  // test mutex with shared counter
  for (int i = 0; i < NTHREADS; i++) {
    int tid = thread_create(counter_worker, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create counter thread %d\n", i + 1);
      exit();
    }
    tids[i] = tid;
  }

  for (int i = 0; i < NTHREADS; i++) {
    ret = thread_join(tids[i]);
    (void)ret;
  }

  printf(1, "counter final value = %d (expected %d)\n",
         counter, NTHREADS * NLOOPS);

  // test condition variable
  int tw = thread_create(cond_waiter, 0);
  int ts = thread_create(cond_signaler, 0);

  if (tw < 0 || ts < 0) {
    printf(1, "failed to create cond threads\n");
    exit();
  }

  thread_join(tw);
  thread_join(ts);

  // channel test
  printf(1, "channel test: start\n");

  test_ch = channel_create(4);
  if (test_ch == 0) {
    printf(1, "channel_create failed\n");
    printf(1, "test_final_part2: done\n");
    exit();
  }

  int tp = thread_create(channel_producer, 0);
  int tc = thread_create(channel_consumer, 0);

  if (tp < 0 || tc < 0) {
    printf(1, "failed to create channel threads\n");
    printf(1, "test_final_part2: done\n");
    exit();
  }

  thread_join(tp);
  thread_join(tc);

  printf(1, "channel test: done\n");

  printf(1, "test_final_part2: done\n");
  exit();
}
