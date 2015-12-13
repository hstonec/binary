#include <bsd/stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jstring.h"
#include "arraylist.h"
#include "macros.h"
#include "func.h"
#include "sish.h"
#include "parse.h"

static void exec_command(JSTRING *);
static void do_exec(PARSED_CMD *, int, int);
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
		} else
			exec_command(command);

	}
	
	return 0;
}

static void
exec_command(JSTRING *cmd)
{
	size_t i, last;
	int parse_status;
	ARRAYLIST *cmd_list;
	int left_pipefd[2];
	int right_pipefd[2];
	int fd_in, fd_out;
	pid_t pid;
	
	cmd_list = arrlist_create();
	pid = 0;
	
	parse_status = parse_command(cmd, cmd_list);
	if (parse_status != 0)
		exit(parse_status);
	
	/* 
	 * If the size of cmd_list is 0, that means the command
	 * is null. So, exit without changing $?. 
	 */
	if (arrlist_size(cmd_list) == 0)
		exit(env_question);
	
	last = arrlist_size(cmd_list) - 1;
	for (i = 0; i <= last; i++) {
		fd_in = STDIN_FILENO;
		fd_out = STDOUT_FILENO;
		
		if (i != last)
			pipe(right_pipefd);
		/* 
		 * If it's not the first subcommand, it should read from its left pipe 
		 * line.
		 */
		if (i != 0)
			fd_in = left_pipefd[0];
		/*
		 * If it's not the last subcommand, it should write to its right pipe
		 * line.
		 */
		if (i != last)
			fd_out = right_pipefd[1];
		
		if (i != last) {
			if ((pid = fork()) == -1)
				perror_exit("fork error");
			
			if (pid == 0) {
				if (i != 0) {
					(void)close(left_pipefd[0]);
					(void)close(left_pipefd[1]);
				}
			} else
				do_exec((PARSED_CMD *)arrlist_get(cmd_list, i), 
					    fd_in, fd_out);
		} else
			do_exec((PARSED_CMD *)arrlist_get(cmd_list, i), 
					fd_in, fd_out);
		
		/* 
		 * Before deal with next subcommand, move the pipe on the right
		 * to left.
		 */
		left_pipefd[0] = right_pipefd[0];
		left_pipefd[1] = right_pipefd[1];
		
	}
	
	arrlist_free(cmd_list);
}

static void
do_exec(PARSED_CMD *parsed, int fd_in, int fd_out)
{
	size_t i;
	ARRAYLIST *redlist;
	REDIRECT *red;
	int read_fd, write_fd, open_flags;
	char **argv, **ite;
	JSTRING *opt_str;
	
	read_fd = -1;
	write_fd = -1;
	
	if (fd_in != STDIN_FILENO)
		if (dup2(fd_in, STDIN_FILENO) == -1)
			perror_exit("dup2 STDIN_FILENO error");
	
	if (fd_out != STDOUT_FILENO)
		if (dup2(fd_out, STDOUT_FILENO) == -1)
			perror_exit("dup2 STDOUT_FILENO error");
	
	redlist = parsed->redirect_list;
	for (i = 0; i < arrlist_size(redlist); i++) {
		red = (REDIRECT *)arrlist_get(redlist, i);
		if (red->type == REDIR_STDIN) {
			/* 
			 * If read_fd != -1, it means another reading file has been
 			 * opened before. So, it must close the previous fd first.
			 */
			if (read_fd != -1)
				(void)close(read_fd);
			
			read_fd = open(jstr_cstr(red->filename), O_RDONLY);
			if (read_fd == -1)
				perror_exit("open file error");
			if (dup2(read_fd, STDIN_FILENO) == -1)
				perror_exit("dup2 STDIN_FILENO error");
			
		} else if (red->type == REDIR_STDOUT ||
				   red->type == REDIR_STDAPPEND) {
					   
			/* Close the file opened before if it exists. */
			if (write_fd != -1)
				(void)close(write_fd);
			
			open_flags = O_WRONLY | O_CREAT;
			if (red->type == REDIR_STDOUT)
				open_flags = open_flags | O_TRUNC;
			else // red->type == REDIR_STDAPPEND
				open_flags = open_flags | O_APPEND;
			
			write_fd = open(jstr_cstr(red->filename), 
					  open_flags,
					  S_IRUSR | S_IWUSR);
			if (write_fd == -1)
				perror_exit("open file error");
			if (dup2(write_fd, STDOUT_FILENO) == -1)
				perror_exit("dup2 STDOUT_FILENO error");
			
		} else {
			(void)fprintf(stderr,
				"%s: wrong redirect type\n",
				getprogname());
			exit(SISH_EXIT_FAILURE);
		}
	}
	
	/* Initialize the argument list */
	MALLOC(argv, char *, arrlist_size(parsed->opt) + 2);
	ite = argv;
	
	*ite = jstr_cstr(parsed->command);
	ite++;
	
	printf("size:   %zu\n", arrlist_size(parsed->opt));
	
	for (i = 0; i < arrlist_size(parsed->opt); i++, ite++) {
		opt_str = (JSTRING *)arrlist_get(parsed->opt, i);
		*ite = jstr_cstr(opt_str);
		
		printf("opt:   %s\n", *ite);
	}
	*ite = NULL;
	
	if (execvp(jstr_cstr(parsed->command), argv) == -1)
		perror_exit("execute command error");
	
	free(argv);
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