#ifndef NILSSON_H
#define NILSSON_H

#include "utils.h"

#define MAXBLOCKS 100
#define TRUE 1
#define FALSE 0
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef int boolean;
typedef int character;

typedef struct listrec *list;
struct listrec {
	string str;
	list next;
	int length;
};

typedef struct {
	void *block[MAXBLOCKS];
	int allocnr;
	int nr;
	int blocksize;
	void *current, *first, *last;
} memory;

void initmem(memory *m, int elemsize, int blocksize);
void *allocmem(memory *m, int elemsize);
void *deallocmem(memory *m, int elemsize);
void resetmem(memory *m);
void freemem(memory *m);

list ListInsertsort(list head, list *tail , int length);

#endif //NILSSON_H
