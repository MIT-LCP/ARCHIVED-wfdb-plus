#include <curl/curl.h>

struct netfile {
  char *url;
  char *data;
  int mode;
  long base_addr;
  long cont_len;
  long pos;
  long err;
  int fd;
  char *redirect_url;
  unsigned int redirect_time;
};

struct chunk {
  long size, buffer_size;
  unsigned long start_pos, end_pos, total_size;
  char *data;
  char *url;
};

// Constants/Config

/* cache redirections for 5 minutes */
#define REDIRECT_CACHE_TIME (5 * 60)

#define NF_PAGE_SIZE 32768 /* default bytes per http range request */

#ifndef EROFS /* errno value: attempt to write to a read-only file system */
#ifdef EINVAL
#define EROFS EINVAL
#else
#define EROFS 1
#endif
#endif

/* values for netfile 'err' field */
#define NF_NO_ERR 0   /* no errors */
#define NF_EOF_ERR 1  /* file pointer at EOF */
#define NF_REAL_ERR 2 /* read request failed */

/* values for netfile 'mode' field */
#define NF_CHUNK_MODE 0 /* http range requests supported */
#define NF_FULL_MODE 1  /* http range requests not supported */
