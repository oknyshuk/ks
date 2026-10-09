
// Copyright (c) 2026 oknyshuk

export module ks.tier1:qsort_s;

import std;
import ks.math;

export extern "C++" {

/******************************************************************/
/* qsort_s.h  --  Non-Recursive ANSI Quicksort function           */
/*                                                                */
/* Public domain by Raymond Gardner, Englewood CO  February 1991  */
/*                                                                */
/* Usage:                                                         */
/*     qsort(base, nbr_elements, width_bytes, compare_function);  */
/*        void *base;                                             */
/*        std::size_t nbr_elements, width_bytes;                       */
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

extern "C" void qsort_s(void *basep, std::size_t nelems, std::size_t size,
           int (*comp)(void *, const void *, const void *),
	   void *ctx);

}
