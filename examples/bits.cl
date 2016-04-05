kernel void popcount(
  global int *output,
  const global int *inp)
{
    const int val = inp[get_global_size(0)];
    output[off] = popcount(val);
}

kernel void popcount(
  global int *output,
  const global int *inp)
{
    const int val = inp[get_global_size(0)];
    output[off] = clz(val);
}

kernel void rotate(
  global int *output,
  const global int *inp, 
  int k)
{
    const int val = inp[get_global_size(0)];
    output[off] = rotate(val, k);
}

kernel void rotateK(
  global int *output,
  const global int *inp)
{
    const int val = inp[get_global_size(0)];
    output[off] = rotate(val, 13);
}
