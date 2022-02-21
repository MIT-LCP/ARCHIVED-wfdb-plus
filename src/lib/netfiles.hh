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

struct Chunk {
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

int nf_vfprintf(netfile *nf, const char *format, va_list ap);
char *curl_get_ua_string();
int curl_try(CURLcode err);
unsigned int www_time();
size_t curl_null_write(void *ptr, size_t size, size_t nmemb, void *stream);
void www_parse_passwords(const char *str);
const char *www_userpwd(const char *url);

Chunk *curl_chunk_new(long len);
void curl_chunk_delete(Chunk *c);
Chunk *www_get_url_chunk(const char *url);

void nf_delete(netfile *nf);
Chunk *nf_get_url_range_chunk(netfile *nf, long startb, long len);
netfile *nf_new(const char *url);
long nf_get_range(netfile *nf, long startb, long len, char *rbuf);
int nf_feof(netfile *nf);
int nf_eof(netfile *nf);
netfile *nf_fopen(const char *url, const char *mode);
int nf_fclose(netfile *nf);
int nf_fgetc(netfile *nf);
size_t nf_fread(void *ptr, size_t size, size_t nmemb, netfile *nf);
int nf_fseek(netfile *nf, long offset, int whence);
long nf_ftell(netfile *nf);
int nf_ferror(netfile *nf);
void nf_clearerr(netfile *nf);
int nf_fflush(netfile *nf);
size_t nf_fwrite(const void *ptr, size_t size, size_t nmemb, netfile *nf);
int nf_putc(int c, netfile *nf);

void wfdb_wwwquit();
void www_init();
int www_perform_request(CURL *c);
long www_get_cont_len(const char *url);
size_t curl_chunk_header_write(void *ptr, size_t size, size_t nmemb,
                               void *stream);
size_t curl_chunk_write(void *ptr, size_t size, size_t nmemb, void *stream);
void curl_chunk_putb(Chunk *chunk, char *data, size_t len);
Chunk *www_get_url_range_chunk(const char *url, long startb, long len);
