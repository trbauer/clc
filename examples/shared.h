struct value {
    float r;
    float i;
};

// shared between host and device
float mag2(struct value v)
{
    return v.r*v.r + v.i*v.i;
}
