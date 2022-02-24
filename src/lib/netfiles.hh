#ifndef WFDB_LIB_NETFILES_H_
#define WFDB_LIB_NETFILES_H_

#include <curl/curl.h>

struct Netfile {
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

/* values for Netfile 'err' field */
#define NF_NO_ERR 0   /* no errors */
#define NF_EOF_ERR 1  /* file pointer at EOF */
#define NF_REAL_ERR 2 /* read request failed */

/* values for Netfile 'mode' field */
#define NF_CHUNK_MODE 0 /* http range requests supported */
#define NF_FULL_MODE 1  /* http range requests not supported */

/* The next group of functions, which enable input from remote
(http and ftp) files, were first implemented in version 10.0.1 by Michael
Dakin.  Thanks, Mike!

In the current version of the WFDB library, output to remote files is not
implemented;  for this reason, several of the functions listed below are
stubs (placeholders) only, as noted.
*/

/* These functions, defined here if WFDB_NETFILES is non-zero, are intended only
for the use of the functions in the next group below */

// Load username/password information
void www_parse_passwords(const char *str);
// Get username/password for a given url
const char *www_userpwd(const char *url);
// Shut down libcurl cleanly
void wfdb_wwwquit();
// Initialize libcurl
void www_init();
// Request a url and check the response code
int www_perform_request(CURL *c);
// Find length of data for a given url
long www_get_cont_len(const char *url);
// Get a block of data from a given url
Chunk *www_get_url_range_chunk(const char *url, long startb, long len);
// Get all data from a given url
Chunk *www_get_url_chunk(const char *url);
// Free data structures associated with an open netfile
void nf_delete(Netfile *nf);

// Associate a Netfile with a url
Netfile *nf_new(const char *url);
// get a block of data from a netfile
long nf_get_range(Netfile *nf, long startb, long len, char *rbuf);
// Emulates feof, for netfiles
int nf_feof(Netfile *nf);
// TRUE if Netfile pointer points to EOF
int nf_eof(Netfile *nf);
// Emulate fopen, for netfiles; read-only access
Netfile *nf_fopen(const char *url, const char *mode);
// Emulates fclose, for netfiles
int nf_fclose(Netfile *nf);
// Emulates fgetc, for netfiles
int nf_fgetc(Netfile *nf);
// Emulates fgets, for netfiles
static char *nf_fgets(char *s, int size, Netfile *nf);
// Emulates fread, for netfiles
size_t nf_fread(void *ptr, size_t size, size_t nmemb, Netfile *nf);
// Emulates fseek, for netfiles
int nf_fseek(Netfile *nf, long offset, int whence);
// Emulates ftell, for netfiles
long nf_ftell(Netfile *nf);
// Emulates ferror, for netfiles
int nf_ferror(Netfile *nf);
// Emulates clearerr, for netfiles
void nf_clearerr(Netfile *nf);
// Emulates fflush, for netfiles [stub]
int nf_fflush(Netfile *nf);
// Emulates fwrite, for netfiles [stub]
size_t nf_fwrite(const void *ptr, size_t size, size_t nmemb, Netfile *nf);
// Emulates putc, for netfiles) [stub]
int nf_putc(int c, Netfile *nf);
// emulates fprintf, for netfiles) [stub]
int nf_vfprintf(Netfile *nf, const char *format, va_list ap);

char *curl_get_ua_string();
int curl_try(CURLcode err);
unsigned int www_time();
size_t curl_null_write(void *ptr, size_t size, size_t nmemb, void *stream);
Chunk *curl_chunk_new(long len);
void curl_chunk_delete(Chunk *c);
Chunk *nf_get_url_range_chunk(Netfile *nf, long startb, long len);
size_t curl_chunk_header_write(void *ptr, size_t size, size_t nmemb,
                               void *stream);
size_t curl_chunk_write(void *ptr, size_t size, size_t nmemb, void *stream);
void curl_chunk_putb(Chunk *chunk, char *data, size_t len);

#endif  // WFDB_LIB_NETFILES_H_
