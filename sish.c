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
#include "sish.h"
#include "parse.h"
#include "builtin.h"

#define PERROR_RETURN(message) \
do { \
	env_question = SISH_EXIT_FAILURE;\
	(void)tcsetpgrp(STDERR_FILENO, getpgrp()); \
	(void)fprintf(stderr, "%s: ", getprogname()); \
	perror(message); \
	return; \
} while(0)

static void exec_command(ARRAYLIST *, BOOL, BOOL);
static void do_exec(PARSED_CMD *, int, int, BOOL);
static void print_prompt();
static void print_trace(PARSED_CMD *);
static void sigint_handler(int);
static void sigchld_handler(int);

static JSTRING *command;
static pid_t env_dollar;
static int env_question;

int 
sish_run(struct sishopt *ssopt)
{
	char c;
	extern JSTRING *command;
	extern pid_t env_dollar;
	extern int env_question;
	int parse_status;
	ARRAYLIST *cmd_list;
	BOOL run_bg;
	
	command = jstr_create("");
	env_dollar = getpid();
	env_question = SISH_EXIT_SUCCESS;
	
	cmd_list = arrlist_create();
	
	if (signal(SIGINT, sigint_handler) == SIG_ERR)
		perror_exit("register SIGINT handler error");
	
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR)
		perror_exit("register SIGCHLD handler error");
	
	if (signal(SIGTTOU, SIG_IGN) == SIG_ERR)
		perror_exit("ignore SIGTTOU signal error");
	
	/* 
	 * If -c is set, sish will run as a command interpreter which 
	 * parses and executes the command, then exit; or, it will run 
	 * as a shell which reads command from stdin.
	 */
	if (ssopt->c_flag == TRUE) {
		/* Parse the command, if error occurs, set %?. */
		parse_status = parse_command(ssopt->command, cmd_list, &run_bg);
		if (parse_status != 0) {
			env_question = parse_status;
			return env_question;
		}
		exec_command(cmd_list, run_bg, ssopt->x_flag);
	} else {
		for (;;) {
			/* Clear what have been read before printing prompt */
			jstr_trunc(command, 0, 0);
			free_pcmd(cmd_list);
			print_prompt();

			/* Read one line command from stdin */
			while ((c = (char)getc(stdin)) != '\n')
				jstr_append(command, c);
			/* Parse the command, if error occurs, set %?. */
			parse_status = parse_command(command, cmd_list, &run_bg);
			if (parse_status != 0) {
				env_question = parse_status;
				continue;
			}
			
			exec_command(cmd_list, run_bg, ssopt->x_flag);
		}
	}
	
	jstr_free(command);
	arrlist_free(cmd_list);
	return env_question;
}

static void
exec_command(ARRAYLIST *cmd_list, BOOL run_bg, BOOL trace_cmd)
{
	size_t cmd_num, i;
	PARSED_CMD *parsed;
	int builtin_result;
	extern pid_t env_dollar;
	extern int env_question;
	pid_t pid, group_leader;
	int left_pipefd[2];
	int right_pipefd[2];
	int fd_in, fd_out;
	siginfo_t infop;
	int exit_status;
	
	cmd_num = arrlist_size(cmd_list);
	group_leader = -1;
	
	/* If the size of cmd_list is 0, directly return. */
	if (cmd_num == 0)
		return;
	
	/* 
	 * If the size of cmd_list is 1, runs in foreground, and 
	 * matches to some built in commands, it should be executed 
	 * directly by current process.
	 */
	if (cmd_num == 1 && run_bg == FALSE) {
		parsed = (PARSED_CMD *)arrlist_get(cmd_list, 0);
		
		if (trace_cmd == TRUE && is_builtin(parsed) == TRUE)
			print_trace(parsed);
		
		builtin_result = call_builtin(
							parsed, 
							STDIN_FILENO, STDOUT_FILENO,
							env_dollar, env_question);
		if (builtin_result != -1) {
			env_question = builtin_result;
			return;
		}
	}
	
	/* 
	 * For each subcommand divided by pipeline, create a subprocess
	 * to execute it.
	 */
	for (i = 0; i < cmd_num; i++) {
		/* 
		 * If this is not the last subcommand, a pipe should be 
		 * generated
		 */
		if (i != cmd_num - 1) {
			if (pipe(right_pipefd) == -1)
				PERROR_RETURN("pipe error");
			
		}
		
		/* Fork a process for each subcommand */
		if ((pid = fork()) == -1)
			PERROR_RETURN("fork error");
		
		if (pid > 0) {
			/* Set the first child process as the process group leader */
			if (i == 0)
				group_leader = pid;
			
			
			/* Add subprocess to its process group */
			if (setpgid(pid, group_leader) == -1)
				PERROR_RETURN("parent setpgid error");
			
			
			/* 
			 * If this process group is running in foreground, connect it
			 * with controlling terminal.
			 */
			if (run_bg == FALSE) {
				if (tcsetpgrp(STDERR_FILENO, group_leader) == -1)
					PERROR_RETURN("tcsetpgrp error");
				
			}
			
			/* 
			 * If there are more than 1 subcommand, and this is not the
			 * first child process's loop, parent should close both sides
			 * of the left pipe.
			 */
			if (cmd_num > 1 && i != 0) {
				(void)close(left_pipefd[0]);
				(void)close(left_pipefd[1]);
			}
		} else {
			/* 
			 * Cancel SIGINT, SIGCHLD, SIGTTOU handler which is inherited 
			 * from parent 
			 */
			if (signal(SIGINT, SIG_DFL) == SIG_ERR)
				perror_exit("cancel SIGINT handler error");
			if (signal(SIGCHLD, SIG_DFL) == SIG_ERR)
				perror_exit("cancel SIGCHLD handler error");
			if (signal(SIGTTOU, SIG_DFL) == SIG_ERR)
				perror_exit("cancel SIGTTOU handler error");
			
			/* 
			 * Child also need to add itself to leader's group to make
			 * sure the subsequent process works in the correct environment
			 */
			if (i == 0)
				group_leader = getpid();
				
			if (setpgid(0, group_leader) == -1)
				perror_exit("child setpgid error");
			
			/*
			 * For the leftmost subcommand, it should read from stdin,
			 * write to right_pipefd[1] and close right_pipefd[0].
			 *
			 * For the rightmost subcommand, it should read from 
			 * left_pipefd[0], write to stdout and close left_pipefd[1].
			 *
			 * For the middle subcommand, it should read from 
			 * left_pipefd[0], write to right_pipefd[1], close 
			 * left_pipefd[1] and close right_pipefd[0].
			 */
			fd_in = STDIN_FILENO;
			fd_out = STDOUT_FILENO;
			if (cmd_num > 1) {
				if (i != 0) {
					fd_in = left_pipefd[0];
					(void)close(left_pipefd[1]);
				}
				if (i != cmd_num - 1) {
					fd_out = right_pipefd[1];
					(void)close(right_pipefd[0]);
				}
			}
			
			/* Execute the command */
			do_exec((PARSED_CMD *)arrlist_get(cmd_list, i), 
					    fd_in, fd_out, trace_cmd);
		}
		
		/* After each loop, the right pipe becomes the left pipe */
		if (cmd_num > 1) {
			left_pipefd[0] = right_pipefd[0];
			left_pipefd[1] = right_pipefd[1];
		}
	}
	
	/* 
	 * If this is a foreground command, sish should wait until
	 * all children has finished, then set itself as the
	 * controlling process.
	 */
	if (run_bg == FALSE) {
		for (;;) {
			if (waitid(P_PGID, group_leader, &infop, WEXITED | WNOWAIT) == -1)
				break;
			pid = waitpid(infop.si_pid, &exit_status, 0);
			if (pid != -1 && pid != 0)
				env_question = WEXITSTATUS(exit_status);
		}
		(void)tcsetpgrp(STDERR_FILENO, getpgrp());
	}
	
}


static void
do_exec(PARSED_CMD *parsed, int fd_in, int fd_out, BOOL trace_command)
{
	size_t i;
	ARRAYLIST *redlist;
	REDIRECT *red;
	int read_fd, write_fd, open_flags;
	char **argv, **ite;
	JSTRING *opt_str;
	int builtin_result;
	extern pid_t env_dollar;
	extern int env_question;
	
	/* If -x is set, write each command to stderr */
	if (trace_command == TRUE && is_builtin(parsed) == TRUE)
		print_trace(parsed);
	/* Try to match the command with builtin function */
	builtin_result = call_builtin(
						parsed, 
						fd_in, fd_out,
						env_dollar, env_question);
	if (builtin_result != -1)
		exit(builtin_result);
	
	read_fd = -1;
	write_fd = -1;
	
	/* Deal with redirection based on pipe */
	if (fd_in != STDIN_FILENO)
		if (dup2(fd_in, STDIN_FILENO) == -1)
			perror_exit("dup2 STDIN_FILENO error");
	if (fd_out != STDOUT_FILENO)
		if (dup2(fd_out, STDOUT_FILENO) == -1)
			perror_exit("dup2 STDOUT_FILENO error");
	
	/* 
	 * If subcommand specify redirection like '<', '>', '>>',
	 * it will override the redirection by pipe.
	 */
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
	MALLOC(argv, char *, arrlist_size(parsed->opt) + 1);
	ite = argv;
	
	for (i = 0; i < arrlist_size(parsed->opt); i++, ite++) {
		opt_str = (JSTRING *)arrlist_get(parsed->opt, i);
		*ite = jstr_cstr(opt_str);
	}
	*ite = NULL;
	
	/* If -x is set, write each command to stderr */
	if (trace_command == TRUE)
		print_trace(parsed);
	
	if (execvp(jstr_cstr(parsed->command), argv) == -1)
		perror_exit("execute command error");
	
	free(argv);
}

static void
print_prompt()
{
	char *cwd;
	
	(void)fprintf(stdout, "sish");
	
	cwd = getcwd(NULL, 0);
	if (cwd != NULL) {
		(void)fprintf(stdout, ":%s", cwd);
		free(cwd);
	}
	
	(void)fprintf(stdout, "$ ");
	
	if (fflush(stdout) == EOF)
		perror_exit("flush stdout error");
	
}

static void
print_trace(PARSED_CMD *parsed)
{
	size_t i;
	JSTRING *trace;
	JSTRING *arg;
	ARRAYLIST *list;
	
	trace = jstr_create("+");
	
	list = parsed->opt;
	for (i = 0; i < arrlist_size(list); i++) {
		jstr_append(trace, ' ');
		arg = (JSTRING *)arrlist_get(list, i);
		jstr_concat(trace, jstr_cstr(arg));
	}
	
	(void)fprintf(stderr,
		"%s\n", jstr_cstr(trace));
	
	jstr_free(trace);
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

/*
 * When receives SIGCHLD signal, this function will be invoked.
 * It first use waitid(2) to get the information of terminated
 * process. If the child process belongs to foreground process
 * group, ignore it; or, call waitpid(2) to reap this child 
 * process.
 */
static void 
sigchld_handler(int signum)
{
	pid_t child_pgid, fg_pgid;
	siginfo_t infop;
	int exit_status;
	extern int env_question;
	
	if (waitid(P_ALL, 0, &infop, WEXITED | WNOWAIT) == -1)
		PERROR_RETURN("sigchld waitid error");
	if ((child_pgid = getpgid(infop.si_pid)) == -1)
		PERROR_RETURN("sigchld getpgid error");
	
	if ((fg_pgid = tcgetpgrp(STDERR_FILENO)) == -1)
		PERROR_RETURN("sigchld tcgetpgrp error");
	
	if (child_pgid == fg_pgid)
		return;
	else {
		if (waitpid(infop.si_pid, &exit_status, 0) == -1) {
			(void)fprintf(stderr, "%s: ", getprogname());
			perror("sigchld waitpid error");
			(void)tcsetpgrp(STDERR_FILENO, getpgrp());
			return;
		}
	}
}

void
perror_exit(char *message)
{
	(void)fprintf(stderr, "%s: ", getprogname());
	perror(message);
	exit(SISH_EXIT_FAILURE);
}