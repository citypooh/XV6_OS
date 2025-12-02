#include "types.h"
#include "user.h"          // open, read, write, close, malloc, free
#include "uthreads.h"
#include "safeio.h"
#include "safeio_private.h"

static channel_t *safeio_req_ch = 0;
static int safeio_started = 0;

// worker thread: now returns void*
static void *safeio_worker(void *arg);

// start IO worker once
static void
safeio_init_once(void) {
  if (safeio_started)
    return;
  safeio_started = 1;

  safeio_req_ch = channel_create(16);
  if (safeio_req_ch == 0)
    return;

  // thread_create now expects void* (*)(void*)
  int tid = thread_create(safeio_worker, 0);
  (void)tid;
}

// wait until worker finishes this request
static int
safeio_wait_result(struct safeio_req *req) {
  mutex_lock(&req->lock);
  while (!req->finished) {
    cond_wait(&req->done, &req->lock);
  }
  int res = req->result;
  mutex_unlock(&req->lock);
  return res;
}

// worker thread main loop
static void *
safeio_worker(void *arg) {
  (void)arg;
  struct safeio_req *req;

  while (1) {
    int r = channel_recv(safeio_req_ch, (void **)&req);
    if (r < 0 || req == 0) {
      break;
    }

    if (req->op == SAFEIO_OPEN) {
      req->result = open(req->path, req->mode);
      if (req->path)
        free((void *)req->path);
    } else if (req->op == SAFEIO_CLOSE) {
      req->result = close(req->fd);
    } else if (req->op == SAFEIO_READ) {
      req->result = read(req->fd, req->buf, req->n);
    } else if (req->op == SAFEIO_WRITE) {
      req->result = write(req->fd, req->cbuf, req->n);
    } else {
      req->result = -1;
    }

    mutex_lock(&req->lock);
    req->finished = 1;
    cond_signal(&req->done);
    mutex_unlock(&req->lock);
  }

  // thread_trampoline가 이 반환값을 받아 thread_exit(retval) 호출
  return 0;
}

int
safe_open(const char *path, int mode) {
  safeio_init_once();
  if (safeio_req_ch == 0)
    return -1;

  struct safeio_req *req = (struct safeio_req *)malloc(sizeof(*req));
  if (req == 0)
    return -1;

  int len = 0;
  const char *p = path;
  while (p && *p) {
    len++;
    p++;
  }

  char *copy = 0;
  if (len > 0) {
    copy = (char *)malloc(len + 1);
    if (!copy) {
      free(req);
      return -1;
    }
    for (int i = 0; i < len; i++) {
      copy[i] = path[i];
    }
    copy[len] = 0;
  }

  req->op = SAFEIO_OPEN;
  req->path = copy;
  req->mode = mode;
  req->fd = -1;
  req->buf = 0;
  req->cbuf = 0;
  req->n = 0;
  req->result = -1;
  req->finished = 0;

  mutex_init(&req->lock);
  cond_init(&req->done);

  if (channel_send(safeio_req_ch, req) < 0) {
    if (copy)
      free(copy);
    free(req);
    return -1;
  }

  int res = safeio_wait_result(req);
  free(req);
  return res;
}

int
safe_close(int fd) {
  safeio_init_once();
  if (safeio_req_ch == 0)
    return -1;

  struct safeio_req *req = (struct safeio_req *)malloc(sizeof(*req));
  if (req == 0)
    return -1;

  req->op = SAFEIO_CLOSE;
  req->path = 0;
  req->mode = 0;
  req->fd = fd;
  req->buf = 0;
  req->cbuf = 0;
  req->n = 0;
  req->result = -1;
  req->finished = 0;

  mutex_init(&req->lock);
  cond_init(&req->done);

  if (channel_send(safeio_req_ch, req) < 0) {
    free(req);
    return -1;
  }

  int res = safeio_wait_result(req);
  free(req);
  return res;
}

int
safe_read(int fd, void *buf, int n) {
  safeio_init_once();
  if (safeio_req_ch == 0)
    return -1;

  struct safeio_req *req = (struct safeio_req *)malloc(sizeof(*req));
  if (req == 0)
    return -1;

  req->op = SAFEIO_READ;
  req->path = 0;
  req->mode = 0;
  req->fd = fd;
  req->buf = buf;
  req->cbuf = 0;
  req->n = n;
  req->result = -1;
  req->finished = 0;

  mutex_init(&req->lock);
  cond_init(&req->done);

  if (channel_send(safeio_req_ch, req) < 0) {
    free(req);
    return -1;
  }

  int res = safeio_wait_result(req);
  free(req);
  return res;
}

int
safe_write(int fd, const void *buf, int n) {
  safeio_init_once();
  if (safeio_req_ch == 0)
    return -1;

  struct safeio_req *req = (struct safeio_req *)malloc(sizeof(*req));
  if (req == 0)
    return -1;

  req->op = SAFEIO_WRITE;
  req->path = 0;
  req->mode = 0;
  req->fd = fd;
  req->buf = 0;
  req->cbuf = buf;
  req->n = n;
  req->result = -1;
  req->finished = 0;

  mutex_init(&req->lock);
  cond_init(&req->done);

  if (channel_send(safeio_req_ch, req) < 0) {
    free(req);
    return -1;
  }

  int res = safeio_wait_result(req);
  free(req);
  return res;
}
