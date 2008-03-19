#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define CHARS 256
#define INSERTBREAK 20
typedef unsigned char* string;

void mkqsort(unsigned char **, int n, int depth);
void inssort(unsigned char **, int n, int depth);
int  scmp(unsigned char*, unsigned char*);

#endif //UTILS_H
