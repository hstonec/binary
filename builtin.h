#ifndef _BUILTIN_H_
#define _BUILTIN_H_

/* return -1: not match any builtin function
 *         0: success
 *		  >0: error
 */
int call_builtin(PARSED_CMD *, int, int, pid_t, int);

#endif /* !_BUILTIN_H_ */
