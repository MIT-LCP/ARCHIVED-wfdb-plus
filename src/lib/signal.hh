#ifndef WFDB_LIB_SIGNAL_H_
#define WFDB_LIB_SIGNAL_H_

#include "wfdb.hh"

void wfdb_sampquit();
void wfdb_sigclose();
void wfdb_osflush();
void wfdb_freeinfo();
int wfdb_oinfoclose();

int isigopen(char *record, WFDB_Siginfo *siarray, int nsig);
int osigopen(char *record, WFDB_Siginfo *siarray, unsigned int nsig);
int osigfopen(const WFDB_Siginfo *siarray, unsigned int nsig);
int findsig(const char *signame);
int getspf();
void setgvmode(int mode);
int getgvmode();
int setifreq(WFDB_Frequency freq);
WFDB_Frequency getifreq();
int getvec(WFDB_Sample *vector);
int getframe(WFDB_Sample *vector);
int putvec(const WFDB_Sample *vector);
int isigsettime(WFDB_Time t);
int isgsettime(WFDB_Group g, WFDB_Time t);
WFDB_Time tnextvec(WFDB_Signal s, WFDB_Time t);
char *timstr(WFDB_Time t);
char *mstimstr(WFDB_Time t);
WFDB_Time strtim(const char *time_string);
char *datstr(WFDB_Date d);
WFDB_Date strdat(const char *date_string);
int adumuv(WFDB_Signal s, WFDB_Sample a);
WFDB_Sample muvadu(WFDB_Signal s, int microvolts);
double aduphys(WFDB_Signal s, WFDB_Sample a);
WFDB_Sample physadu(WFDB_Signal s, double v);
WFDB_Sample sample(WFDB_Signal s, WFDB_Time t);
int sample_valid();
char *getinfo(char *record);
int putinfo(const char *info);
int setinfo(char *record);
void wfdb_freeinfo();
int newheader(char *record);
int setheader(char *record, const WFDB_Siginfo *siarray,
                      unsigned int nsig);
int setmsheader(char *record, char **segnames, unsigned int nsegments);
int getseginfo(WFDB_Seginfo **segments);
int wfdbgetskew(WFDB_Signal s);
void wfdbsetiskew(WFDB_Signal s, int skew);
void wfdbsetskew(WFDB_Signal s, int skew);
long wfdbgetstart(WFDB_Signal s);
void wfdbsetstart(WFDB_Signal s, long bytes);
int wfdbputprolog(const char *prolog, long bytes, WFDB_Signal s);
extern WFDB_Frequency sampfreq(char *record);
int setsampfreq(WFDB_Frequency sampling_frequency);
extern WFDB_Frequency getcfreq();
void setcfreq(WFDB_Frequency counter_frequency);
double getbasecount();
void setbasecount(double count);
int setbasetime(char *time_string);
int setibsize(int input_buffer_size);
int setobsize(int output_buffer_size);

#endif  // WFDB_LIB_SIGNAL_H_
