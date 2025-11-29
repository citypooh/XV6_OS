#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NPROD 3
#define NCONS 2
#define ITEMS_PER_PROD 10

static channel_t *pc_ch;

static void
producer(void *arg) {
  int id = (int)arg;
  int i;

  printf(1, "Producer %d: start\n", id);

  for (i = 0; i < ITEMS_PER_PROD; i++) {
    int *p = (int *)malloc(sizeof(int));
    if (!p) {
      printf(1, "Producer %d: malloc failed at %d\n", id, i);
      break;
    }

    *p = id * 100 + i;  // encode producer id and item number

    if (channel_send(pc_ch, p) < 0) {
      printf(1, "Producer %d: send failed at %d\n", id, i);
      free(p);
      break;
    }

    printf(1, "Producer %d: generated item %d (val=%d)\n", id, i, *p);

    if ((i % 3) == 0) {
      thread_yield();
    }
  }

  printf(1, "Producer %d: done\n", id);
  thread_exit(0);
}

static void
consumer(void *arg) {
  int id = (int)arg;
  void *data;
  int count = 0;

  printf(1, "Consumer %d: start\n", id);

  while (1) {
    int r = channel_recv(pc_ch, &data);
    if (r < 0) {
      break;
    }

    int *p = (int *)data;
    int val = *p;
    free(p);

    printf(1, "Consumer %d: consumed val=%d\n", id, val);
    count++;

    if ((count % 4) == 0) {
      thread_yield();
    }
  }

  printf(1, "Consumer %d: received %d items, exit\n", id, count);
  thread_exit(0);
}

int
main(int argc, char *argv[]) {
  int prod_tids[NPROD];
  int cons_tids[NCONS];
  int i;
  void *ret;

  printf(1, "test_final_part3_producer_consumer: start\n");

  thread_init();

  pc_ch = channel_create(8);
  if (pc_ch == 0) {
    printf(1, "channel_create failed\n");
    exit();
  }

  for (i = 0; i < NPROD; i++) {
    int tid = thread_create(producer, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create producer %d\n", i + 1);
      exit();
    }
    prod_tids[i] = tid;
  }

  for (i = 0; i < NCONS; i++) {
    int tid = thread_create(consumer, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create consumer %d\n", i + 1);
      exit();
    }
    cons_tids[i] = tid;
  }

  for (i = 0; i < NPROD; i++) {
    ret = thread_join(prod_tids[i]);
    (void)ret;
    printf(1, "main: joined producer tid=%d\n", prod_tids[i]);
  }

  channel_close(pc_ch);

  for (i = 0; i < NCONS; i++) {
    ret = thread_join(cons_tids[i]);
    (void)ret;
    printf(1, "main: joined consumer tid=%d\n", cons_tids[i]);
  }

  printf(1, "test_final_part3_producer_consumer: done\n");
  exit();
}
