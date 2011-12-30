/*
   This is an implementation of Burstsort using linked lists for buckets. A
   more complete discussion of the algorithm, the implementation and a
   comparison with other well known sorting algorithms, both radix sorting and
   comparison-based methods, can be found in:

   R. Sinha and J. Zobel, "Cache-Conscious Sorting of Large Sets of Strings
   with Dynamic Tries", In Proc. 5th Workshop Algorithm Engineering and
   Experiments (ALENEX), R. Ladner (ed), Baltimore, Maryland, USA, January
   2003.

   R. Sinha and J. Zobel, "Efficient Trie-based Sorting of Large Sets of
   Strings", In Proc. Australasian Computer Science Conference, M. Oudshoorn
   (ed), Adelaide, Australia, February, 2003.

   The code presented in this file has been tested with care but is not
   guaranteed for any purpose. The writer does not offer any warranties nor
   does he accept any liabilities with respect to the code.

   Ranjan Sinha, 28 July 2003.

   School of Computer Science and Information Technology, RMIT University,
   Melbourne, Australia rsinha@cs.rmit.edu.au

   note:
       1. It is a work in progress
       2. Not tuned for number of instructions, use the highest optimizations such as O3
       3. Any relevant changes to code may please be intimated to me
       4. It is solely meant for academic use
*/

#include "routine.h"
#include "utils.h"
#include <stdlib.h>

#define THRESHOLD 8192
#define ALPHABET 256

typedef struct trierec
{
    struct trierec *ptrs[ALPHABET];
    int counts[ALPHABET];
} TRIE;

typedef struct strlistrec
{
    unsigned char	*word;
    struct strlistrec *next;
} LIST;

static void
burstinsertL(TRIE *root, LIST *list,  size_t scnt)
{
    TRIE	*new_;
    TRIE	*curr;
    LIST	*node;
    LIST	*lp, *np;
    unsigned int	i, p;
    unsigned char	c, cc;

    for( i=0 ; i<scnt ; i++ )
    {
        curr = root;
        node = &list[i];

        for( p=0, c=list[i].word[p] ; curr->counts[c]<0 ; curr=curr->ptrs[c], p++, c=list[i].word[p] )
            ;

        node->next = (LIST *) curr->ptrs[c];
        curr->ptrs[c] = (TRIE *) node;

        if( c=='\0' )
        {
            ;  /* leave counter alone to avoid overflow, no burst */
        }
        else
        {
            curr->counts[c]++;
            if( curr->counts[c]>THRESHOLD )  /* burst */
            {
                curr->counts[c] = -1;
                p++;
                new_ = (TRIE *) calloc(1, sizeof(TRIE));

                lp = (LIST *) curr->ptrs[c], cc = lp->word[p], np = lp->next;
                while( lp!=NULL )
                {
                    lp->next = (LIST *) new_->ptrs[cc];
                    new_->ptrs[cc] = (TRIE *) lp;
                    new_->counts[cc] ++;
                    lp = np;
                    if( lp!=NULL )
                    {
                        cc = lp->word[p];
                        np = lp->next;
                    }
                }
                curr->ptrs[c] = new_;
                curr->counts[c] = -1; /* used to traverse along the trie hierarchy                     */
                curr = new_;           /* used to burst recursive, so point curr to new                 */
                c = cc;               /* point to the character that the last string was inserted into */
            }
        }
    }
}

static int
bursttraverseL(TRIE *node, unsigned char **strings, int pos, int deep)
{
    LIST	*l;
    unsigned int i, off;
    unsigned int sizeOfContainer = 0;

    for( i=0 ; i<ALPHABET ; i++ )
    {
        if( node->counts[i]<0 )
        {
            pos = bursttraverseL(node->ptrs[i], strings, pos, deep+1);
	    }
        else
        {
            for( off=pos, l=(LIST *) node->ptrs[i] ; l!=NULL ; off++, l=l->next )
            {
                strings[off] = l->word;
            }
            sizeOfContainer = (off - pos); 

            if( i>0 && sizeOfContainer > 1 )
            {
                if (sizeOfContainer < INSERTBREAK)
                    inssort( strings+pos, off-pos, deep + 1);     
                else
                    mkqsort( strings+pos, off-pos, deep + 1);
            }
            pos = off;
        }
    }
    free(node);
    return pos;
}

void
burstsortL(unsigned char *strings[], size_t scnt)
{
    TRIE	*root;
    LIST	*listnodes;
    unsigned int i;

    listnodes = (LIST *) calloc(scnt, sizeof(LIST));

    for( i=scnt; i-- ;)
        listnodes[i].word = strings[i];

    root = (TRIE *) calloc(1, sizeof(TRIE));

    (void) burstinsertL(root, listnodes, scnt);

    (void) bursttraverseL(root, strings, 0, 0);

    free(listnodes);

    return;
}
ROUTINE_REGISTER_SINGLECORE(burstsortL,
		"Burstsort with List buckets by R. Sinha and J. Zobel")
