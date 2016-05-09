
#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "point.h"

#define MODE_INTERPOLATION_QUICK   0
#define MODE_INTERPOLATION_LINEAR  1

void interpolate(point_t *P0, point_t *P1, int mode);
void interpolate_ramp(point_t *P0, point_t *P1);

#endif
