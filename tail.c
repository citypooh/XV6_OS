#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define BUF_CHUNK 512

typedef struct {
  char *data;
  int len;
  int cap;
} Line;

static void free_line(Line *line) {
  if (line->data) {
    free(line->data);
  }

  line->data = 0;
  line->len = 0;
  line->cap = 0;
}

static int grow_line(Line *line, int need) {
  int newCap;
  char *newData;

  if (line->cap >= need) {
    return 0;
  }

  newCap = line->cap ? line->cap : 64;
  while (newCap < need) {
    newCap <<= 1;
  }

  newData = (char*)malloc(newCap);
  if (!newData) {
    return -1;
  }

  if (line->data && line->len > 0) {
    memmove(newData, line->data, line->len);
    free(line->data);
  }

  line->data = newData;
  line->cap  = newCap;
  return 0;
}

static int append_char(Line *line, char ch) {
  if (grow_line(line, line->len + 1) < 0) {
    return -1;
  }

  line->data[line->len++] = ch;
  return 0;
}

static void print_lines(Line *ringBuf, int keep, int lineCount, int headIndex,
                        int showHeader, const char *filename) {
  int start;
  int i;

  if (showHeader && filename) {
    printf(1, "==> %s <==\n", filename);
  }

  if (keep == 0 || lineCount == 0) {
    return;
  }

  start = (headIndex - lineCount + keep);
  for (i = 0; i < lineCount; i++) {
    Line *line = &ringBuf[(start + i) % keep];
    if (line->len > 0) {
      write(1, line->data, line->len);
    }
  }
}

static void consume_fd(int fd, const char *filename, int keep, int isMulti) {
  Line *ringBuf;
  int headIndex;
  int lineCount;
  Line current;
  char chunk[BUF_CHUNK];
  int bytesRead;
  int i;
  int k;
  int headerOn;

  if (keep < 0) {
    keep = 10;
  }

  if (keep == 0) {
    while (read(fd, chunk, sizeof(chunk)) > 0) {
      /* drain */
    }

    if (isMulti && filename) {
      printf(1, "==> %s <==\n", filename);
    }

    return;
  }

  ringBuf = (Line*)malloc(sizeof(Line) * keep);
  if (!ringBuf) {
    printf(2, "tail: out of memory\n");
    return;
  }

  for (i = 0; i < keep; i++) {
    ringBuf[i].data = 0;
    ringBuf[i].len = 0;
    ringBuf[i].cap = 0;
  }

  headIndex = 0;
  lineCount = 0;

  current.data = 0;
  current.len = 0;
  current.cap = 0;

  while ((bytesRead = read(fd, chunk, sizeof(chunk))) > 0) {
    for (i = 0; i < bytesRead; i++) {
      char ch = chunk[i];
      if (append_char(&current, ch) < 0) {
        printf(2, "tail: out of memory\n");
        for (k = 0; k < keep; k++) {
          free_line(&ringBuf[k]);
        }

        free_line(&current);
        free(ringBuf);
        return;
      }

      if (ch == '\n') {
        if (ringBuf[headIndex].data) {
          free_line(&ringBuf[headIndex]);
        }

        ringBuf[headIndex] = current;  // move ownership
        headIndex = (headIndex + 1) % keep;
        if (lineCount < keep) {
          lineCount++;
        }

        current.data = 0;
        current.len = 0;
        current.cap = 0;
      }
    }
  }

  // EOF: handle trailing line without '\n'
  if (current.len > 0) {
    if (ringBuf[headIndex].data) {
      free_line(&ringBuf[headIndex]);
    }

    ringBuf[headIndex] = current;
    headIndex = (headIndex + 1) % keep;
    if (lineCount < keep) {
      lineCount++;
    }

    current.data = 0;
    current.len = 0;
    current.cap = 0;
  }

  headerOn = (isMulti && filename != 0);
  print_lines(ringBuf, keep, lineCount, headIndex, headerOn, filename);

  for (k = 0; k < keep; k++) {
    free_line(&ringBuf[k]);
  }

  free(ringBuf);
}

static int parse_int(const char *str, int *outValue) {
  int value = 0;

  if (!str || !*str) {
    return -1;
  }

  for (; *str; str++) {
    if (*str < '0' || *str > '9') {
      return -1;
    }

    value = value*10 + (*str - '0');
  }

  *outValue = value;
  return 0;
}

static void usage(void) {
  printf(2, "usage: tail [-N] | [-n N] [FILE...]\n");
}

int main(int argc, char *argv[]) {
  int keep = 10;
  int argi = 1;
  int fileCount;
  int isMulti;
  int fd;

  if (argi < argc && argv[argi][0] == '-') {
    if (argv[argi][1] == 'n') {
      if (argi + 2 > argc) {
        usage();
        exit();
      }

      if (parse_int(argv[argi+1], &keep) < 0) {
        usage();
        exit();
      }

      argi += 2;
    }
    else {
      // form: -N (no space)
      int tmpN;
      if (parse_int(argv[argi] + 1, &tmpN) < 0) {
        usage();
        exit();
      }

      keep = tmpN;
      argi += 1;
    }
  }

  // remaining args are files; if none, read stdin
  fileCount = argc - argi;
  if (fileCount <= 0) {
    consume_fd(0, 0, keep, 0);
    exit();
  }

  isMulti = (fileCount > 1);
  for (; argi < argc; argi++) {
    fd = open(argv[argi], O_RDONLY);
    if (fd < 0) {
      printf(2, "tail: cannot open %s\n", argv[argi]);
      continue;
    }

    consume_fd(fd, argv[argi], keep, isMulti);
    close(fd);

    if (isMulti && argi != argc - 1) {
      printf(1, "\n");
    }
  }

  exit();
}
