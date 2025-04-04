/* @(#)e_lgamma.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include <sys/cdefs.h>
#include "math.h"
#include "math_private.h"

extern int signgam;

long double lgammal_r(long double x, int *signgamp);

long double
lgammal(long double x)
{
	return lgammal_r(x,&signgam);
}
