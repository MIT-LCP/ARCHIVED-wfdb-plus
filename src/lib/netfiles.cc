#include "netfiles.hh"

#include <absl/strings/str_format.h>
#include <errno.h>
#include <stdlib.h>

#include <string>

#include "wfdb.hh"

// State tracking
static int nf_open_files = 0;         /* number of open netfiles */
static long page_size = NF_PAGE_SIZE; /* bytes per range request (0: disable
                                         range requests) */
static int www_done_init = 0;         /* TRUE once libcurl is initialized */

static CURL *curl_ua = NULL;
static char curl_error_buf[CURL_ERROR_SIZE];

static char **passwords;

int nf_vfprintf(Netfile *nf, const char *format, va_list ap) {
  /* no support yet for writing to remote files */
  errno = EROFS;
  return 0;
}

/* Construct the User-Agent string to be sent with HTTP requests. */
std::string curl_get_ua_string() {
  /* The +3XX flag informs the server that this client understands
     and supports HTTP redirection (CURLOPT_FOLLOWLOCATION
     enabled.) */
  return absl::StrFormat("libwfdb/%d.%d.%d (%s +3XX)", WFDB_MAJOR, WFDB_MINOR,
                         WFDB_RELEASE, curl_version());
}

/* This function will print out the curl error message if there was an
   error. Zero means there was no error. */
int curl_try(CURLcode err) {
  if (err) {
    wfdb_error(absl::StrFormat("curl error: %s\n", curl_error_buf));
  }
  return err;
}

/* Get the current time, as an unsigned number of seconds since some
   arbitrary starting point. */
unsigned int www_time() {
#ifdef HAVE_CLOCK_GETTIME
  struct timespec ts;
  if (!clock_gettime(CLOCK_MONOTONIC, &ts)) return ((unsigned int)ts.tv_sec);
#endif
  return ((unsigned int)time(NULL));
}

/* This is a dummy write callback, for when we don't care about the
   data curl is receiving. */
size_t curl_null_write(void *ptr, size_t size, size_t nmemb, void *stream) {
  return (size * nmemb);
}

/* www_parse_passwords parses the WFDBPASSWORD environment variable.
This environment variable contains a list of URL prefixes and
corresponding usernames/passwords.  Alternatively, the environment
variable may contain '@' followed by the name of a file containing
password information.

Each item in the list consists of a URL prefix, followed by a space,
then the username and password separated by a colon.  For example,
setting WFDBPASSWORD to "https://example.org john:letmein" would use
the username "john" and the password "letmein" for all HTTPS requests
to example.org.

If there are multiple items in the list, they must be separated by
end-of-line or tab characters. */
void www_parse_passwords(const char *str) {
  static char sep[] = "\t\n\r";
  char *xstr = NULL, *p, *q;
  int n;

  SSTRCPY(xstr, str);
  wfdb_getiwfdb(&xstr);
  if (!xstr) return;

  SALLOC(passwords, 1, sizeof(char *));
  n = 0;
  for (p = strtok(xstr, sep); p; p = strtok(NULL, sep)) {
    if (!(q = strchr(p, ' ')) || !strchr(q, ':')) continue;
    SREALLOC(passwords, n + 2, sizeof(char *));
    if (!passwords) return;
    SSTRCPY(passwords[n], p);
    n++;
  }
  passwords[n] = NULL;

  SFREE(xstr);
}

/* www_userpwd determines which username/password should be used for a
given URL.  It returns a string of the form "username:password" if one
is defined, or returns NULL if no login information is required for
that URL. */
const char *www_userpwd(const char *url) {
  int i, n;
  const char *p;

  for (i = 0; passwords && passwords[i]; i++) {
    p = strchr(passwords[i], ' ');
    if (!p || p == passwords[i]) continue;

    n = p - passwords[i];
    if (strncmp(passwords[i], url, n) == 0 &&
        (url[n] == 0 || url[n] == '/' || url[n - 1] == '/')) {
      return &passwords[i][n + 1];
    }
  }

  return NULL;
}

/* Create a new, empty chunk. */
Chunk *curl_chunk_new(long len) {
  Chunk *c;

  SUALLOC(c, 1, sizeof(Chunk));
  SALLOC(c->data, 1, len);
  c->size = 0L;
  c->buffer_size = len;
  c->start_pos = 0;
  c->end_pos = 0;
  c->total_size = 0;
  c->url = NULL;
  return c;
}

/* Delete a chunk */
void curl_chunk_delete(Chunk *c) {
  if (c) {
    SFREE(c->data);
    SFREE(c->url);
    SFREE(c);
  }
}

Chunk *www_get_url_chunk(const char *url) {
  Chunk *chunk = NULL;

  chunk = curl_chunk_new(1024);
  if (!chunk) return (NULL);

  if (/* Send a GET request */
      curl_try(curl_easy_setopt(curl_ua, CURLOPT_NOBODY, 0L)) ||
      curl_try(curl_easy_setopt(curl_ua, CURLOPT_HTTPGET, 1L))
      /* URL to retrieve */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_URL, url))
      /* Set username/password */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_USERPWD, www_userpwd(url)))
      /* No range request */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_RANGE, NULL))
      /* Write to the chunk specified ... */
      || curl_try(
             curl_easy_setopt(curl_ua, CURLOPT_WRITEFUNCTION, curl_chunk_write))
      /* ... by this pointer */
      || curl_try(
             curl_easy_setopt(curl_ua, CURLOPT_HEADERFUNCTION, curl_null_write))
      /* and ignore the header data */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_WRITEDATA, chunk))
      /* perform the request */
      || www_perform_request(curl_ua)) {
    curl_chunk_delete(chunk);
    return (NULL);
  }
  if (!chunk->data) {
    curl_chunk_delete(chunk);
    chunk = NULL;
  }

  return (chunk);
}

void nf_delete(Netfile *nf) {
  if (nf) {
    SFREE(nf->url);
    SFREE(nf->data);
    SFREE(nf->redirect_url);
    SFREE(nf);
  }
}

Chunk *nf_get_url_range_chunk(Netfile *nf, long startb, long len) {
  char *url;
  Chunk *chunk;
  unsigned int request_time;

  /* If a previous request for this file was recently redirected,
     use the previous (redirected) URL; otherwise, use the original
     URL.  (If the system clock moves backwards, the cache is
     assumed to be out-of-date.) */
  request_time = www_time();
  if (request_time - nf->redirect_time > REDIRECT_CACHE_TIME) {
    SFREE(nf->redirect_url);
  }
  url = (nf->redirect_url ? nf->redirect_url : nf->url);

  chunk = www_get_url_range_chunk(url, startb, len);

  if (chunk && chunk->url) {
    /* don't update redirect_time if we didn't hit nf->url */
    if (!nf->redirect_url) nf->redirect_time = request_time;
    SSTRCPY(nf->redirect_url, chunk->url);
  }
  return (chunk);
}

/* nf_new attempts to read (at least part of) the file named by its
   argument (normally an http:// or ftp:// url).  If page_size is nonzero and
   the file can be read in segments (this will be true for files served by http
   servers that support range requests, and possibly for other types of files
   if NETFILES support is available), nf_new reads the first page_size bytes
   (or fewer, if the file is shorter than page_size).  Otherwise, nf_new
   attempts to read the entire file into memory.  If there is insufficient
   memory, if the file contains no data, or if the file does not exist (the
   most common of these three cases), nf_new returns a NULL pointer; otherwise,
   it allocates, fills in, and returns a pointer to a Netfile structure that
   can be used by nf_fread, etc., to obtain the contents of the file.

   It would be useful to be able to copy a file that cannot be read in segments
   to a local file, but this operation is not currently implemented.  The way
   to do this would be to invoke
      HTLoadToFile(url, request, filename);
*/
Netfile *nf_new(const char *url) {
  Netfile *nf;
  Chunk *chunk = NULL;

  SUALLOC(nf, 1, sizeof(Netfile));
  if (nf && url && *url) {
    SSTRCPY(nf->url, url);
    nf->base_addr = 0;
    nf->pos = 0;
    nf->data = NULL;
    nf->err = NF_NO_ERR;
    nf->fd = -1;
    nf->redirect_url = NULL;

    if (page_size > 0L) /* Try to read the first part of the file. */
      chunk = nf_get_url_range_chunk(nf, 0L, page_size);
    else
      /* Try to read the entire file. */
      chunk = www_get_url_chunk(nf->url);

    if (!chunk) {
      nf_delete(nf);
      return (NULL);
    }

    if (chunk->start_pos == 0 && chunk->end_pos == chunk->size - 1 &&
        chunk->total_size >= chunk->size &&
        (chunk->size == page_size || chunk->size == chunk->total_size)) {
      /* Range request works and the total file size is known. */
      nf->cont_len = chunk->total_size;
      nf->mode = NetfileMode::kChunkMode;
    } else if (chunk->total_size == 0) {
      if (page_size > 0 && chunk->size == page_size)
        /* This might be a range response from a protocol that
           doesn't report the file size, or might be a file
           that happens to be exactly the size we requested.
           Check the full size of the file. */
        nf->cont_len = www_get_cont_len(nf->url);
      else
        nf->cont_len = chunk->size;

      if (nf->cont_len > chunk->size) {
        nf->mode = NetfileMode::kChunkMode;
      } else {
        nf->mode = NetfileMode::kFullMode;
      }
    } else {
      wfdb_error("nf_new: unexpected range response (%lu-%lu/%lu)\n",
                 chunk->start_pos, chunk->end_pos, chunk->total_size);
      curl_chunk_delete(chunk);
      nf_delete(nf);
      return (NULL);
    }
    if (chunk->size > 0L) {
      nf->data = chunk->data;
      chunk->data = NULL;
    }
    if (nf->data == NULL) {
      if (chunk->size > 0L)
        wfdb_error("nf_new: insufficient memory (needed %ld bytes)\n",
                   chunk->size);
      /* If no bytes were received, the remote file probably doesn't
         exist.  This happens routinely while searching the WFDB path, so
         it's not flagged as an error.  Note, however, that we can't tell
         the difference between a nonexistent file and an empty one.
         Another possibility is that range requests are unsupported
         (e.g., if we're trying to read an ftp url), and there is
         insufficient memory for the entire file. */
      nf_delete(nf);
      nf = NULL;
    }
    if (chunk) curl_chunk_delete(chunk);
  }
  return (nf);
}

long nf_get_range(Netfile *nf, long startb, long len, char *rbuf) {
  Chunk *chunk = NULL;
  char *rp = NULL;
  long avail = nf->cont_len - startb;

  if (len > avail) len = avail; /* limit request to available bytes */
  if (nf == NULL || nf->url == NULL || *nf->url == '\0' || startb < 0L ||
      startb >= nf->cont_len || len <= 0L || rbuf == NULL)
    return (0L); /* invalid inputs -- fail silently */

  if (nf->mode == NetfileMode::kChunkMode) { /* range requests acceptable */
    long rlen = (avail >= page_size) ? page_size : avail;

    if (len <= page_size) { /* short request -- check if cached */
      if ((startb < nf->base_addr) ||
          ((startb + len) > (nf->base_addr + page_size))) {
        /* requested data not in cache -- update the cache */
        if (chunk = nf_get_url_range_chunk(nf, startb, rlen)) {
          if (chunk->size != rlen) {
            wfdb_error(
                "nf_get_range: requested %ld bytes, received %ld bytes\n", rlen,
                (long)chunk->size);
            len = 0L;
          } else {
            nf->base_addr = startb;
            memcpy(nf->data, chunk->data, rlen);
          }
        } else { /* attempt to update cache failed */
          wfdb_error(
              "nf_get_range: couldn't read %ld bytes of %s starting at %ld\n",
              len, nf->url, startb);
          len = 0L;
        }
      }

      /* move cached data to the return buffer */
      rp = nf->data + startb - nf->base_addr;
    }

    else if (chunk = nf_get_url_range_chunk(nf, startb, len)) {
      /* long request (> page_size) */
      if (chunk->size != len) {
        wfdb_error("nf_get_range: requested %ld bytes, received %ld bytes\n",
                   len, (long)chunk->size);
        len = 0L;
      }
      rp = chunk->data;
    } else {
      wfdb_error(
          "nf_get_range: couldn't read %ld bytes of %s starting at %ld\n", len,
          nf->url, startb);
      len = 0L;
    }
  }

  else /* cannot use range requests -- cache contains full file */
    rp = nf->data + startb;

  if (rp != NULL && len > 0) memcpy(rbuf, rp, len);
  if (chunk) curl_chunk_delete(chunk);
  return (len);
}

/* nf_feof returns true after reading past the end of a file but before
   repositioning the pos in the file. */
int nf_feof(Netfile *nf) { return ((nf->err == NF_EOF_ERR) ? 1 : 0); }

/* nf_eof returns true if the file pointer is at the EOF. */
int nf_eof(Netfile *nf) { return (nf->pos >= nf->cont_len) ? 1 : 0; }

Netfile *nf_fopen(const char *url, const char *mode) {
  Netfile *nf = NULL;

  if (!www_done_init) www_init();
  if (*mode == 'w' || *mode == 'a')
    errno = EROFS; /* no support for output */
  else if (*mode != 'r')
    errno = EINVAL; /* invalid mode string */
  else if (nf = nf_new(url))
    nf_open_files++;
  return (nf);
}

int nf_fclose(Netfile *nf) {
  nf_delete(nf);
  nf_open_files--;
  return (0);
}

int nf_fgetc(Netfile *nf) {
  char c;

  if (nf_get_range(nf, nf->pos++, 1, &c)) return (c & 0xff);
  nf->err = (nf->pos >= nf->cont_len) ? NF_EOF_ERR : NF_REAL_ERR;
  return (EOF);
}

static char *nf_fgets(char *s, int size, Netfile *nf) {
  int c = 0, i = 0;

  if (s == NULL) return (NULL);
  while (c != '\n' && i < (size - 1) && (c = nf_fgetc(nf)) != EOF) s[i++] = c;
  if ((c == EOF) && (i == 0)) return (NULL);
  s[i] = 0;
  return (s);
}

size_t nf_fread(void *ptr, size_t size, size_t nmemb, Netfile *nf) {
  long bytes_available, bytes_read = 0L, bytes_requested = size * nmemb;

  if (nf == NULL || ptr == NULL || bytes_requested == 0) return ((size_t)0);
  bytes_available = nf->cont_len - nf->pos;
  if (bytes_requested > bytes_available) bytes_requested = bytes_available;
  if (bytes_requested > page_size && page_size) bytes_requested = page_size;
  nf->pos += bytes_read = nf_get_range(nf, nf->pos, bytes_requested, ptr);
  return ((size_t)(bytes_read / size));
}

int nf_fseek(Netfile *nf, long offset, int whence) {
  int ret = -1;

  if (nf) switch (whence) {
      case SEEK_SET:
        if (offset >= 0 && offset <= nf->cont_len) {
          nf->pos = offset;
          nf->err = NF_NO_ERR;
          ret = 0;
        }
        break;
      case SEEK_CUR:
        if (((unsigned long)nf->pos + offset) <= nf->cont_len) {
          nf->pos += offset;
          nf->err = NF_NO_ERR;
          ret = 0;
        }
        break;
      case SEEK_END:
        if (offset <= 0 && offset >= -nf->cont_len) {
          nf->pos = nf->cont_len + offset;
          nf->err = NF_NO_ERR;
          ret = 0;
        }
        break;
      default:
        break;
    }
  return (ret);
}

long nf_ftell(Netfile *nf) { return (nf->pos); }

int nf_ferror(Netfile *nf) { return ((nf->err == NF_REAL_ERR) ? 1 : 0); }

void nf_clearerr(Netfile *nf) { nf->err = NF_NO_ERR; }

int nf_fflush(Netfile *nf) {
  /* no support yet for writing to remote files */
  errno = EROFS;
  return (EOF);
}

size_t nf_fwrite(const void *ptr, size_t size, size_t nmemb, Netfile *nf) {
  /* no support yet for writing to remote files */
  errno = EROFS;
  return (0);
}

int nf_putc(int c, Netfile *nf) {
  /* no support yet for writing to remote files */
  errno = EROFS;
  return (EOF);
}

void wfdb_wwwquit() {
  int i;
  if (www_done_init) {
#ifndef _WINDOWS
    curl_easy_cleanup(curl_ua);
    curl_ua = NULL;
    curl_global_cleanup();
#endif
    www_done_init = 0;
    for (i = 0; passwords && passwords[i]; i++) SFREE(passwords[i]);
    SFREE(passwords);
  }
}

void www_init() {
  if (!www_done_init) {
    char *p;

    if ((p = getenv("WFDB_PAGESIZE")) && *p) page_size = strtol(p, NULL, 10);

    /* Initialize the curl "easy" handle. */
    curl_global_init(CURL_GLOBAL_ALL);
    curl_ua = curl_easy_init();
    /* Buffer for error messages */
    curl_easy_setopt(curl_ua, CURLOPT_ERRORBUFFER, curl_error_buf);
    /* String to send as a User-Agent header */
    curl_easy_setopt(curl_ua, CURLOPT_USERAGENT, curl_get_ua_string());
#ifdef USE_NETRC
    /* Search $HOME/.netrc for passwords */
    curl_easy_setopt(curl_ua, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
#endif
    /* Get password information from the environment if available */
    if ((p = getenv("WFDBPASSWORD")) && *p) www_parse_passwords(p);

    /* Get the name of the CA bundle file */
    if ((p = getenv("CURL_CA_BUNDLE")) && *p)
      curl_easy_setopt(curl_ua, CURLOPT_CAINFO, p);

    /* Use any available authentication method */
    curl_easy_setopt(curl_ua, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    /* Follow up to 5 redirections */
    curl_easy_setopt(curl_ua, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_ua, CURLOPT_MAXREDIRS, 5L);

    /* Show details of URL requests if WFDB_NET_DEBUG is set */
    if ((p = getenv("WFDB_NET_DEBUG")) && *p)
      curl_easy_setopt(curl_ua, CURLOPT_VERBOSE, 1L);

    atexit(wfdb_wwwquit);
    www_done_init = 1;
  }
}

/* Send a request and wait for the response.  If using HTTP, check the
   response code to see whether the request was successful. */
int www_perform_request(CURL *c) {
  long code;
  if (curl_easy_perform(c)) return (-1);
  if (curl_easy_getinfo(c, CURLINFO_HTTP_CODE, &code)) return (0);
  return (code < 400 ? 0 : -1);
}

long www_get_cont_len(const char *url) {
  double length;

  length = 0;
  if (/* We just want the content length; NOBODY means we want to
         send a HEAD request rather than GET */
      curl_try(curl_easy_setopt(curl_ua, CURLOPT_NOBODY, 1L))
      /* Set the URL to retrieve */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_URL, url))
      /* Set username/password */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_USERPWD, www_userpwd(url)))
      /* Don't send a range request */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_RANGE, NULL))
      /* Ignore both headers and body */
      || curl_try(curl_easy_setopt(curl_ua, CURLOPT_WRITEFUNCTION,
                                   curl_null_write)) ||
      curl_try(
          curl_easy_setopt(curl_ua, CURLOPT_HEADERFUNCTION, curl_null_write))
      /* Actually perform the request and wait for a response */
      || www_perform_request(curl_ua))
    return (0);

  if (curl_easy_getinfo(curl_ua, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &length))
    return (0);

  return ((long)length);
}

/* Write metadata (e.g., HTTP headers) into a chunk.  This function is
   called by curl and must take the same arguments as fwrite().  ptr
   points to the data received, size*nmemb is the number of bytes, and
   stream is the user data specified by CURLOPT_WRITEHEADER. */
size_t curl_chunk_header_write(void *ptr, size_t size, size_t nmemb,
                               void *stream) {
  char *s = (char *)ptr;
  Chunk *c = (Chunk *)stream;

  if (0 == strncasecmp(s, "Content-Range:", 14)) {
    s += 14;
    while (*s == ' ') s++;
    if (0 == strncasecmp(s, "bytes ", 6))
      sscanf(s + 6, "%lu-%lu/%lu", &c->start_pos, &c->end_pos, &c->total_size);
  }
  return (size * nmemb);
}

/* Write data into a chunk.  This function is called by curl and must
   take the same arguments as fwrite().  ptr points to the data
   received, size*nmemb is the number of bytes, and stream is the user
   data specified by CURLOPT_WRITEDATA. */
size_t curl_chunk_write(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t count = 0;
  char *p;
  Chunk *c = (Chunk *)stream;

  while (nmemb > 0) {
    while ((c->size + size) > c->buffer_size) {
      c->buffer_size += 1024;
      SREALLOC(c->data, c->buffer_size, 1);
    }
    if (c->data) memcpy(c->data + c->size, ptr, size);
    c->size += size;
    count += size;
    p = (char *)ptr + size; /* avoid arithmetic on void pointer */
    ptr = (void *)p;
    nmemb--;
  }
  return (count);
}

/* This function emulates the libwww function HTChunk_putb. */
void curl_chunk_putb(Chunk *chunk, char *data, size_t len) {
  curl_chunk_write(data, 1, len, chunk);
}

Chunk *www_get_url_range_chunk(const char *url, long startb, long len) {
  Chunk *chunk = NULL, *extra_chunk = NULL;
  char range_req_str[6 * sizeof(long) + 2];
  const char *url2 = NULL;

  if (url && *url) {
    sprintf(range_req_str, "%ld-%ld", startb, startb + len - 1);
    chunk = curl_chunk_new(len);
    if (!chunk) return (NULL);

    if (/* In this case we want to send a GET request rather than
           a HEAD */
        curl_try(curl_easy_setopt(curl_ua, CURLOPT_NOBODY, 0L)) ||
        curl_try(curl_easy_setopt(curl_ua, CURLOPT_HTTPGET, 1L))
        /* URL to retrieve */
        || curl_try(curl_easy_setopt(curl_ua, CURLOPT_URL, url))
        /* Set username/password */
        ||
        curl_try(curl_easy_setopt(curl_ua, CURLOPT_USERPWD, www_userpwd(url)))
        /* Range request */
        || curl_try(curl_easy_setopt(curl_ua, CURLOPT_RANGE, range_req_str))
        /* This function will be used to "write" data as it is received */
        || curl_try(curl_easy_setopt(curl_ua, CURLOPT_WRITEFUNCTION,
                                     curl_chunk_write))
        /* The pointer to pass to the write function */
        || curl_try(curl_easy_setopt(curl_ua, CURLOPT_WRITEDATA, chunk))
        /* This function will be used to parse HTTP headers */
        || curl_try(curl_easy_setopt(curl_ua, CURLOPT_HEADERFUNCTION,
                                     curl_chunk_header_write))
        /* The pointer to pass to the header function */
        || curl_try(curl_easy_setopt(curl_ua, CURLOPT_WRITEHEADER, chunk))
        /* Perform the request */
        || www_perform_request(curl_ua)) {
      curl_chunk_delete(chunk);
      return (NULL);
    }
    if (!chunk->data) {
      curl_chunk_delete(chunk);
      chunk = NULL;
    } else if (!curl_easy_getinfo(curl_ua, CURLINFO_EFFECTIVE_URL, &url2) &&
               url2 && *url2 && strcmp(url, url2)) {
      SSTRCPY(chunk->url, url2);
    }
  }
  return (chunk);
}
