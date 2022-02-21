/* file: wfdblib.h	G. Moody	13 April 1989
                        Last revised:    20 May 2020          wfdblib 10.7.0
External definitions for WFDB library private functions

_______________________________________________________________________________
wfdb: a library for reading and writing annotated waveforms (time series data)
Copyright (C) 1989-2012 George B. Moody

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option) any
later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU Library General Public License for more
details.

You should have received a copy of the GNU Library General Public License along
with this library; if not, see <http://www.gnu.org/licenses/>.

You may contact the author by e-mail (wfdb@physionet.org) or postal mail
(MIT Room E25-505A, Cambridge, MA 02139 USA).  For updates to this software,
please visit PhysioNet (http://www.physionet.org/).
_______________________________________________________________________________

These definitions may be included in WFDB library source files to permit them
to share information about library private functions, which are not intended
to be invoked directly by user programs.  By convention, all externally defined
symbols reserved to the library begin with the characters "wfdb_".
*/

#include <sys/stat.h>
#ifndef FILE
#include <stdio.h>
#endif

#include "netfiles.hh"
#include "wfdb.h"

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

// TODO(cx1111): Remove the need for this variable
#define HAS_PUTENV

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Structures used by internal WFDB library functions only */
struct WFDB_FILE {
  FILE *fp;
  netfile *netfp;
  int type;  // TODO: Replace with enum
};

/* Values for WFDB_FILE 'type' field */
#define WFDB_LOCAL 0 /* a local file, read via C standard I/O */
#define WFDB_NET 1   /* a remote file, read via libwww */

/* These functions are defined in wfdbio.c */
extern int wfdb_fclose(WFDB_FILE *fp);
extern WFDB_FILE *wfdb_open(const char *file_type, const char *record,
                            int mode);
extern int wfdb_checkname(const char *name, const char *description);
extern void wfdb_striphea(char *record);
extern int wfdb_g16(WFDB_FILE *fp);
extern long wfdb_g32(WFDB_FILE *fp);
extern void wfdb_p16(unsigned int x, WFDB_FILE *fp);
extern void wfdb_p32(long x, WFDB_FILE *fp);
extern int wfdb_parse_path(const char *wfdb_path);
extern void wfdb_addtopath(const char *pathname);
__attribute__((__format__(__printf__, 2, 3))) extern int wfdb_asprintf(
    char **buffer, const char *format, ...);
extern WFDB_FILE *wfdb_fopen(char *fname, const char *mode);
extern int wfdb_fprintf(WFDB_FILE *fp, const char *format, ...);
extern void wfdb_setirec(const char *record_name);
extern char *wfdb_getirec(void);
extern void wfdb_clearerr(WFDB_FILE *fp);
extern int wfdb_feof(WFDB_FILE *fp);
extern int wfdb_ferror(WFDB_FILE *fp);
extern int wfdb_fflush(WFDB_FILE *fp);
extern char *wfdb_fgets(char *s, int size, WFDB_FILE *fp);
extern size_t wfdb_fread(void *ptr, size_t size, size_t nmemb, WFDB_FILE *fp);
extern int wfdb_fseek(WFDB_FILE *fp, long offset, int whence);
extern long wfdb_ftell(WFDB_FILE *fp);
extern size_t wfdb_fwrite(const void *ptr, size_t size, size_t nmemb,
                          WFDB_FILE *fp);
extern int wfdb_getc(WFDB_FILE *fp);
extern int wfdb_putc(int c, WFDB_FILE *fp);
