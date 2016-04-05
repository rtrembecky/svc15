// GPLv2

#ifdef __UINT32_TYPE__
typedef __UINT32_TYPE__ u_int32_t;
typedef __UINT32_TYPE__ uint32_t;
#else
typedef	unsigned int u_int32_t; //klee-uclibc/include/sys/types.h
typedef unsigned int uint32_t; //stdint.h
#endif

#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;
#else
#if __x86_64__
typedef unsigned long int size_t;
#else
typedef unsigned int size_t;
#endif
#endif

_Bool __VERIFIER_nondet__Bool();

/* add our own versions of malloc and calloc */
/* non-deterministically return memory or NULL */
void *__VERIFIER_malloc(size_t size)
{
	if (__VERIFIER_nondet__Bool())
		return ((void *) 0);

	void *mem = malloc(size);
	klee_make_symbolic(mem, size, "malloc");

	return mem;
}

void *memset(void *s, int c, size_t n);
void *__VERIFIER_calloc(size_t nmem, size_t size)
{
	if (__VERIFIER_nondet__Bool())
		return ((void *) 0);

	void *mem = malloc(nmem * size);
	/* do it symbolic, so that subsequent
	 * uses will be symbolic, but initialize it
	 * to 0s */
	klee_make_symbolic(mem, nmem * size, "calloc");
	memset(mem, 0, nmem * size);

	return mem;
}

/* this versions never return NULL */
void *__VERIFIER_malloc0(size_t size)
{
	void *mem = malloc(size);
	// NOTE: klee already assumes that
	//klee_assume(mem != (void *) 0);
	klee_make_symbolic(mem, size, "malloc0");

	return mem;
}

void *__VERIFIER_calloc0(size_t nmem, size_t size)
{
	void *mem = malloc(nmem * size);
	//klee_assume(mem != (void *) 0);
	klee_make_symbolic(mem, nmem * size, "calloc0");
	memset(mem, 0, nmem * size);

	return mem;
}

