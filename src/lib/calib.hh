#ifndef WFDB_LIB_CALIB_H_
#define WFDB_LIB_CALIB_H_

int calopen(const char *calibration_filename);
int getcal(const char *description, const char *units,
                   WFDB_Calinfo *cal);
int putcal(const WFDB_Calinfo *cal);
int newcal(const char *calibration_filename);
void flushcal();

#endif  // WFDB_LIB_CALIB_H_
