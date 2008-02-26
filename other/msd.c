/*
   MSD radix sort with a fixed sized alphabet.

   S. Nilsson. Radix Sorting and Searching. PhD thesis, Department
   of Computer Science, Lund University, 1990.

   The code presented in this file has been tested with care but is
   not guaranteed for any purpose. The writer does not offer any
   warranties nor does he accept any liabilities with respect to
   the code.

   Stefan Nilsson, 8 jan 1997.

   Laboratory of Information Processing Science
   Helsinki University of Technology
   Stefan.Nilsson@hut.fi
*/

#include "nilsson.h"

#define CHAR(s, p) s[p]

typedef struct bucketrec {
   list head, tail;
   int size;   /* size of list, 0 if already sorted */
} bucket;

typedef struct stackrec {
   list head, tail;
   int size;   /* size of list, 0 if already sorted */
   int pos;    /* current position in string */
} stack;

static memory stackmem[1];
static stack *stackp;

static void push(list head, list tail, int size, int pos)
{   
   stackp = (stack *) allocmem(stackmem, sizeof(struct stackrec));
   stackp->head = head;
   stackp->tail = tail;
   stackp->size = size;
   stackp->pos = pos;
}

static stack *pop()
{
   stack *temp;

   temp = stackp;
   stackp = (stack *) deallocmem(stackmem, sizeof(struct stackrec));
   return temp;
}

static stack *top()
{
   return stackp;
}

static boolean stackempty()
{
   return !stackp;
}

/* Put a list of elements into a bucket. The minimum and maximum
   character seen so far (chmin, chmax) are updated when the bucket
   is updated for the first time. */
static void intobucket(bucket *b, list h, list t, int size,
                       character ch, character *chmin, character *chmax)
{
   if (!b->head) {
      b->head = h;
      b->tail = t;
      b->size = size;
      if (ch != '\0') {
         if (ch < *chmin) *chmin = ch;
         if (ch > *chmax) *chmax = ch;
      }
   } else {
      b->tail->next = h;
      b->tail = t;
      b->size += size;
   }
}

/* Put the list in a bucket onto the stack. If the list is small
   (contains at most INSERTBREAK elements) sort it using insertion
   sort. If both the the list on top of the stack and the list to
   be added to the stack are already sorted the new list is appended
   to the end of the list on the stack and no new stack record is
   created. */
static void ontostack(bucket *b, int pos)
{
   b->tail->next = NULL;
   if (b->size <= INSERTBREAK) {
      if (b->size > 1)
         b->head = ListInsertsort(b->head, &b->tail, pos);
      b->size = 0;   /* sorted */
   }
   if (!b->size && !stackempty() && !top()->size) {
      top()->tail->next = b->head;
      top()->tail = b->tail;
   }
   else {
      push(b->head, b->tail, b->size, pos);
      b->size = 0;
   }
   b->head = NULL;
}

/* Traverse a list and put the elements into buckets according
   to the character in position pos. The elements are moved in
   blocks consisting of strings that have a common character in
   position pos. We keep track of the minimum and maximum nonzero
   characters encountered. In this way we may avoid looking at
   some empty buckets when we traverse the buckets in ascending
   order and push the lists onto the stack */
static void bucketing(list a, int pos)
{
   static bucket b[CHARS];
   bucket *bp;
   character ch, prevch;
   character chmin = CHARS-1, chmax = 0;
   list t = a, tn;
   int size = 1;

   prevch = CHAR(t->str, pos);
   for ( ; (tn = t->next); t = tn) {
      ch = CHAR(tn->str, pos); size++;
      if (ch == prevch) continue;
      intobucket(b+prevch, a, t, size-1, prevch, &chmin, &chmax);
      a = tn;
      prevch = ch;
      size = 1;
   }
   intobucket(b+prevch, a, t, size, prevch, &chmin, &chmax);

   if (b->head) {    /* ch = '\0', end of string */
      b->size = 0;   /* already sorted */
      ontostack(b, pos);
   }
   for (bp = b + chmin; bp <= b + chmax; bp++)
      if (bp->head) ontostack(bp, pos+1);
}

list MSD1(list a, int n)
{
   list res = NULL;
   stack *s;

   if (n < 2) return a;
   initmem(stackmem, sizeof(struct stackrec), n/50);
   push(a, NULL, n, 0);

   while (!stackempty()) {
      s = pop();
      if (!s->size) {   /* sorted */
         s->tail->next = res;
         res = s->head;
         continue;
      }
      bucketing(s->head, s->pos);
   }

   freemem(stackmem);
   return res;
}

void MSDsort(string strings[], size_t scnt)
{
   list listnodes;
   size_t i;

    /* allocate memory based on the number of strings in the array */
    listnodes = (list ) calloc(scnt, sizeof(struct listrec));

    /* point the linked list nodes to the strings in the array */
    for( i=0; i<scnt; i++)
    {
        listnodes[i].str = strings[i];
        if (i<(scnt-1))
           listnodes[i].next = &listnodes[i+1];
        else
           listnodes[i].next = NULL;
    }

    /* sort */
    listnodes = MSD1(listnodes, scnt);

    /* write the strings back into the array */
    for (i = 0;  i < scnt ; i++, listnodes=listnodes->next)
       strings[i] = listnodes->str;

    free(listnodes);

   return;
}
