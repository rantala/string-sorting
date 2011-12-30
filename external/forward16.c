/*
   Forward radixsort. Uses two characters for bucketing.

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

#include "routine.h"
#include "nilsson.h"
#include <stdlib.h>

#define BUCKETS CHARS*CHARS
#define IS_ENDMARK(ch) ((ch & (CHARS-1)) == 0)
#define SHORT(s, p) s[p] << 8 | (s[p] ? s[p+1] : 0)

#define HIGH(ch) ch >> 8
#define LOW(ch) ch & (CHARS-1)

typedef struct grouprec *group;
typedef struct bucketrec *bucket;

struct grouprec {
   list head, tail; /* a list of elements */
   group next;      /* the next group */
   group nextunf;   /* the next unfinished group */
   group insp;      /* insertion point */
   boolean finis;   /* is the group finished? */
};

struct bucketrec {
   list head, tail; /* a list of elements */
   int size;        /* list length */
   group tag;       /* group tag */
   bucket next;     /* next bucket item */
};

static memory groupmem[1];
static memory bucketmem[1];

/*
   String comparisons are performed starting at position p
   of each string. The algorithm returns the head and the tail
   of the sorted list.
*/
list ForListInsertsort(list r, list *tail, int p)
{
   list fi, la, t;

   for (fi = la = r, r = r->next; r; r = la->next)
      if (scmp(r->str+p, la->str+p) >= 0) /* add to tail */
         la = r;
      else if (scmp(r->str+p, fi->str+p) <= 0) { /* add to head */
         la->next = r->next;
         r->next = fi;
         fi = r;
      } else { /* insert into middle */
         for (t = fi; scmp(r->str+p, t->next->str+p) >= 0; )
            t = t->next;
         la->next = r->next;
         r->next = t->next;
         t->next = r;
      }
   *tail = la;
   return fi;
}


/* Put a list of elements into a bucket. */
static void intobucket(bucket *b, list head, list tail,
                       int size, group g)
{
   bucket btemp = *b, newb;

   if (!btemp || btemp->tag != g) {   /* create new tag */
      newb = (bucket) allocmem(bucketmem, sizeof(struct bucketrec));
      newb->next = btemp;
      newb->head = head;
      newb->size = size;
      newb->tag = g;
      *b = btemp = newb;
   } else {   /* append */
      btemp->tail->next = head;
      btemp->size += size;
   }
   tail->next = NULL;
   btemp->tail = tail;
}

/* Travers the groups and put the elements into buckets.
   Finished groups are located and skipped.
   The elements are moved in blocks. */
static void intobuckets(group g, bucket b[], 
                        int *used1, int *used2, int pos)
{
   group prevg;
   character ch, prevch;
   boolean split;
   list tail, tailn;
   int size;
   character buckets1, buckets2;

   for (ch = 0; ch < CHARS; ch++)
      used1[ch] = used2[ch] = FALSE;
   resetmem(bucketmem);
   for (prevg = g, g = g->nextunf ; g; g = g->nextunf) {
      if (g->finis)
         {prevg->nextunf = g->nextunf; continue;}
      tail = g->head; split = FALSE;
      prevch = SHORT(tail->str, pos); size = 1;
      for ( ; (tailn = tail->next); tail = tailn) {
         ch = SHORT(tailn->str, pos); size++;
         if (ch == prevch) continue;
         intobucket(b+prevch, g->head, tail, size-1, g);
         g->head = tailn; split = TRUE;
         used1[HIGH(prevch)] = used2[LOW(prevch)] = TRUE;
         prevch = ch; size = 1;
      }
      if (split) {
         intobucket(b+prevch, g->head, tail, size, g);
         g->head = NULL;
         used1[HIGH(prevch)] = used2[LOW(prevch)] = TRUE;
         prevg = g;
      } else if (IS_ENDMARK(prevch))
         prevg->nextunf = g->nextunf;
      else
         prevg = g;
   }
   buckets1 = buckets2 = 0;
   for (ch = 0; ch < CHARS; ch++) {
      if (used1[ch]) used1[buckets1++] = ch;
      if (used2[ch]) used2[buckets2++] = ch;
   }
   used1[CHARS] = buckets1; used2[CHARS] = buckets2;
/* printf("#buckets: %i\n", bucketp-bucketbase); */
}

/* Put a list into group g and, at the same time, split g.
   Do not split if two consecutive groups are finished. */
static void intogroup(group g, list head, list tail, boolean finis)
{
   group newg;

   if (!g->head) {   /* back into old group */
      g->head = head;
      g->tail = tail;
      g->finis = finis;
      g->insp = g;
   } else if (finis && g->insp->finis) {  /* don't split if both */
      g->insp->tail->next = head;         /* groups are finished */
      g->insp->tail = tail;
   }
   else {   /* split */
      newg = (group) allocmem(groupmem, sizeof(struct grouprec));
      newg->head = head;
      newg->tail = tail;
      newg->next = g->insp->next;
      newg->nextunf = g->insp->nextunf;
      newg->finis = finis;
      g->insp = g->insp->nextunf = g->insp->next = newg;
   }
}

/* Traverse the buckets and put the elements back into their groups.
   Split the groups and mark all finished groups.
   The elements are moved in blocks. */
static void intogroups(bucket b[], int *used1, int *used2, int pos)
{
   character ch, ch1, ch2, high;
   bucket s;
   boolean finis;

   for (ch1 = 0; ch1 < used1[CHARS]; ch1++) {
      high = used1[ch1] << 8;
      for (ch2 = 0; ch2 < used2[CHARS]; ch2++) {
         ch = high | used2[ch2];
         if (!b[ch]) continue;
         for (s = b[ch]; s; s = s->next) {
            finis = IS_ENDMARK(ch);
            if (s->size < INSERTBREAK && !finis) {
               if (s->size > 1)
                  s->head = ForListInsertsort(s->head, &s->tail, pos);
               finis = TRUE;
            }
            intogroup(s->tag, s->head, s->tail, finis);
         }
         b[ch] = NULL;
      }
   }
}

/* Travers the groups and return the elements in sorted order. */
static list collect(group g)
{
   list head, tail;

   g = g->next;
   head = g->head;
   tail = g->tail;
   for (g = g->next; g; g = g->next) {
      tail->next = g->head;
      tail = g->tail;
   }
   return head;
}

static inline list forward2(list t, int n)
{
   static bucket b[BUCKETS]; /* buckets */
   character used1[CHARS+1]; /* What buckets are used? The number of */
   character used2[CHARS+1]; /* buckets is stored in the last element */
   group g, g2;              /* groups */
   int pos = 0;              /* pos in string */

   if (n<2) return t;

   initmem(groupmem, sizeof(struct grouprec), n/15);
   initmem(bucketmem, sizeof(struct bucketrec), n/5);

   g = (group) allocmem(groupmem, sizeof(struct grouprec));
   g2 = (group) allocmem(groupmem, sizeof(struct grouprec));
   g->next = g->nextunf = g2;
   g2->head = t;
   g2->next = g2->nextunf = NULL; 
   g2->finis = FALSE;

   intobuckets(g, b, used1, used2, pos);
   while (g->nextunf) {
      pos += 2;
      intogroups(b, used1, used2, pos);
      intobuckets(g, b, used1, used2, pos);
   }
   t = collect(g);

   freemem(bucketmem);
   freemem(groupmem);

   return t;
}

void frssort(string strings[], size_t scnt)
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
    listnodes = forward2(listnodes, scnt);

    /* write the strings back into the array */
    for (i = 0;  i < scnt ; i++, listnodes=listnodes->next)
       strings[i] = listnodes->str;

   return;
}

void forward16(unsigned char **strings, size_t n)
{
	return frssort(strings, n);
}
ROUTINE_REGISTER_SINGLECORE(forward16,
		"Forward Radix Sort 16-bit by Stefan Nilsson")
