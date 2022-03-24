/* file: wfdbio.cc	G. Moody	18 November 1988

Low-level I/O functions for the WFDB library

*/

#include "wfdbio.hh"

#include <absl/strings/str_split.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <vector>

#include "wfdb.hh"

inline constexpr WfdbConfig kDefaultWfdbConfig{
    /* This value is edited by the configuration script
       (../configure), which also edits this block of comments to match.*/
    // TODO: Account for non-netfiles
    .wfdb_path = ". DBDIR http://physionet.org/physiobank/database",
    .wfdb_cal = "wfdbcal",
    .ann_sort = true,
    .getvec_mode = GetVecMode::kLowRes};

/* Global configuration variables */

struct WfdbRuntimeConfig {
  std::string wfdb_path;
  std::vector<WfdbPathComponent>
      wfdb_path_list;  // TODO: replace this with a db source list
  std::string wfdb_cal;
  bool ann_sort;
  GetVecMode get_vec_mode;
  std::string wfdb_filename;
};

static WfdbRuntimeConfig wfdb_runtime_config;

/* getwfdb is used to obtain the WFDB path, a list of places in which to search
for database files to be opened for reading.  In most environments, this list
is obtained from the shell (environment) variable WFDB, which may be set by the
user.  A default value may be set at compile time (DEFWFDB in wfdblib.h).
 */

const std::string &getwfdb() { return wfdb_runtime_config.wfdb_path; }

/* resetwfdb is called by wfdbquit, and can be called within an application,
to restore the WFDB path to the value that was returned by the first call
to getwfdb (or NULL if getwfdb was not called). */

void resetwfdb() { setwfdb(kDefaultWfdbConfig.wfdb_path); }

/* Changes the WFDB path. */
void setwfdb(std::string_view path) {
  // TODO: Validate path. Or stop storing the full path string.
  wfdb_runtime_config.wfdb_path = path;
  wfdb_parse_path(path);
}

// Set the WFDB path from the environment variable or config file if present
void init_wfdb_path() {
  const std::string path = getenv("WFDB");

  if (path.empty()) {
    // TODO: Get value from config file
    resetwfdb();
  } else {
    // Get WFDB path from file
    if (path.starts_with("@")) {
      std::ifstream file;
      file.open(path, std::ios::in);

      if (file.is_open()) {
        std::string line;
        std::getline(file, line);
        setwfdb(line);
      }
    } else {
      setwfdb(path);
    }
    // TODO: Use default if previous calls are unsuccessful
  }
};

/* wfdbfile returns the pathname or URL of a WFDB file. */

char *wfdbfile(const char *s, char *record) {
  WFDB_FILE *ifile;

  if (s == NULL && record == NULL) return (wfdb_filename);

  /* Remove trailing .hea, if any, from record name. */
  wfdb_striphea(record);

  if ((ifile = wfdb_open(s, record, WFDB_READ))) {
    // Using function to set global, following by returning the global
    (void)wfdb_fclose(ifile);
    return (wfdb_filename);
  } else
    return (NULL);
}

/* The next four functions read and write integers in PDP-11 format, which is
common to both MIT and AHA database files.  The purpose is to achieve
interchangeability of binary database files between machines which may use
different byte layouts.  The routines below are machine-independent; in some
cases (notably on the PDP-11 itself), taking advantage of the native byte
layout can improve the speed.  For 16-bit integers, the low (least significant)
byte is written (read) before the high byte; 32-bit integers are represented as
two 16-bit integers, but the high 16 bits are written (read) before the low 16
bits. These functions, in common with other WFDB library functions, assume that
a byte is 8 bits, a "short" is 16 bits, an "int" is at least 16 bits, and a
"long" is at least 32 bits.  The last two assumptions are valid for ANSI C
compilers, and for almost all older C compilers as well.  If a "short" is not
16 bits, it may be necessary to rewrite wfdb_g16() to obtain proper sign
extension. */

/* read a 16-bit integer in PDP-11 format */
int wfdb_g16(WFDB_FILE *fp) {
  int x;

  x = wfdb_getc(fp);
  return ((int)((short)((wfdb_getc(fp) << 8) | (x & 0xff))));
}

/* read a 32-bit integer in PDP-11 format */
long wfdb_g32(WFDB_FILE *fp) {
  long x, y;

  x = wfdb_g16(fp);
  y = wfdb_g16(fp);
  return ((x << 16) | (y & 0xffff));
}

/* write a 16-bit integer in PDP-11 format */
void wfdb_p16(unsigned int x, WFDB_FILE *fp) {
  (void)wfdb_putc((char)x, fp);
  (void)wfdb_putc((char)(x >> 8), fp);
}

/* write a 32-bit integer in PDP-11 format */
void wfdb_p32(long x, WFDB_FILE *fp) {
  wfdb_p16((unsigned int)(x >> 16), fp);
  wfdb_p16((unsigned int)x, fp);
}

/* Operating system and compiler dependent code

All of the operating system and compiler dependencies in the WFDB library are
contained within the following section of this file.  There are three
significant types of platforms addressed here:

   UNIX and variants
     This includes GNU/Linux, FreeBSD, Solaris, HP-UX, IRIX, AIX, and other
     versions of UNIX, as well as Mac OS/X and Cygwin/MS-Windows.
   MS-DOS and variants
     This group includes MS-DOS, DR-DOS, OS/2, and all versions of MS-Windows
     when using the native MS-Windows libraries (as when compiling with MinGW
     gcc).
   MacOS 9 and earlier
     "Classic" MacOS.

Differences among these platforms:

1. Directory separators vary:
     UNIX and variants (including Mac OS/X and Cygwin) use '/'.
     MS-DOS and OS/2 use '\'.
     MacOS 9 and earlier uses ':'.

2. Path component separators also vary:
     UNIX and variants use ':' (as in the PATH environment variable)
     MS-DOS and OS/2 use ';' (also as in the PATH environment variable;
       ':' within a path component follows a drive letter)
     MacOS uses ';' (':' is a directory separator, as noted above)
   See the notes above wfdb_open for details about path separators and how
   WFDB file names are constructed.

3. By default, MS-DOS files are opened in "text" mode.  Since WFDB files are
   binary, they must be opened in binary mode.  To accomplish this, ANSI C
   libraries, and those supplied with non-ANSI C compilers under MS-DOS, define
   argument strings "rb" and "wb" to be supplied to fopen(); unfortunately,
   most other non-ANSI C versions of fopen do not recognize these as legal.
   The "rb" and "wb" forms are used here for ANSI and MS-DOS C compilers only,
   and the older "r" and "w" forms are used in all other cases.

4. Before the ANSI/ISO C standard was adopted, there were at least two
   (commonly used but incompatible) ways of declaring functions with variable
   numbers of arguments (such as printf).  The ANSI/ISO C standard defined
   a third way, incompatible with the other two.  For this reason, the only two
   functions in the WFDB library (wfdb_error and wfdb_fprintf) that need to use
   variable argument lists are included below in three different versions (with
   additional minor variations noted below).

OS- and compiler-dependent definitions:

   For MS-DOS/MS-Windows compilers.  Note that not all such compilers
   predefine the symbol MSDOS;  for those that do not, MSDOS is usually defined
   by a compiler command-line option in the "make" description file. */

/* For other ANSI C compilers.  Such compilers must predefine __STDC__ in order
   to conform to the ANSI specification. */
#define DSEP '/'
#define PSEP ':'
#define AB "ab"
#define RB "rb"
#define WB "wb"

/* Constructs a linked list of path components by splitting its string input
(usually the value of WFDB), and sets the path list variable. Allowed delimiters
are space, tab, and newline characters */
void wfdb_parse_path(std::string_view path_string) {
  for (std::string &path :
       absl::StrSplit(path_string, absl::ByAnyChar(" \t\n\r"))) {
    wfdb_addtopath(path);
  }
}

/* wfdb_addtopath adds the path component of its string argument (i.e.
everything except the file name itself) to the WFDB path, inserting it
there if it is not already in the path.  If the first component of the WFDB
path is '.' (the current directory), the new component is moved to the second
position; otherwise, it is moved to the first position.

wfdb_open calls this function whenever it finds and opens a file.

Since the files comprising a given record are most often kept in the
same directory, this strategy improves the likelihood that subsequent
files to be opened will be found in the first or second location wfdb_open
checks.

If the current directory (.) is at the head of the WFDB path, it remains there,
so that wfdb_open will continue to find the user's own files in preference to
like-named files elsewhere in the path.  If this behavior is not desired, the
current directory should not be specified initially as the first component of
the WFDB path.
 */

// TODO: Replace this when db config objects are set up
void wfdb_addtopath(const std::string &path) {
  if (path.starts_with("http")) {
    wfdb_runtime_config.wfdb_path_list.push_back(
        WfdbPathComponent{.prefix = path, .type = FileType::kNet});
  } else {
    wfdb_runtime_config.wfdb_path_list.push_back(
        WfdbPathComponent{.prefix = path, .type = FileType::kLocal});
  }
}

#include <stdarg.h>
#ifdef va_copy
#define VA_COPY(dst, src) va_copy(dst, src)
#define VA_END2(ap) va_end(ap)
#else
#ifdef __va_copy
#define VA_COPY(dst, src) __va_copy(dst, src)
#define VA_END2(ap) va_end(ap)
#else
#define VA_COPY(dst, src) memcpy(&(dst), &(src), sizeof(va_list))
#define VA_END2(ap) (void)(ap)
#endif
#endif

/* The wfdb_fprintf function handles all formatted output to files.  It is
used in the same way as the standard fprintf function, except that its first
argument is a pointer to a WFDB_FILE rather than a FILE. */

int wfdb_fprintf(WFDB_FILE *wp, const char *format, ...) {
  int ret;
  va_list args;

  va_start(args, format);
#if FileType::kNetFILES
  if (wp->type == FileType::kNet)
    ret = nf_vfprintf(wp->netfp, format, args);
  else
#endif
    ret = vfprintf(wp->fp, format, args);
  va_end(args);
  return (ret);
}

#define spr1(S, RECORD, TYPE)                       \
  ((*TYPE == '\0') ? wfdb_asprintf(S, "%s", RECORD) \
                   : wfdb_asprintf(S, "%s.%s", RECORD, TYPE))
#ifdef FIXISOCD
#define spr2(S, RECORD, TYPE)                         \
  ((*TYPE == '\0') ? wfdb_asprintf(S, "%s;1", RECORD) \
                   : wfdb_asprintf(S, "%s.%.3s;1", RECORD, TYPE))
#else
#define spr2(S, RECORD, TYPE)                        \
  ((*TYPE == '\0') ? wfdb_asprintf(S, "%s.", RECORD) \
                   : wfdb_asprintf(S, "%s.%.3s", RECORD, TYPE))
#endif

static char
    irec[WFDB_MAXRNL + 1]; /* current record name, set by wfdb_setirec */

/* wfdb_open is used by other WFDB library functions to open a database file
for reading or writing.  wfdb_open accepts two string arguments and an integer
argument.  The first string specifies the file type ("hea", "atr", etc.),
and the second specifies the record name.  The integer argument (mode) is
either WFDB_READ or WFDB_WRITE.  Note that a function which calls wfdb_open
does not need to know the filename itself; thus all system-specific details of
file naming conventions can be hidden in wfdb_open.  If the first argument is
"-", or if the first argument is "hea" and the second is "-", wfdb_open
returns a file pointer to the standard input or output as appropriate.  If
either of the string arguments is null or empty, wfdb_open takes the other as
the file name.  Otherwise, it constructs the file name by concatenating the
string arguments with a "." between them.  If the file is to be opened for
reading, wfdb_open searches for it in the list of directories obtained from
getwfdb(); output files are normally created in the current directory.  By
prefixing the record argument with appropriate path specifications, files can
be opened in any directory, provided that the WFDB path includes a null
(empty) component.

Beginning with version 10.0.1, the WFDB library accepts whitespace (space, tab,
or newline characters) as path component separators under any OS.  Multiple
consecutive whitespace characters are treated as a single path component
separator.  Use a '.' to specify the current directory as a path component when
using whitespace as a path component separator.

If the WFDB path includes components of the forms 'http://somewhere.net/mydata'
or 'ftp://somewhere.else/yourdata', the sequence '://' is explicitly recognized
as part of a URL prefix (under any OS), and the ':' and '/' characters within
the '://' are not interpreted further.  Note that the MS-DOS '\' is *not*
acceptable as an alternative to '/' in a URL prefix.  To make WFDB paths
containing URL prefixes more easily (human) readable, use whitespace for path
component separators.

WFDB file names are usually formed by concatenating the record name, a ".", and
the file type, using the spr1 macro (above).  If an input file name, as
constructed by spr1, does not match that of an existing file, wfdb_open uses
spr2 to construct an alternate file name.  In this form, the file type is
truncated to no more than 3 characters (as MS-DOS does).  When searching for
input files, wfdb_open tries both forms with each component of the WFDB path
before going on to the next path component.

If the record name is empty, wfdb_open swaps the record name and the type
string.  If the type string (after swapping, if necessary) is empty, spr1 uses
the record name as the literal file name, and spr2 uses the record name with an
appended "." as the file name.

In environments in which ISO 9660 version numbers are visible in CD-ROM file
names, define the symbol FIXISOCD.  This causes spr2 to append the
characters ";1" (the version number for an ordinary file) to the file names
it generates.  This feature is needed in order to read post-1992 CD-ROMs
with pre-5.0 versions of the Macintosh "ISO 9660 File Access" software,
with some versions of HP-UX, and possibly in other environments as well.

Pre-10.0.1 versions of this library that were compiled for environments other
than MS-DOS used file names in the format TYPE.RECORD.  This file name format
is no longer supported. */

WFDB_FILE *wfdb_open(const char *s, const char *record, int mode) {
  char *wfdb, *p, *q, *r, *buf = NULL;
  int rlen;
  WfdbPathComponent *c0;
  int bufsize, len, ireclen;
  WFDB_FILE *ifile;

  /* If the type (s) is empty, replace it with an empty string so that
     strcmp(s, ...) will not segfault. */
  if (s == NULL) s = "";

  /* If the record name is empty, use s as the record name and replace s
     with an empty string. */
  if (record == NULL || *record == '\0') {
    if (*s) {
      record = s;
      s = "";
    } else
      return (NULL); /* failure -- both components are empty */
  }

  /* Check to see if standard input or output is requested. */
  if (strcmp(record, "-") == 0)
    if (mode == WFDB_READ) {
      static WFDB_FILE wfdb_stdin;

      wfdb_stdin.type = FileType::kLocal;
      wfdb_stdin.fp = stdin;
      return (&wfdb_stdin);
    } else {
      static WFDB_FILE wfdb_stdout;

      wfdb_stdout.type = FileType::kLocal;
      wfdb_stdout.fp = stdout;
      return (&wfdb_stdout);
    }

  /* If the record name ends with '/', expand it by adding another copy of
     the final element (e.g., 'abc/123/' becomes 'abc/123/123'). */
  rlen = strlen(record);
  p = (char *)(record + rlen - 1);
  if (rlen > 1 && *p == '/') {
    for (q = p - 1; q > record; q--)
      if (*q == '/') {
        q++;
        break;
      }
    if (q < p) {
      SUALLOC(r, rlen + p - q + 1, 1); /* p-q is length of final element */
      strcpy(r, record);
      strncpy(r + rlen, q, p - q);
    } else {
      SUALLOC(r, rlen + 1, 1);
      strcpy(r, record);
    }
  } else {
    SUALLOC(r, rlen + 1, 1);
    strcpy(r, record);
  }

  /* If the file is to be opened for output, use the current directory.
     An output file can be opened in another directory if the path to
     that directory is the first part of 'record'. */
  if (mode == WFDB_WRITE) {
    spr1(&wfdb_filename, r, s);
    SFREE(r);
    return (wfdb_fopen(wfdb_filename, WB));
  } else if (mode == WFDB_APPEND) {
    spr1(&wfdb_filename, r, s);
    SFREE(r);
    return (wfdb_fopen(wfdb_filename, AB));
  }

  /* If the filename begins with 'http://' or 'https://', it's a URL.  In
     this case, don't search the WFDB path, but add its parent directory
     to the path if the file can be read. */
  if (strncmp(r, "http://", 7) == 0 || strncmp(r, "https://", 8) == 0) {
    spr1(&wfdb_filename, r, s);
    if ((ifile = wfdb_fopen(wfdb_filename, RB)) != NULL) {
      /* Found it! Add its path info to the WFDB path. */
      wfdb_addtopath(wfdb_filename);
      SFREE(r);
      return (ifile);
    }
  }

  for (c0 = wfdb_path_list; c0; c0 = c0->next) {
    char *long_filename = NULL;

    ireclen = strlen(irec);
    bufsize = 64;
    SALLOC(buf, 1, bufsize);
    len = 0;
    wfdb = c0->prefix;
    while (*wfdb) {
      while (len + ireclen >= bufsize) {
        bufsize *= 2;
        SREALLOC(buf, bufsize, 1);
      }
      if (!buf) break;

      if (*wfdb == '%') {
        /* Perform substitutions in the WFDB path where '%' is found */
        wfdb++;
        if (*wfdb == 'r') {
          /* '%r' -> record name */
          (void)strcpy(buf + len, irec);
          len += ireclen;
          wfdb++;
        } else if ('1' <= *wfdb && *wfdb <= '9' && *(wfdb + 1) == 'r') {
          /* '%Nr' -> first N characters of record name */
          int n = *wfdb - '0';

          if (ireclen < n) n = ireclen;
          (void)strncpy(buf + len, irec, n);
          len += n;
          buf[len] = '\0';
          wfdb += 2;
        } else /* '%X' -> X, if X is neither 'r', nor a non-zero digit
                  followed by 'r' */
          buf[len++] = *wfdb++;
      } else
        buf[len++] = *wfdb++;
    }
    /* Unless the WFDB component was empty, or it ended with a directory
       separator, append a directory separator to wfdb_filename;  then
       append the record and type components.  Note that names of remote
       files (URLs) are always constructed using '/' separators, even if
       the native directory separator is '\' (MS-DOS) or ':' (Macintosh).
    */
    if (len + 2 >= bufsize) {
      bufsize = len + 2;
      SREALLOC(buf, bufsize, 1);
    }
    if (!buf) continue;
    if (len > 0) {
      if (c0->type == FileType::kNet) {
        if (buf[len - 1] != '/') buf[len++] = '/';
      } else if (buf[len - 1] != DSEP)
        buf[len++] = DSEP;
    }
    buf[len] = 0;
    wfdb_asprintf(&buf, "%s%s", buf, r);
    if (!buf) continue;

    spr1(&wfdb_filename, buf, s);
    if ((ifile = wfdb_fopen(wfdb_filename, RB)) != NULL) {
      /* Found it! Add its path info to the WFDB path. */
      wfdb_addtopath(wfdb_filename);
      SFREE(buf);
      SFREE(r);
      return (ifile);
    }
    /* Not found -- try again, using an alternate form of the name,
       provided that that form is distinct. */
    SSTRCPY(long_filename, wfdb_filename);
    spr2(&wfdb_filename, buf, s);
    if (strcmp(wfdb_filename, long_filename) &&
        (ifile = wfdb_fopen(wfdb_filename, RB)) != NULL) {
      wfdb_addtopath(wfdb_filename);
      SFREE(long_filename);
      SFREE(buf);
      SFREE(r);
      return (ifile);
    }
    SFREE(long_filename);
    SFREE(buf);
  }
  /* If the file was not found in any of the directories listed in wfdb,
     return a null file pointer to indicate failure. */
  SFREE(r);
  return (NULL);
}

/* wfdb_checkname checks record and annotator names -- they must not be empty,
   and they must contain only letters, digits, hyphens, tildes, underscores, and
   directory separators. */

int wfdb_checkname(const char *p, const char *s) {
  do {
    if (('0' <= *p && *p <= '9') || *p == '_' || *p == '~' || *p == '-' ||
        *p == DSEP || ('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z'))
      p++;
    else {
      wfdb_error("init: illegal character %d in %s name\n", *p, s);
      return (-1);
    }
  } while (*p);
  return (0);
}

/* wfdb_setirec saves the current record name (its argument) in irec (defined
above) to be substituted for '%r' in the WFDB path by wfdb_open as necessary.
wfdb_setirec is invoked by isigopen (except when isigopen is invoked
recursively to open a segment within a multi-segment record) and by annopen
(when it is about to open a file for input). */

void wfdb_setirec(const char *p) {
  const char *r;
  int len;

  for (r = p; *r; r++)
    if (*r == DSEP) p = r + 1; /* strip off any path information */
  len = strlen(p);
  if (len > WFDB_MAXRNL) len = WFDB_MAXRNL;
  if (strcmp(p, "-")) { /* don't record '-' (stdin) as record name */
    strncpy(irec, p, len);
    irec[len] = '\0';
  }
}

char *wfdb_getirec() { return (*irec ? irec : NULL); }

/* Remove trailing '.hea' from a record name, if present. */
void wfdb_striphea(char *p) {
  if (p) {
    int len = strlen(p);

    if (len > 4 && strcmp(p + len - 4, ".hea") == 0) p[len - 4] = '\0';
  }
}

/* WFDB file I/O functions

The WFDB library normally reads and writes local files.  If libcurl
(http://curl.haxx.se/) is available, the WFDB library can also read files from
any accessible World Wide Web (HTTP) or FTP server.  (Writing files to a remote
WWW or FTP server may be supported in the future.)

If you do not wish to allow access to remote files, or if libcurl is not
available, simply define the symbol FileType::kNetFILES as 0 when compiling the
WFDB library.  If the symbol FileType::kNetFILES is zero, wfdblib.h defines
wfdb_fread as fread, wfdb_fwrite as fwrite, etc.;  thus in this case, the I/O is
performed using the standard C I/O functions, and the function definitions in
the next section are not compiled.  This behavior exactly mimics that of
versions of the WFDB library earlier than version 10.0.1 (which did not support
remote file access), with no additional run-time overhead.

If FileType::kNetFILES is non-zero, however, these functions are compiled.  The
WFDB_FILE pointers that are among the arguments to these functions point to
objects that may contain either (local) FILE handles or (remote) NETFILE
handles, depending on the value of the 'type' member of the WFDB_FILE object.
All access to local files is handled by passing the 'fp' member of the
WFDB_FILE object to the appropriate standard C I/O function.  Access to remote
files via http or ftp is handled by passing the 'netfp' member of the WFDB_FILE
object to the appropriate libcurl function(s).

In order to read remote files, the WFDB environment variable should include
one or more components that specify http:// or ftp:// URL prefixes.  These
components are concatenated with WFDB file names to obtain complete URLs.  For
example, if the value of WFDB is
  /usr/local/database http://dilbert.bigu.edu/wfdb /cdrom/database
then an attempt to read the header file for record xyz would look first for
/usr/local/database/xyz.hea, then http://dilbert.bigu.edu/wfdb/xyz.hea, and
finally /cdrom/database/xyz.hea.  The second and later possibilities would
be checked only if the file had not been found already.  As a practical matter,
it would be best in almost all cases to search all available local file systems
before looking on remote http or ftp servers, but the WFDB library allows you
to set the search order in any way you wish, as in this example.
*/

#if !WFDB_NETFILES
#define nf_feof(nf) (0)
#define nf_fgetc(nf) (EOF)
#define nf_fgets(s, size, nf) (NULL)
#define nf_fread(ptr, size, nmemb, nf) (0)
#define nf_fseek(nf, offset, whence) (-1)
#define nf_ftell(nf) (-1)
#define nf_ferror(nf) (0)
#define nf_clearerr(nf) ((void)0)
#define nf_fflush(nf) (EOF)
#define nf_fwrite(ptr, size, nmemb, nf) (0)
#define nf_putc(c, nf) (EOF)
#endif

/* The definition of nf_vfprintf (which is a stub) has been moved;  it is
   now just before wfdb_fprintf, which refers to it.  There is no completely
   portable way to make a forward reference to a static (local) function. */

void wfdb_clearerr(WFDB_FILE *wp) {
  if (wp->type == FileType::kNet)
    nf_clearerr(wp->netfp);
  else
    clearerr(wp->fp);
}

int wfdb_feof(WFDB_FILE *wp) {
  if (wp->type == FileType::kNet) return (nf_feof(wp->netfp));
  return (feof(wp->fp));
}

int wfdb_ferror(WFDB_FILE *wp) {
  if (wp->type == FileType::kNet) return (nf_ferror(wp->netfp));
  return (ferror(wp->fp));
}

int wfdb_fflush(WFDB_FILE *wp) {
  if (wp == NULL) { /* flush all WFDB_FILEs */
    nf_fflush(NULL);
    return (fflush(NULL));
  } else if (wp->type == FileType::kNet)
    return (nf_fflush(wp->netfp));
  else
    return (fflush(wp->fp));
}

char *wfdb_fgets(char *s, int size, WFDB_FILE *wp) {
  if (wp->type == FileType::kNet) return (nf_fgets(s, size, wp->netfp));
  return (fgets(s, size, wp->fp));
}

size_t wfdb_fread(void *ptr, size_t size, size_t nmemb, WFDB_FILE *wp) {
  if (wp->type == FileType::kNet)
    return (nf_fread(ptr, size, nmemb, wp->netfp));
  return (fread(ptr, size, nmemb, wp->fp));
}

int wfdb_fseek(WFDB_FILE *wp, long int offset, int whence) {
  if (wp->type == FileType::kNet) return (nf_fseek(wp->netfp, offset, whence));
  return (fseek(wp->fp, offset, whence));
}

long wfdb_ftell(WFDB_FILE *wp) {
  if (wp->type == FileType::kNet) return (nf_ftell(wp->netfp));
  return (ftell(wp->fp));
}

size_t wfdb_fwrite(const void *ptr, size_t size, size_t nmemb, WFDB_FILE *wp) {
  if (wp->type == FileType::kNet)
    return (nf_fwrite(ptr, size, nmemb, wp->netfp));
  return (fwrite(ptr, size, nmemb, wp->fp));
}

int wfdb_getc(WFDB_FILE *wp) {
  if (wp->type == FileType::kNet) return (nf_fgetc(wp->netfp));
  return (getc(wp->fp));
}

int wfdb_putc(int c, WFDB_FILE *wp) {
  if (wp->type == FileType::kNet) return (nf_putc(c, wp->netfp));
  return (putc(c, wp->fp));
}

int wfdb_fclose(WFDB_FILE *wp) {
  int status;

  status = (wp->type == FileType::kNet) ? nf_fclose(wp->netfp) : fclose(wp->fp);

  if (wp->fp != stdin) SFREE(wp);
  return (status);
}

WFDB_FILE *wfdb_fopen(char *fname, const char *mode) {
  char *p = fname;
  WFDB_FILE *wp;

  if (p == NULL || strstr(p, "..")) return (NULL);
  SUALLOC(wp, 1, sizeof(WFDB_FILE));
  if (strstr(p, "://")) {
#if WFDB_NETFILES
    if (wp->netfp = nf_fopen(fname, mode)) {
      wp->type = FileType::kNet;
      return (wp);
    }
#endif
    SFREE(wp);
    return (NULL);
  }
  if (wp->fp = fopen(fname, mode)) {
    wp->type = FileType::kLocal;
    return (wp);
  }
  if (strcmp(mode, WB) == 0 || strcmp(mode, AB) == 0) {
    int stat = 1;

    /* An attempt to create an output file failed.  Check to see if all
       of the directories in the path exist, create them if necessary
       and possible, then try again. */
    for (p = fname; *p; p++)
      if (*p == '/') { /* only Unix-style directory separators */
        *p = '\0';
        stat = mkdir(fname, 0755);
        /*
           The '0755' means that (under Unix), the directory will
           be world-readable, but writable only by the owner. */
        *p = '/';
      }
    /* At this point, we may have created one or more directories.
       Only the last attempt to do so matters here:  if and only if
       it was successful (i.e., if stat is now 0), we should try again
       to create the output file. */
    if (stat == 0 && (wp->fp = fopen(fname, mode))) {
      wp->type = FileType::kLocal;
      return (wp);
    }
  }
  SFREE(wp);
  return (NULL);
}

/* Functions that expose configuration constants used by the WFDB Toolkit for
   Matlab. */

#ifndef VERSION
#define VERSION "VERSION not defined"
#endif

#ifndef LDFLAGS
#define LDFLAGS "LDFLAGS not defined"
#endif

#ifndef CFLAGS
#define CFLAGS "CFLAGS not defined"
#endif

const char *wfdbversion() { return VERSION; }

const char *wfdbldflags() { return LDFLAGS; }

const char *wfdbcflags() { return CFLAGS; }

const char *wfdbdefwfdb() { return kDefaultWfdbConfig.wfdb_path; }

const char *wfdbdefwfdbcal() { return kDefaultWfdbConfig.wfdb_cal; }
