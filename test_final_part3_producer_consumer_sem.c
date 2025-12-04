#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NPROD           3
#define NCONS           2
#define ITEMS_PER_PROD 10
#define BUF_SIZE        5
#define SENTINEL       -1

static int buffer[BUF_SIZE];
static int in_idx = 0;
static int out_idx = 0;

static sem_t empty_slots;
static sem_t full_slots;
static mutex_t buf_lock;

static mutex_t stats_lock;
static int produced_items = 0;
static int consumed_items = 0;

static void *
sem_producer(void *arg) {
  int id = (int)arg;

  printf(1, "[SEM] Producer %d: start\n", id);

  for (int i = 0; i < ITEMS_PER_PROD; i++) {
    int val = id * 100 + i;

    sem_wait(&empty_slots);

    mutex_lock(&buf_lock);
    buffer[in_idx] = val;
    in_idx = (in_idx + 1) % BUF_SIZE;
    mutex_unlock(&buf_lock);

    sem_post(&full_slots);

    mutex_lock(&stats_lock);
    produced_items++;
    mutex_unlock(&stats_lock);

    printf(1, "[SEM] Producer %d: produced item %d (val=%d)\n", id, i, val);

    if ((i % 3) == 0) {
      thread_yield();
    }
  }

  printf(1, "[SEM] Producer %d: done\n", id);
  return 0;
}

static void *
sem_consumer(void *arg) {
  int id = (int)arg;
  int local_count = 0;

  printf(1, "[SEM] Consumer %d: start\n", id);

  while (1) {
    sem_wait(&full_slots);

    mutex_lock(&buf_lock);
    int val = buffer[out_idx];
    out_idx = (out_idx + 1) % BUF_SIZE;
    mutex_unlock(&buf_lock);

    sem_post(&empty_slots);

    if (val == SENTINEL) {
      printf(1, "[SEM] Consumer %d: got sentinel, exit\n", id);
      break;
    }

    printf(1, "[SEM] Consumer %d: consumed val=%d\n", id, val);
    local_count++;

    mutex_lock(&stats_lock);
    consumed_items++;
    mutex_unlock(&stats_lock);

    if ((local_count % 4) == 0) {
      thread_yield();
    }
  }

  printf(1, "[SEM] Consumer %d: consumed %d real items\n", id, local_count);
  return 0;
}

int
main(int argc, char *argv[]) {
  int prod_tids[NPROD];
  int cons_tids[NCONS];
  void *ret;

  (void)argc;
  (void)argv;

  printf(1, "test_final_part3_producer_consumer_sem: start\n");

  thread_init();

  sem_init(&empty_slots, BUF_SIZE);
  sem_init(&full_slots, 0);
  mutex_init(&buf_lock);
  mutex_init(&stats_lock);
  in_idx = 0;
  out_idx = 0;
  produced_items = 0;
  consumed_items = 0;

  for (int i = 0; i < NCONS; i++) {
    int tid = thread_create(sem_consumer, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create consumer %d\n", i + 1);
      exit();
    }
    cons_tids[i] = tid;
  }

  for (int i = 0; i < NPROD; i++) {
    int tid = thread_create(sem_producer, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create producer %d\n", i + 1);
      exit();
    }
    prod_tids[i] = tid;
  }

  for (int i = 0; i < NPROD; i++) {
    ret = thread_join(prod_tids[i]);
    (void)ret;
    printf(1, "main: joined producer tid=%d\n", prod_tids[i]);
  }

  for (int i = 0; i < NCONS; i++) {
    sem_wait(&empty_slots);
    mutex_lock(&buf_lock);
    buffer[in_idx] = SENTINEL;
    in_idx = (in_idx + 1) % BUF_SIZE;
    mutex_unlock(&buf_lock);
    sem_post(&full_slots);
  }

  for (int i = 0; i < NCONS; i++) {
    ret = thread_join(cons_tids[i]);
    (void)ret;
    printf(1, "main: joined consumer tid=%d\n", cons_tids[i]);
  }

  printf(1, "produced_items = %d (expected %d)\n",
         produced_items, NPROD * ITEMS_PER_PROD);
  printf(1, "consumed_items = %d (expected %d)\n",
         consumed_items, NPROD * ITEMS_PER_PROD);

  printf(1, "test_final_part3_producer_consumer_sem: done\n");
  exit();
}
