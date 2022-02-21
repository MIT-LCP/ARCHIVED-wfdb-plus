#include "wfdb.hh"

int wfdbinit(char *record, const WFDB_Anninfo *aiarray, unsigned int nann,
             WFDB_Siginfo *siarray, unsigned int nsig);
void wfdbquit();
void wfdbflush();
