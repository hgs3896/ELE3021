#include "fraction.h"

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