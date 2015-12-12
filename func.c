#include <bsd/stdlib.h>

#include <stdio.h>
#include <stdlib.h>

#include "jstring.h"
#include "macros.h"
#include "func.h"
#include "sish.h"

void
perror_exit(char *message)
{
	(void)fprintf(stderr, "%s: ", getprogname());
	perror(message);
	exit(SISH_EXIT_FAILURE);
}