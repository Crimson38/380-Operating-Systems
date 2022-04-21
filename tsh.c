/*
 * tsh - A tiny shell program
 *
 * DISCUSS YOUR IMPLEMENTATION HERE!
 * For my SIGCHLD handler I am using waitpid to reap the incoming zombie child processes.
 * I am also using the option WUNTRACED for waitpid to catch processes that are being stopped 
 * by a SIGSTOP or SIGTSTP signal. If a foreground job triggers the SIGCHLD handler set 
 * runningpid to 0. Then if the foreground process was terminated or stopped 
 * print a string about the job. For my sigInt handler I am making sure that there is a 
 * running foreground process then if there is killpg will send a sigInt to the foreground 
 * process. My sigstp handler checks to make sure there's a foreground process and if there is 
 * killpg will send a SIGTSTP to the foreground process and set suspendedPid to runningPid.
 * When a foreground job is run my evaluate method calls waitfg (pid_t pid). waitfg sets 
 * runngingpid to the pid of the new process. I then suspend the parent process with 
 * sigsuspend(&prev) till the runningpid is finished and set to 0.   
 */

/*
 *******************************************************************************
 * INCLUDE DIRECTIVES
 *******************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 *******************************************************************************
 * TYPE DEFINITIONS
 *******************************************************************************
 */

typedef void handler_t (int);

/*
 *******************************************************************************
 * PREPROCESSOR DEFINITIONS
 *******************************************************************************
 */

// max line size 
#define MAXLINE 1024 
// max args on a command line 
#define MAXARGS 128

/*
 *******************************************************************************
 * GLOBAL VARIABLES
 *******************************************************************************
 */

// defined in libc
extern char** environ;   

// command line prompt 
char prompt[] = "tsh> ";

// for composing sprintf messages
char sbuf[MAXLINE];

// PID of the foreground job's leader, or 0 if there is no foreground job
volatile pid_t g_runningPid = 0;
// PID of the suspended job's leader, or 0 if there is no suspended job
volatile pid_t g_suspendedPid = 0; 

// 
sigset_t mask, prev;

/*
 *******************************************************************************
 * FUNCTION PROTOTYPES
 *******************************************************************************
 */
void
evaluate (char* cmdline);

void
waitfg ();

int 
builtin_cmd (char** args);

int
parseline (const char* cmdline, char**argv);

void
sigint_handler (int sig);

void
sigtstp_handler (int sig);

void
sigchld_handler (int sig);

void
sigquit_handler (int sig);

void
unix_error (char* msg);

void
app_error (char* msg);

handler_t*
Signal (int signum, handler_t* handler);

/*
 *******************************************************************************
 * MAIN
 *******************************************************************************
 */

int
main (int argc, char** argv)
{
  /* Redirect stderr to stdout */
  dup2 (1, 2);

  /* Install signal handlers */
  /* ctrl-c */
  Signal (SIGINT, sigint_handler);
  Signal (SIGTSTP, sigtstp_handler); /* ctrl-z */
  /* Terminated or stopped child */
  Signal (SIGCHLD, sigchld_handler);
  Signal (SIGQUIT, sigquit_handler); /* quit */

  /* TODO -- shell goes here*/
  while (1)
  {
    printf(prompt);
    fgets(sbuf, MAXLINE, stdin);
    if (feof(stdin))
    {
      // printf("\n");
      break;
    }
    evaluate(sbuf);
  }

  return 0;
  /* Quit */
  exit (0);
}

/*
*
*/
void
evaluate(char* cmdline)
{
  char *args[MAXARGS];
  int bg = parseline(cmdline, args);  

  if (args[0] == NULL)
    return;


  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  int cmd = builtin_cmd(args);
  if (cmd)
    return;

  pid_t pid = fork();
  
  if (pid < 0)
  {
    fprintf(stderr, "fork error (%s) -- exiting\n",
        strerror(errno));
    exit(1);
  }

  if (pid == 0)
  {
    if (setpgid(0, 0) < 0)
      printf("error\n");
    
    if (execvp(args[0], args) < 0 )
    {
      cmdline[strcspn(cmdline, "\n")] = 0;
      printf("%s: Command not found\n", cmdline);
      exit(1);
    }
    
  }

  if (!bg)
    waitfg(pid);
  else  
  {
    
    sigprocmask(SIG_BLOCK, &mask, &prev);

    printf("(%d) %s", pid, cmdline);
    
    fflush(stdout);
    sigprocmask(SIG_SETMASK, &prev, NULL);
  }

}

void 
waitfg (pid_t pid)
{
  g_runningPid = pid;
  
  while(g_runningPid)
  {
    sigsuspend(&prev); 
  }
}

int 
builtin_cmd (char **args)
{
  if (strcmp(args[0], "quit") == 0)
    exit(0);
  else if (strcmp(args[0], "fg") == 0)
  {
    killpg(g_suspendedPid, SIGCONT);
    g_runningPid = g_suspendedPid;
    g_suspendedPid = 0;
    waitfg(g_runningPid);
    return 1;
  }
  return 0; 
}

/*
*  parseline - Parse the command line and build the argv array.
*
*  Characters enclosed in single quotes are treated as a single
*  argument.
*
*  Returns true if the user has requested a BG job, false if
*  the user has requested a FG job.
*/
int
parseline (const char* cmdline, char** argv)
{
  static char array[MAXLINE]; /* holds local copy of command line*/
  char* buf = array;          /* ptr that traverses command line*/
  char* delim;                /* points to first space delimiter*/
  int argc;                   /* number of args*/
  int bg;                     /* background job?*/

  strcpy (buf, cmdline);
  buf[strlen (buf) - 1] = ' ';  /* replace trailing '\n' with space*/
  while (*buf && (*buf == ' ')) /* ignore leading spaces*/
    buf++;

  /* Build the argv list*/
  argc = 0;
  if (*buf == '\'')
  {
    buf++;
    delim = strchr (buf, '\'');
  }
  else
  {
    delim = strchr (buf, ' ');
  }

  while (delim)
  {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces*/
      buf++;

    if (*buf == '\'')
    {
      buf++;
      delim = strchr (buf, '\'');
    }
    else
    {
      delim = strchr (buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line*/
    return 1;

  /* should the job run in the background?*/
  if ((bg = (*argv[argc - 1] == '&')) != 0)
  {
    argv[--argc] = NULL;
  }
  return bg;
}

/*
 *******************************************************************************
 * SIGNAL HANDLERS
 *******************************************************************************
 */

/*
*  sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
*      a child job terminates (becomes a zombie), or stops because it
*      received a SIGSTOP or SIGTSTP signal. The handler reaps all
*      available zombie children, but doesn't wait for any other
*      currently running children to terminate.
*/
void
sigchld_handler (int sig)
{
  int child_status;
  pid_t pid;
  while ((pid = waitpid(-1, &child_status, WUNTRACED)) > 0)
  {
    if (WIFEXITED(child_status))
    {
      g_runningPid = 0;
      return;
    }
    else if (WIFSIGNALED(child_status))
    {
      if (pid == g_runningPid)
      {
        printf("Job (%d) terminated by signal %d\n", pid, WTERMSIG(child_status));
        fflush(stdout);
        g_runningPid = 0;
        return;
      }
    }
    else if (WIFSTOPPED(child_status))
    {
      if (pid == g_suspendedPid)
      {
        printf("Job (%d) stopped by signal %d\n", pid, WSTOPSIG(child_status));
        fflush(stdout);
        g_runningPid = 0;
        return;
      }
    }
    else
    { 
      printf("Child terminated abnormally\n");   
      fflush(stdout);  
      g_runningPid = 0;      
      return;
    }
  }
}

/*
 *  sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *     user types ctrl-c at the keyboard.  Catch it and send it along
 *     to the foreground job.
 */
void
sigint_handler (int sig)
{  
  if (g_runningPid != 0)
    killpg(g_runningPid, sig);
}

/*
 *  sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *      the user types ctrl-z at the keyboard. Catch it and suspend the
 *      foreground job by sending it a SIGTSTP.
 */
void
sigtstp_handler (int sig)
{
  if(g_runningPid != 0)
  {
    killpg(g_runningPid, sig);
    g_suspendedPid = g_runningPid;
  }
}

/*
 *******************************************************************************
 * HELPER ROUTINES
 *******************************************************************************
 */


/*
 * unix_error - unix-style error routine
 */
void
unix_error (char* msg)
{
  fprintf (stdout, "%s: %s\n", msg, strerror (errno));
  exit (1);
}

/*
*  app_error - application-style error routine
*/
void
app_error (char* msg)
{
  fprintf (stdout, "%s\n", msg);
  exit (1);
}

/*
*  Signal - wrapper for the sigaction function
*/
handler_t*
Signal (int signum, handler_t* handler)
{
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset (&action.sa_mask); /* block sigs of type being handled*/
  action.sa_flags = SA_RESTART;  /* restart syscalls if possible*/

  if (sigaction (signum, &action, &old_action) < 0)
    unix_error ("Signal error");
  return (old_action.sa_handler);
}

/*
*  sigquit_handler - The driver program can gracefully terminate the
*     child shell by sending it a SIGQUIT signal.
*/
void
sigquit_handler (int sig)
{
  printf ("Terminating after receipt of SIGQUIT signal\n");
  exit (1);
}
