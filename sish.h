#ifndef _SISH_H_
#define _SISH_H_

/* 
 * If any error occurred before the execution
 * of the command, the status code of sish
 * is always set to 127.
 */
#define SISH_EXIT_FAILURE 127
#define SISH_EXIT_SUCCESS 0

struct sishopt {
	BOOL c_flag;
	JSTRING *command;
	BOOL x_flag;
};

int sish_run(struct sishopt *);
void perror_exit(char *);

#endif /* !_SISH_H_ */
