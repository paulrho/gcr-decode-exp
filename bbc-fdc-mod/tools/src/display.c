//#define TYPE35
//#define TYPE1541
#ifndef TYPE35
#define TYPE1571
#endif

#include "display.h"

#ifdef TYPE35
#include "display3p5.c"
#endif
#ifdef TYPE1571
#include "display1571.c"
#endif
