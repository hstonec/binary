#ifndef _BUILTIN_H_
#define _BUILTIN_H_

BOOL is_builtin(PARSED_CMD *);

/* return -1: not match any builtin function
 *         0: success
 *		  >0: error
 */
int call_builtin(PARSED_CMD *, int, int);

#endif /* !_BUILTIN_H_ */
