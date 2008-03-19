/*
   This is an implementation of Burstsort using arrays for buckets. A more
   complete discussion of the algorithm, the implementation and a comparison
   with other well known sorting algorithms, both radix sorting and
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

#include "sortstring.h"
#include "utils.h"

#define THRESHOLD 8192 
#define THRESHOLDMINUSONE (THRESHOLD - 1) 
#define LEVEL 7				/* no. of levels used in dynamic growth of bucket */
#define ALPHABET 256

#define EXIT exit(EXIT_FAILURE)
#define ERR_PRINT_OUT_OF_MEMORY printf("Error: Out of memory \n")

typedef struct trierec
{
    char **nulltailptr;			/* last element in null bucket		*/ 
    unsigned char levels[ALPHABET];	/* level counter of bucket size		*/ 
    int  counts[ALPHABET];		/* count of items in bucket		*/ 
    char **ptrs[ALPHABET];		/* pointers to buckets or trie node	*/ 
} TRIE;

/* levels of the staggered approach */
static unsigned short bucket_inc[LEVEL] = {0, 16, 128, 1024, 8192, 16384, 32768};

/* 
** Function : burstinsertA
**
** Input    :
**    root    : root of the trie node
**    strings : all the strings 
**    scnt    : count of strings
** 
** Output   : None
*/
static void
burstinsertA(TRIE *root, string strings[], int scnt)
{
    TRIE    *new_;				
    TRIE    *curr;			
    int     i, j;
    int     newcounts, currcounts;
    unsigned char c, cc=0, p;

    /* insert all strings into the burst trie */
    for( i=0 ; i<scnt ; i++ )
    {
        /* start at root */
        curr = root; 

        /* locate TRIE node to insert string */
        for( p = 0, c = strings[i][p]; curr->counts[c] < 0; curr = (TRIE *) curr->ptrs[c], p++, c = strings[i][p])
            ;
        /* are buckets already created ? */ 
        if(curr->counts[c] < 1) /* create bucket */ 
        {
            if (c == 0) 
            { 
                /* allocate memory for the bucket */
                if ((curr->ptrs[c] = (char **) calloc(THRESHOLD, sizeof(char *))) == NULL)
                {
                    ERR_PRINT_OUT_OF_MEMORY;
                    EXIT;
                }
                curr->nulltailptr = curr->ptrs[c];		/* point to the first cell of the bucket */
                *curr->nulltailptr = (char *) strings[i];	/* insert the string */
                curr->nulltailptr++;				/* point to next cell */
                curr->counts[c]++;				/* increment count of items */
             }
             else 
             {
                 if ((curr->ptrs[c] = (char **) calloc(bucket_inc[1], sizeof(char *))) == NULL)
                 {
                     ERR_PRINT_OUT_OF_MEMORY;
                     EXIT;
                 }

                 curr->ptrs[c][curr->counts[c]++] = (char *) strings[i];
                 curr->levels[c]++;
             }
        }
        else /* bucket already created */
        {
            /* insert string in bucket */
            if (c == 0)
            { 
                *curr->nulltailptr = (char *) strings[i];	/* insert the string		*/
                curr->nulltailptr++;				/* point to next cell		*/
                curr->counts[c]++;				/* increment count of items	*/

                /* check if the bucket is reaching the threshold */
                if ( (curr->counts[c] % THRESHOLDMINUSONE) == 0)
                {
                    if ((*curr->nulltailptr = (char *) calloc(THRESHOLD, sizeof(char *))) == NULL)
                    {
                        ERR_PRINT_OUT_OF_MEMORY;
                        EXIT;
                    }
                    else
                        /* point to the first cell in the new array */
                        curr->nulltailptr = (char **) *curr->nulltailptr;
                }
            }
            else 
            {
                /* insert string in bucket and increment the item counter */
                curr->ptrs[c][curr->counts[c]++] = (char *) strings[i]; 

                /* 
                ** Staggered Approach
                ** if the size of the bucket is above level x, then realloc and increase the level count
                ** check for null string buckets as they are not to be incremented
                ** check if the number of items in the bucket is above a threshold
                */
                currcounts = curr->counts[c];
                if (currcounts < THRESHOLD && currcounts > (bucket_inc[curr->levels[c]] - 1) )
                    if((curr->ptrs[c]  = (char **) realloc(curr->ptrs[c], bucket_inc[++curr->levels[c]] * sizeof(char *))) == NULL)
                    {
                        ERR_PRINT_OUT_OF_MEMORY;
                        EXIT;
                    }
            }
           
            /* is bucket size above the THRESHOLD? */
            while ( curr->counts[c] >= THRESHOLD && c != 0 )
            { 
                p++; /* advance depth of character */

                /* allocate memory for new trie node */
                if ((new_ = (TRIE *) calloc(1, sizeof(TRIE))) == NULL)
                {
                    ERR_PRINT_OUT_OF_MEMORY;
                    EXIT;
                }

                /* burst ...*/
                 currcounts = curr->counts[c];

                 for(j=0; j < currcounts; j++) 
                 {
                     cc = curr->ptrs[c][j][p]; /* access the next depth character */

                     /* Insert string in bucket in new node, create bucket if necessary */
                     if(new_->counts[cc] < 1)
                     {
                         /* initialize the nullbucketsize, used to keep count
                         ** of the number of times the bucket has been reallocated
                         ** also make the nulltailptr point to the first element in the bucket
                         */
                         if (cc == 0)
                         {
                             if ((new_->ptrs[cc] = (char **) calloc(THRESHOLD, sizeof(char *))) == NULL)
                             {
                                 ERR_PRINT_OUT_OF_MEMORY;
                                 EXIT;
                             }
                             new_->nulltailptr = new_->ptrs[cc];        /* point to the first cell of the bucket */
                             *new_->nulltailptr = (char *) curr->ptrs[c][j]; /* insert the string */
                             new_->nulltailptr++;                       /* point to next cell */
                             new_->counts[cc]++;                        /* increment count of items */
                         }
                         else
                         {
                             if ((new_->ptrs[cc] = (char **) calloc(bucket_inc[1], sizeof(char *))) == NULL)
                             {
                                 ERR_PRINT_OUT_OF_MEMORY;
                                 EXIT;
                             }
                             /* insert string into bucket
                             ** increment the item counter for the bucket
                             ** increment the level counter for the bucket
                             */
                             new_->ptrs[cc][new_->counts[cc]++] = (char *) curr->ptrs[c][j];
                             new_->levels[cc]++;
                         }
                     }
                     else
                     {
                         /* insert the string in the buckets */
                         if (cc == 0)
                         {
                             *new_->nulltailptr = (char *) curr->ptrs[c][j]; /* insert the string */
                             new_->nulltailptr++;  /* point to next cell */
                             new_->counts[cc]++;    /* increment count of items */

                             /* check if the bucket is reaching the threshold */
                             if ( (new_->counts[cc] % THRESHOLDMINUSONE) == 0)
                             {
                                 if ((*new_->nulltailptr = (char *) calloc(THRESHOLD, sizeof(char *))) == NULL)
                                 {
                                     ERR_PRINT_OUT_OF_MEMORY;
                                     EXIT;
                                 }
                                 else
                                     /* point to the first cell in the new array */
                                     new_->nulltailptr = (char **) *new_->nulltailptr;
                             }
                         }
                         else
                         {
                             /* insert string in bucket and increment the item counter */
                             new_->ptrs[cc][new_->counts[cc]++] = (char *) curr->ptrs[c][j];
            
                             /*
                             ** Staggered Approach
                             ** if the size of the bucket is above level x, then realloc and increase the level count
                             ** check for null string buckets as they are not to be incremented
                             ** check if the number of items in the bucket is above a threshold
                             */
                             newcounts = new_->counts[cc];
                             if (newcounts < THRESHOLD && newcounts > (bucket_inc[new_->levels[cc]] - 1) )
                                 if((new_->ptrs[cc]  = (char **) realloc(new_->ptrs[cc], bucket_inc[++new_->levels[cc]] * sizeof(char *))) == NULL)
                                 {
                                    ERR_PRINT_OUT_OF_MEMORY;
                                    EXIT;
                                 }
                         } /*else */
                     } /* else */
                 } /* for */
                 /* after burst : free up old buckets, point to new node ... */
                 free(curr->ptrs[c]);
                 curr->ptrs[c] = (char **) new_;	/* old pointer points to the new trie node		*/
                 curr->counts[c] = -1;		/* flag to indicate pointer to trie node and not bucket	*/ 
                 curr = new_;			/* used to burst recursive, so point curr to new	*/
                 c = cc;			/* point to character used in previous string		*/
            } /* while */
         } /* else */
    } /* for */
} /* burstinsert */

/*
** Function : bursttraverseA
**
** Input    :
**    node    : pass the trie node on which to traverse, initially starts from the root
**    strings : all the strings
**    pos     : position in the original array, used to insert the strings back from the buckets
**    depth   : depth of character
**
** Output   : 
**    pos     : position in the original array, used to insert the strings back from the buckets 
*/
static int
bursttraverseA(TRIE *node, string strings[], int pos, unsigned short deep)
{
    int i, j, k; 
    int off = 0;
    unsigned short no_of_buckets=0, no_elements_in_last_bucket=0, no_elements_in_bucket=0; 
    char **nullbucket, **headbucket;
    int count;

    for( i=0 ; i < ALPHABET; i++)
    {
        count = node->counts[i];

        if( !++count )
        {
            pos = bursttraverseA((TRIE *) node->ptrs[i], strings, pos, deep+1);
        }
        else if (--count)
        {
            if(i==0)
            {
                no_of_buckets = (count/THRESHOLDMINUSONE) + 1;
                no_elements_in_last_bucket = count % THRESHOLDMINUSONE;
                nullbucket = node->ptrs[i];
                headbucket = node->ptrs[i];
                off=pos;

                /* traverse all arrays in the bucket */
                for (k=1; k <= no_of_buckets; k++)
                {
                    if(k==no_of_buckets)
                        no_elements_in_bucket = no_elements_in_last_bucket;
                    else
                        no_elements_in_bucket = THRESHOLDMINUSONE;

                    /* traverse all elements in each bucket */
                    for (j=0; j < no_elements_in_bucket ; j++, off++) 
                    {
			strings[off] = (unsigned char*) nullbucket[j];
                    }
                    nullbucket = (char **) nullbucket[j];
                    free(headbucket);
                    headbucket = nullbucket;
                }
            }
            else
            {
                for( j=0, off=pos ; j < count ; j++, off++)
                    strings[off] = (unsigned char*) node->ptrs[i][j];

                free( node->ptrs[i] );

                if (count > 1)
                {
                    if (count < INSERTBREAK)
                        inssort( strings+pos, off-pos, deep + 1);  
                    else
                        mkqsort( strings+pos, off-pos, deep + 1);
                }
            }
            pos = off;
        }
    }
    free(node);
    return pos; 
}

void
burstsortA(string strings[], size_t scnt)
{
    TRIE	*root;

    root = (TRIE *) calloc(1, sizeof(TRIE));

    (void) burstinsertA(root, strings, scnt);
    (void) bursttraverseA(root, strings, 0, 0);

    return;
}
