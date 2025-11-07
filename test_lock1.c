// test_lock1.c
#include "types.h"
#include "stat.h"
#include "user.h"

static int read_int(int fd, int *out){
    return read(fd, out, sizeof(int));
}

int
main(void)
{
    printf(1, "\n=== [LOCK1] Basic Lock/Unlock & Invalid IDs: start tick=%d ===\n", uptime());

    int rc_invalid = 1;
    if(lock(0)    != -1) rc_invalid = 0;
    if(release(0) != -1) rc_invalid = 0;
    if(lock(8)    != -1) rc_invalid = 0;
    if(release(8) != -1) rc_invalid = 0;
    printf(1, "[invalid-id] %s\n", rc_invalid ? "PASS" : "FAIL");

    int p[2];
    if(pipe(p) < 0){ printf(1, "pipe failed\n"); exit(); }

    int child = fork();
    if(child < 0){ printf(1, "fork failed\n"); exit(); }

    if(child == 0){
        close(p[0]);
        printf(1, "[child] waiting lock 1 at tick=%d\n", uptime());
        if(lock(1) < 0){ printf(1, "[child] lock(1) failed\n"); exit(); }
        int t_acq = uptime();
        printf(1, "[child] acquired lock 1 at tick=%d\n", t_acq);
        write(p[1], &t_acq, sizeof(t_acq));
        release(1);
        printf(1, "[child] released lock 1 at tick=%d\n", uptime());
        close(p[1]); exit();
    }

    // parent
    close(p[1]);
    if(lock(1) < 0){ printf(1, "[parent] lock(1) failed\n"); exit(); }
    printf(1, "[parent] acquired lock 1 at tick=%d\n", uptime());
    sleep(200);
    int t_rel = uptime();
    release(1);
    printf(1, "[parent] released lock 1 at tick=%d\n", t_rel);

    int child_acq_tick = -1;
    read_int(p[0], &child_acq_tick);
    close(p[0]);

    wait();

    int pass_order = (child_acq_tick >= t_rel);
    printf(1, "[order] child_acquired_after_parent_release: %s\n", pass_order ? "PASS" : "FAIL");

    int all_pass = rc_invalid && pass_order;
    printf(1, "[RESULT] LOCK1: %s\n", all_pass ? "PASS" : "FAIL");
    printf(1, "=== [LOCK1] end at tick=%d ===\n\n", uptime());
    exit();
}
