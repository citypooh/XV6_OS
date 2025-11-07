// test2.c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

typedef struct {
  int who; // 0=A, 1=B
  int prime_count;
  int elapsed_ticks;
  int pid;
} dyn_result_t;

static int
is_prime(int n){
  if(n < 2) return 0;
  if(n % 2 == 0) return n == 2;
  for(int j=3; j*j <= n; j+=2){
    if(n % j == 0) return 0;
  }
  return 1;
}

static void
run_worker_dyn(int who, int initial_nice, int duration_ticks, int write_fd){
  int my_pid = getpid();
  nice(my_pid, initial_nice);
  int t0 = uptime();
  int cnt = 0;
  for(int i=2;; i++){
    if(is_prime(i)) cnt++;
    if(uptime() - t0 >= duration_ticks) break;
  }
  dyn_result_t r = { who, cnt, uptime() - t0, my_pid };
  write(write_fd, &r, sizeof(r));
}

static int
read_full(int fd, void *buf, int n){
  int got = 0, r;
  while(got < n){
    r = read(fd, (char*)buf + got, n - got);
    if(r <= 0) return r;
    got += r;
  }
  return got;
}

int
main(void)
{
  const int DURATION_TICKS = 600; // ~6s
  printf(1, "\n=== [TEST2] Dynamic Priority Change: start at tick=%d ===\n", uptime());

  int pipeA[2], pipeB[2];
  if(pipe(pipeA) < 0 || pipe(pipeB) < 0){ printf(1, "pipe failed\n"); exit(); }

  int pidA = fork();
  if(pidA < 0){ printf(1, "fork A failed\n"); exit(); }
  if(pidA == 0){
    close(pipeA[0]);
    run_worker_dyn(0, 2, DURATION_TICKS, pipeA[1]);
    close(pipeA[1]); exit();
  }
  close(pipeA[1]);

  int pidB = fork();
  if(pidB < 0){ printf(1, "fork B failed\n"); exit(); }
  if(pidB == 0){
    close(pipeB[0]);
    run_worker_dyn(1, 2, DURATION_TICKS, pipeB[1]);
    close(pipeB[1]); exit();
  }
  close(pipeB[1]);

  // 200틱 뒤 B를 boost
  sleep(200);
  int old = nice(pidB, 0);
  printf(1, "[parent] boosted pid=%d from old_nice=%d to 0 at tick=%d\n",
         pidB, old, uptime());

  dyn_result_t rA, rB;
  read_full(pipeA[0], &rA, sizeof(rA));
  read_full(pipeB[0], &rB, sizeof(rB));
  close(pipeA[0]); close(pipeB[0]);

  wait(); wait();

  printf(1, "A: pid=%d primes=%d ticks=%d\n", rA.pid, rA.prime_count, rA.elapsed_ticks);
  printf(1, "B: pid=%d primes=%d ticks=%d (boosted)\n", rB.pid, rB.prime_count, rB.elapsed_ticks);

  int pass = (rB.prime_count >= rA.prime_count);
  printf(1, "[RESULT] TEST2: %s\n", pass ? "PASS" : "FAIL");
  printf(1, "=== [TEST2] end at tick=%d ===\n\n", uptime());
  exit();
}
