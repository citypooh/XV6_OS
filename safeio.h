#ifndef SAFEIO_H
#define SAFEIO_H

#include "types.h"   // for int
#include "uthreads.h" // for our thread library types

int safe_open(const char *path, int mode);        // open file in a thread friendly way
int safe_close(int fd);                           // close file in a thread friendly way
int safe_read(int fd, void *buf, int n);          // read that does not block other threads
int safe_write(int fd, const void *buf, int n);   // write that does not block other threads

#endif