
#include <stdint.h>
#include <math.h>
#include "local.h"

double infinity(void)
{
  union {double d; uint64_t u;} u;
  u.u = 0x7ffULL << 52;
  return u.d;
}

double fabs(double x)
{
  return x < 0 ? -x : x;
}

double atan2(double x, double y) { return atan(x/y); }
double sinh(double x) { return (exp(x)-exp(-x))/2.0; }
double cosh(double x) { return (exp(x)+exp(-x))/2.0; }
double tanh(double x) { return sinh(x)/cosh(x); }
double asin(double x) { return atan(x/sqrt(1-x*x)); }
double acos(double x) { return atan(sqrt(1-x*x)/x); }
double asinh(double x) { return log(x+sqrt(x*x+1)); }
double acosh(double x) { return log(x+sqrt(x*x-1)); }
double atanh(double x) { return log((1+x)/(1-x)) / 2.0; }

double floor(double x)
{
  long l = (long)x;
  if (l > x) --l;
  return (double)l;
}

double ceil(double x)
{
  long l = (long)x;
  if (l < x) ++l;
  return (double)l;
}

double fmod(double x, double y)
{
  return x - floor(x/y)*y;
}

double nearbyint(double x) { return x; }

double trunc(double x)
{
  return (double)(long)x;
}

double modf(double x, double *y)
{
  long l = (long)x;
  if (l > x) --l;
  *y = (double)l;
  return *y-x;
}

double tan(double x) { return sin(x)/cos(x); }

double ldexp(double value, int mult)
{
        union {double d; uint64_t u;} u;

        if (value) {
                u.d = value;
                int sgn = u.u >> 63;
                int exp = (int) ( (u.u & 0x7fffffffffffffffULL) >> 52 );
                if (exp != 0x7ff) {
                        exp += mult;
                        if ((mult >= 4096) || (exp >= 2047)) return (sgn ? -infinity() : infinity());
                        if ((mult <= -4096) || (exp < -60)) return (sgn ? -0.0 : 0.0);
                        if (exp <= 0)
                          {
                            // deal with (probably rarely encountered) sub-normalized case
                            uint64_t rnd = ((u.u & 0xfffffffffffffULL ) | 0x10000000000000ULL) >> (-exp);
                            u.u = (rnd >>1) + (rnd&1);
                            return (sgn ? -u.d : u.d);
                          }
                        else
                          {
                            // replace the exponent with the new adjusted exponent
                            u.u &= ~(2047ULL << 52);
                            u.u |= ((uint64_t)exp) << 52;
                          }
                }
                if (0) printm("mult = %d, ldexp = %f\n", mult, u.d);
                return u.d;
        } else {
                return 0.0;
        }
}

double frexp(double value, int *eptr)
{
        union{ double d; uint64_t u;}u;

        *eptr = 0;
        u.d = value;
        if (value) {
          int sgn = u.u >> 63;
          int32_t exp = (uint32_t) ( (u.u & 0x7fffffffffffffffULL) >> 52 );
          if (exp == 0)
            {
              int i;
              // deal with (probably rarely encountered) sub-normalized case
              uint64_t rnd = u.u & 0xfffffffffffffULL;
              *eptr = -1021;
              for (i = 0; i < 52; i++) if (0x10000000000000ULL & ~rnd)
                {
                rnd <<= 1;
                (*eptr)--;
                }
              u.u = (sgn ? (1ULL<<63) : 0) | (1022ULL << 52) | (rnd & 0xfffffffffffffULL);
            }
          else if (exp != 0x7ff)
            {
              *eptr = exp - 1022;
              u.u &= ~(2047ULL << 52);
              u.u |= 1022ULL << 52;
            }
        }
        return (u.d);
}

long long longofdouble(double value)
{
        union{ double d; uint64_t u;}u;

        u.d = value;
        if (value) {
          int sgn = u.u >> 63;
          int32_t exp = (uint32_t) ( (u.u & 0x7fffffffffffffffULL) >> 52 );
          long long mant = (u.u & 0xfffffffffffffULL) | 0x10000000000000ULL;
          if (exp == 0) return 0;
          else if (exp != 0x7ff)
            {
#ifdef VERBOSE_DEBUG
              printf("exp = %d, mant(hi,lo) = %X,%X\n",
                     exp, (unsigned)(mant>>32), (unsigned)(mant&0xFFFFFFFF));
#endif
              if (exp < 1075)
                mant = (sgn ? -mant : mant) >> (1075-exp);
              else
                mant = (sgn ? -mant : mant) << (exp-1075);
#ifdef VERBOSE_DEBUG
              printf("after shift, mant(hi,lo) = %X,%X\n",
                     (unsigned)(mant>>32), (unsigned)(mant&0xFFFFFFFF));
#endif
              return mant;
            }
          else return u.u | 0x7FFFFFFFFFFFFFFF;
        }
        return 0;
}
int myfpclassify( double value )
{
  union {double d; uint64_t u;} u = {value};

          int32_t exp = (uint32_t) ( (u.u & 0x7fffffffffffffffULL) >> 52 );
          uint64_t mant = u.u & 0xfffffffffffffULL;
          if (exp == 0)
            {
              // deal with (probably rarely encountered) sub-normalized case
              if (mant) return FP_SUBNORMAL;
              else return FP_ZERO;
            }
          else if (exp != 0x7ff)
              return FP_NORMAL;
          else if (mant)
            return FP_NAN;
          else
            return FP_INFINITE;
}

double exp_2(double x)
{
  int n, i = 0;
  double e = 1.0;
  double f = frexp(x, &n);
  double m = ldexp(f, 49 + (n < 0 ? n : 0));
#ifdef VERBOSE_DEBUG
  printf("x = %.12f, f = %.12f, m = %.12f, n = %d\n", x, f, m, n);
#endif
  long long mant = longofdouble(m);
  if (x != 0.0)
    {
#ifdef VERBOSE_DEBUG
      printf("x = %f, exp = %d, mant(hi,lo) = %X,%X\n",
          x, n, (unsigned)(mant>>32), (unsigned)(mant&0xFFFFFFFF));
#endif
      for (i = 0; i < sizeof(lookup)/sizeof(*lookup); i++)
        {
          if (mant & (1LL<<48))
            {
              e *= lookup[i];
#ifdef VERBOSE_DEBUG
              printf("i = %d, e = %f\n", i, e);
#endif
            }
          mant = (0xFFFFFFFFFFFFLL & mant) << 1;
        }
      for (i = 0; i < 128; i++) if (i < n)
        {
          e *= e;
#ifdef VERBOSE_DEBUG
          printf("n = %d, e = %f\n", n, e);
#endif
        }
    }
  return e;
}

double exp(double x)
{
  double e;
  // double ex = exp(x);
  /* exp_2(x) = exp(ln(2)*x) */
#ifdef VERBOSE_DEBUG
  printf("x = %.12f, one_log2 = %.12f\n", x, one_log2);   
#endif
  if (x > 0.0)
    e = exp_2(x * one_log2);
  else
    e = 1.0/exp_2(-x * one_log2);
#ifdef VERBOSE_DEBUG
  printf("myexp(%.12f) = %.12f\n", x, e);   
#endif
  return e;
}

double sqrt(double x)
{
  int n;
  double f = frexp(x, &n);
  double m = ldexp(f, n/2);
  double old_m;
  if (m!=0.0 && (myfpclassify (x) == FP_NORMAL)) do
    {
      old_m = m;
      m = (old_m + x/old_m)/2.0;
#ifdef VERBOSE_DEBUG
      printm("x = %.15f, expon = %d, m = %.15f\n", x, n, m);
#endif
    }
  while (old_m != m);
#ifdef VERBOSE_DEBUG
  printm("sqrt(%f) = %f\n", x, m);
#endif
  return m;
}
/* Natural log. Uses the fact that ln(x^2) = 2*ln(x)
    The series used is:
       ln(x) = 2(a+a^3/3+a^5/5+...) where a=(x-1)/(x+1)
*/

double log(double x) {
  double v;
  int f = 2;
  int i = 3;
  /* return something for the special case. */
  if (x <= 0.0) return -infinity();

  /* Precondition x to make .7 < x < 1.5. */
  while ((x >= 1.5) || (x <= .7)) {  /* for large/small numbers */
    f <<= 1;
    x = sqrt(x);
  }

  /* Set up the loop. */
  double n = (x-1)/(x+1);
  double ve = n;
  double m = n*n;
  /* Sum the series. */
  do {
    v = ve;
    ve = v + (n *= m) / i;
    // printf("x = %.15f, i = %d, v = %.15f\n", x, i, v);
    i += 2;
  }
  while (ve != v);
  return f*v;
}

double pow(double x, double y)
{
  return exp(log(x)*y);
}

/* Sin(x)  uses the standard series:
   sin(x) = x - x^3/3! + x^5/5! - x^7/7! ... */

double sin(double x) {
  double  b, e, i, m, s, v, z;
  int n;
  
  /* precondition x. */
  v = atan(1);
  if (x < 0) {
    m = 1;
    x = -x;
  }
  n = (x / v + 2 )/4;
    x = x - 4*n*v;
    if (n%2) x = -x;

  /* Do the loop. */
    v = e = x;
    s = -x*x;
  for (i=3; 1; i+=2) {
    e *= s/(i*(i-1));
    if (e == 0) {
      if (m) return (-v/1);
      return (v/1);
    }
    v += e;
  }
}

/* Cosine : cos(x) = sin(x+pi/2) */
double cos(double x) {
  double b, v, z;

  v = sin(x+atan(1)*2);
  return (v/1);
}

double atan(double x) {
  double a, b, e, f, i, m, n, s, v, z;

  f = 0.0;
  
  /* a is the value of a(.2) if it is needed. */
  /* f is the value to multiply by a in the return. */
  /* e is the value of the current term in the series. */
  /* v is the accumulated value of the series. */
  /* m is 1 or -1 depending on x (-x -> -1).  results are divided by m. */
  /* i is the denominator value for series element. */
  /* n is the numerator value for the series element. */
  /* s is -x*x. */

  /* Negative x? */
  m = 1;
  if (x<0) {
    m = -1;
    x = -x;
  }

  /* Special case and for fast answers */
  if (x==1) {
    return (.7853981633974483096156608/m);
  }
  if (x==.2) {
    return (.1973955598498807583700497/m);
  }

  /* Note: a and f are known to be zero due to being double vars. */
  /* Calculate atan of a known number. */ 
  if (x > .2)  {
    a = atan(.2);
  }
   
  /* Precondition x. */
  while (x > .2) {
    f += 1;
    x = (x-.2) / (1+x*.2);
  }

  /* Initialize the series. */
  v = n = x;
  s = -x*x;

  /* Calculate the series. */
  for (i=3; 1; i+=2) {
    e = (n *= s) / i;
    if (e == 0) {
      return ((f*a+v)/m);
    }
    v += e;
  }
}
