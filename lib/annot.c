/* file: annot.c	G. Moody       	 13 April 1989
			Last revised:  15 September 1999	wfdblib 10.1.0
WFDB library functions for annotations

_______________________________________________________________________________
wfdb: a library for reading and writing annotated waveforms (time series data)
Copyright (C) 1999 George B. Moody

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option) any
later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU Library General Public License for more
details.

You should have received a copy of the GNU Library General Public License along
with this library; if not, write to the Free Software Foundation, Inc., 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.

You may contact the author by e-mail (george@mit.edu) or postal mail
(MIT Room E25-505A, Cambridge, MA 02139 USA).  For updates to this software,
please visit PhysioNet (http://www.physionet.org/).
_______________________________________________________________________________

This file contains definitions of the following functions, which are not
visible outside of this file:
 get_ann_table  (reads tables used by annstr, strann, and anndesc)
 put_ann_table  (writes tables used by annstr, strann, and anndesc)

This file also contains definitions of the following WFDB library functions:
 annopen        (opens annotation files)
 getann         (reads an annotation)
 ungetann [5.3] (pushes an annotation back into an input stream)
 putann         (writes an annotation)
 iannsettime    (skips to a specified time in input annotation files)
 ecgstr         (converts MIT annotation codes to ASCII strings)
 strecg         (converts ASCII strings to MIT annotation codes)
 setecgstr      (modifies code-to-string translation table)
 annstr [5.3]   (converts user-defined annotation codes to ASCII strings)
 strann [5.3]   (converts ASCII strings to user-defined annotation codes)
 setannstr [5.3](modifies code-to-string translation table)
 anndesc [5.3]  (converts user-defined annotation codes to text descriptions)
 setanndesc [5.3](modifies code-to-text translation table)
 iannclose [9.1](closes an input annotation file)
 oannclose [9.1](closes an output annotation file)

(Numbers in brackets in the list above indicate the first version of the WFDB
library that included the corresponding function.  Functions not so marked
have been included in all published versions of the WFDB library.)

These functions, also defined here, are intended only for the use of WFDB
library functions defined elsewhere:
 wfdb_anclose     (closes all annotation files)
 wfdb_oaflush     (flushes output annotations)

Beginning with version 5.3, the functions in this file read and write
annotation translation table modifications as `modification labels' (`NOTE'
annotations attached to sample 0 of signal 0).  This feature provides
transparent support for custom annotation definitions in WFDB applications.
Previous versions of these functions, if used to read files containing
modification labels, treat them as ordinary NOTE annotations.

Simultaneous annotations attached to different signals (as indicated by the
`chan' field) are supported by version 6.1 and later versions.  Annotations
must be written in time order; simultaneous annotations must be written in
`chan' order.  Simultaneous annotations are readable but not writeable by
earlier versions.
*/

#include "wfdblib.h"
#include "ecgcodes.h"
#define isqrs
#define map1
#define map2
#define annpos
#include "ecgmap.h"

/* Annotation word format */
#define CODE	0176000	/* annotation code segment of annotation word */
#define CS	10	/* number of places by which code must be shifted */
#define DATA	01777	/* data segment of annotation word */
#define MAXRR	01777	/* longest interval which can be coded in a word */

/* Pseudo-annotation codes.  Legal pseudo-annotation codes are between PAMIN
   (defined below) and CODE (defined above).  PAMIN must be greater than
   ACMAX << CS (see <ecg/ecgcodes.h>). */
#define PAMIN	((unsigned)(59 << CS))
#define SKIP	((unsigned)(59 << CS))	/* long null annotation */
#define NUM	((unsigned)(60 << CS))	/* change 'num' field */
#define SUB	((unsigned)(61 << CS))	/* subtype */
#define CHN	((unsigned)(62 << CS))	/* change 'chan' field */
#define AUX	((unsigned)(63 << CS))	/* auxiliary information */

/* Constants for AHA annotation files only */
#define ABLKSIZ	1024		/* AHA annotation file block length */
#define AUXLEN	6		/* length of AHA aux field */
#define EOAF	0377		/* padding for end of AHA annotation files */

/* Shared local data */
static unsigned niaf;			/* number of open input annotators */
static WFDB_FILE *iaf[WFDB_MAXANN];     /* file pointers for open input
					   annotators */
static WFDB_Anninfo iafinfo[WFDB_MAXANN];    /* input annotator information */
static WFDB_Annotation iann[WFDB_MAXANN];/* next annotation from each input */
static unsigned iaword[WFDB_MAXANN];	 /* next word from each input file */
static int ateof[WFDB_MAXANN];	   /* EOF-reached indicator for each input */
static WFDB_Time iantime[WFDB_MAXANN]; /* annotation time (MIT format only).
					  This equals iann[].time unless a SKIP
					  follows iann;  in such cases, it is
					  the time of the SKIP (i.e., the time
					  of the annotation following iann) */

static unsigned noaf;		       /* number of open output annotators */
static WFDB_FILE *oaf[WFDB_MAXANN];      /* file pointers for open output
					    annotators */
static WFDB_Anninfo oafinfo[WFDB_MAXANN];    /* output annotator information */
static WFDB_Annotation oann[WFDB_MAXANN];/* latest annotation in each output */
static int oanum[WFDB_MAXANN];	 /* output annot numbers (AHA format only) */
static char out_of_order[WFDB_MAXANN];	/* if >0, one or more annotations
					   written by putann are not in the
					   canonical (time, chan) order    */
static char oarec[WFDB_MAXANN][WFDB_MAXRNL+1];
		  /* record with which each output annotator is associated */

static int tmul;			/* `time' fields in annotations are
					  tmul * times in annotation files */

/* Local functions (for the use of other functions in this module only). */

static int get_ann_table(i)
WFDB_Annotator i;
{
    char *p1, *p2, *s1, *s2;
    int a;
    WFDB_Annotation annot;

    if (getann(i, &annot) < 0)	/* prime the pump */
	return (-1);
    while (getann(i,&annot) == 0 && annot.time == 0L && annot.anntyp == NOTE) {
	if (annot.aux == NULL || *annot.aux < 1 || *(annot.aux+1) == '#')
	    continue;
	p1 = strtok(annot.aux+1, " \t");
	a = atoi(p1);
	if (0 <= a && a <= ACMAX && (p1 = strtok((char *)NULL, " \t"))) {
	    p2 = p1 + strlen(p1) + 1;
	    if ((s1 = (char *)malloc(((unsigned)(strlen(p1) + 1)))) == NULL ||
		(*p2 &&
		 (s2 = (char *)malloc(((unsigned)(strlen(p2)+1)))) == NULL)) {
		wfdb_error("annopen: insufficient memory\n");
		return (-1);
	    }
	    (void)strcpy(s1, p1);
	    (void)setannstr(a, s1);
	    if (*p2) {
		(void)strcpy(s2, p2);
		(void)setanndesc(a, s2);
	    }
	    else
		(void)setanndesc(a, (char*)NULL);
	}

    }
    (void)ungetann(i, &annot);
    return (0);
}

static char modified[ACMAX+1];	/* modified[i] is non-zero if setannstr() or
				   setanndesc() has modified the mnemonic or
				   description for annotation type i */   

static int put_ann_table(i)
WFDB_Annotator i;
{
    int a;
    char buf[256];
    WFDB_Annotation annot;

    annot.time = 0L;
    annot.anntyp = NOTE;
    annot.subtyp = annot.chan = annot.num = 0;
    annot.aux = buf;
    for (a = 0; a <= ACMAX; a++)
	if (modified[a]) {
	    (void)sprintf(buf+1, "%d %s %s", a, annstr(a), anndesc(a));
	    buf[0] = strlen(buf+1);
	    if (putann(i, &annot) < 0) return (-1);
	}
    return (0);
}

/* WFDB library functions (for general use). */

FINT annopen(record, aiarray, nann)
char *record;
WFDB_Anninfo *aiarray;
unsigned int nann;
{
    int a;
    unsigned int i;

    if (*record == '+')		/* don't close open annotation files */
	record++;		/* discard the '+' prefix */
    else {
	wfdb_anclose();		/* close previously opened annotation files */
	tmul = getspf();
    }

    for (i = 0; i < nann; i++) { /* open the annotation files */
	switch (aiarray[i].stat) {
	  case WFDB_READ:	/* standard (MIT-format) input file */
	  case WFDB_AHA_READ:	/* AHA-format input file */
	    wfdb_setirec(record);
	    if (niaf >= WFDB_MAXANN) {
		wfdb_error("annopen: too many (> %d) input annotators\n",
			 WFDB_MAXANN);
		return (-3);
	    }
	    if ((iaf[niaf] = wfdb_open(aiarray[i].name,record,WFDB_READ)) ==
		NULL) {
		wfdb_error("annopen: can't read annotator %s for record %s\n",
			 aiarray[i].name, record);
		return (-3);
	    }
	    if ((iafinfo[niaf].name =
		 (char *)malloc((unsigned)(strlen(aiarray[i].name)+1)))
		 == NULL) {
		wfdb_error("annopen: insufficient memory\n");
		return (-3);
	    }
	    (void)strcpy(iafinfo[niaf].name, aiarray[i].name);

	    /* Try to figure out what format the file is in.  AHA-format files
	       begin with a null byte and an ASCII character which is one
	       of the legal AHA annotation codes other than '[' or ']'.
	       MIT annotation files cannot begin in this way. */
	    iaword[niaf] = (unsigned)wfdb_g16(iaf[niaf]);
	    a = (iaword[niaf] >> 8) & 0xff;
	    if ((iaword[niaf]&0xff) || ammap(a)==NOTQRS || a=='[' || a==']') {
		if (aiarray[i].stat != WFDB_READ) {
		    wfdb_error("warning (annopen, annotator %s, record %s):\n",
			     aiarray[i].name, record);
		    wfdb_error(" file appears to be in MIT format\n");
		    wfdb_error(" ... continuing under that assumption\n");
		}
		iafinfo[niaf].stat = WFDB_READ;
		/* read any initial null annotation(s) */
		while ((iaword[niaf] & CODE) == SKIP) {
		    iantime[niaf] += wfdb_g32(iaf[niaf]);
		    iaword[niaf] = (unsigned)wfdb_g16(iaf[niaf]);
		}
	    }
	    else {
		if (aiarray[i].stat != WFDB_AHA_READ) {
		    wfdb_error("warning (annopen, annotator %s, record %s):\n",
			     aiarray[i].name, record);
		    wfdb_error(" file appears to be in AHA format\n");
		    wfdb_error(" ... continuing under that assumption\n");
		}
		iafinfo[niaf].stat = WFDB_AHA_READ;
	    }
	    iann[niaf].anntyp = 0;    /* any pushed-back annot is invalid */
	    niaf++;
	    (void)get_ann_table(niaf-1);
	    break;

	  case WFDB_WRITE:	/* standard (MIT-format) output file */
	  case WFDB_AHA_WRITE:	/* AHA-format output file */
	    if (noaf >= WFDB_MAXANN) {
		wfdb_error("annopen: too many (> %d) output annotators\n",
			 WFDB_MAXANN);
		return (-4);
	    }
	    /* Quit (with message from wfdb_checkname) if name is illegal */
	    if (wfdb_checkname(aiarray[i].name, "annotator"))
		return (-4);
	    if ((oaf[noaf]=wfdb_open(aiarray[i].name,record,WFDB_WRITE)) ==
		NULL) {
		wfdb_error("annopen: can't write annotator %s for record %s\n",
			 aiarray[i].name, record);
		return (-4);
	    }
	    if ((oafinfo[noaf].name =
		 (char *)malloc((unsigned)(strlen(aiarray[i].name)+1)))
		== NULL) {
		wfdb_error("annopen: insufficient memory\n");
		return (-4);
	    }
	    (void)strcpy(oafinfo[noaf].name, aiarray[i].name);
	    (void)strncpy(oarec[noaf], record, WFDB_MAXRNL);
	    oann[noaf].time = 0L;
	    oafinfo[noaf].stat = aiarray[i].stat;
	    out_of_order[noaf] = 0;
	    (void)put_ann_table(noaf++);
	    break;

	  default:
	    wfdb_error(
		     "annopen: illegal stat %d for annotator %s, record %s\n",
		     aiarray[i].stat, aiarray[i].name, record);
	    return (-5);
	}
    }
    return (0);
}

static WFDB_Annotation ungotten[WFDB_MAXANN];	/* pushed-back annotations */

FINT getann(an, annot)	/* read an annotation from iaf[an] into *annot */
WFDB_Annotator an;		/* annotator number */
WFDB_Annotation *annot;		/* address of structure to be filled in */
{
    static char auxstr[WFDB_MAXANN][1+255+1];	/* byte count + data + null */
    static unsigned ai[WFDB_MAXANN];
    int a, len;

    if (an >= niaf || iaf[an] == NULL) {
	wfdb_error("getann: can't read annotator %d\n", an);
	return (-2);
    }

    if (ungotten[an].anntyp) {
	*annot = ungotten[an];
	ungotten[an].anntyp = 0;
	return (0);
    }

    if (ateof[an]) {
	if (ateof[an] != -1)
	    return (-1);	/* reached logical EOF */
	wfdb_error("getann: unexpected EOF in annotator %s\n",
		 iafinfo[an].name);
	return (-3);
    }
    *annot = iann[an];

    switch (iafinfo[an].stat) {
      case WFDB_READ:		/* MIT-format input file */
      default:
	if (iaword[an] == 0) {	/* logical end of file */
	    ateof[an] = 1;
	    return (0);
	}
	iantime[an] += iaword[an]&DATA; /* annotation time */
	iann[an].time = iantime[an] * tmul;
	iann[an].anntyp = (iaword[an]&CODE) >> CS; /* set annotation type */
	iann[an].subtyp = 0;	/* reset subtype field */
	iann[an].aux = NULL;	/* reset aux field */
	while (((iaword[an] = (unsigned)wfdb_g16(iaf[an]))&CODE) >= PAMIN &&
	       !wfdb_feof(iaf[an]))
	    switch (iaword[an] & CODE) { /* process pseudo-annotations */
	      case SKIP:  iantime[an] += wfdb_g32(iaf[an]); break;
	      case SUB:   iann[an].subtyp = DATA & iaword[an]; break;
	      case CHN:   iann[an].chan = DATA & iaword[an]; break;
	      case NUM:	  iann[an].num = DATA & iaword[an]; break;
	      case AUX:			/* auxiliary information */
		len = iaword[an] & 0377;	/* length of auxiliary data */
		if (ai[an] >= 256 - len) ai[an] = 0;	/* buffer index */
		iann[an].aux = auxstr[an]+ai[an];    /* save buffer pointer */
		auxstr[an][ai[an]++] = len;		/* save length byte */
		/* Now read the data.  Note that an extra byte may be
		   present in the annotation file to preserve word alignment;
		   if so, this extra byte is read and then overwritten by
		   the null in the second statement below. */
		(void)wfdb_fread(auxstr[an]+ai[an], 1, (len+1)&~1, iaf[an]);
		auxstr[an][ai[an]+len] = '\0';		/* add a null */
		ai[an] += len+1;		/* update buffer pointer */
		break;
	      default: break;
	    }
	break;
      case WFDB_AHA_READ:		/* AHA-format input file */
	if ((iaword[an]&0377) == EOAF) { /* logical end of file */
	    ateof[an] = 1;
	    return (0);
	}
	a = iaword[an] >> 8;		 /* AHA annotation code */
	iann[an].anntyp = ammap(a);	 /* convert to MIT annotation code */
	iann[an].time = wfdb_g32(iaf[an]) * tmul; /* time of annotation */
	if (wfdb_g16(iaf[an]) <= 0)	 /* serial number (starts at 1) */
	    wfdb_error("getann: unexpected annot number in file %d\n", an);
	iann[an].subtyp = wfdb_getc(iaf[an]); /* MIT annotation subtype */
	if (a == 'U' && iann[an].subtyp == 0)
	    iann[an].subtyp = -1;	 /* unreadable (noise subtype -1) */
	iann[an].chan = wfdb_getc(iaf[an]);	 /* MIT annotation code */
	if (ai[an] >= 256 - (AUXLEN+2)) ai[an] = 0;
	(void)wfdb_fread(auxstr[an]+ai[an]+1,1,AUXLEN,iaf[an]); /* read aux
								   data */
	/* There is very limited space in AHA format files for auxiliary
	   information, so no length byte is recorded;  instead, we
	   assume that if the first byte of auxiliary data is
	   not null, that up to AUXLEN bytes may be significant. */
	if (auxstr[an][ai[an]+1]) {
	    auxstr[an][ai[an]] = AUXLEN;
	    iann[an].aux = auxstr[an]+ai[an];	 /* save buffer pointer */
	    auxstr[an][ai[an]+1+AUXLEN] = '\0';	 /* add a null */
	    ai[an] += AUXLEN+2;		 /* update buffer pointer */
	}
	else
	    iann[an].aux = NULL;
	iaword[an] = (unsigned)wfdb_g16(iaf[an]);
	break;
    }
    if (wfdb_feof(iaf[an]))
	ateof[an] = -1;
    return (0);
}

FINT ungetann(an, annot)   /* push back an annotation into an input stream */
WFDB_Annotator an;		/* annotator number */
WFDB_Annotation *annot;		/* address of annotation to be pushed back */
{
    if (an >= niaf) {
	wfdb_error("ungetann: annotator %d is not initialized\n", an);
	return (-2);
    }
    if (ungotten[an].anntyp) {
	wfdb_error("ungetann: pushback buffer is full\n");
	wfdb_error(
		 "ungetann: annotation at %ld, annotator %d not pushed back\n",
		 annot->time, an);
	return (-1);
    }
    ungotten[an] = *annot;
    return (0);
}

FINT putann(an, annot)    /* write annotation addressed by annot to oaf[an] */
WFDB_Annotator an;		/* annotator number */
WFDB_Annotation *annot;		/* address of annotation to be written */
{
    unsigned annwd;
    char *ap;
    int i, len;
    long delta, t;

    if (an >= noaf || oaf[an] == NULL) {
	wfdb_error("putann: can't write annotation file %d\n", an);
	return (-2);
    }
    t = annot->time / tmul;
    if (((delta = t - oann[an].time) < 0L ||
	(delta == 0L && annot->chan <= oann[an].chan)) &&
	(annot->time != 0L || oann[an].time != 0L)) {
	out_of_order[an] = 1;
    }
    switch (oafinfo[an].stat) {
      case WFDB_WRITE:	/* MIT-format output file */
      default:
	if (annot->anntyp == 0) {
	    /* The caller intends to write a null annotation here, but putann
	       must not write a word of zeroes that would be interpreted as
	       an EOF.  To avoid this, putann writes a SKIP to the location
	       just before the desired one;  thus annwd (below) is never 0. */
	    wfdb_p16(SKIP, oaf[an]); wfdb_p32(delta-1, oaf[an]); delta = 1;
	}
	else if (delta > MAXRR || delta < 0L) {
	    wfdb_p16(SKIP, oaf[an]); wfdb_p32(delta, oaf[an]); delta = 0;
	}	
	annwd = (int)delta + ((int)(annot->anntyp) << CS);
	wfdb_p16(annwd, oaf[an]);
	if (annot->subtyp != 0) {
	    annwd = SUB + (DATA & annot->subtyp);
	    wfdb_p16(annwd, oaf[an]);
	}
	if (annot->chan != oann[an].chan) {
	    annwd = CHN + (DATA & annot->chan);
	    wfdb_p16(annwd, oaf[an]);
	}
	if (annot->num != oann[an].num) {
	    annwd = NUM + (DATA & annot->num);
	    wfdb_p16(annwd, oaf[an]);
	}
	if (annot->aux != NULL && *annot->aux != 0) {
	    annwd = AUX+(unsigned)(*annot->aux); 
	    wfdb_p16(annwd, oaf[an]);
	    (void)wfdb_fwrite(annot->aux + 1, 1, *annot->aux, oaf[an]);
	    if (*annot->aux & 1)
		(void)wfdb_putc('\0', oaf[an]);
	}
	break;
      case WFDB_AHA_WRITE:	/* AHA-format output file */
	(void)wfdb_putc('\0', oaf[an]);
	(void)wfdb_putc(mamap(annot->anntyp, annot->subtyp), oaf[an]);
	wfdb_p32(annot->time, oaf[an]);
	wfdb_p16((unsigned int)(++oanum[an]), oaf[an]);
	(void)wfdb_putc(annot->subtyp, oaf[an]);
	(void)wfdb_putc(annot->anntyp, oaf[an]);
	if (ap = annot->aux)
	    len = (*ap < AUXLEN) ? *ap : AUXLEN;
	else
	    len = 0;
	for (i = 0, ap++; i < len; i++, ap++)
	    (void)wfdb_putc(*ap, oaf[an]);
	for ( ; i < AUXLEN; i++)
	    (void)wfdb_putc('\0', oaf[an]);
	break;
    }
    if (wfdb_ferror(oaf[an])) {
	wfdb_error("putann: write error on annotation file %d\n",an);
	return (-1);
    }
    oann[an] = *annot;
    oann[an].time = t;
    return (0);
}

FINT iannsettime(t)
WFDB_Time t;
{
    int stat = 0;
    WFDB_Annotation tempann;
    WFDB_Annotator i;

    /* Handle negative arguments as equivalent positive arguments. */
    if (t < 0L) t = -t;

    for (i = 0; i < niaf && stat == 0; i++) {
	if (iann[i].time >= t) {	/* "rewind" the annotation file */
	    ungotten[i].anntyp = 0;	/* flush pushback buffer */
	    if (wfdb_fseek(iaf[i], 0L, 0) == -1) {
		wfdb_error("iannsettime: improper seek\n");
		return (-1);
	    }
	    iann[i].subtyp = iann[i].chan = iann[i].num = ateof[i] = 0;
	    iann[i].time = iantime[i] = 0L;
	    iaword[i] = wfdb_g16(iaf[i]);
	    if (iafinfo[i].stat == WFDB_READ)
		while ((iaword[i] & CODE) == SKIP) {
		    iantime[i] += wfdb_g32(iaf[i]);
		    iaword[i] = wfdb_g16(iaf[i]);
		}
	    (void)getann(i, &tempann);
	}
	while (iann[i].time < t && (stat = getann(i, &tempann)) == 0)
	    ;
    }
    return (stat);
}

static char *cstring[ACMAX+1] = {  /* ECG mnemonics for each code */
        " ",	"N",	"L",	"R",	"a",		/* 0 - 4 */
	"V",	"F",	"J",	"A",	"S",		/* 5 - 9 */
	"E",	"j",	"/",	"Q",	"~",		/* 10 - 14 */
	"[15]",	"|",	"[17]",	"s",	"T",		/* 15 - 19 */
	"*",	"D",	"\"",	"=",	"p",		/* 20 - 24 */
	"B",	"^",	"t",	"+",	"u",		/* 25 - 29 */
	"?",	"!",	"[",	"]",	"e",		/* 30 - 34 */
	"n",	"@",	"x",	"f",	"(",		/* 35 - 39 */
	")",	"r",    "[42]",	"[43]",	"[44]",		/* 40 - 44 */
	"[45]",	"[46]",	"[47]",	"[48]",	"[49]"		/* 45 - 49 */
};

FSTRING ecgstr(code)
int code;
{
    static char buf[9];

    if (0 <= code && code <= ACMAX)
	return (cstring[code]);
    else {
	(void)sprintf(buf,"[%d]", code);
	return (buf);
    }
}

FINT strecg(str)
char *str;
{
    int code;

    for (code = 1; code <= ACMAX; code++)
	if (strcmp(str, cstring[code]) == 0)
	    return (code);
    return (NOTQRS);
}

FINT setecgstr(code, string)
int code;
char *string;
{
    if (NOTQRS <= code && code <= ACMAX) {
	cstring[code] = string;
	return (0);
    }
    wfdb_error("setecgstr: illegal annotation code %d\n", code);
    return (-1);
}

static char *astring[ACMAX+1] = {  /* mnemonic strings for each code */
        " ",	"N",	"L",	"R",	"a",		/* 0 - 4 */
	"V",	"F",	"J",	"A",	"S",		/* 5 - 9 */
	"E",	"j",	"/",	"Q",	"~",		/* 10 - 14 */
	"[15]",	"|",	"[17]",	"s",	"T",		/* 15 - 19 */
	"*",	"D",	"\"",	"=",	"p",		/* 20 - 24 */
	"B",	"^",	"t",	"+",	"u",		/* 25 - 29 */
	"?",	"!",	"[",	"]",	"e",		/* 30 - 34 */
	"n",	"@",	"x",	"f",	"(",		/* 35 - 39 */
	")",	"r",    "[42]",	"[43]",	"[44]",		/* 40 - 44 */
	"[45]",	"[46]",	"[47]",	"[48]",	"[49]"		/* 45 - 49 */
};

FSTRING annstr(code)
int code;
{
    static char buf[9];

    if (0 <= code && code <= ACMAX)
	return (astring[code]);
    else {
	(void)sprintf(buf,"[%d]", code);
	return (buf);
    }
}

FINT strann(str)
char *str;
{
    int code;

    for (code = 1; code <= ACMAX; code++)
	if (strcmp(str, astring[code]) == 0)
	    return (code);
    return (NOTQRS);
}

FINT setannstr(code, string)
int code;
char *string;
{
    if (0 < code && code <= ACMAX) {
	astring[code] = string;
	modified[code] = 1;
	return (0);
    }
    else if (-ACMAX < code && code <= 0) {
	astring[-code] = string;
	return (0);
    }
    else {
	wfdb_error("setannstr: illegal annotation code %d\n", code);
	return (-1);
    }
}

static char *tstring[ACMAX+1] = {  /* descriptive strings for each code */
    "",
    "Normal beat",
    "Left bundle branch block beat",
    "Right bundle branch block beat",
    "Aberrated atrial premature beat",
    "Premature ventricular contraction",
    "Fusion of ventricular and normal beat",
    "Nodal (junctional) premature beat",
    "Atrial premature beat",
    "Supraventricular premature or ectopic beat",
    "Ventricular escape beat",
    "Nodal (junctional) escape beat",
    "Paced beat",
    "Unclassifiable beat",
    "Change in signal quality",
    (char *)NULL,
    "Isolated QRS-like artifact",
    (char *)NULL,
    "ST segment change",
    "T-wave change",
    "Systole",
    "Diastole",
    "Comment annotation",
    "Measurement annotation",
    "P-wave peak",
    "Bundle branch block beat (unspecified)",
    "(Non-captured) pacemaker artifact",
    "T-wave peak",
    "Rhythm change",
    "U-wave peak",
    "Beat not classified during learning",
    "Ventricular flutter wave",
    "Start of ventricular flutter/fibrillation",
    "End of ventricular flutter/fibrillation",
    "Atrial escape beat",
    "Supraventricular escape beat",
    "Link to external data (aux contains URL)",
    "Non-conducted P-wave (blocked APC)",
    "Fusion of paced and normal beat",
    "Waveform onset",
    "Waveform end",
    "R-on-T premature ventricular contraction",
    (char *)NULL,
    (char *)NULL,
    (char *)NULL,
    (char *)NULL,
    (char *)NULL,
    (char *)NULL,
    (char *)NULL,
    (char *)NULL
};

FSTRING anndesc(code)
int code;
{
    if (0 <= code && code <= ACMAX)
	return (tstring[code]);
    else
	return ("illegal annotation code");
}

FINT setanndesc(code, string)
int code;
char *string;
{
    if (0 < code && code <= ACMAX) {
	tstring[code] = string;
	modified[code] = 1;
	return (0);
    }
    else if (-ACMAX < code && code <= 0) {
	tstring[-code] = string;
	return (0);
    }
    else {
	wfdb_error("setanndesc: illegal annotation code %d\n", code);
	return (-1);
    }
}

FVOID iannclose(an)      /* close input annotation file 'an' */
WFDB_Annotator an;
{
    if (an < WFDB_MAXANN && iaf[an] != NULL) {
	(void)wfdb_fclose(iaf[an]);
	(void)free(iafinfo[an].name);
	while (an < niaf-1) {
	    iaf[an] = iaf[an+1];
	    iafinfo[an] = iafinfo[an+1];
	    iann[an] = iann[an+1];
	    iaword[an] = iaword[an+1];
	    ateof[an] = ateof[an+1];
	    iantime[an] = iantime[an+1];
	    ungotten[an] = ungotten[an+1];
	    an++;
	}
	iaf[an] = NULL;
	ungotten[an].anntyp = iann[an].subtyp = iann[an].chan =
	    iann[an].num = ateof[an] = 0;
	iantime[an] = 0L;
	niaf--;
    }
}

FVOID oannclose(an)      /* close output annotation file 'an' */
WFDB_Annotator an;
{
    int i;
    char cmdbuf[256];

    if (an < WFDB_MAXANN && oaf[an] != NULL) {
	switch (oafinfo[an].stat) {
	  case WFDB_WRITE:   /* write logical EOF for MIT-format files */
	    wfdb_p16(0, oaf[an]);
	    break;
	  case WFDB_AHA_WRITE:       /* write logical EOF for AHA-format files */
	    i = ABLKSIZ - (unsigned)(wfdb_ftell(oaf[an])) % ABLKSIZ;
	    while (i-- > 0)
		(void)wfdb_putc(EOAF, oaf[an]);
	    break;
	}
	(void)wfdb_fclose(oaf[an]);
	while (an < noaf-1) {
	    oaf[an] = oaf[an+1];
	    oafinfo[an] = oafinfo[an+1];
	    oann[an] = oann[an+1];
	    oanum[an] = oanum[an+1];
	    an++;
	}
	oaf[an] = NULL;
	oann[an].subtyp = oann[an].chan = oann[an].num = oanum[an] = 0;
	oann[an].time = 0L;
	if (out_of_order[an]) {
	    (void)sprintf(cmdbuf, "sortann -r %s -a %s",
			  oarec[an], oafinfo[an].name);
#ifndef WFDBNOSORT
	    if (getenv("WFDBNOSORT") == NULL) {
		if (system(NULL) != 0) {
		    wfdb_error(
		    "Rearranging annotations for output annotator %d ...", an);
		    if (system(cmdbuf) == 0) {
			wfdb_error("done!");
			out_of_order[an] = 0;
		    }
		    else
		      wfdb_error(
			       "\nAnnotations still need to be rearranged.\n");
		}
	    }
	}
	if (out_of_order[an]) {
#endif
	    wfdb_error("Use the command:\n  %s\n", cmdbuf);
	    wfdb_error("to rearrange annotations in the correct order.\n");
	}
	(void)free(oafinfo[an].name);
	noaf--;
    }
}

/* Private functions (for the use of other WFDB library functions only). */

void wfdb_oaflush()
{
    unsigned int i;

    for (i = 0; i < noaf; i++)
	(void)wfdb_fflush(oaf[i]);
}

void wfdb_anclose()
{
    WFDB_Annotator an;

    for (an = niaf; an != 0; an--)
	iannclose(an-1);
    for (an = noaf; an != 0; an--)
	oannclose(an-1);
}
