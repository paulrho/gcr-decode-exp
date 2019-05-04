#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int debug=0;

/**
RFI{date:"25/02/2019",time:"20:26:25",tracks:40,sides:1,rate:6250000,writeable:0}{track:0,side:0,rpm:281.53,enc:"rle",len:381683}
**/
void split1a() {

}

char buf[10000000];

enum {M_READINTRO,M_READHEAD,M_READTRACK_META,M_K,M_V,M_READBIN,M_READTRACK_END };
int m=0;
  long int n=0;
  int track=0;
  int side=0;
  void writesplitbuf() {
					   // out000-track39side0.raw
				     char filename[80];
				     //sprintf(filename,"newsplit/out000-track%02dside%d.raw",track,side);
				     sprintf(filename,"split/out000-track%02dside%d.raw",track,side);
				     if (debug) fprintf(stderr,"Opening %s to write %d bytes\n",filename,n);
				     FILE * fw=fopen(filename,"ab");
				     if (fw!=NULL) {
				       //write(fp,
				       for (int i=0; i<100; ++i) fprintf(fw,"%c%c",0xFF,0x00); // leader blank

				       //for (int i=0; i<n; ++i) fprintf(fw,"%c",buf[i]);
				       fwrite(buf,1,n,fw);

				       if (n%2==1) fprintf(fw,"%c",255);
				       for (int i=0; i<100; ++i) fprintf(fw,"%c%c",0xFF,0x00); // trailer blank
				       fclose(fw);
				     }
}
void main(int argc, char*argv[]) {
  // open the file
  // read disk header
  //   read the RFI head 3 byes
  //   read from { until }
  //   read track header
  //     read from { until } 
  //       read keyword : data, (data could be quotes
  for (int f=1; f<argc; ++f) {
//FILE * fp=fopen(argv[1],"rb");
  //if (debug) fprintf(stderr,"opening %s\n",argv[f]);
  if (debug) fprintf(stderr,"opening %s\n",argv[f]);
  FILE * fp=fopen(argv[f],"rb");

   // read it ALL in, in one chunk
  m=M_READINTRO;
  long int len=0;
  int v=0; int k=0;
  char key[100];
  char val[100];
  int w;
  n=0; w=0;

  int rr=0;
  int topr=0;
  char readbuf[1000000];
  while (1) {
    if (rr==topr) {
      topr=fread(readbuf,1,1000000,fp);
      //printf("top=%d\n",topr);
      rr=0;
    }
    if (rr>=topr) break;
    char a=readbuf[rr++];
    
    //char a=fgetc(fp);
    //if (feof(fp)) break;
    //printf("%c",a);

    switch(m) {
	    case M_READINTRO: if (a!='{') break; else { m++; } break;
	    case M_READHEAD: if (a!='}') break; else { m++; } break;
	    case M_READTRACK_META: if (a!='{') { if (debug) fprintf(stderr,"ERROR1 got=%02x\n",a); exit(1); } else { m++; k=0; v=0; } break;
	    case M_K: if (a!=':') { key[k++]=a; } else { key[k]=0; if (debug) fprintf(stdout,"key=%s\n",key); k=0; m++; } break;
	    case M_V: if (a!='}' && a!=',') { val[v++]=a; } 
	              else { 
		        val[v]=0; if (debug) fprintf(stdout,"val=%s\n",val);  v=0;
			if (strcmp(key,"track")==0) {
			  track=(int)atoi(val);
			  if (debug) fprintf(stdout,"track=%d\n",track);
			}
			if (strcmp(key,"side")==0) {
			  side=(int)atoi(val);
			  if (debug) fprintf(stdout,"side=%d\n",side);
			}
			if (strcmp(key,"len")==0) {
			  len=(long int)atoi(val);
			  if (debug) fprintf(stdout,"length=%ld\n",len);
			}
		        if (a=='}') { m++; n=0; w=0; if (debug) fprintf(stdout,"\n"); } else {m=M_K;} 
		      } break;
	    case M_READBIN: 
	                    if (n==len) { 
			           if (a!='{') {
					 if (debug) fprintf(stderr,"at end but no open brace n=%d len=%d a=%02x\n",n,len,a); 
			           } else {
			             m=M_K; 
				   }
	                    } else { /* this is binary data */
		              buf[n++]=a; 
	                      if (n==len) { writesplitbuf(); w=1; }
                            }
    } // switch
    //fprintf(stderr,"m=%d a=%c\n",m,a);
    //if (feof(fp)) break;
  } // while chars
  if (w==0) writesplitbuf(); // write out partial buf
  if (debug) printf("m=%d n=%ld\n",m,n);
  } // for each file
  if (debug) fprintf(stderr,"got to end\n");
}

