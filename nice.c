#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NICE_MIN 0
#define NICE_MAX 4

static void
usage(void) {
    printf(2, "Usage:\n");
    printf(2, "  nice <pid> <value>\n");
    printf(2, "  nice <value>        (applies to current process)\n");
}

int
main(int argc, char *argv[])
{
    if(argc != 2 && argc != 3){
        usage();
        exit();
    }

    int pid, val, old;

    if(argc == 2){
        val = atoi(argv[1]);
        pid = getpid();
    } else {
        pid = atoi(argv[1]);
        val = atoi(argv[2]);
    }

    old = nice(pid, val);
    if(old < 0){
        printf(2, "error: invalid pid or value (value must be %d..%d)\n", NICE_MIN, NICE_MAX);
        exit();
    }

    printf(1, "%d %d\n", pid, old);
    exit();
}