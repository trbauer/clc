#include "shared.h"

kernel void krnl(global float *out, global struct value *roots)
{
  out[get_global_id(0)] = sqrt(mag2(roots[get_global_id(0)]));
}