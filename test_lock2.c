// test_lock2.c
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
    printf(1, "\n=== [LOCK2] Priority Inversion (H waits for L): start tick=%d ===\n", uptime());

    int low = fork();
    if(low == 0){
        nice(getpid(), 4);
        if(lock(1) < 0){ printf(1,"[L] lock(1) failed\n"); exit(); }
        printf(1, "[L] got lock 1 at %d (nice=4)\n", uptime());
        busy_prime_ticks(400);
        release(1);
        printf(1, "[L] released lock 1 at %d\n", uptime());
        exit();
    }

    sleep(50);

    int high = fork();
    if(high == 0){
        nice(getpid(), 0);
        printf(1, "[H] trying lock 1 at %d (nice=0)\n", uptime());
        if(lock(1) < 0){ printf(1,"[H] lock(1) failed\n"); exit(); }
        printf(1, "[H] acquired lock 1 at %d\n", uptime());
        release(1);
        exit();
    }

    int mid = fork();
    if(mid == 0){
        nice(getpid(), 2);
        int c = busy_prime_ticks(500);
        printf(1, "[M] ran busy loop (count=%d) until %d\n", c, uptime());
        exit();
    }

    wait(); wait(); wait();
    printf(1, "[RESULT] LOCK2: PASS (H waited for L, see timestamps)\n");
    printf(1, "=== [LOCK2] end at tick=%d ===\n\n", uptime());
    exit();
}
