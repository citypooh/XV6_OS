// test_final_extra.c
// extra credit: file-based producer-consumer using fork and safe I/O

#include "types.h"
#include "user.h"
#include "uthreads.h"
#include "safeio.h"
#include "fcntl.h"

#define N_LINES 20
#define BUF_SZ  64
#define EXTRA_FILE "extra.log"

static void
writer_proc(void) {
  int fd = safe_open(EXTRA_FILE, O_CREATE | O_WRONLY);
  if (fd < 0) {
    printf(1, "writer: safe_open failed\n");
    exit();
  }

  char buf[] = "hello world!";

  printf(1, "writer: start\n");

  for (int i = 0; i < N_LINES; i++) {
    if (safe_write(fd, buf, sizeof(buf) - 1) < 0) {
      printf(1, "writer: safe_write failed at %d\n", i);
      break;
    }
    printf(1, "writer: wrote '%s'\n", buf);
    sleep(10);
  }

  safe_close(fd);

  printf(1, "writer: done\n");
}

static void
reader_proc(void) {
  int fd;

  printf(1, "reader: start\n");

  // wait until file is created
  while ((fd = safe_open(EXTRA_FILE, O_RDONLY)) < 0) {
    printf(1, "reader: waiting for file\n");
    sleep(10);
  }

  char buf[BUF_SZ];
  int empty_reads = 0;

  while (1) {
    int n = safe_read(fd, buf, BUF_SZ - 1);
    if (n > 0) {
      buf[n] = 0;
      printf(1, "reader: read '%s'\n", buf);
      empty_reads = 0;
      thread_yield();
    } else {
      empty_reads++;
      if (empty_reads > 5)
        break;
      sleep(10);
    }
  }

  safe_close(fd);

  printf(1, "reader: done\n");
}

int
main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf(1, "test_final_extra: start\n");

  int pid = fork();
  if (pid < 0) {
    printf(1, "fork failed\n");
    exit();
  }

  if (pid == 0) {
    // child: reader process
    thread_init();
    reader_proc();
    exit();
  } else {
    // parent: writer process
    thread_init();
    writer_proc();
    // wait child to avoid zombie
    wait();
    printf(1, "test_final_extra: done\n");
    exit();
  }
}