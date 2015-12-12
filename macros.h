#ifndef _MACROS_H_
#define _MACROS_H_

#define BOOL int
#define TRUE 1
#define FALSE 0

#define MALLOC(ptr, ptrtype, len) \
do { \
	ptr = (ptrtype *)malloc(sizeof(ptrtype) * (len)); \
	if (ptr == NULL) { \
		(void)fprintf(stderr, \
			"create " #ptrtype " *" \
			#ptr " error: malloc() failed\n"); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

#define REALLOC(ptr, ptrtype, len) \
do { \
	ptr = (ptrtype *)realloc(ptr, sizeof(ptrtype) * (len)); \
	if (ptr == NULL) { \
		(void)fprintf(stderr, \
			"change " #ptrtype " *" \
			#ptr " error: realloc() failed\n"); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

#endif /* !_MACROS_H_ */
