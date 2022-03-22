/* file: wfdbinit.c	G. Moody	 23 May 1983

WFDB library functions wfdbinit, wfdbquit, and wfdbflush

This file contains definitions of the following WFDB library functions:
 wfdbinit	(opens annotation files and input signals)
 wfdbquit	(closes all annotation and signal files)
 wfdbflush	(writes all buffered output annotation and signal files)
*/
#include "wfdbinit.h"

#include "annot.hh"
#include "signal.hh"
#include "wfdbio.hh"

int wfdbinit(char *record, const WFDB_Anninfo *aiarray, unsigned int nann,
             WFDB_Siginfo *siarray, unsigned int nsig) {
  int stat;

  if ((stat = annopen(record, aiarray, nann)) == 0)
    stat = isigopen(record, siarray, (int)nsig);
  return (stat);
}

void wfdbquit() {
  wfdb_anclose();    /* close annotation files, reset variables */
  wfdb_oinfoclose(); /* close info file */
  wfdb_sigclose();   /* close signals, reset variables */
  resetwfdb();       /* restore the WFDB path */
  wfdb_sampquit();   /* release sample data buffer */
  wfdb_freeinfo();   /* release info strings */
}

void wfdbflush() /* write all buffered output to files */
{
  wfdb_oaflush(); /* flush buffered output annotations */
  wfdb_osflush(); /* flush buffered output samples */
}
