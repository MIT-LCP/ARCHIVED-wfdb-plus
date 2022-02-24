#ifndef WFDB_LIB_ANNOT_H_
#define WFDB_LIB_ANNOT_H_

#include "wfdb.hh"

void wfdb_anclose();
void wfdb_oaflush();

int annopen(char *record, const WFDB_Anninfo *aiarray, unsigned int nann);
int getann(WFDB_Annotator a, WFDB_Annotation *annot);
int ungetann(WFDB_Annotator a, const WFDB_Annotation *annot);
int putann(WFDB_Annotator a, const WFDB_Annotation *annot);
int iannsettime(WFDB_Time t);
char *ecgstr(int annotation_code);
int strecg(const char *annotation_mnemonic_string);
int setecgstr(int annotation_code,
                      const char *annotation_mnemonic_string);
extern char *annstr(int annotation_code);
int strann(const char *annotation_mnemonic_string);
int setannstr(int annotation_code,
                      const char *annotation_mnemonic_string);
char *anndesc(int annotation_code);
int setanndesc(int annotation_code, const char *annotation_description);
void setafreq(WFDB_Frequency f);
extern WFDB_Frequency getafreq();
void setiafreq(WFDB_Annotator a, WFDB_Frequency f);
WFDB_Frequency getiafreq(WFDB_Annotator a);
WFDB_Frequency getiaorigfreq(WFDB_Annotator a);
void iannclose(WFDB_Annotator a);
void oannclose(WFDB_Annotator a);
int wfdb_isann(int code);
int wfdb_isqrs(int code);
int wfdb_setisqrs(int code, int newval);
int wfdb_map1(int code);
int wfdb_setmap1(int code, int newval);
int wfdb_map2(int code);
int wfdb_setmap2(int code, int newval);
int wfdb_ammap(int code);
int wfdb_mamap(int code, int subtype);
int wfdb_annpos(int code);
int wfdb_setannpos(int code, int newval);

#endif  // WFDB_LIB_ANNOT_H_
