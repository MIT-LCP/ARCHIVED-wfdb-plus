/* file: plotstm.c		G. Moody	24 March 1992
				Last revised:	 3 May 1999

-------------------------------------------------------------------------------
plotstm: Make a PostScript scatter plot of ST measurements from `epic' output
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

*/

#include <stdio.h>
#include <math.h>
#ifndef __STDC__
extern void exit();
#endif

/* The PostScript code below is the prolog for the scatter plot.  Portions of
   this code were extracted from the output of `plt', a 2-D plotting package
   written by Paul Albrecht at MIT. */
char *prolog[] = {
"%!",
"save 100 dict begin /plotstm exch def",
"/M {newpath PMTX transform moveto} def",
"/N {PMTX dtransform rlineto} def",
"/L {N currentpoint stroke moveto} def",
"/T {/ang exch def /pos exch def M ang rotate /str exch def str stringwidth",
" pop XOFF pos get mul YOFF pos get PS mul XYM 0.85 mul add rmoveto str show",
" /XYM 0 def DMTX setmatrix} def",
"/XT {/XYM XM neg def 3 0 T /XM 0 def} def",
"/YT {/XYM YM def 7 90 T /YM 0 def} def",
"/XN {/XM PS def 3 0 T} def",
"/YN {-1 1 0 6 3 roll M /str exch def str stringwidth pop add /len exch def",
" len mul .2 PS mul add mul PS -.37 mul rmoveto str show /YM len .2 PS mul",
" add YM 2 copy lt {exch pop} {pop} ifelse def} def",
"/SP {/y exch def /x exch def x y M currentpoint (.) false charpath PBOX",
" newpath exch dup add x0 x1 add 2 div sub exch dup add y0 y1 add 2 div sub",
" moveto (.) false charpath gsave fill grestore PMTX concat PBOX DMTX",
" setmatrix y1 y sub 0 le {0 y1 EB} if y0 y sub 0 ge {0 y0 EB} if} def",
"/EB {newpath x exch M 0 exch N x0 x sub 0 N x1 x0 sub 0 N stroke} def",
"/LW {.2 mul 0.85 mul setlinewidth} def",
"/SF {0.85 mul /PS exch def findfont PS scalefont setfont} def",
"/PBOX {pathbbox /y1 exch def /x1 exch def /y0 exch def /x0 exch def} def",
"/DMTX matrix defaultmatrix def",
"/XOFF [0 0 0 -.5 -1 -1 -1 -.5 -.5 -.5 -.6 -.6] def",
"/YOFF [0 -.37 -1 -1 -1 -.37 0 0 -.37 .25 -1.25 .25] def",
"/XM 0 def /YM 0 def /XYM 0 def 0 setlinecap",
"gsave matrix setmatrix 57.8 243.2 translate 0.119 0.119 scale",
"/PMTX matrix currentmatrix def grestore",
"[] 0 setdash 0 setgray 3 LW /Times-Roman 14 SF 4103 720 M -3239 0 L",
"864 672 M 0 48 L (-2000) 864 672 XN 1269 688 M 0 32 L 1674 672 M 0 48 L",
"(-1000) 1674 672 XN 2079 688 M 0 32 L 2484 672 M 0 48 L (0) 2484 672 XN",
"2888 688 M 0 32 L 3293 672 M 0 48 L (1000) 3293 672 XN 3698 688 M 0 32 L",
"4103 672 M 0 48 L (2000) 4103 672 XN 864 3060 M 0 -2340 L 816 720 M 48 0 L",
"(-2000) 816 720 YN 832 1013 M 32 0 L 816 1305 M 48 0 L (-1000) 816 1305 YN",
"832 1598 M 32 0 L 816 1890 M 48 0 L (0) 816 1890 YN 832 2183 M 32 0 L",
"816 2475 M 48 0 L (1000) 816 2475 YN 832 2768 M 32 0 L 816 3060 M 48 0 L",
"(2000) 816 3060 YN 2 LW /Times-Roman 16 SF",
"(Test ST deviations \\(microvolts\\)) 2483 505 XT",
"(Reference ST deviations \\(microvolts\\)) 613 1890 YT",
"(ST deviation comparison) 2483 3284 7 0 T",
(char *)NULL
};

char *pname;		/* name by which this program was invoked */

main(argc, argv)
int argc;
char *argv[];
{
    static char buf[256];
    char *prog_name();
    FILE *ifile;
    int i;
    static long n, nd;

    pname = prog_name(argv[0]);
    if (argc < 2) {
	(void)fprintf(stderr, "usage: %s FILE >PSFILE\n", pname);
	(void)fprintf(stderr,
" where FILE is the name of a file of ST measurement comparisons generated\n");
	(void)fprintf(stderr,
" using `epic', and PSFILE is the name of the PostScript file generated by\n");
	(void)fprintf(stderr, " this program.\n");
	exit(1);
    }

    if (strcmp(argv[1], "-") == 0)
	ifile = stdin;
    else if ((ifile = fopen(argv[1], "r")) == NULL) {
	(void)fprintf(stderr, "%s: can't open %s\n", pname, argv[1]);
	exit(2);
    }

    /* Check that the input file is the proper type. */
    if (fgets(buf, 256, ifile) == NULL ||
	strcmp(buf, "(ST measurements)\n") != 0 ||
	fgets(buf, 256, ifile) == NULL ||
	strcmp(buf, "Record     Time  Signal  Reference  Test\n") != 0) {
	(void)fprintf(stderr,
	    "%s: input file `%s' does not appear to be an `epic' file of ST\n",
		      pname, argv[1]);
	(void)fprintf(stderr, " measurements\n");
	exit(3);
    }

    for (i = 0; prolog[i] != NULL; i++)
	(void)puts(prolog[i]);

    while (fgets(buf, 256, ifile)) {
	double ref, test;

	if (sscanf(buf, "%*s %*s %*d %lf %lf", &ref, &test) != 2) {
	    (void)fprintf(stderr, "improperly formatted line in `%s':\n %s\n",
			  argv[1], buf);
	    continue;
	}
	(void)printf("%d %d SP\n", (int)(0.8104*test + 2483.9),
		     (int)(0.585*ref + 1890.5));
	n++;
	if (ref > test) {
	    if (ref-test > 100.0) nd++;
	}
	else
	    if (test-ref > 100.0) nd++;
    }

    if (n == 0L)
	(void)printf("(No measurements were found in the input file)");
    else
	(void)printf(
    "(%ld of %ld measurements \\(%.4g%%\\) are discrepant by >100 microvolts)",
		 nd, n, 100.0*nd/(double)n);
    (void)puts(" 2483 0 XT\nshowpage clear plotstm end restore");

    if (ifile != stdin) (void)fclose(ifile);
    exit(0);	/*NOTREACHED*/
}

char *prog_name(s)
char *s;
{
    char *p = s + strlen(s);

#ifdef MSDOS
    while (p >= s && *p != '\\' && *p != ':') {
	if (*p == '.')
	    *p = '\0';		/* strip off extension */
	if ('A' <= *p && *p <= 'Z')
	    *p += 'a' - 'A';	/* convert to lower case */
	p--;
    }
#else
    while (p >= s && *p != '/')
	p--;
#endif
    return (p+1);
}
