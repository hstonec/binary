#ifndef _ARRAYLIST_H_
#define _ARRAYLIST_H_

#define ARRAYLIST_INIT_CAPACITY 15

typedef struct arraylist {
	size_t size;
	size_t capacity;
	void **arraylist;
} ARRAYLIST;

ARRAYLIST *arrlist_create();
void arrlist_add(ARRAYLIST *, void *);
void arrlist_insert(ARRAYLIST *, size_t, void *);
void arrlist_sort(ARRAYLIST *, int (*)(const void *, const void *));
void arrlist_reverse(ARRAYLIST *);
size_t arrlist_size(ARRAYLIST *);
void *arrlist_get(ARRAYLIST *, size_t);
void *arrlist_remove(ARRAYLIST *, size_t);
void arrlist_free(ARRAYLIST *);

#endif /* !_ARRAYLIST_H_ */
