/*
 * This program provides a String object with 
 * a series of operation functions on it.
 * This idea comes from Java so it is named 
 * JSTRING.
 *
 * To use this String object, you don't need to
 * worry about memory allocation or boundary check.
 * You can append or concatenate arbitrary characters
 * as you want and the String will be expanded 
 * appropriately and automatically.
 */
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jstring.h"

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

static JSTRING *jstr_create_from_arr(char *, size_t);
static void jstr_realloc(JSTRING *);
static void check_index(JSTRING *, size_t);
static void check_ptr(void *);

JSTRING *
jstr_create(char *str)
{
	check_ptr(str);

	return jstr_create_from_arr(str, strlen(str));
}

char *
jstr_cstr(JSTRING *jstr)
{
	check_ptr(jstr);
	
	return jstr->str;
}

size_t
jstr_length(JSTRING *jstr)
{
	check_ptr(jstr);
	
	return jstr->length;
}

char
jstr_charat(JSTRING *jstr, size_t index)
{
	check_ptr(jstr);
	check_index(jstr, index);
	
	return jstr->str[index];
}

JSTRING *
jstr_substr(JSTRING *jstr, size_t index, size_t len)
{
	check_ptr(jstr);
	check_index(jstr, index);
	
	if (index + len > jstr->length) {
		(void)fprintf(stderr, 
		      "jstring: length out of range\n");
		exit(EXIT_FAILURE);
	}
	
	return jstr_create_from_arr(jstr->str + index, len);
}

void
jstr_trunc(JSTRING *jstr, size_t index, size_t len)
{
	size_t i;
	
	check_ptr(jstr);
	
	if (index >= jstr->length) {
		jstr->str[0] = '\0';
		jstr->length = 0;
		return;
	}
	
	if (index + len > jstr->length) {
		(void)fprintf(stderr, 
		      "jstring: length out of range\n");
		exit(EXIT_FAILURE);
	}
	
	if (index != 0)
		for (i = 0; i < len; i++, index++)
			jstr->str[i] = jstr->str[index];
	jstr->str[len] = '\0';
	jstr->length = len;
}

void jstr_insert(JSTRING *jstr, size_t index, char *str)
{
	size_t i, move_index;
	size_t str_len;
	
	check_ptr(jstr);
	check_ptr(str);
	
	if (index > jstr->length) {
		(void)fprintf(stderr, 
		      "jstring: index [%zu] out of range [0, %zu]\n",
			  index,
			  jstr->length);
		exit(EXIT_FAILURE);
	}
	
	str_len = strlen(str);
	if (str_len == 0)
		return;
	
	if (index == jstr->length) {
		jstr_concat(jstr, str);
		return;
	}
	
	move_index = jstr->length;
	
	jstr->length += str_len;
	jstr_realloc(jstr);
	
	for (i = jstr->length; move_index > index; i--, move_index--)
		jstr->str[i] = jstr->str[move_index];
	/* avoid index == 0, move_index-- will be a big number */
	jstr->str[i] = jstr->str[move_index]; 
	
	
	for (i = 0; i < str_len; i++, index++)
		jstr->str[index] = str[i];
}

void
jstr_concat(JSTRING *jstr, char *str)
{
	check_ptr(jstr);
	check_ptr(str);
	
	jstr->length += strlen(str);
	jstr_realloc(jstr);
		
	(void)strcat(jstr->str, str);
	
}

void
jstr_append(JSTRING *jstr, char c)
{
	check_ptr(jstr);
	
	jstr->length += 1;
	jstr_realloc(jstr);
	
	jstr->str[jstr->length - 1] = c;
	jstr->str[jstr->length] = '\0';
	
}

int
jstr_equals(JSTRING *jstr, char *str)
{
	check_ptr(jstr);
	check_ptr(str);
	
	return strcmp(jstr_cstr(jstr), str);
}

void
jstr_free(JSTRING *jstr)
{
	check_ptr(jstr);
	
	free(jstr->str);
	jstr->str = NULL;
	jstr->length = 0;
	jstr->capacity = 0;
	free(jstr);
	
}

static JSTRING *
jstr_create_from_arr(char *arr, size_t len)
{
	
	JSTRING *jstr;
	
	MALLOC(jstr, JSTRING, 1);
	
	jstr->length = len;
	jstr->capacity = len + 1;
	
	MALLOC(jstr->str, char, jstr->capacity);
	(void)memcpy(jstr->str, arr, sizeof(char) * len);
	
	jstr->str[len] = '\0';
	
	return jstr;
}

static void
jstr_realloc(JSTRING *jstr)
{
	
	if (jstr->length >= jstr->capacity) {
		jstr->capacity = jstr->length + 1;
		jstr->capacity *= 2;
		REALLOC(jstr->str, char, jstr->capacity);
	}
	
}

static void
check_ptr(void *ptr)
{
	if (ptr == NULL) {
		(void)fprintf(stderr, "jstring: NULL pointer\n");
		exit(EXIT_FAILURE);
	}
}

static void
check_index(JSTRING *jstr, size_t index)
{
	if (index >= jstr->length) {
		(void)fprintf(stderr, 
		      "jstring: index [%zu] out of range [0, %zu]\n",
			  index,
			  jstr->length - 1);
		exit(EXIT_FAILURE);
	}
}


