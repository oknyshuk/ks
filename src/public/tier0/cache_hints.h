#ifndef TIER0_CACHE_HINTS_HDR
#define TIER0_CACHE_HINTS_HDR

// Same for other platforms.
#define PREFETCH_128(POINTER,OFFSET)	{ /* Nothing to do here */ }
#define PREZERO_128(POINTER,OFFSET)											\
	{																		\
		intptr_t __tempPtr__ = (intptr_t)((char *)(POINTER) + (OFFSET));	\
		__tempPtr__ &= -128;												\
		memset((void*)__tempPtr__, 0, 128);									\
	}

// This exists for backward compatibility until a massive search and replace is done
#define PREFETCH_CACHE_LINE PREFETCH_128
// Indicate that the cache line is 128. It is not correct on PC, but this will have no side effects related to the macros above.
#define CACHE_LINE_SIZE	128

#ifdef IVP_VECTOR_INCLUDED
template<class T>
inline void UnsafePrefetchLastElementOf(IVP_U_Vector<T>&array)
{
	PREFETCH_128(array.element_at(array.len()-1),0);
}
template<class T>
inline void PrefetchLastElementOf(IVP_U_Vector<T>&array)
{
	if(array.len() > 0)
		PREFETCH_128(array.element_at(array.len()-1),0);
}
#endif

#endif 
