/*
 * This program is a basic implementation of ArrayList.
 * It just supports adding, sorting and reversing operations. 
 * 
 * Notice that when you add an object to the list, you
 * only add the reference of the object to the list. So,
 * arrlist_free() won't affect the original object but just
 * release the space which the reference took.
 * 
 * The initial capacity of the list is 15, you can change it
 * in arraylist.h if you wish.
 */
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arraylist.h"

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

void merge(void **, void **, size_t, size_t, size_t,
           int (*)(const void *, const void *));
void check_index(ARRAYLIST *, size_t);
void check_ptr(void *);
void arrlist_realloc(ARRAYLIST *);

ARRAYLIST *
arrlist_create()
{
	ARRAYLIST *list;
	
	MALLOC(list, ARRAYLIST, 1);
	list->size = 0;
	list->capacity = ARRAYLIST_INIT_CAPACITY;
	MALLOC(list->arraylist, void *, list->capacity);
	
	return list;
}

void
arrlist_add(ARRAYLIST *list, void *ptr)
{
	check_ptr(list);
	
	list->size++;
	arrlist_realloc(list);

	list->arraylist[list->size - 1] = ptr;
}

void
arrlist_insert(ARRAYLIST *list, size_t index, void *ptr)
{
	size_t i;
	
	check_ptr(list);
	check_index(list, index);
	
	list->size++;
	arrlist_realloc(list);
	
	for (i = list->size - 1; i > index; i--)
		list->arraylist[i] = list->arraylist[i - 1];
	list->arraylist[index] = ptr;
}

void 
arrlist_sort(ARRAYLIST *list, 
         int (*cmp)(const void *ptr1, const void *ptr2))
{
	
	size_t subarrsize, subarrindex;
	size_t start1, end1, end2;
	void **auxlist;
	
	check_ptr(list);
	
	if (list->size <= 1)
		return;
	
	MALLOC(auxlist, void *, list->size);
	
	for (subarrsize = 1; 
	     subarrsize < list->size; 
		 subarrsize *= 2) {
		
		for (subarrindex = 0; 
		     subarrindex < list->size - subarrsize; 
             subarrindex += 2 * subarrsize) {

			start1 = subarrindex;
			end1 = start1 + subarrsize - 1;
			end2 = end1 + subarrsize < list->size - 1 ? 
                   end1 + subarrsize : list->size - 1;
			
			merge(list->arraylist, 
			      auxlist, start1, 
				  end1, end2, cmp);
		}
	}
		
	free(auxlist);
}

void
arrlist_reverse(ARRAYLIST *list)
{
	void *temp;
	size_t left, right;
	
	check_ptr(list);
	
	if (list->size <= 1)
		return;
	
	left = 0;
	right = list->size - 1;
	
	while (left < right) {
		temp = list->arraylist[left];
		list->arraylist[left] = list->arraylist[right];
		list->arraylist[right] = temp;
		left++;
		right--;
	}
}

size_t
arrlist_size(ARRAYLIST *list)
{
	check_ptr(list);
	
	return list->size;
}

void *
arrlist_get(ARRAYLIST *list, size_t index)
{
	check_ptr(list);
	check_index(list, index);
	
	return list->arraylist[index];
}

void *
arrlist_remove(ARRAYLIST *list, size_t index)
{
	void *target;
	size_t i;
	
	check_ptr(list);
	check_index(list, index);
	
	target = list->arraylist[index];
	
	for (i = index + 1; i < list->size; i++)
		list->arraylist[i - 1] = list->arraylist[i];
	list->size--;
	
	return target;
}

void
arrlist_free(ARRAYLIST *list)
{
	check_ptr(list);
	
	free(list->arraylist);
	list->arraylist = NULL;
	list->size = 0;
	list->capacity = 0;
	free(list);
}

void
merge(void **arraylist, void **auxlist, size_t start1, 
	  size_t end1, size_t end2, 
	  int (*cmp)(const void *ptr1, const void *ptr2))
{
	
	size_t start2, i;
	size_t start, end;
	int result;
	
	if (start1 >= end2)
		return;
	
	start2 = end1 + 1;
	start = start1;
	end = end2;
	
	for (i = start; start1 <= end1 || 
	     start2 <= end2; i++) {
		
		if (start1 > end1)
			result = 1;
		else if (start2 > end2)
			result = -1;
		else
			result = cmp(&arraylist[start1], 
		                 &arraylist[start2]);
		
		if (result <= 0) {
			auxlist[i] = arraylist[start1];
			start1++;
		} else {
			auxlist[i] = arraylist[start2];
			start2++;
		}
	}
	
	for (; start <= end; start++)
		arraylist[start] = auxlist[start];

}

void
check_ptr(void *ptr)
{
	if (ptr == NULL) {
		(void)fprintf(stderr, "arraylist: NULL pointer\n");
		exit(EXIT_FAILURE);
	}
}

void
check_index(ARRAYLIST *list, size_t index)
{
	if (index >= list->size) {
		(void)fprintf(stderr, 
		      "arraylist: index [%zu] out of range [0, %zu]\n",
			  index,
			  list->size - 1);
		exit(EXIT_FAILURE);
	}
}

void
arrlist_realloc(ARRAYLIST *list)
{
	if (list->size >= list->capacity) {
		list->capacity = list->size + 1;
		list->capacity *= 2;
		
		REALLOC(list->arraylist, void *, list->capacity);
	}
}
