#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jstring.h"
#include "arraylist.h"
#include "macros.h"
#include "func.h"
#include "sish.h"

static void print_prompt();
static void sigint_handler(int);
static void sigchld_handler(int);

static JSTRING *command;
static pid_t env_dollar;
static int env_question;

int 
sish_run(struct sishopt *ssopt)
{
	char c;
	int exit_status;
	extern JSTRING *command;
	extern pid_t env_dollar;
	extern int env_question;
	pid_t pid;
	
	command = jstr_create("");
	env_dollar = getpid();
	env_question = 0;
	
	if (signal(SIGINT, sigint_handler) == SIG_ERR)
		perror_exit("register SIGINT handler error");
	
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
		perror_exit("register SIGCHLD handler error");
	
	for (;;) {
		jstr_trunc(command, 0, 0);
		
		print_prompt();
		while ((c = (char)getc(stdin)) != '\n')
			jstr_append(command, c);
		
		printf("com: %s\n", jstr_cstr(command));
		
		if ((pid = fork()) == -1)
			perror_exit("fork error");
		
		if (pid > 0) {
			// if this is not a background command, then father should wait()
			if (wait(&exit_status) != -1)
				env_question = WEXITSTATUS(exit_status);
		} else {
			//exec_command();
			exit(100);
		}
		
	}
	
	return 0;
}


static void
print_prompt()
{
	(void)fprintf(stdout, SISH_PROMPT);
	if (fflush(stdout) == EOF)
		perror_exit("flush stdout error");
}

/*
 * When receives SIGINT(Ctrl-c) signal, this function will
 * be invoked. It discards what have been read from stdin
 * before and start a new line with prompt.
 */
static void 
sigint_handler(int signum)
{
	extern JSTRING *command;
	
	jstr_trunc(command, 0, 0);
	
	(void)fprintf(stdout, "\n");
	print_prompt();
}

static void 
sigchld_handler(int signum)
{
	int exit_status;
	extern int env_question;
	
	if (wait(&exit_status) != -1)
		env_question = WEXITSTATUS(exit_status);
}