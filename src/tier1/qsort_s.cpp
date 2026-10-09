/******************************************************************/
/* qsort_s.c  --  Non-Recursive ANSI Quicksort function             */
/*                                                                */
/* Public domain by Raymond Gardner, Englewood CO  February 1991  */
/*                                                                */
/* Usage:                                                         */
/*     qsort(base, nbr_elements, width_bytes, compare_function);  */
/*        void *base;                                             */
/*        size_t nbr_elements, width_bytes;                       */
/*        int (*compare_function)(const void *, const void *);    */
/*                                                                */
/* Sorts an array starting at base, of length nbr_elements, each  */
/* element of size width_bytes, ordered via compare_function,     */
/* which is called as  (*compare_function)(ptr_to_element1,       */
/* ptr_to_element2) and returns < 0 if element1 < element2,       */
/* 0 if element1 = element2, > 0 if element1 > element2.          */
/* Most refinements are due to R. Sedgewick. See "Implementing    */
/* Quicksort Programs", Comm. ACM, Oct. 1978, and Corrigendum,    */
/* Comm. ACM, June 1979.                                          */
/******************************************************************/

// modified to take (and use) a context object, ala Microsoft's qsort_s
// "extension" to the stdlib


import std;

/*
**  swap nbytes between a and b
*/

static void swap_bytes(char *a, char *b, std::size_t nbytes)
{
   char tmp;
   do {
      tmp = *a; *a++ = *b; *b++ = tmp;
   } while ( --nbytes );
}

constexpr std::size_t kInsertionSortThreshold = 7;	// subfiles of this many elements or fewer are sorted by insertion; at least 3

extern "C" void qsort_s(void *basep, std::size_t nelems, std::size_t size,
           int (*comp)(void *, const void *, const void *),
	   void *ctx)
{
   char *stack[40], **sp;       /* stack and stack pointer        */
   char *i, *j, *limit;         /* scan and limit pointers        */
   std::size_t thresh;               /* size of T elements in bytes    */
   char *base;                  /* base pointer as char *         */


// Compares two elements through the caller-supplied comparer.}
   auto Comp = [&]( void *ctx, char *a, char *b ) { return (*comp)( (void *)ctx, (void *)a, (void *)b ); };
   base = (char *)basep;        /* set up char * base pointer     */
   thresh = kInsertionSortThreshold * size;           /* init threshold                 */
   sp = stack;                  /* init stack pointer             */
   limit = base + nelems * size;/* pointer past end of array      */
   for ( ;; ) {                 /* repeat until break...          */
      if ( limit - base > thresh ) {  /* if more than T elements  */
                                      /*   swap base with middle  */
         swap_bytes( (char *)((((limit-base)/size)/2)*size+base), (char *)(base), size );
         i = base + size;             /* i scans left to right    */
         j = limit - size;            /* j scans right to left    */
         if ( Comp(ctx, i, j) > 0 )   /* Sedgewick's              */
            swap_bytes( (char *)(i), (char *)(j), size );               /*    three-element sort    */
         if ( Comp(ctx, base, j) > 0 )/*        sets things up    */
            swap_bytes( (char *)(base), (char *)(j), size );            /*            so that       */
         if ( Comp(ctx, i, base) > 0 )/*      *i <= *base <= *j   */
            swap_bytes( (char *)(i), (char *)(base), size );            /* *base is pivot element   */
         for ( ;; ) {                 /* loop until break         */
            do                        /* move i right             */
               i += size;             /*        until *i >= pivot */
            while ( Comp(ctx, i, base) < 0 );
            do                        /* move j left              */
               j -= size;             /*        until *j <= pivot */
            while ( Comp(ctx, j, base) > 0 );
            if ( i > j )              /* if pointers crossed      */
               break;                 /*     break loop           */
            swap_bytes( (char *)(i), (char *)(j), size );       /* else swap elements, keep scanning*/
         }
         swap_bytes( (char *)(base), (char *)(j), size );         /* move pivot into correct place  */
         if ( j - base > limit - i ) {  /* if left subfile larger */
            sp[0] = base;             /* stack left subfile base  */
            sp[1] = j;                /*    and limit             */
            base = i;                 /* sort the right subfile   */
         } else {                     /* else right subfile larger*/
            sp[0] = i;                /* stack right subfile base */
            sp[1] = limit;            /*    and limit             */
            limit = j;                /* sort the left subfile    */
         }
         sp += 2;                     /* increment stack pointer  */
      } else {      /* else subfile is small, use insertion sort  */
         for ( j = base, i = j+size; i < limit; j = i, i += size )
            for ( ; Comp(ctx, j, j+size) > 0; j -= size ) {
               swap_bytes( (char *)(j), (char *)(j+size), size );
               if ( j == base )
                  break;
            }
         if ( sp != stack ) {         /* if any entries on stack  */
            sp -= 2;                  /* pop the base and limit   */
            base = sp[0];
            limit = sp[1];
         } else                       /* else stack empty, done   */
            break;
      }
   }
}


