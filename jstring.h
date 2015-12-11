#ifndef _JSTRING_H_
#define _JSTRING_H_

typedef struct jstring {
	size_t capacity;
	size_t length;
	char *str;
} JSTRING;

JSTRING *jstr_create(char *);
char *jstr_cstr(JSTRING *);
size_t jstr_length(JSTRING *);
char jstr_charat(JSTRING *, size_t);
JSTRING *jstr_substr(JSTRING *, size_t, size_t);
void jstr_trunc(JSTRING *, size_t, size_t);
void jstr_insert(JSTRING *, size_t, char *);
void jstr_concat(JSTRING *, char *);
void jstr_append(JSTRING *, char);
int jstr_equals(JSTRING *, char *);
void jstr_free(JSTRING *);


#endif /* !_JSTRING_H_ */
