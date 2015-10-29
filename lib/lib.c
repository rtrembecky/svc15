// GPLv2

#define FEXP_MASK 0x7f800000
#define SIGN_MASK 0x80000000
#define dHighMan 0x000FFFFF
#define dExpMask 0x7FF00000
#define dSgnMask 0x80000000
#define FEXP_MASK 0x7f800000
#define FFRAC_MASK 0x007fffff

/* All floating-point numbers can be put in one of these categories.  */
enum
  {
    FP_NAN,
# define FP_NAN FP_NAN
    FP_INFINITE,
# define FP_INFINITE FP_INFINITE
    FP_ZERO,
# define FP_ZERO FP_ZERO
    FP_SUBNORMAL,
# define FP_SUBNORMAL FP_SUBNORMAL
    FP_NORMAL
# define FP_NORMAL FP_NORMAL
  };

typedef	unsigned int u_int32_t; //klee-uclibc/include/sys/types.h
typedef unsigned long int uint32_t; //stdint.h
typedef unsigned int size_t;
typedef unsigned long uintptr_t;

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
void klee_assume(uintptr_t condition);

/* add our own versions of malloc and calloc */
void *__VERIFIER_malloc(size_t size)
{
	extern void *malloc(size_t);

	void *mem = malloc(size);
	klee_make_symbolic(mem, size, "malloc");

	return mem;
}

void *__VERIFIER_calloc(size_t nmem, size_t size)
{
	extern void *calloc(size_t, size_t);

	void *mem = calloc(nmem, size);
	klee_make_symbolic(mem, nmem * size, "calloc");

	return mem;
}

void *__VERIFIER_malloc0(size_t size)
{
	extern void *malloc(size_t);

	void *mem = malloc(size + 16);
	klee_assume(mem != (void *) 0);
	klee_make_symbolic(mem, size + 16, "malloc");

	return mem;
}

void *__VERIFIER_calloc0(size_t nmem, size_t size)
{
	extern void *calloc(size_t, size_t);

	void *mem = calloc(nmem, size);
	klee_assume(mem != (void *) 0);
	klee_make_symbolic(mem, nmem * size, "calloc");

	return mem;
}

extern void __VERIFIER_error(void);

extern void __assert_fail (__const char *__assertion, __const char *__file,
			   unsigned int __line, __const char *__function);

void __VERIFIER_error(void)
{
	/* FILE and LINE will be wrong, but that doesn't matter, klee will
	   replace this call by its own handler anyway */
	__assert_fail("verifier assertion failed", __FILE__, __LINE__, __func__);
}

void __VERIFIER_assert(int expr) __attribute__((weak));
void __VERIFIER_assert(int expr)
{
	if (!expr)
		__assert_fail("verifier assertion failed", __FILE__, __LINE__, __func__);
}

void __VERIFIER_assume(int expr)
{
	klee_assume(expr);
}

#define MAKE_NONDET(type)				\
type __VERIFIER_nondet_ ## type(void)			\
{							\
	type x;						\
	klee_make_symbolic(&x, sizeof(x), # type);	\
	return x;					\
}

MAKE_NONDET(char);
MAKE_NONDET(short);
MAKE_NONDET(int);
MAKE_NONDET(long);
MAKE_NONDET(float);
MAKE_NONDET(double);
MAKE_NONDET(_Bool);

_Bool __VERIFIER_nondet_bool(void)
{
	return __VERIFIER_nondet__Bool();
}

#undef MAKE_NONDET

#define MAKE_NONDET(type)				\
type nondet_ ## type(void)				\
{							\
	return __VERIFIER_nondet_ ## type();		\
}

MAKE_NONDET(char);
MAKE_NONDET(short);
MAKE_NONDET(int);
MAKE_NONDET(long);

#undef MAKE_NONDET

#define MAKE_NONDET(type)				\
type __VERIFIER_nondet_u ## type(void)			\
{							\
	return __VERIFIER_nondet_ ## type();		\
}

MAKE_NONDET(char);
MAKE_NONDET(short);
MAKE_NONDET(int);
MAKE_NONDET(long);

#undef MAKE_NONDET

void *__VERIFIER_nondet_pointer()
{
	void *x;						\
	klee_make_symbolic(&x, sizeof(void *), "void*");	\
	return x;					\
}

/* these are crippled */

unsigned int __VERIFIER_nondet_u32()
{
	return __VERIFIER_nondet_uint();
}

unsigned int __VERIFIER_nondet_U32()
{
	return __VERIFIER_nondet_uint();
}

unsigned int __VERIFIER_nondet_u8()
{
	return __VERIFIER_nondet_uchar();
}

/* these type are not sane, but benchmarks use them */
unsigned int __VERIFIER_nondet_U8()
{
	return __VERIFIER_nondet_uchar();
}

unsigned int __VERIFIER_nondet_u16()
{
	return __VERIFIER_nondet_ushort();
}

unsigned int __VERIFIER_nondet_U16()
{
	return __VERIFIER_nondet_ushort();
}

unsigned int __VERIFIER_nondet_unsigned()
{
	return __VERIFIER_nondet_uint();
}

char *__VERIFIER_nondet_pchar()
{
	return __VERIFIER_nondet_pointer();
}

void *kzalloc(int size, int gfp)
{
	extern void *malloc(size_t size);
	return malloc(size);
}

int __fpclassifyf ( float x )
{
   unsigned int iexp;

   union {
      u_int32_t lval;
      float fval;
   } z;

   z.fval = x;
   iexp = z.lval & FEXP_MASK;                 /* isolate float exponent */

   if (iexp == FEXP_MASK) {                   /* NaN or INF case */
      if ((z.lval & 0x007fffff) == 0)
         return FP_INFINITE;
	return FP_NAN;
   }

   if (iexp != 0)                             /* normal float */
      return FP_NORMAL;

   if (x == 0.0)
      return FP_ZERO;             /* zero */
   else
      return FP_SUBNORMAL;        /* must be subnormal */
}

typedef struct                   /*      Hex representation of a double.      */
      {
#if (__BYTE_ORDER == __BIG_ENDIAN)
      uint32_t high;
      uint32_t low;
#else
      uint32_t low;
      uint32_t high;
#endif
      } dHexParts;


#if 0
int
__signbitf (float __x)
{
  __extension__ union { float __f; int __i; } __u = { .__f = __x };
  return __u.__i < 0;
}

int
__signbit (double __x)
{
  __extension__ union { double __d; int __i[2]; } __u = { .__d = __x };
  return __u.__i[1] < 0;
}

int
__signbitl (long double __x)
{
  __extension__ union { long double __l; int __i[3]; } __u = { .__l = __x };
  return (__u.__i[2] & 0x8000) != 0;
}
#endif

int __signbitf ( float x )
{
   union {
      u_int32_t lval;
      float fval;
   } z;

   z.fval = x;
   return ((z.lval & SIGN_MASK) != 0);
}

int __signbitl (long double __x)
{
  return __signbit ((double)__x);
}

int __signbit ( double arg )
{
      union
            {
            dHexParts hex;
            double dbl;
            } x;
      int sign;

      x.dbl = arg;
      sign = ( ( x.hex.high & dSgnMask ) == dSgnMask ) ? 1 : 0;
      return sign;
}

int __fpclassify ( double arg )
{
	register unsigned int exponent;
      union
            {
            dHexParts hex;
            double dbl;
            } x;

	x.dbl = arg;

	exponent = x.hex.high & dExpMask;
	if ( exponent == dExpMask )
		{
		if ( ( ( x.hex.high & dHighMan ) | x.hex.low ) == 0 )
			return FP_INFINITE;
		else
            	return FP_NAN;
		}
	else if ( exponent != 0)
		return FP_NORMAL;
	else {
		if ( arg == 0.0 )
			return FP_ZERO;
		else
			return FP_SUBNORMAL;
		}
}

int __isinff ( float x )
{
    int class = __fpclassifyf(x);
    if ( class == FP_INFINITE ) {
	return ( (__signbitf(x)) ? -1 : 1);
    }
    return 0;
}

int __isinf ( double x )
{
    int class = __fpclassify(x);
    if ( class == FP_INFINITE ) {
	return ( (__signbit(x)) ? -1 : 1);
    }
    return 0;
}

int __isinfl ( long double x )
{
    int class = __fpclassify(x);
    if ( class == FP_INFINITE ) {
	return ( (__signbit(x)) ? -1 : 1);
    }
    return 0;
}

int __isnanf ( float x )
{
   union {
      u_int32_t lval;
      float fval;
   } z;

   z.fval = x;
   return (((z.lval&FEXP_MASK) == FEXP_MASK) && ((z.lval&FFRAC_MASK) != 0));
}

int __isnan ( double x )
{
	int class = __fpclassify(x);
	return ( class == FP_NAN );
}

int __isnanl ( long double x )
{
	int class = __fpclassify(x);
	return ( class == FP_NAN );
}

int
fesetround (int round)
{
  #define SPEFSCR_FRMC (0x00000003)
  unsigned long fpescr;

  if ((unsigned int) round > 3)
    return 1;

  fpescr = (fpescr & ~SPEFSCR_FRMC) | (round & 3);
  #undef SPEFSCR_FRMC

  return 0;
}
