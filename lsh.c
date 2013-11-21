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
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"

/*
 * Function declarations
 */

int  NumberOfCommands(Command);
int  StartPipeline(Pgm *);
int  RunPipeline(Pgm *, int, int);
void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);

/* When non-zero, this global means the user is done using this program. */
int done = 0;

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
  int num;
  
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
        /* PrintCommand(n, &cmd); */
	
	/* printf("Executing cool command %s!\n", cmd.pgm->pgmlist[0]); */

	StartPipeline(cmd.pgm);
	num = NumberOfCommands(cmd);
	printf("Number of commands: %d\n", num);

      }
    }
    
    if(line) {
      free(line);
    }
  }
  return 0;
}

int 
StartPipeline(Pgm *p)
{
  pid_t pid;
  if ( (pid = fork()) == 0) {
    RunPipeline(p, 0, -1);
    exit(-1);    
  }
  else {
    waitpid(pid, NULL, 0);
  }
  return 0;
}


/*
 * Name: RunPipeline
 *
 * Description: An auxilliary function to `StartPipeline()', takes care
 *   of actually running the pipeline.
 *
 * Arguments:
 *   p      --- The pipeline.
 *   cmdNum --- The number of the command starting from last = 0, penultimate = 1, …
 *
 */
int
RunPipeline(Pgm *p, int cmdNum, int pipeWrite)
{
  pid_t  pid;
  char **program;
  int    fd[2];
  int    firstCmd;
  int    lastCmd;

  firstCmd = p->next == NULL;
  lastCmd  = cmdNum  == 0;
  
  /* We don't create a new pipe for the first command. */
  if (!firstCmd) {
    printf("+ CREATE PIPE\n");
    pipe(fd);
    RunPipeline(p->next, cmdNum + 1, fd[0]);
  }

  program = p->pgmlist;

  /* Last command is not forked */
  if (lastCmd) {

    /* More than a single command! */
    if (!firstCmd) {
      printf("+ LAST: DUPLICATE FD[0] TO 0 (READ, STDIN)\n");
      dup2(fd[0],  0);
    }
    
    printf("+ LAST \"%s\"\n", p->pgmlist[0]);
    execvp(program[0], program);
    exit(EXIT_SUCCESS);
  }

  if ( (pid = fork()) == 0) {
    if (firstCmd) {
      printf("+ FIRST: DUPLICATE pipeWrite TO 1 (WRITE, STDOUT)\n");
      dup2(pipeWrite, 1);
      printf("+ %s \"%s\"\n", "FIRST", program[0]);
      execvp(program[0], p->pgmlist);
      exit(0);
    }
    /* Middle commands (neither first nor last) */
    else {
      printf("+ MEDIA: CLOSE fd[1]\n");
      close(fd[1]);
      printf("+ MEDIA: DUPLICATE fd[0] TO 0 (READ, STDIN)\n");
      dup2(fd[0],  0);
      printf("+ MEDIA: DUPLICATE pipeWrite TO 1 (WRITE, STDOUT)\n");
      dup2(pipeWrite, 1);
      printf("+ %s \"%s\"\n", "EXEC", program[0]);
      execvp(program[0], p->pgmlist);
      exit(0);
    }
  }
  
  return 1;
}

/* void */
/* RunPipeline(Pgm *p, int n, int pipefd[2]) */
/* { */
/*   if (p == NULL) { */
/*     return; */
/*   } */

/*   pid_t pid; */

/*   char **pl = p->pgmlist; */
/*   RunPipeline(p->next, n + 1, NULL); */
/*   printf("Running: %s\n", pl[0]); */

/*   if ( (pid = fork()) < 0) { */
/*     perror("fork"); */
/*     exit(EXIT_FAILURE); */
/*   } */

/*   else if (pid == 0) {		/\* child *\/ */
/*     execvp(pl[0], pl); */
/*     perror("execlp"); */
/*     exit(EXIT_FAILURE); */
/*   } */

/*   else {			/\* parent *\/ */
/*     wait(NULL); */
/*   } */

/*   return; */
/* } */

int
NumberOfCommands(Command cmd)
{
  int total = 0;
  Pgm *inter;
  for (inter = cmd.pgm;
       inter != NULL;
       inter = inter->next) 
    total++;
  return total;
}

/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void
PrintCommand (int n, Command *cmd)
{
  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void
PrintPgm (Pgm *p)
{
  if (p == NULL) {
    return;
  }
  else {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    PrintPgm(p->next);
    printf("    [");
    while (*pl) {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
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
