int calopen(const char *calibration_filename);
int getcal(const char *description, const char *units,
                   WFDB_Calinfo *cal);
int putcal(const WFDB_Calinfo *cal);
int newcal(const char *calibration_filename);
void flushcal();
