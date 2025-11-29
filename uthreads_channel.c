#include "types.h"
#include "user.h"
#include "uthreads_private.h"

channel_t *
channel_create(int capacity) {
  if (capacity <= 0)
    return 0;

  channel_t *ch = (channel_t *)malloc(sizeof(*ch));
  if (ch == 0)
    return 0;

  ch->buf = (void **)malloc(sizeof(void *) * capacity);
  if (ch->buf == 0) {
    free(ch);
    return 0;
  }

  ch->capacity = capacity;
  ch->count = 0;
  ch->rpos = 0;
  ch->wpos = 0;
  ch->closed = 0;

  mutex_init(&ch->lock);
  cond_init(&ch->not_empty);
  cond_init(&ch->not_full);

  return ch;
}

int
channel_send(channel_t *ch, void *data) {
  if (ch == 0)
    return -1;

  mutex_lock(&ch->lock);

  while (ch->count == ch->capacity && !ch->closed) {
    cond_wait(&ch->not_full, &ch->lock);
  }

  if (ch->closed) {
    mutex_unlock(&ch->lock);
    return -1;
  }

  ch->buf[ch->wpos] = data;
  ch->wpos = (ch->wpos + 1) % ch->capacity;
  ch->count++;

  cond_signal(&ch->not_empty);

  mutex_unlock(&ch->lock);
  return 0;
}

int
channel_recv(channel_t *ch, void **data) {
  if (ch == 0 || data == 0)
    return -1;

  mutex_lock(&ch->lock);

  while (ch->count == 0 && !ch->closed) {
    cond_wait(&ch->not_empty, &ch->lock);
  }

  if (ch->count == 0 && ch->closed) {
    mutex_unlock(&ch->lock);
    return -1;
  }

  *data = ch->buf[ch->rpos];
  ch->rpos = (ch->rpos + 1) % ch->capacity;
  ch->count--;

  cond_signal(&ch->not_full);

  mutex_unlock(&ch->lock);
  return 0;
}

void
channel_close(channel_t *ch) {
  if (ch == 0)
    return;

  mutex_lock(&ch->lock);
  ch->closed = 1;
  cond_broadcast(&ch->not_empty);
  cond_broadcast(&ch->not_full);
  mutex_unlock(&ch->lock);
}