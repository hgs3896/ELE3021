#ifndef _H_FRACTION_
#define _H_FRACTION_
#include "types.h"

struct frac {
  uint num;
  uint denom;
};

typedef struct frac frac;

extern int frac_init(frac *self, uint num, uint denom);
extern int frac_add(frac *self, const frac *a, const frac *b);
extern int frac_sub(frac *self, const frac *a, const frac *b);
extern int frac_zero(frac *self);
extern int frac_is_less_than(const frac *lhs, const frac *rhs);
#endif