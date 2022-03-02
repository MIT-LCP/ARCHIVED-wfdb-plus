/* file: wfdbio.cc	G. Moody	18 November 1988

Low-level I/O functions for the WFDB library

*/

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "wfdb.hh"
#include "wfdbio.hh"

/* WFDB library functions */

/* getwfdb is used to obtain the WFDB path, a list of places in which to search
for database files to be opened for reading.  In most environments, this list
is obtained from the shell (environment) variable WFDB, which may be set by the
user (typically as part of the login script).  A default value may be set at
compile time (DEFWFDB in wfdblib.h); this is necessary for environments that do
not support the concept of environment variables, such as MacOS9 and earlier.

If WFDB or DEFWFDB is of the form '@FILE', getwfdb reads the WFDB path from the
specified (local) FILE (using wfdb_getiwfdb); such files may be nested up to
10 levels. */

static char *wfdbpath = NULL, *wfdbpath_init = NULL;

/* resetwfdb is called by wfdbquit, and can be called within an application,
to restore the WFDB path to the value that was returned by the first call
to getwfdb (or NULL if getwfdb was not called). */

void resetwfdb() { SSTRCPY(wfdbpath, wfdbpath_init); }

char* getwfdb() {
  if (wfdbpath == NULL) {
    const char *p = getenv("WFDB");

    if (p == NULL) p = DEFWFDB;
    SSTRCPY(wfdbpath, p);
    p = wfdb_getiwfdb(&wfdbpath);
    SSTRCPY(wfdbpath_init, wfdbpath);
    wfdb_parse_path(p);
  }
  return (wfdbpath);
}

/* setwfdb can be called within an application to change the WFDB path. */

void setwfdb(const char *p) {
  wfdb_export_config();

  if (p == NULL && (p = getenv("WFDB")) == NULL) p = DEFWFDB;
  SSTRCPY(wfdbpath, p);
  wfdb_export_config();

  SSTRCPY(wfdbpath, p);
  p = wfdb_getiwfdb(&wfdbpath);
  wfdb_parse_path(p);
}



static char *wfdb_filename;

/* wfdbfile returns the pathname or URL of a WFDB file. */

char* wfdbfile(const char *s, char *record) {
  WFDB_FILE *ifile;

  if (s == NULL && record == NULL) return (wfdb_filename);

  /* Remove trailing .hea, if any, from record name. */
  wfdb_striphea(record);

  if ((ifile = wfdb_open(s, record, WFDB_READ))) {
    (void)wfdb_fclose(ifile);
    return (wfdb_filename);
  } else
    return (NULL);
}

/* Determine how the WFDB library handles memory allocation errors (running
out of memory).  Call wfdbmemerr(0) in order to have these errors returned
to the caller;  by default, such errors cause the running process to exit. */

static int wfdb_mem_behavior = 1;

void wfdbmemerr(int behavior) { wfdb_mem_behavior = behavior; }

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

const char* wfdbversion() { return VERSION; }

const char* wfdbldflags(void) { return LDFLAGS; }

const char* wfdbcflags(void) { return CFLAGS; }

const char* wfdbdefwfdb(void) { return DEFWFDB; }

const char* wfdbdefwfdbcal(void) { return DEFWFDBCAL; }

/* Private functions (for the use of other WFDB library functions only). */

int wfdb_me_fatal() /* used by the MEMERR macro defined in wfdblib.h */
{
  return (wfdb_mem_behavior);
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

struct WfdbPathComponent {
  char *prefix;
  WfdbPathComponent *next, *prev;
  int type; /* WFDB_LOCAL or WFDB_NET */
};

static WfdbPathComponent *wfdb_path_list;

/* wfdb_free_path_list clears out the path list, freeing all memory allocated
   to it. */
void wfdb_free_path_list() {
  WfdbPathComponent *c0 = NULL, *c1 = wfdb_path_list;

  while (c1) {
    c0 = c1->next;
    SFREE(c1->prefix);
    SFREE(c1);
    c1 = c0;
  }
  wfdb_path_list = NULL;
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

/* wfdb_parse_path constructs a linked list of path components by splitting
its string input (usually the value of WFDB). */

int wfdb_parse_path(const char *p) {
  const char *q;
  int current_type, slashes, found_end;
  WfdbPathComponent *c0 = NULL, *c1 = wfdb_path_list;
  static int first_call = 1;

  /* First, free the existing wfdb_path_list, if any. */
  wfdb_free_path_list();

  /* Do nothing else if no path string was supplied. */
  if (p == NULL) return (0);

  /* Register the cleanup function so that it is invoked on exit. */
  if (first_call) {
    atexit(wfdb_free_path_list);
    first_call = 0;
  }
  q = p;

  /* Now construct the wfdb_path_list from the contents of p. */
  while (*q) {
    /* Find the beginning of the next component (skip whitespace). */
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    p = q--;
    current_type = WFDB_LOCAL;
    /* Find the end of the current component. */
    found_end = 0;
    slashes = 0;
    do {
      switch (*++q) {
        case ':': /* might be a component delimiter, part of '://',
                     a drive suffix (MS-DOS), or a directory separator
                     (Mac) */
          if (*(q + 1) == '/' && *(q + 2) == '/') current_type = WFDB_NET;
          /* Allow colons within the authority portion of the URL.
             For example, "http://[::1]:8080/database:/usr/database"
             is a database path with two components. */
          else if (current_type != WFDB_NET || slashes > 2)
            found_end = 1;
          break;
        case ';': /* definitely a component delimiter */
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\0':
          found_end = 1;
          break;
        case '/':
          slashes++;
          break;
      }
    } while (!found_end);

    /* current component begins at p, ends at q-1 */
    SUALLOC(c1, 1, sizeof(WfdbPathComponent));
    SALLOC(c1->prefix, q - p + 1, sizeof(char));
    memcpy(c1->prefix, p, q - p);
    c1->type = current_type;
    c1->prev = c0;
    if (c0)
      c0->next = c1;
    else
      wfdb_path_list = c1;
    c0 = c1;
    if (*q) q++;
  }
  return (0);
}

/* wfdb_getiwfdb reads a new value for WFDB from the file named by the second
through last characters of its input argument.  If that value begins with '@',
this procedure is repeated, with nesting up to ten levels.

Note that the input file must be local (it is accessed using the standard C I/O
functions rather than their wfdb_* counterparts). This limitation is
intentional, since the alternative (to allow remote files to determine the
contents of the WFDB path) seems an unnecessary security risk. */

#ifndef SEEK_END
#define SEEK_END 2
#endif

static const char *wfdb_getiwfdb(char **p) {
  FILE *wfdbpfile;
  int i = 0;
  long len;

  for (i = 0; i < 10 && *p != NULL && **p == '@'; i++) {
    if ((wfdbpfile = fopen((*p) + 1, RB)) == NULL)
      **p = 0;
    else {
      if (fseek(wfdbpfile, 0L, SEEK_END) == 0)
        len = ftell(wfdbpfile);
      else
        len = 255;
      SALLOC(*p, 1, len + 1);
      if (*p == NULL) {
        fclose(wfdbpfile);
        break;
      }
      rewind(wfdbpfile);
      len = fread(*p, 1, (int)len, wfdbpfile);
      while ((*p)[len - 1] == '\n' || (*p)[len - 1] == '\r') (*p)[--len] = '\0';
      (void)fclose(wfdbpfile);
    }
  }
  if (*p != NULL && **p == '@') {
    wfdb_error("getwfdb: files nested too deeply\n");
    **p = 0;
  }
  return (*p);
}

static char *p_wfdb, *p_wfdbcal, *p_wfdbannsort, *p_wfdbgvmode;

/* wfdb_free_config frees all memory allocated by wfdb_export_config.
   This function must be invoked before exiting to avoid a memory leak.
   It must not be invoked at any other time, since pointers passed to
   putenv must be maintained by the caller, according to POSIX.1-2001
   semantics for putenv.  */
void wfdb_free_config() {
  static char n_wfdb[] = "WFDB=";
  static char n_wfdbcal[] = "WFDBCAL=";
  static char n_wfdbannsort[] = "WFDBANNSORT=";
  static char n_wfdbgvmode[] = "WFDBGVMODE=";
  if (p_wfdb) putenv(n_wfdb);
  if (p_wfdbcal) putenv(n_wfdbcal);
  if (p_wfdbannsort) putenv(n_wfdbannsort);
  if (p_wfdbgvmode) putenv(n_wfdbgvmode);
  SFREE(p_wfdb);
  SFREE(p_wfdbcal);
  SFREE(p_wfdbannsort);
  SFREE(p_wfdbgvmode);
  SFREE(wfdbpath);
  SFREE(wfdbpath_init);
  SFREE(wfdb_filename);
}

/* wfdb_export_config is invoked from setwfdb to place the configuration
   variables into the environment if possible. */
void wfdb_export_config() {
  static int first_call = 1;
  char *envstr = NULL;

  /* Register the cleanup function so that it is invoked on exit. */
  if (first_call) {
    atexit(wfdb_free_config);
    first_call = 0;
  }
  SALLOC(envstr, 1, strlen(wfdbpath) + 6);
  if (envstr) {
    sprintf(envstr, "WFDB=%s", wfdbpath);
    putenv(envstr);
    SFREE(p_wfdb);
    p_wfdb = envstr;
  }
  if (getenv("WFDBCAL") == NULL) {
    SALLOC(p_wfdbcal, 1, strlen(DEFWFDBCAL) + 9);
    if (p_wfdbcal) {
      sprintf(p_wfdbcal, "WFDBCAL=%s", DEFWFDBCAL);
      putenv(p_wfdbcal);
    }
  }
  if (getenv("WFDBANNSORT") == NULL) {
    SALLOC(p_wfdbannsort, 1, 14);
    if (p_wfdbannsort) {
      sprintf(p_wfdbannsort, "WFDBANNSORT=%d", DEFWFDBANNSORT == 0 ? 0 : 1);
      putenv(p_wfdbannsort);
    }
  }
  if (getenv("WFDBGVMODE") == NULL) {
    SALLOC(p_wfdbgvmode, 1, 13);
    if (p_wfdbgvmode) {
      sprintf(p_wfdbgvmode, "WFDBGVMODE=%d", DEFWFDBGVMODE == 0 ? 0 : 1);
      putenv(p_wfdbgvmode);
    }
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

void wfdb_addtopath(const char *s) {
  const char *p;
  int i, len;
  WfdbPathComponent *c0, *c1;

  if (s == NULL || *s == '\0') return;

  /* Start at the end of the string and search backwards for a directory
     separator (accept any of the possible separators). */
  for (p = s + strlen(s) - 1; p >= s && *p != '/' && *p != '\\' && *p != ':';
       p--)
    ;

  /* A path component specifying the root directory must be treated as a
     special case;  normally the trailing directory separator is not
     included in the path component, but in this case there is nothing
     else to include. */

  if (p == s && (*p == '/' || *p == '\\' || *p == ':')) p++;

  if (p < s) return; /* argument did not contain a path component */

  /* If p > s, then p points to the first character following the path
     component of s. Search the current WFDB path for this path component. */
  if (wfdbpath == NULL) (void)getwfdb();
  for (c0 = c1 = wfdb_path_list, i = p - s; c1; c1 = c1->next) {
    if (strncmp(c1->prefix, s, i) == 0) {
      if (c0 == c1 || (c1->prev == c0 && strcmp(c0->prefix, ".") == 0))
        return; /* no changes needed, quit */
      /* path component of s is already in WFDB path -- unlink its node */
      if (c1->next) (c1->next)->prev = c1->prev;
      if (c1->prev) (c1->prev)->next = c1->next;
      break;
    }
  }
  if (!c1) {
    /* path component of s not in WFDB path -- make a new node for it */
    SUALLOC(c1, 1, sizeof(WfdbPathComponent));
    SALLOC(c1->prefix, p - s + 1, sizeof(char));
    memcpy(c1->prefix, s, p - s);
    if (strstr(c1->prefix, "://"))
      c1->type = WFDB_NET;
    else
      c1->type = WFDB_LOCAL;
  }
  /* (Re)link the unlinked node. */
  if (strcmp(c0->prefix, ".") == 0) { /* skip initial "." if present */
    c1->prev = c0;
    if ((c1->next = c0->next) != NULL) (c1->next)->prev = c1;
    c0->next = c1;
  } else { /* no initial ".";  insert the node at the head of the path */
    wfdb_path_list = c1;
    c1->prev = NULL;
    c1->next = c0;
    c0->prev = c1;
  }
  return;
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
#if WFDB_NETFILES
  if (wp->type == WFDB_NET)
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

      wfdb_stdin.type = WFDB_LOCAL;
      wfdb_stdin.fp = stdin;
      return (&wfdb_stdin);
    } else {
      static WFDB_FILE wfdb_stdout;

      wfdb_stdout.type = WFDB_LOCAL;
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

  /* Parse the WFDB path if not done previously. */
  if (wfdb_path_list == NULL) (void)getwfdb();

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
      if (c0->type == WFDB_NET) {
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
available, simply define the symbol WFDB_NETFILES as 0 when compiling the WFDB
library.  If the symbol WFDB_NETFILES is zero, wfdblib.h defines wfdb_fread as
fread, wfdb_fwrite as fwrite, etc.;  thus in this case, the I/O is performed
using the standard C I/O functions, and the function definitions in the next
section are not compiled.  This behavior exactly mimics that of versions of the
WFDB library earlier than version 10.0.1 (which did not support remote file
access), with no additional run-time overhead.

If WFDB_NETFILES is non-zero, however, these functions are compiled.  The
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

#if WFDB_NETFILES
#else /* !WFDB_NETFILES */
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
  if (wp->type == WFDB_NET)
    nf_clearerr(wp->netfp);
  else
    clearerr(wp->fp);
}

int wfdb_feof(WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_feof(wp->netfp));
  return (feof(wp->fp));
}

int wfdb_ferror(WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_ferror(wp->netfp));
  return (ferror(wp->fp));
}

int wfdb_fflush(WFDB_FILE *wp) {
  if (wp == NULL) { /* flush all WFDB_FILEs */
    nf_fflush(NULL);
    return (fflush(NULL));
  } else if (wp->type == WFDB_NET)
    return (nf_fflush(wp->netfp));
  else
    return (fflush(wp->fp));
}

char *wfdb_fgets(char *s, int size, WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_fgets(s, size, wp->netfp));
  return (fgets(s, size, wp->fp));
}

size_t wfdb_fread(void *ptr, size_t size, size_t nmemb, WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_fread(ptr, size, nmemb, wp->netfp));
  return (fread(ptr, size, nmemb, wp->fp));
}

int wfdb_fseek(WFDB_FILE *wp, long int offset, int whence) {
  if (wp->type == WFDB_NET) return (nf_fseek(wp->netfp, offset, whence));
  return (fseek(wp->fp, offset, whence));
}

long wfdb_ftell(WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_ftell(wp->netfp));
  return (ftell(wp->fp));
}

size_t wfdb_fwrite(const void *ptr, size_t size, size_t nmemb, WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_fwrite(ptr, size, nmemb, wp->netfp));
  return (fwrite(ptr, size, nmemb, wp->fp));
}

int wfdb_getc(WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_fgetc(wp->netfp));
  return (getc(wp->fp));
}

int wfdb_putc(int c, WFDB_FILE *wp) {
  if (wp->type == WFDB_NET) return (nf_putc(c, wp->netfp));
  return (putc(c, wp->fp));
}

int wfdb_fclose(WFDB_FILE *wp) {
  int status;

  status = (wp->type == WFDB_NET) ? nf_fclose(wp->netfp) : fclose(wp->fp);

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
      wp->type = WFDB_NET;
      return (wp);
    }
#endif
    SFREE(wp);
    return (NULL);
  }
  if (wp->fp = fopen(fname, mode)) {
    wp->type = WFDB_LOCAL;
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
      wp->type = WFDB_LOCAL;
      return (wp);
    }
  }
  SFREE(wp);
  return (NULL);
}
