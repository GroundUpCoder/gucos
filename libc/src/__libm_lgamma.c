#include <math.h>
#include <__libm.h>

double lgamma(double x)
{
	return __lgamma_r(x, &__signgam);
}
