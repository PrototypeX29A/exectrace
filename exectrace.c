/* ExecTrace by Trent Waddington 11 May 2000

   ExecTrace is a linux only debugging tool.  It uses ptrace to track the
   execution of a child program compiled with debugging info.  To use
   ExecTrace you would do something similar to the following:

     gcc testprog.c -o testprog -ggdb
     exectrace ./testprog 
  
   and ExecTrace will generate a (big) file called "exec.log" which
   contains the source lines of your program as it executes.  You may wish
   to tail -f this file as the program is running (perhaps in another
   window of your X session).  This can be useful when you have a program
   that segfaults for no apparent reason and you want to know 1) where the 
   segfault occured and 2) what code was run leading up to the segfault.     

   This code is distributed under the GNU Public Licence (GPL) version 2.  
   See http://www.gnu.org/ for further details of the GPL.  If you do not
   have a web browser you can read the LICENSE file in this directory.
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <stdio.h>
#include <sys/user.h>

#include <bfd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

bfd *bf;
char lastline[1024];
int verbose_flag = 0;
int resolve_flag = 0;
char *logfile = "exec.log";

int
get_opts (int argc, char **argv)
{
  int c;
  while (1) {
    static struct option long_options[] = {
      /* These options set a flag. */
      {"logfile", required_argument, 0, 'l'},
      {"resolve", no_argument, &resolve_flag, 1},
      {"verbose", no_argument, &verbose_flag, 1},
      {0, 0, 0, 0}
    };
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "l:v", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
	break;
      printf ("option %s", long_options[option_index].name);
      if (optarg)
	printf (" with arg %s", optarg);
      printf ("\n");
      break;

    case 'l':
      logfile = optarg;
      break;

    case 'r':
      resolve_flag = 1;
      break;
    case 'v':
      verbose_flag = 1;
      break;


    case '?':
      /* getopt_long already printed an error message. */
      break;

    default:
      abort ();
    }
  }
}

char *
get_line_for_vma (bfd_vma src)
{
  CONST char *filename;
  CONST char *functionname;
  unsigned int line;
  asection *p;
  static asymbol **syms = NULL;
  FILE *f = 0;
  for (p = bf->sections; p != NULL; p = p->next) {
    /*printf("processing section %s vma %08X size %i\n",p->name,p->vma,p->_cooked_size); */
    if (src >= p->vma && src <= p->vma + p->size) {
      int off = src - p->vma;
      if (bfd_find_nearest_line (bf, p, syms, off, &filename,
				 &functionname, &line)) {
	static char theline[1024];
	int l;
	sprintf (theline, "line %i in function %s of %s", line,
		 functionname, filename);
	if (strncmp (theline, lastline, 1024)) {
	  strncpy (lastline, theline, 1024);
	  if (!resolve_flag) {
	    return theline;
	  }
	  f = fopen (filename, "r");
	  if (!f) {
	    return theline;
	  }
	  for (l = 0; l < line; l++)
	    fgets (theline, 1024, f);
	  fclose (f);
	  strtok (theline, "\r\n");
	  return theline;
	}
      }
    }
  }
  return NULL;
}

int
initfile (char *fname)
{
  bfd_error_type t;
  bfd_init ();
  bf = bfd_openr (fname, 0);
  if (bf) {
    char **matching;
    if (bfd_check_format_matches (bf, bfd_object, &matching)) {
      return 1;
    }

    if (bfd_get_error () == bfd_error_file_ambiguously_recognized) {
      printf ("ambiguously recognized\n");
    }
    bfd_close (bf);
  }
  else {
    t = bfd_get_error ();
    printf ("error %s\n", bfd_errmsg (t));
  }
  return 0;
}

void
main (int argc, char **argv)
{
  int status;
  int pid;
  struct user_regs_struct regs;
  FILE *log;
  get_opts (argc, argv);
  log = fopen (logfile, "w");
  initfile (argv[optind]);
  if ((pid = fork ()) == 0) {
    ptrace (PTRACE_TRACEME, 0, 0, 0);
    if (execvp (argv[optind], argv + optind) < 0) {
      fprintf (stderr, "Could not execute file\n");
      exit (-1);
    };
  }
  wait (&status);
  while (!WIFEXITED (status)) {
    if (WIFSTOPPED (status)) {
      char *line = 0;
      ptrace (PTRACE_GETREGS, pid, 0, &regs);
      line = get_line_for_vma (regs.eip);
      if (line)
	fprintf (log, "%s\n", line);
      ptrace (PTRACE_SINGLESTEP, pid, 0, 0);
    }
    wait (&status);
  }
  fclose (log);
  printf ("exectrace exiting\n");
}
