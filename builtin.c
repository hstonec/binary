#include <bsd/stdlib.h>

#include <sys/stat.h>
#include <bsd/stdlib.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jstring.h"
#include "arraylist.h"
#include "macros.h"
#include "sish.h"
#include "parse.h"
#include "builtin.h"

static int sish_cd(PARSED_CMD *);
static int sish_echo(PARSED_CMD *, int, pid_t, int);
static int sish_exit();

static int write2fd(int, char *, size_t);

int call_builtin(PARSED_CMD *parsed, int fd_in, int fd_out,
                 pid_t env_dollar, int env_question)
{
	JSTRING *cmd;
	
	cmd = parsed->command;
	if (jstr_equals(cmd, "cd") == 0)
		return sish_cd(parsed);
	else if (jstr_equals(cmd, "echo") == 0)
		return sish_echo(parsed, fd_out, env_dollar, env_question);
	else if (jstr_equals(cmd, "exit") == 0)
		return sish_exit();
	else
		return -1;
}

static int 
sish_cd(PARSED_CMD *parsed)
{
	int errnum;
	uid_t uid;
	struct passwd *pw;
	char *dir;
	JSTRING *arg;
	char *cwd, *oldcwd;
	
	oldcwd = getcwd(NULL, 0);
	if (oldcwd == NULL) {
		errnum = errno;
		(void)fprintf(stderr,
			"%s: getcwd: %s\n",
			getprogname(),
			strerror(errnum));
		
		return errnum;
	}
	
	/* 
	 * If target directory is not specified, change to the user’s home
	 * directory. 
	 */
	if (arrlist_size(parsed->opt) < 2) {
		uid = getuid();
		errno = 0;
		pw = getpwuid(uid);
		errnum = errno;
		if (pw == NULL) {
			(void)fprintf(stderr,
				"%s: cd: %s\n",
				getprogname(),
				strerror(errnum));
			
			free(oldcwd);
			return errnum;
		}
		dir = pw->pw_dir;
	} else {
		arg = (JSTRING *)arrlist_get(parsed->opt, 1);
		dir = jstr_cstr(arg);
	}
	
	if (chdir(dir) == -1) {
		errnum = errno;
		(void)fprintf(stderr,
			"%s: cd: %s: %s\n",
			getprogname(),
			dir,
			strerror(errnum));
		
		free(oldcwd);
		return errnum;
	}
	
	/* On success, should modify environment variable PWD as well */
	cwd = getcwd(NULL, 0);
	if (cwd == NULL) {
		errnum = errno;
		(void)fprintf(stderr,
			"%s: getcwd: %s\n",
			getprogname(),
			strerror(errnum));
		
		free(oldcwd);
		return errnum;
	}
	if (setenv("PWD", cwd, TRUE) == -1) {
		errnum = errno;
		(void)fprintf(stderr,
			"%s: set PWD error: %s\n",
			getprogname(),
			strerror(errnum));
		
		free(oldcwd);
		free(cwd);
		return errnum;
	}
	if (setenv("OLDPWD", oldcwd, TRUE) == -1) {
		errnum = errno;
		(void)fprintf(stderr,
			"%s: set OLDPWD error: %s\n",
			getprogname(),
			strerror(errnum));
		
		free(oldcwd);
		free(cwd);
		return errnum;
	}
	
	free(oldcwd);
	free(cwd);
	return SISH_EXIT_SUCCESS;
}

static int
sish_echo(PARSED_CMD *parsed, int fd_out,
	      pid_t env_dollar, int env_question)
{
	size_t i;
	ARRAYLIST *redlist, *optlist;
	REDIRECT *red;
	int write_fd, open_flags, write_result;
	int errnum;
	JSTRING *opt_str;
	char dollar[20];
	char question[20];
	JSTRING *output;

	/* 
	 * Since echo could include '>' or '>>' to redirect
	 * its output, it's necessary to check it.
	 */
	write_fd = -1;
	redlist = parsed->redirect_list;
	for (i = 0; i < arrlist_size(redlist); i++) {
		red = (REDIRECT *)arrlist_get(redlist, i);
		if (red->type == REDIR_STDOUT ||
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
			if (write_fd == -1) {
				errnum = errno;
				(void)fprintf(stderr,
					"%s: could not open '%s': %s\n",
					getprogname(),
					jstr_cstr(red->filename),
					strerror(errnum));
				
				return errnum;
			}
			
			fd_out = write_fd;
		}
	}
	
	/* Transform $$ and $? to character string */
	(void)sprintf(dollar, "%d", env_dollar);
	(void)sprintf(question, "%d", env_question);
	
	output = jstr_create("");
	optlist = parsed->opt;
	for (i = 1; i < arrlist_size(optlist); i++) {
		opt_str = (JSTRING *)arrlist_get(optlist, i);
		if (jstr_equals(opt_str, "$$") == 0)
			jstr_concat(output, dollar);
		else if (jstr_equals(opt_str, "$?") == 0)
			jstr_concat(output, question);
		else
			jstr_concat(output, jstr_cstr(opt_str));
		
		if (i != arrlist_size(optlist) - 1)
			jstr_append(output, ' ');
	}
	
	/* 'echo' always ends with a '\n' */
	jstr_append(output, '\n');
	
	write_result = write2fd(fd_out, jstr_cstr(output), jstr_length(output));
	if (write_result != 0) {
		if (write_fd != -1)
			(void)close(write_fd);
		jstr_free(output);
		return write_result;
	}
	
	if (write_fd != -1)
		(void)close(write_fd);
	jstr_free(output);
	return SISH_EXIT_SUCCESS;
}

static int
sish_exit()
{
	exit(SISH_EXIT_SUCCESS);
}

static int
write2fd(int fd, char *buf, size_t len)
{
	ssize_t count;
	int errnum;
	
	count = 0;
	while ((count = write(fd, buf, len)) > 0) {
		if (count < len) {
			len -= count;
			buf += count;
		} else
			break;
	}
	if (count == -1) {
		errnum = errno;
		(void)fprintf(stderr,
			"%s: echo: %s\n",
			getprogname(),
			strerror(errnum));
		return errnum;
	}
	
	return 0;
}
