#include <stdio.h>
#include <stdlib.h>
#include "display.h"

extern FILE * fd;
void main () {
	// run with : $ make display && ./display 3>&1 1>1
	// to switch stdout and 3
  fd=fdopen(3,"a");
  if (fd==NULL) exit(1);
  //test();
  dummy=1;
  printlayout();
  resetscreen();
  printf("this is normally stdout\n");
}

