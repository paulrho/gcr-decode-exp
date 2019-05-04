int decode(long in);
#define DECODEBYTE    decb=(decode((keep&0x3F8)>>5)<<4)+decode(keep&0x1F)
