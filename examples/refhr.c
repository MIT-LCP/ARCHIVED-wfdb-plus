/* file: refhr.c	G. Moody	20 March 1992
			Last revised:	  7 May 1999

-------------------------------------------------------------------------------
refhr: Sample program for generating heart rate measurement annotation file
Copyright (C) 1999 George B. Moody

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA.

You may contact the author by e-mail (george@mit.edu) or postal mail
(MIT Room E25-505A, Cambridge, MA 02139 USA).  For updates to this software,
please visit the author's web site (http://ecg.mit.edu/).
_______________________________________________________________________________

This program copies an annotation file, inserting MEASURE annotations
containing two illustrative types of heart rate measurements into the output.
Any MEASURE annotations in the input file are not copied.  The output of this
program is suitable for input to `mxm'.
*/

#include <stdio.h>
#include <wfdb/wfdb.h>
#include <wfdb/ecgmap.h>

main(argc, argv)
int argc;
char *argv[];
{
    char hr_string[20];	/* temporary storage for heart rate measurements */
    double sps;		/* sampling frequency (samples per second) */
    int i;
    static char *record;
    static WFDB_Anninfo an[2];
    static WFDB_Annotation annot, hr_annot;

    for (i = 1; i < argc; i++) {    /* read and interpret command arguments */
	if (*argv[i] == '-') switch (*(argv[i]+1)) {
	  case 'a':	/* annotator names follow */
	    if (++i >= argc-1) {
		(void)fprintf(stderr,
		     "%s: input and output annotator names must follow -a\n",
		       argv[0]);
		exit(1);
	    }
	    an[0].name = argv[i];   an[0].stat = WFDB_READ;
	    an[1].name = argv[++i]; an[1].stat = WFDB_WRITE;
	    break;
	  case 'r':	/* record name follows */
	    if (++i >= argc) {
		(void)fprintf(stderr,
			      "%s: record name must follow -r\n", argv[0]);
		exit(1);
	    }
	    record = argv[i];
	    break;
	  default:
	    (void)fprintf(stderr,
			  "%s: unrecognized option %s\n", argv[0], argv[i]);
	    exit(1);
	}
	else {
	    (void)fprintf(stderr,
			  "%s: unrecognized argument %s\n",argv[0],argv[i]);
	    exit(1);
	}
    }

    if (record == NULL || an[0].name == NULL) {
	(void)fprintf(stderr,
	 "usage: %s -r record -a input-annotator output-annotator\n", argv[0]);
	exit(1);
    }

    if ((sps = sampfreq(record)) <= 0) {
	(void)fprintf(stderr,
		      "%s: (warning) %g Hz sampling frequency assumed\n",
		argv[0], WFDB_DEFFREQ);
	sps = WFDB_DEFFREQ;
    }

    /* Open the input and output annotation files. */
    if (annopen(record, an, 2) < 0)
	exit(2);

    /* Initialize the constant fields of the annotation structure which is
       to be used for heart rate measurements. */
    hr_annot.anntyp = MEASURE;
    hr_annot.aux = hr_string;

    /* Read an annotation on each iteration. */
    while (getann(0, &annot) == 0) {

	/* Copy the annotation to the output file, unless it's a MEASURE
	   annotation (these get filtered out, so that they won't be
	   mistaken for MEASURE annotations generated by this program) */
	if (annot.anntyp != MEASURE)
	    (void)putann(0, &annot);

	/* Update heart rate measurements only if this annotation is a beat
	   label. */
	if (isqrs(annot.anntyp)) {
	    double heart_rate;
	    static int a1;		/* anntyp of previous beat label */
	    static long t1, t2, t3;	/* times of 3 previous beat labels */
	    static long nn0, nn1, nn2, nn3, nn4, nn5;
	    			     /* current and previous 5 N-N intervals */

	    /* This sample program calculates two simple heart rate
	       measurements for illustrative purposes.  First is a
	       measurement based on the last three R-R intervals, valid only
	       after the fourth beat. */
	    if (t3 > 0L) {
		heart_rate = 180.0*sps/(annot.time - t3);
		/* Convert the measurement to a string. */
		sprintf(hr_string+1, "%g", heart_rate);
		/* The first byte of the `aux' field must be a byte count. */
		hr_string[0] = strlen(hr_string+1);
		/* The measurement annotation can't be attached to the same
		   sample as the current beat annotation, so attach it to
		   the next sample. */
		hr_annot.time = annot.time + 1;
		/* The `subtyp' field specifies the measurement type.  Here
		   the 3-beat average is assigned type 0. */
		hr_annot.subtyp = 0;
		/* Write the measurement annotation. */
		(void)putann(0, &hr_annot);
	    }
	    t3 = t2; t2 = t1; t1 = annot.time;

	    /* The second measurement is based on the last six normal R-R 
	       intervals, valid only after six normal R-R intervals have
	       been observed, and updated only if the current and previous
	       beats are both normal. */
	    if (map2(annot.anntyp) == NORMAL && a1 == NORMAL) {
		nn0 = t1 - t2;
		if (nn5 > 0L) {
		    heart_rate = 360.0*sps/(nn5 + nn4 + nn3 + nn2 + nn1 + nn0);
		    sprintf(hr_string+1, "%g", heart_rate);
		    hr_string[0] = strlen(hr_string+1);
		    /* This measurement annotation will need to be written
		       two sample intervals after the beat annotation, so
		       that it doesn't coincide with the first measurement. */
		    hr_annot.time = annot.time + 2;
		    /* The 6 N-N interval-based average is assigned type 1. */
		    hr_annot.subtyp = 1;
		    (void)putann(0, &hr_annot);
		}
		nn5 = nn4; nn4 = nn3; nn3 = nn2; nn2 = nn1; nn1 = nn0;
	    }
	    a1 = map2(annot.anntyp);
	}

	/* Note: we might run into trouble if another input annotation
	   follows a beat annotation within two sample intervals.  In this
	   case, putann() will complain that annotations were not supplied
	   in time order.  The measurement annotations will have been
	   written properly, but the offending input annotation will not
	   have been written.  This is harmless if we're only using the
	   output file as input for `mxm', since `mxm' ignores everything
	   but the measurement annotations anyway. */
    }

    wfdbquit();			/* close input files */
    exit(0);	/*NOTREACHED*/
}

