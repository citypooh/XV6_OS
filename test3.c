#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

typedef struct {
  int nice_value;
  int prime_count;
  int elapsed_ticks;
  int pid;
} result_t;

static int
is_prime(int n) {
  if (n < 2) return 0;
  if (n % 2 == 0) return n == 2;
  for (int j = 3; j * j <= n; j += 2) {
    if (n % j == 0) return 0;
  }
  return 1;
}

static void
run_worker(int target_nice, int duration_ticks, int write_fd) {
  int my_pid = getpid();
  nice(my_pid, target_nice);
  int t0 = uptime();
  int cnt = 0;
  for (int i = 2;; i++) {
    if (is_prime(i)) cnt++;
    if (uptime() - t0 >= duration_ticks) break;
  }
  result_t r = { target_nice, cnt, uptime() - t0, my_pid };
  write(write_fd, &r, sizeof(r));
}

static int
read_full(int fd, void *buf, int n) {
  int got = 0, r;
  while (got < n) {
    r = read(fd, (char*)buf + got, n - got);
    if (r <= 0) return r;
    got += r;
  }
  return got;
}

int
main(void) {
  const int DURATION_TICKS = 600;
  printf(1, "\n=== [TEST3] Extreme Priorities: start at tick=%d ===\n", uptime());

  int pa[2], pb[2];
  pipe(pa); pipe(pb);

  int pid_high = fork();
  if (pid_high == 0) {
    close(pa[0]);
    run_worker(0, DURATION_TICKS, pa[1]);
    close(pa[1]); exit();
  }
  close(pa[1]);

  int pid_low = fork();
  if (pid_low == 0) {
    close(pb[0]);
    run_worker(4, DURATION_TICKS, pb[1]);
    close(pb[1]); exit();
  }
  close(pb[1]);

  result_t hi, lo;
  read_full(pa[0], &hi, sizeof(hi));
  read_full(pb[0], &lo, sizeof(lo));
  close(pa[0]); close(pb[0]);
  wait(); wait();

  printf(1, "HIGH(nice=0) pid=%d primes=%d ticks=%d\n", hi.pid, hi.prime_count, hi.elapsed_ticks);
  printf(1, "LOW (nice=4) pid=%d primes=%d ticks=%d\n", lo.pid, lo.prime_count, lo.elapsed_ticks);

  int pass = (hi.prime_count > lo.prime_count);
  printf(1, "[RESULT] TEST3: %s\n", pass ? "PASS" : "FAIL");
  printf(1, "=== [TEST3] end at tick=%d ===\n\n", uptime());
  exit();
}