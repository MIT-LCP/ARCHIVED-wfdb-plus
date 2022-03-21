## Global and Environment Variables

Sources:
- `linux-slib.def`
- `wpg0.tex`
- The Database Path and Other Environment Variables: http://www.physionet.org/physiotools/wpg/wpg_11.htm#WFDB-path
- More about the WFDB Path: https://www.physionet.org/physiotools/wpg/wpg_14.htm#WFDB-path-syntax


- WFDBROOT: where the WFDB Software Package will be installed. eg. `/usr`

DEF as in 'default':
- DEFWFDB
- DEFWFDBCAL
- DEFWFDBGVMODE
- DEFWFDBANNSORT

See wfdb_export_config() env vars:
- WFDB
- WFDBCAL
- WFDBANNSORT
- WFDBGVMODE
- And variables: static char *p_wfdb, *p_wfdbcal, *p_wfdbannsort, *p_wfdbgvmode;
