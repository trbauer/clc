// #ifdef USEIMG
//  image_2d_t output,
// #else

// note, what can we do if t is constant?
kernel blend2(
  global uchar4 * restrict output,
  const global uchar4 *img1,
  const global uchar4 *img2,
  const float t)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);  
    const int off = x + y*get_global_size(0);
    
    uchar4 in1 = img1[off];
    uchar4 in2 = img2[off];
    output[id] = clamp(mix(in1, in2, 0.5), 0, 255);
}
  

kernel blend2_50_50_50(
  global uchar4 * restrict output,
  const global uchar4 *img1,
  const global uchar4 *img2)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);  
    const int off = x + y*get_global_size(0);
    
    uchar4 in1 = img1[off];
    uchar4 in2 = img2[off];
    output[id] = clamp(mix(in1, in2, 0.5),0,255);
}
  
