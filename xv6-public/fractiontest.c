#include "types.h"
#include "stat.h"
#include "user.h"

typedef struct frac frac;

struct frac {
  uint num;
  uint denom;
};

static uint
_gcd(uint a, uint b)
{
  uint r;
  while((r = a % b)) {
    a = b;
    b = r;
  }
  return b;
}

static uint
_lcm(uint a, uint b)
{
  return a / _gcd(a, b) * b;
}

int
frac_init(frac *self, uint num, uint denom)
{
  if(self == 0 || denom == 0)
    return -1;

  uint gcd = _gcd(num, denom);
  self->num = num / gcd;
  self->denom = denom / gcd;
  return 0;
}

int
frac_add(frac *self, const frac *a, const frac *b)
{
  if(self == 0 || a == 0 || b == 0)
    return -1;

  uint denom = _lcm(a->denom, b->denom);
  uint num = a->num * (denom / a->denom) + b->num * (denom / b->denom);
  return frac_init(self, num, denom);
}

int
frac_sub(frac *self, const frac *a, const frac *b)
{
  if(self == 0 || a == 0 || b == 0)
    return -1;

  uint denom = _lcm(a->denom, b->denom);
  uint num = a->num * (denom / a->denom) - b->num * (denom / b->denom);
  return frac_init(self, num, denom);
}

int
frac_zero(frac *self)
{
  return frac_init(self, 0, 1);
}

int
frac_is_less_than(const frac *a, const frac *b)
{
  uint denom = _lcm(a->denom, b->denom);
  return a->num * (denom / a->denom) < b->num * (denom / b->denom);
}

unsigned long randstate = 1;
static unsigned int
rand()
{
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}

int
main(int argc, char *argv[])
{
  int i;
  frac a = {}, b = {}, c = {};
  for(i = 0; i < 10; i++) {
    if(!frac_init(&a, rand() % 101, rand() % 100 + 1) &&
       !frac_init(&b, rand() % 101, rand() % 100 + 1) &&
       !frac_add(&c, &a, &b)) {
      printf(1, "%d/%d + %d/%d = %d/%d\n", a.num, a.denom, b.num, b.denom,
             c.num, c.denom);
    }
  }

  for(i = 0; i < 10; i++) {
    if(!frac_init(&a, rand() % 11, rand() % 10 + 1) &&
       !frac_init(&b, rand() % 11, rand() % 10 + 1)) {
      char sign = '=';
      if(frac_is_less_than(&a, &b))
        sign = '<';
      if(frac_is_less_than(&b, &a))
        sign = '>';
      printf(1, "%d/%d %c %d/%d\n", a.num, a.denom, sign, b.num, b.denom);
    }
  }

  if(!frac_init(&a, rand() % 11, rand() % 10 + 1)) {
    char sign = '=';
    if(frac_is_less_than(&a, &a))
      sign = '<';
    if(frac_is_less_than(&a, &a))
      sign = '>';
    printf(1, "%d/%d %c %d/%d\n", a.num, a.denom, sign, a.num, a.denom);
  }

  exit();
}