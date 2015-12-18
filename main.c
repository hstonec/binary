/*
 * This program implements a simple shell.
 */
#include <bsd/stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jstring.h"
#include "arraylist.h"
#include "macros.h"
#include "parse.h"
#include "sish.h"

int main(int, char **);
static JSTRING *rel2abs(char *);
static void usage();

/*
 * This function parses all command-line options 
 * and sets all flags, then calls sish_run()
 * function to start the shell.
 */
int
main(int argc, char *argv[])
{
	int opt;
	struct sishopt ssopt;
	JSTRING *shell_path;
	
	/* By default, set all options to be FALSE */
	ssopt.c_flag = FALSE;
	ssopt.x_flag = FALSE;
	
	setprogname(argv[0]);
	
	while ((opt = getopt(argc, argv, 
					"c:x")) != -1) {
		switch (opt) {
		case 'c':
			ssopt.c_flag = TRUE;
			ssopt.command = jstr_create(optarg);
			break;
		case 'x':
			ssopt.x_flag = TRUE;
			break;
		case '?':
			usage();
			/* NOTREACHED */
		default:
			break;
		}
	}
	argc -= optind;
	
	/* If passed any argument, just print out error message and exit */
	if (argc > 0) {
		(void)fprintf(stderr,
		  "-%s: too many operands\n",
		  getprogname());
		usage();
		/* NOTREACHED */
	}
	
	/* Set SHELL to be an environment variable */
	shell_path = rel2abs(argv[0]);
	if (setenv("SHELL", jstr_cstr(shell_path), TRUE) == -1)
		perror_exit("set variable SHELL error");
	
	
	return sish_run(&ssopt);
}

/*
 * If parameter "path" is a relative path, this function 
 * transforms it to absolute path based on CWD.
 */
static JSTRING *
rel2abs(char *path) 
{
	size_t i;
	char *cwd, *last_slash, *ite;
	JSTRING *abs;
	char *new_dir;
	JSTRING *trimed;
	
	if (path[0] == '/')
		return jstr_create(path);
	
	cwd = getcwd(NULL, 0);
	if (cwd == NULL)
		perror_exit("getcwd error");
	
	abs = jstr_create(cwd);
	if (jstr_charat(abs, jstr_length(abs) - 1) != '/')
		jstr_append(abs, '/');
	
	last_slash = strrchr(path, '/');
	if (last_slash == NULL) {
		jstr_concat(abs, path);
		return abs;
	}
	
	for (i = 0, ite = path; ite != last_slash; i++, ite++)
		jstr_append(abs, *ite);
	
	if (chdir(jstr_cstr(abs)) == -1)
		perror_exit("chdir error");
	
	new_dir = getcwd(NULL, 0);
	if (new_dir == NULL)
		perror_exit("getcwd error");
	
	trimed = jstr_create(new_dir);
	if (jstr_charat(trimed, jstr_length(trimed) - 1) != '/')
		jstr_append(trimed, '/');
	
	jstr_concat(trimed, last_slash + 1);
	
	if (chdir(cwd) == -1)
		perror_exit("chdir error");
	
	free(new_dir);
	jstr_free(abs);
	free(cwd);
	return trimed;
}

static void
usage()
{
	(void)fprintf(stderr, 
	  "usage: %s [-x] [-c command]\n", 
	  getprogname());
	exit(SISH_EXIT_FAILURE);
}
