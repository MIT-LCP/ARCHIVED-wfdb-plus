/* file: wavescript.c		G. Moody	10 October 1996
				Last revised:	29 April 1999
Remote control for WAVE via script

-------------------------------------------------------------------------------
WAVE: Waveform analyzer, viewer, and editor
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

When a WAVE process starts, it creates an empty `mailbox' file named
/tmp/.wave.UID.PID, where UID is the user's ID, and PID is the (decimal)
process id;  this file should be removed when WAVE exits.

This program controls a separate WAVE process by writing a message to that
process's mailbox and then by sending a SIGUSR1 signal to that process.
When WAVE receives a SIGUSR1 signal, it reads the message, performs the
requested action(s), and truncates the mailbox (so that it is once again
empty).

The (text) message written by this program may contain any or all of:
  -r RECORD	[to (re)open RECORD]
  -a ANNOTATOR	[to (re)open the specified ANNOTATOR for the current record]
  -f TIME	[to go to the specified TIME in the current record]
These messages are copied from the file named in the first command-line
argument.

If you wish to control a specific (known) WAVE process, use the -pid option
to specify its process id;  otherwise, wave-remote attempts to control the
WAVE process with the highest process id (in most cases, the most recently
started WAVE process).

wavescript attempts to detect orphaned mailboxes (those left behind as a
result of WAVE or the system crashing, for example).  If a mailbox is not
empty when wavescript first looks in it, wavescript waits for a short interval
to give WAVE a chance to empty it.  If the mailbox is still not empty on a
second look, wavescript advises the user to delete it and try again.

In unusual cases, wavescript may be unable to read the mailbox.  This can
happen if the highest-numbered WAVE process belongs to another user, for
example.  In such cases, wavescript advises the user to delete the mailbox
and try again.  (Of course, you won't be able to do that if the mailbox really
does belong to another user;  in this case, you can try using wavescript's
-pid option to tell it to look in the mailbox belonging to your own WAVE
process.)

If a record name is specified, and no WAVE processes can be found, wavescript
starts a new WAVE process.  The option -pid 0 prevents wavescript from
looking for an existing WAVE process, so this method can be used to start
WAVE unconditionally (though it's unclear why this would be useful).

This program would be more reliable if it checked to see if the target process
has the same user ID as that of the wavescript process.  On a typical
workstation, with only one user running WAVE at a time, however, the current
implementation is adequate.
*/

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>

char *pname;

void help()
{
    fprintf(stderr, "usage: %s SCRIPT [ -pid PROCESSID ]\n", pname);
    fprintf(stderr, "The SCRIPT may include:\n");
    fprintf(stderr, " -a ANNOTATOR\n");
    fprintf(stderr, " -f TIME\n");
    fprintf(stderr, " -r RECORD\n");
    fprintf(stderr,
	    "Any lines in the SCRIPT not beginning with '-' are ignored.\n");
}

int find_wave_pid()
{
    DIR *dir;
    struct dirent *dir_entry;
    char pattern[16];
    int i, imax = 0, pl;

    dir = opendir("/tmp");
    sprintf(pattern, ".wave.%d.", (int)getuid());
    pl = strlen(pattern);
    while (dir_entry = readdir(dir)) {
	if (strncmp(dir_entry->d_name, pattern, pl) == 0) {
	    i = atoi(dir_entry->d_name+pl);
	    if (i > imax) imax = i;
	}
    }
    closedir(dir);
    return (imax);
}

char **environ;

int start_new_wave(record, annotator, ptime)
char *record, *annotator, *ptime;
{
    if (*record) {
	static char *arg[10];
	int nargs;

	arg[0] = "wave";
	arg[1] = "-r";
	arg[2] = record;
	nargs = 3;
	if (*annotator) {
	    arg[nargs++] = "-a";
	    arg[nargs++] = annotator;
	}
	if (*ptime) {
	    arg[nargs++] = "-f";
	    arg[nargs++] = ptime;
	}
	arg[nargs] = NULL;
	/* Send the standard error output to /dev/null.  This avoids having
	   such error output appear as dialog boxes when wavescript is run from
	   Netscape.  WAVE's and the DB library's error messages are unaffected
	   by this choice (since they are handled within WAVE), but the XView
	   library sometimes generates error messages (particularly under
	   Linux, where the message "ttysw_sigwinch, can't get tty process
	   group: Not a typewriter" appears whenever the Analysis Commands
	   window opens or closes).  These may be safely ignored. */
	freopen("/dev/null", "w", stderr);
	return (execve("/usr/local/bin/wave", arg, environ));
#if 0
	char command[128];

	sprintf(command, "wave -r %s", record);
	if (*annotator) sprintf(command+strlen(command)," -a %s", annotator);
	if (*ptime) sprintf(command+strlen(command), " -f '%s'", ptime);
	system(command);
	return (0);
#endif
    }

    else {	/* We can't start WAVE without specifying which record to open
		   -- give up. */
	fprintf(stderr, "%s: no record is open or specified\n", pname);
	return (2);
    }
}

main(argc, argv, env)
int argc;
char **argv, **env;
{
    char fname[30];
    FILE *ofile = NULL, *script;
    int i, pid;
    static char buf[80], record[80], annotator[80], ptime[80];

    pname = argv[0];
    environ = env;
    if (argc < 2 || (script = fopen(argv[1], "r")) == NULL) {
	help();
	exit(1);
    }
    while (fgets(buf, sizeof(buf), script)) {
	if (buf[0] == '-') switch (buf[1]) {
	  case 'a':	/* annotator name follows */
	    strcpy(annotator, buf+3);
	    annotator[strlen(annotator)-1] = '\0';
	    break;
	  case 'f':	/* time follows */
	    strcpy(ptime, buf+3);
	    ptime[strlen(ptime)-1] = '\0';
	    break;
	  case 'r':	/* record name follows */
	    strcpy(record, buf+3);
	    record[strlen(record)-1] = '\0';
	    break;
	}
    }
    fclose(script);

    if (*annotator == '\0' && *ptime == '\0' && *record == '\0') {
	help();
	exit(1);
    }
    if (argc == 4 && strncmp(argv[2], "-p", 2) == 0) {	/* pid specified */
	pid = atoi(argv[3]);
	if (pid == 0) exit(start_new_wave(record, annotator, ptime));
	sprintf(fname, "/tmp/.wave.%d.%d", (int)getuid(), pid);
	if ((ofile = fopen(fname, "r")) == NULL) {
	    fprintf(stderr,
   "You don't seem to have a WAVE process with pid %d.  Please try again.\n",
		    pid);
	    exit(2);
	}
    }
    else {		/* Try to find a running WAVE process. */
	pid = find_wave_pid();
	if (pid) {
	    sprintf(fname, "/tmp/.wave.%d.%d", (int)getuid(), pid);
	    ofile = fopen(fname, "r");	/* attempt to read from file */
	    if (ofile == NULL && pid == find_wave_pid()) {
		/* The mailbox is unreadable -- it may be owned by another
		   user. */
		fprintf(stderr, "Please remove %s and try again.\n", fname);
		exit(3);
	    }
	}
    }

    if (ofile) {	/* We seem to have found a running WAVE process. */
	if (fgetc(ofile) != EOF) {	/* ... but the mailbox isn't empty! */
	    fclose(ofile);
	    sleep(2);			/* give WAVE a chance to empty it */
	    if ((ofile = fopen(fname, "r")) == NULL)
		/* WAVE must have just exited, or someone else cleaned up. */
		exit(start_new_wave(record, annotator, ptime));
	    if (fgetc(ofile) != EOF) {
		/* The mailbox is still full -- WAVE may be stuck, or
		   it may have crashed without removing the mailbox. */
		fclose(ofile);
		unlink(fname);
		fprintf(stderr,
	    "WAVE process %d not responding -- starting new WAVE process\n",
			pid);
		exit(start_new_wave(record, annotator, ptime));
	    }
	}

	/* OK, we've got an empty mailbox -- let's post the message! */
	fclose(ofile);
	ofile = fopen(fname, "w");
	if (*record) fprintf(ofile, "-r %s\n", record);
	if (*annotator) fprintf(ofile, "-a %s\n", annotator);
	if (*ptime) fprintf(ofile, "-f %s\n", ptime);
	fclose(ofile);
    
	kill(pid, SIGUSR1);	/* signal to WAVE that the message is ready */
	exit(0);
    }

    else 
	exit(start_new_wave(record, annotator, ptime));
}
