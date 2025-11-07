// test_lock3.c
#include "types.h"
#include "stat.h"
#include "user.h"

static int
busy_prime_ticks(int dur){
  int start = uptime(), cnt=0;
  for(int i=2;;i++){
    int prime=1;
    for(int j=2;j*j<=i;j++){
      if(i%j==0){ prime=0; break; }
    }
    if(prime) cnt++;
    if(uptime()-start >= dur) break;
  }
  return cnt;
}

int
main(void)
{
  printf(1, "\n=== [LOCK3] Priority Inheritance: start tick=%d ===\n", uptime());

  int pa[2]; pipe(pa);
  int childA = fork();
  if(childA == 0){
    // L
    int low = fork();
    if(low == 0){
      nice(getpid(), 4);
      lock(1);
      busy_prime_ticks(500);
      release(1);
      exit();
    }
    // M
    int mid = fork();
    if(mid == 0){
      nice(getpid(), 2);
      int cnt = busy_prime_ticks(800);
      write(pa[1], &cnt, sizeof(cnt));
      exit();
    }
    wait(); wait(); exit();
  }
  close(pa[1]);
  int m_baseline = 0; read(pa[0], &m_baseline, sizeof(m_baseline)); close(pa[0]);
  wait();

  int pb[2]; pipe(pb);
  int childB = fork();
  if(childB == 0){
    // L
    int low = fork();
    if(low == 0){
      nice(getpid(), 4);
      lock(1);
      busy_prime_ticks(400);
      release(1);
      exit();
    }
    sleep(50);

    // H
    int high = fork();
    if(high == 0){
      nice(getpid(), 0);
      lock(1);
      release(1);
      exit();
    }

    // M
    int mid = fork();
    if(mid == 0){
      nice(getpid(), 2);
      int cnt = busy_prime_ticks(500);
      write(pb[1], &cnt, sizeof(cnt));
      exit();
    }
    wait(); wait(); wait(); exit();
  }
  close(pb[1]);
  int m_with_pi = 0; read(pb[0], &m_with_pi, sizeof(m_with_pi)); close(pb[0]);
  wait();

  printf(1, "[PI] M baseline count=%d, with_inheritance=%d\n", m_baseline, m_with_pi);

  int pass = (m_with_pi < m_baseline);
  printf(1, "[RESULT] LOCK3: %s\n", pass ? "PASS" : "FAIL");
  printf(1, "=== [LOCK3] end at tick=%d ===\n\n", uptime());
  exit();
}
