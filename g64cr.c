#include <stdio.h>
#include <stdlib.h>
/* implement simple g64 */
#define RAWADD 0
//#define ACTADD 230
#define ACTADD 50
void main() 
{
  //int tsize=7928;
  int tsize=7928+RAWADD;
  //int tsize=17928;
  printf("%8s%c%c%c%c","GCR-1541",0x00,84, /* size */ tsize&0xFF,(tsize>>8)&0xFF);
  long tstart=0x02AC; 
  for (int t=2; t<=85; ++t) {
	  long v;
	  if (t%2==1 || t>70) {
	    v=0;
	    printf("%c%c%c%c",0,0,0,0);
	  } else {
	    v=tstart+(t/2-1)*(tsize+2);
	    printf("%c%c%c%c",v&0xFF,(v>>8)&0xFF,(v>>16)&0xFF,(v>>24)&0xFF);
	  }
	  fprintf(stderr,"Track %.1f at %08X\n",t/2.0,v);
  }
  /* speed zones, four bytes for each track */
  for (int t=2; t<=85; ++t) {
	  int sz;
	  if (t%2==1 || t>70) {
		  printf("%c%c%c%c",0x00,0x00,0x00,0x00);
	  } else {
		  if (t/2<=17) sz=3;
		  else if (t/2<=24) sz=2;
		  else if (t/2<=30) sz=1;
		  else sz=0;
		  printf("%c%c%c%c",sz,0x00,0x00,0x00);
	  }

  }
  for (int t=2; t<=85; ++t) {
	  if (t%2==1 || t>70) {
	  } else {
	    //for (int i=0; i<asize; ++i) printf("%c",0xda);
	     FILE *fi;
		    char filename[80];
		    char buff[20000];
		    //sprintf(filename,"../keepdata/track%02d.raw",t/2);
		    sprintf(filename,"../data/track%02d.raw",t/2);
		    fi=fopen(filename,"rb");
		    if (fi==NULL) {fprintf(stderr,"could not open %s\n",filename); exit(2); }

               long int n=fread(buff, 1, /*MAX*/ 20000, fi);
	       fprintf(stderr,"got %d bytes\n",n);
               //long asize=7692+RAWADD+ACTADD; // actual size
               long asize=n;
	       printf("%c%c",asize&0xFF,(asize>>8)&0xFF);
	       /* data goes here  asize */
	       for (int i=0; i<asize; ++i) printf("%c",buff[i]);  
	       fclose(fi);
	    
	    /* end filler bytes here  tsize-asize */
	    for (int i=0; i<tsize-asize; ++i) printf("%c",0xFF);
	  }
  }
}
