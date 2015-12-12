#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jstring.h"
#include "macros.h"
#include "func.h"
#include "sish.h"

static void print_prompt();

int 
sish_run(struct sishopt *ssopt)
{
	char c;
	JSTRING *command;
	
	for (;;) {
		command = jstr_create("");
		
		print_prompt();
		while ((c = (char)getc(stdin)) != '\n')
			jstr_append(command, c);
		
		printf("com: %s\n", jstr_cstr(command));
		
		system("env");
		
		jstr_free(command);
	}
}

static void
print_prompt()
{
	(void)fprintf(stdout, SISH_PROMPT);
	if (fflush(stdout) == EOF)
		perror_exit("flush stdout error");
}