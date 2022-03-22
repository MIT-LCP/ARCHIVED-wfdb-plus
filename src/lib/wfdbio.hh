/* file: wfdbio.hh	G. Moody	13 April 1989

External definitions for WFDB library private functions

These definitions may be included in WFDB library source files to permit them
to share information about library private functions, which are not intended
to be invoked directly by user programs.  By convention, all externally defined
symbols reserved to the library begin with the characters "wfdb_".
*/
#ifndef WFDB_LIB_IO_H_
#define WFDB_LIB_IO_H_

#include <sys/stat.h>
#ifndef FILE
#include <stdio.h>
#endif

#include <string>
#include <vector>

#include "netfiles.hh"
#include "wfdb.hh"

#ifndef SEEK_END
#define SEEK_END 2
#endif

/* getvec operating modes */
/* When reading multifrequency records, getvec() can operate in two modes:
   WFDB_LOWRES (returning one sample per signal per frame), or WFDB_HIGHRES
   (returning each sample of any oversampled signals, and duplicating samples
   of other signals as necessary).  If the operating mode is not selected by
   invoking setgvmode(), the value of the environment variable WFDBGVMODE
   determines the mode (0: WFDB_LOWRES, 1: WFDB_HIGHRES);  if WFDBGVMODE
   is not set, the value of DEFWFDBMODE determines the mode. */
enum class GetVecMode {
  kLowRes = 0,  /* return one sample per signal per frame */
  kHighRes = 1, /* return each sample of oversampled signals, duplicating
                   samples of other signals */
  kGvPad = 2    /* replace invalid samples with previous valid samples */
};

struct WfdbConfig {
  char *wfdb_path;
  /* The name of the WFDB calibration file, used if the
  WFDBCAL environment variable is not set.  This name need not include path
  information if the calibration file is located in a directory included in
  the WFDB path. May be NULL if you prefer not to have a default calibration
  file.  See calopen() in calib.c for further information. */
  char *wfdb_cal;
  /* WFDB applications may write annotations out-of-order, but in
   almost all cases, they expect that annotations they read must be in order.
   The environment variable WFDBANNSORT specifies if wfdbquit() should attempt
   to sort annotations in any output annotation files before closing them (it
   does this if WFDBANNSORT is non-zero, or if WFDBANNSORT is not set, and
   DEFWFDBANNSORT is non-zero).  Sorting is done by invoking 'sortann' (see
   ../app/sortann.c) as a separate process */
  bool ann_sort;
  GetVecMode getvec_mode;
};

inline constexpr WfdbConfig kDefaultWfdbConfig{
    /* This value is edited by the configuration script
       (../configure), which also edits this block of comments to match.*/
    // TODO: Account for non-netfiles
    .wfdb_path = ". DBDIR http://physionet.org/physiobank/database",
    .wfdb_cal = "wfdbcal",
    .ann_sort = true,
    .getvec_mode = GetVecMode::kLowRes};

enum class FileType {
  kLocal, /* a local file, read via C standard I/O */
  kNet    /* a remote file, read via libwww */
};

struct WFDB_FILE {
  FILE *fp;
  struct Netfile *netfp;
  FileType type;
};

// An element of the WFDB Path, specifying where to search for database files
struct WfdbPathComponent {
  std::string prefix;
  FileType type;
};

// Returns the database path string. Initializes it if not already set.
const std::string &getwfdb();
// Sets the database path string
void setwfdb(const char *database_path_string);
// Restores the database path to its initial value
void resetwfdb();

// Returns the complete pathname of a WFDB file
char *wfdbfile(const char *file_type, char *record);
// Set behavior on memory errors
void wfdbmemerr(int behavior);
// Indicates if memory errors are fatal
int wfdb_me_fatal();

// Reads a 16-bit integer
int wfdb_g16(WFDB_FILE *fp);
// Reads a 32-bit integer
long wfdb_g32(WFDB_FILE *fp);
// Writes a 16-bit integer
void wfdb_p16(unsigned int x, WFDB_FILE *fp);
// Writes a 32-bit integer
void wfdb_p32(long x, WFDB_FILE *fp);

// Finds and opens database files
WFDB_FILE *wfdb_open(const char *file_type, const char *record, int mode);
// Checks record and annotator names for validity
int wfdb_checkname(const char *name, const char *description);
// Removes trailing '.hea' from a record name, if present
void wfdb_striphea(char *record);

// Parses the path string and sets the path components
void wfdb_parse_path(std::string_view path_string);

// Sets WFDB from the contents of a file
static const char *wfdb_getiwfdb(char **p);

// Adds path component of string argument to WFDB path
void wfdb_addtopath(const char *pathname);
__attribute__((__format__(__printf__, 2, 3)))

// Like fprintf, but first arg is a WFDB_FILE pointer
int wfdb_fprintf(WFDB_FILE *fp, const char *format, ...);
// Saves current record name
void wfdb_setirec(const char *record_name);
// Gets current record name
char *wfdb_getirec();

// These functions expose config strings needed by the WFDB Toolkit for Matlab:
// Return the string defined by VERSION
const char *wfdbversion();
// Return the string defined by LDFLAGS
const char *wfdbldflags();
// Return the string defined by CFLAGS
const char *wfdbcflags();
// Return the string defined by DEFWFDB
const char *wfdbdefwfdb();
// return the string defined by DEFWFDBCAL
const char *wfdbdefwfdbcal();

/* These functions are compiled only if WFDB_NETFILES is non-zero; they
permit access to remote files via http or ftp (using libcurl) as well as to
local files (using the standard C I/O functions).  The functions in this group
are intended primarily for use by other WFDB library functions, but may also be
called directly by WFDB applications that need to read remote files. Unlike
other private functions in the WFDB library, the interfaces to these are not
likely to change, since they are designed to emulate the similarly-named
ANSI/ISO C standard I/O functions:
*/
// TODO: Remove these functions after WFDB_FILE uses classes and polymorphism

// Emulates clearerr
void wfdb_clearerr(WFDB_FILE *fp);
// Emulates feof
int wfdb_feof(WFDB_FILE *fp);
// Emulates ferror
int wfdb_ferror(WFDB_FILE *fp);
// Emulates fflush, for local files only
int wfdb_fflush(WFDB_FILE *fp);
// Emulates fgets
char *wfdb_fgets(char *s, int size, WFDB_FILE *fp);
// Emulates fread
size_t wfdb_fread(void *ptr, size_t size, size_t nmemb, WFDB_FILE *fp);
// Emulates fseek
int wfdb_fseek(WFDB_FILE *fp, long offset, int whence);
// Emulates ftell
long wfdb_ftell(WFDB_FILE *fp);
// Emulates fwrite, for local files only
size_t wfdb_fwrite(const void *ptr, size_t size, size_t nmemb, WFDB_FILE *fp);
// Emulates getc
int wfdb_getc(WFDB_FILE *fp);
// Emulates putc, for local files only
int wfdb_putc(int c, WFDB_FILE *fp);
// Emulates fclose
int wfdb_fclose(WFDB_FILE *fp);
// Emulates fopen, but returns a WFDB_FILE pointer
WFDB_FILE *wfdb_fopen(char *fname, const char *mode);

#endif  // WFDB_LIB_IO_H_
