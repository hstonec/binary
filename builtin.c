#include <bsd/stdlib.h>
#include <sys/types.h>

#include <errno.h>
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

int call_builtin(PARSED_CMD *parsed, int fd_in, int fd_out)
{
	JSTRING *cmd;
	
	cmd = parsed->command;
	if (jstr_equals(cmd, "cd") == 0)
		return sish_cd(parsed);
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
		
		return errnum;
	}
	
	return SISH_EXIT_SUCCESS;
}

static int
sish_echo(PARSED_CMD *parsed, int fd_out)
{
	return 0;
}

static int
sish_exit()
{
	exit(SISH_EXIT_SUCCESS);
}

