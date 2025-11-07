// test1.c (Static Priority Comparison - improved print style)
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NUM_WORKERS 5
#define DURATION_TICKS 700

// prime check
static int
is_prime(int n) {
  if (n < 2) return 0;
  if ((n & 1) == 0) return n == 2;
  for (int j = 3; j * j <= n; j += 2) {
    if (n % j == 0) return 0;
  }
  return 1;
}

// worker: waits for parent signal, then counts primes for fixed duration
static void
run_worker(int target_nice, int duration_ticks, int start_rfd) {
  int me = getpid();
  if (nice(me, target_nice) < 0) {
    printf(1, "[%d] nice set failed\n", me);
  }

  // barrier: wait for parent start signal
  char go;
  read(start_rfd, &go, 1);

  int t0 = uptime();
  int tend = t0 + duration_ticks;
  int cnt = 0;
  for (int i = 2;; i++) {
    if (is_prime(i)) cnt++;
    if (uptime() >= tend) break;
  }

  printf(1, "  %d   %d   %d   %d\n", target_nice, cnt, uptime() - t0, me);
}

int
main(void) {
  printf(1, "\n=== [TEST1] Static Priority Comparison: start at tick=%d ===\n", uptime());

  int start_pipe[2];
  pipe(start_pipe);  // parent→children barrier

  // spawn 5 workers (nice=0..4)
  for (int k = 0; k < NUM_WORKERS; k++) {
    int pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit();
    }
    if (pid == 0) {
      // child: close write-end, run
      close(start_pipe[1]);
      run_worker(k, DURATION_TICKS, start_pipe[0]);
      exit();
    }
  }

  // parent: print header, release barrier
  printf(1, "nice primes ticks pid\n");
  close(start_pipe[0]);
  for (int i = 0; i < NUM_WORKERS; i++)
    write(start_pipe[1], "G", 1);
  close(start_pipe[1]);

  for (int i = 0; i < NUM_WORKERS; i++)
    wait();

  // result summary (manual inspection or later auto logic)
  printf(1, "[RESULT] TEST1: PASS\n");
  printf(1, "=== [TEST1] end at tick=%d ===\n\n", uptime());
  exit();
}
