/* 
 * Main source code file for lsh shell program.
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file 
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive: 
      $> tar cvf lab1.tgz lab1/
 *
 * All the best 
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"

/*
 * Function declarations
 */

void StartPipeline(Command);
void RunPipeline(Pgm *, Command, int, int);
void stripwhite(char *);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

static void catch_function(int signo) {
  putchar('\n');
  return;
}

/*
 * Name: main
 *
 * Description: Gets the ball rolling...
 *
 */
int main(void)
{
  Command cmd;
  int n;
  /* int num; */

  if (signal(SIGINT, catch_function) == SIG_ERR) {
    perror("signal");
    exit(EXIT_FAILURE);
  }
  
  while (!done) {

    char *line;
    line = readline("> ");

    if (!line) {
      /* Encountered EOF at top level */
      done = 1;
    }
    else {
      /*
       * Remove leading and trailing whitespace from the line
       * Then, if there is anything left, add it to the history list
       * and execute it.
       */
      stripwhite(line);

      if (*line) {
        add_history(line);
	
        /* execute it */
        n = parse(line, &cmd);

	StartPipeline(cmd);
      }
    }
    
    if(line) {
      free(line);
    }
  }
  return 0;
}

/*
 * Name: StartPipeline
 *
 * Description: Runs command.
 *
 * Arguments:
 *   cmd    --- The command to run.
 */
void 
StartPipeline(Command cmd)
{
  pid_t pid;

  /* `exit' and `cd' must be shell-builtins. */
  if (strcmp(cmd.pgm->pgmlist[0], "exit") == 0) { done = 1; }
  if (strcmp(cmd.pgm->pgmlist[0], "cd")   == 0) {
    if (chdir(cmd.pgm->pgmlist[1] == NULL
	      ? getenv("HOME")
	      : cmd.pgm->pgmlist[1]) < 0) {
      perror("cd");
    }
    return;
  }

  /*
    We run the entire pipeline as a single child process which then
    forks n-1 times for n commands (n-1 grandchildren), leaving the
    child of this process to run the final command.
   */
  if ( (pid = fork()) == 0) {
    RunPipeline(cmd.pgm, cmd, 0, -1);
    exit(EXIT_FAILURE);
  }
  else {
    /* We don't wait for backgrounded commands */
    if (!cmd.bakground) {
      waitpid(pid, NULL, 0);
    }
  }
  
  return;
}


/*
 * Name: RunPipeline
 *
 * Description: An auxilliary function to `StartPipeline()', takes care
 *   of actually running the pipeline.
 *
 * Arguments:
 *   p         --- The pipeline.
 *   cmd       --- Command including redirections and backgrounding.
 *   cmdNum    --- Command number starting from last = 0, penultimate = 1, etc.
 *   pipeWrite --- The end of the pipe to communicate with the next command.
 */
void
RunPipeline(Pgm *p, Command cmd, int cmdNum, int pipeWrite)
{
  pid_t   pid;
  char  **program;
  int     fd[2];
  int     firstCmd;
  int     lastCmd;
  int     stdinRedirect;
  int     stdinFile;
  int     stdoutRedirect;
  int     stdoutFile;

  firstCmd       = p->next == NULL;
  lastCmd        = cmdNum  == 0;
  stdinRedirect  = cmd.rstdin  && firstCmd;
  stdoutRedirect = cmd.rstdout && lastCmd;
  stdinFile      = -1;
  stdoutFile     = -1;
  
  program  = p->pgmlist;
  
  /* We don't create a new pipe for the first command. */
  if (!firstCmd) {
    if (pipe(fd) < 0)
      { perror("pipe"); exit(EXIT_FAILURE); }
    RunPipeline(p->next, cmd, cmdNum + 1, fd[1]);
  }

  /* Redirect standard input by replacing STDIN of first command with a file  */
  if (stdinRedirect) {
    if ( (stdinFile = open(cmd.rstdin, O_RDONLY)) < 0)
      { perror("open"); exit(EXIT_FAILURE); }
    if (dup2(stdinFile, 0) < 0)
      { perror("dup2"); exit(EXIT_FAILURE); }
  }

  /* Redirect standard output by replacing STDOUT of last command with a file  */
  if (stdoutRedirect) {
    if ( (stdoutFile = open(cmd.rstdout, O_WRONLY|O_CREAT, 0666)) < 0)
      { perror("open"); exit(EXIT_FAILURE); }
    if (dup2(stdoutFile, 1) < 0)
      { perror("dup2"); exit(EXIT_FAILURE); }
  }

  /* Last command is not forked */
  if (lastCmd) {
    /* More than a single command! */
    if (!firstCmd) {
      if (dup2(fd[0], 0) < 0)
	{ perror("dup2"); exit(EXIT_FAILURE); }
    }
    
    execvp(program[0], program);
    fprintf(stderr, "Failure running `%s'.\n", program[0]);
    exit(EXIT_FAILURE);
  }

  if ( (pid = fork()) == 0) {
    if (firstCmd) {
      if (dup2(pipeWrite, 1) < 0)
	{ perror("dup2"); exit(EXIT_FAILURE); }
      execvp(program[0], program);
      fprintf(stderr, "Failure running `%s'.\n", program[0]);
      exit(EXIT_FAILURE);
    }
    /* Middle commands (neither first nor last) */
    else {
      
      if (dup2(fd[0], 0) < 0)
	{ perror("dup2"); exit(EXIT_FAILURE); }
      if (dup2(pipeWrite, 1) < 0)
	{ perror("dup2"); exit(EXIT_FAILURE); }

      execvp(program[0], program);
      fprintf(stderr, "Failure running `%s'.\n", program[0]);
      exit(EXIT_FAILURE);
    }
  }

  /* Clean up. */
  if (stdinRedirect)  {
    if (close(stdinFile) < 0) 
      { perror("close"); exit(EXIT_FAILURE); }
  }
  if (stdoutRedirect) {
    if (close(stdoutFile) < 0) 
      { perror("close"); exit(EXIT_FAILURE); }
  }
  
  if (close(pipeWrite) < 0)
    { perror("close"), exit(EXIT_FAILURE); }
  
  return;
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void
stripwhite (char *string)
{
  register int i = 0;

  while (whitespace( string[i] )) {
    i++;
  }
  
  if (i) {
    strcpy (string, string + i);
  }

  i = strlen( string ) - 1;
  while (i> 0 && whitespace (string[i])) {
    i--;
  }

  string [++i] = '\0';
}
