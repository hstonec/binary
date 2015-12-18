/*
 * This program include the built in functions that are
 * supported by sish. Importantly, there are only two
 * functions, is_builtin() and call_builtin(), which are 
 * exposed to the user. This is an application of Factory 
 * Pattern which can separate invocation code and 
 * implementation code. For built in functions, this pattern
 * improves expansibility because you don't need to know
 * the concrete function name or worry about adding a new
 * function to sish.
 */
#ifdef _LINUX_
	#include <bsd/stdlib.h>
#endif

#include <sys/stat.h>
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
static int sish_echo(PARSED_CMD *, int);
static int sish_exit();

static int write2fd(int, char *, size_t);

/*
 * This function check if passed command is a built in
 * function.
 */
BOOL is_builtin(PARSED_CMD *parsed)
{
	JSTRING *cmd;
	
	cmd = parsed->command;
	if (jstr_equals(cmd, "cd") == 0)
		return TRUE;
	else if (jstr_equals(cmd, "echo") == 0)
		return TRUE;
	else if (jstr_equals(cmd, "exit") == 0)
		return TRUE;
	else
		return FALSE;
}

/*
 * This function calls the actual built in implementation
 * function based on the passed command; or, return -1
 * if doesn't match any built in function.
 */
int call_builtin(PARSED_CMD *parsed, int fd_in, int fd_out)
{
	JSTRING *cmd;
	
	cmd = parsed->command;
	if (jstr_equals(cmd, "cd") == 0)
		return sish_cd(parsed);
	else if (jstr_equals(cmd, "echo") == 0)
		return sish_echo(parsed, fd_out);
	else if (jstr_equals(cmd, "exit") == 0)
		return sish_exit();
	else
		return -1;
}

/*
 * This function implements 'cd' command. Actually, this function
 * just invokes chdir(2) to change the current working directory
 * of current process. So, if you don't call it in the process
 * of shell itself, it won't affect the shell's environment.
 *
 * In addition, this function maintains two environment variables
 * PWD and OLDPWD, each time it changes the current working directory
 * it will also modify these two variables.
 */
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
			"-%s: getcwd: %s\n",
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
				"-%s: cd: %s\n",
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
			"-%s: cd: %s: %s\n",
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
			"-%s: getcwd: %s\n",
			getprogname(),
			strerror(errnum));
		
		free(oldcwd);
		return errnum;
	}
	if (setenv("PWD", cwd, TRUE) == -1) {
		errnum = errno;
		(void)fprintf(stderr,
			"-%s: set PWD error: %s\n",
			getprogname(),
			strerror(errnum));
		
		free(oldcwd);
		free(cwd);
		return errnum;
	}
	if (setenv("OLDPWD", oldcwd, TRUE) == -1) {
		errnum = errno;
		(void)fprintf(stderr,
			"-%s: set OLDPWD error: %s\n",
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

/*
 * This function implements 'echo' command. 'echo' accepts
 * multiple arguments and print them out to stdout ending
 * with a '\n'.
 */
static int
sish_echo(PARSED_CMD *parsed, int fd_out)
{
	size_t i;
	ARRAYLIST *redlist, *optlist;
	REDIRECT *red;
	int write_fd, open_flags, write_result;
	int errnum;
	JSTRING *opt_str;
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
					"-%s: could not open '%s': %s\n",
					getprogname(),
					jstr_cstr(red->filename),
					strerror(errnum));
				
				return errnum;
			}
			
			fd_out = write_fd;
		}
	}
	
	output = jstr_create("");
	optlist = parsed->opt;
	for (i = 1; i < arrlist_size(optlist); i++) {
		opt_str = (JSTRING *)arrlist_get(optlist, i);
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

/*
 * This function implements 'exit' command. It just calls exit(3)
 * to exit from current process.
 */
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
			"-%s: echo: %s\n",
			getprogname(),
			strerror(errnum));
		return errnum;
	}
	
	return 0;
}
