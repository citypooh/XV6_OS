#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NREADERS     3
#define NWRITERS     2
#define READ_LOOPS   5
#define WRITE_LOOPS  3

typedef struct {
  mutex_t lock;
  cond_t can_read;
  cond_t can_write;
  int readers;
  int writers_waiting;
  int writer_active;
} rwlock_t;

static rwlock_t rw;
static int shared_value = 0;

static void
rwlock_init(rwlock_t *r) {
  mutex_init(&r->lock);
  cond_init(&r->can_read);
  cond_init(&r->can_write);
  r->readers = 0;
  r->writers_waiting = 0;
  r->writer_active = 0;
}

static void
reader_lock(rwlock_t *r) {
  mutex_lock(&r->lock);
  while (r->writer_active || r->writers_waiting > 0) {
    cond_wait(&r->can_read, &r->lock);
  }
  r->readers++;
  mutex_unlock(&r->lock);
}

static void
reader_unlock(rwlock_t *r) {
  mutex_lock(&r->lock);
  r->readers--;
  if (r->readers == 0 && r->writers_waiting > 0) {
    cond_signal(&r->can_write);
  }
  mutex_unlock(&r->lock);
}

static void
writer_lock(rwlock_t *r) {
  mutex_lock(&r->lock);
  r->writers_waiting++;
  while (r->writer_active || r->readers > 0) {
    cond_wait(&r->can_write, &r->lock);
  }
  r->writers_waiting--;
  r->writer_active = 1;
  mutex_unlock(&r->lock);
}

static void
writer_unlock(rwlock_t *r) {
  mutex_lock(&r->lock);
  r->writer_active = 0;
  if (r->writers_waiting > 0) {
    cond_signal(&r->can_write);
  } else {
    cond_broadcast(&r->can_read);
  }
  mutex_unlock(&r->lock);
}

static void *
reader_thread(void *arg) {
  int id = (int)arg;
  int i;

  printf(1, "Reader %d: start\n", id);

  for (i = 0; i < READ_LOOPS; i++) {
    reader_lock(&rw);
    printf(1, "Reader %d: value = %d\n", id, shared_value);
    reader_unlock(&rw);
    thread_yield();
  }

  printf(1, "Reader %d: done\n", id);
  return 0;
}

static void *
writer_thread(void *arg) {
  int id = (int)arg;
  int i;

  printf(1, "Writer %d: start\n", id);

  for (i = 0; i < WRITE_LOOPS; i++) {
    writer_lock(&rw);
    shared_value++;
    printf(1, "Writer %d: wrote value = %d\n", id, shared_value);
    writer_unlock(&rw);
    thread_yield();
  }

  printf(1, "Writer %d: done\n", id);
  return 0;
}

int
main(int argc, char *argv[]) {
  int rtids[NREADERS];
  int wtids[NWRITERS];
  int i;
  void *ret;

  (void)argc;
  (void)argv;

  printf(1, "test_final_part3_reader_writer: start\n");

  thread_init();
  rwlock_init(&rw);

  for (i = 0; i < NREADERS; i++) {
    int tid = thread_create(reader_thread, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create reader %d\n", i + 1);
      exit();
    }
    rtids[i] = tid;
  }

  for (i = 0; i < NWRITERS; i++) {
    int tid = thread_create(writer_thread, (void *)(i + 1));
    if (tid < 0) {
      printf(1, "failed to create writer %d\n", i + 1);
      exit();
    }
    wtids[i] = tid;
  }

  for (i = 0; i < NREADERS; i++) {
    ret = thread_join(rtids[i]);
    (void)ret;
    printf(1, "main: joined reader tid=%d\n", rtids[i]);
  }

  for (i = 0; i < NWRITERS; i++) {
    ret = thread_join(wtids[i]);
    (void)ret;
    printf(1, "main: joined writer tid=%d\n", wtids[i]);
  }

  printf(1, "final shared_value = %d\n", shared_value);
  printf(1, "test_final_part3_reader_writer: done\n");
  exit();
}
