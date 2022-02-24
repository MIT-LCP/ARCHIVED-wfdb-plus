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

#include "netfiles.hh"
#include "wfdb.hh"

/* DEFWFDB is the default value of the WFDB path if the WFDB environment
   variable is not set.  This value is edited by the configuration script
   (../configure), which also edits this block of comments to match.

   If WFDB_NETFILES support is disabled, the string ". DBDIR" is
   usually sufficient for a default WFDB path, thus restricting the search for
   WFDB files to the current directory ("."), followed by DBDIR).

   If WFDB_NETFILES support is enabled, the first setting below adds the
   web-accessible PhysioBank databases to the default path; you may wish to
   change this to use a nearby PhysioNet mirror (for a list of mirrors, see
   http://physionet.org/mirrors/).  DEFWFDB must not be NULL, however.
*/

#ifndef WFDB_NETFILES
#define DEFWFDB ". DBDIR"
#else
#define DEFWFDB ". DBDIR http://physionet.org/physiobank/database"
#endif

/* DEFWFDBCAL is the name of the default WFDB calibration file, used if the
   WFDBCAL environment variable is not set.  This name need not include path
   information if the calibration file is located in a directory included in
   the WFDB path.  The value given below is the name of the standard
   calibration file supplied on the various CD-ROM databases.  DEFWFDBCAL may
   be NULL if you prefer not to have a default calibration file.  See calopen()
   in calib.c for further information. */
#define DEFWFDBCAL "wfdbcal"

/* WFDB applications may write annotations out-of-order, but in almost all
   cases, they expect that annotations they read must be in order.  The
   environment variable WFDBANNSORT specifies if wfdbquit() should attempt to
   sort annotations in any output annotation files before closing them (it
   does this if WFDBANNSORT is non-zero, or if WFDBANNSORT is not set, and
   DEFWFDBANNSORT is non-zero).  Sorting is done by invoking 'sortann' (see
   ../app/sortann.c) as a separate process;  since this cannot be done from
   an MS-Windows DLL, sorting is disabled by default in this case. */

// TODO(cx1111): Remove the need for this variable
#define DEFWFDBANNSORT 1

/* When reading multifrequency records, getvec() can operate in two modes:
   WFDB_LOWRES (returning one sample per signal per frame), or WFDB_HIGHRES
   (returning each sample of any oversampled signals, and duplicating samples
   of other signals as necessary).  If the operating mode is not selected by
   invoking setgvmode(), the value of the environment variable WFDBGVMODE
   determines the mode (0: WFDB_LOWRES, 1: WFDB_HIGHRES);  if WFDBGVMODE
   is not set, the value of DEFWFDBMODE determines the mode. */
#define DEFWFDBGVMODE WFDB_LOWRES

/* Structures used by internal WFDB library functions only */
struct WFDB_FILE {
  FILE *fp;
  netfile *netfp;
  int type;  // TODO: Replace with enum
};

/* Values for WFDB_FILE 'type' field */
#define WFDB_LOCAL 0 /* a local file, read via C standard I/O */
#define WFDB_NET 1   /* a remote file, read via libwww */


// Returns the database path string
char* getwfdb();
// sets the database path string
void setwfdb(const char *database_path_string);
// restores the database path to its initial value
void resetwfdb();
// Suppresses WFDB library error messages
void wfdbquiet();
// Enables WFDB library error messages
void wfdbverbose();

// Returns the complete pathname of a WFDB file
char* wfdbfile(const char *file_type, char *record);
// Set behavior on memory errors
void wfdbmemerr(int behavior);
// Indicates if memory errors are fatal
int wfdb_me_fatal();

// These functions expose config strings needed by the WFDB Toolkit for Matlab:
// Return the string defined by VERSION
const char* wfdbversion();
// Return the string defined by LDFLAGS
const char* wfdbldflags();
// Return the string defined by CFLAGS
const char* wfdbcflags();
// Return the string defined by DEFWFDB
const char* wfdbdefwfdb();
// return the string defined by DEFWFDBCAL
const char* wfdbdefwfdbcal();

// Reads a 16-bit integer
int wfdb_g16(WFDB_FILE *fp);
// Reads a 32-bit integer
long wfdb_g32(WFDB_FILE *fp);
// Writes a 16-bit integer
void wfdb_p16(unsigned int x, WFDB_FILE *fp);
// Writes a 32-bit integer
void wfdb_p32(long x, WFDB_FILE *fp);

// Puts the WFDB path, etc. into the environment
void wfdb_export_config();


// Finds and opens database files
WFDB_FILE *wfdb_open(const char *file_type, const char *record,
                            int mode);
// Checks record and annotator names for validity
int wfdb_checkname(const char *name, const char *description);
// Removes trailing '.hea' from a record name, if present
void wfdb_striphea(char *record);

// Frees data structures assigned to the path list
void wfdb_free_path_list();
// Splits WFDB path into components
int wfdb_parse_path(const char *wfdb_path);

// Sets WFDB from the contents of a file
static const char *wfdb_getiwfdb(char **p);

// Adds path component of string argument to WFDB path
void wfdb_addtopath(const char *pathname);
__attribute__((__format__(__printf__, 2, 3)))

// Allocates and formats a message
int wfdb_asprintf(char **buffer, const char *format, ...);
// Allocates and formats a message
int wfdb_vasprintf(char **buffer, const char *format, ...);

// Like fprintf, but first arg is a WFDB_FILE pointer
int wfdb_fprintf(WFDB_FILE *fp, const char *format, ...);
// Saves current record name
void wfdb_setirec(const char *record_name);
// Gets current record name
char *wfdb_getirec();

// Produces an error message
void wfdb_error(const char *format, ...);
// Produces an error message
char* wfdberror();

/* These functions are compiled only if WFDB_NETFILES is non-zero; they permit
access to remote files via http or ftp (using libcurl) as
well as to local files (using the standard C I/O functions).  The functions in
this group are intended primarily for use by other WFDB library functions, but
may also be called directly by WFDB applications that need to read remote
files. Unlike other private functions in the WFDB library, the interfaces to
these are not likely to change, since they are designed to emulate the
similarly-named ANSI/ISO C standard I/O functions:
*/

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
size_t wfdb_fwrite(const void *ptr, size_t size, size_t nmemb,
                          WFDB_FILE *fp);
// Emulates getc
int wfdb_getc(WFDB_FILE *fp);
// Emulates putc, for local files only
int wfdb_putc(int c, WFDB_FILE *fp);
// Emulates fclose
int wfdb_fclose(WFDB_FILE *fp);
// Emulates fopen, but returns a WFDB_FILE pointer
WFDB_FILE *wfdb_fopen(char *fname, const char *mode);

char *wfdberror();
__attribute__((__format__(__printf__, 1, 2)))
void wfdb_error(const char *format_string, ...);

#endif  // WFDB_LIB_IO_H_
