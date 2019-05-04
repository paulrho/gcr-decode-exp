// dissappear it
// //#define dbgprintf(...)  { }
//#define DEBUG

#ifndef DEBUG
#define dbgprintf(...)    donothing()
#else
#define dbgprintf    fprintf
#endif

#define dbgprintf2    fprintf

extern void donothing();
