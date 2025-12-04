#ifndef SAFEIO_PRIVATE_H
#define SAFEIO_PRIVATE_H

#include "types.h"
#include "uthreads.h"

enum safeio_op {
    SAFEIO_OPEN = 1,
    SAFEIO_CLOSE,
    SAFEIO_READ,
    SAFEIO_WRITE
  };

struct safeio_req {
    int op;
    const char *path;      // for open
    int mode;              // for open
    int fd;                // for read/write/close
    void *buf;             // for read
    const void *cbuf;      // for write
    int n;                 // read/write size

    int result;            // return value from syscall

    mutex_t lock;          // protects finished flag
    cond_t done;           // signals when request is done
    int finished;          // 1 when worker is done
};

#endif
