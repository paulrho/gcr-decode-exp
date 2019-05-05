#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "display.h"

char * CSI="\e[";

int display_dummy=0;
  FILE * fd;
void cls()
{
  fprintf(fd,"%s2J",CSI);
}

void cursorxy(int x, int y) {
  fprintf(fd,"%s%d;%dH",CSI,y,x);
}

/*
   ESC[ 38;5;<n> m Select foreground color
   ESC[ 48;5;<n> m Select background color
      0-  7:  standard colors (as in ESC [ 30–37 m)
      8- 15:  high intensity colors (as in ESC [ 90–97 m)
    16-231:  6 × 6 × 6 cube (216 colors): 16 + 36 × r + 6 × g + b (0 ≤ r, g, b ≤ 5)
   232-255:  grayscale from black to white in 24 steps
*/
void colorfg_rgb(int r,int g,int b) { fprintf(fd,"%s38;5;%dm",CSI,16+36*r+6*g+b); }
void colorbg_rgb(int r,int g,int b) { fprintf(fd,"%s48;5;%dm",CSI,16+36*r+6*g+b); }
void colorfg(int c) { 
	//fprintf(fd,"%s39m",CSI); 
	fprintf(fd,"%s38;5;%dm",CSI,c); 
	//fprintf(fd,"%s%dm",CSI,90+c%8); 

}
void colorbg(int c) { 
	//if (c==0) {
	  //fprintf(fd,"%s49m",CSI); 
	//} else 
	  fprintf(fd,"%s48;5;%dm",CSI,c); 
	//fprintf(fd,"%s48;2;%d;%d;%dm",CSI,255,255,0); 
}

void defaultcolor()
{
  fprintf(fd,"%s39m",CSI);
  fprintf(fd,"%s49m",CSI);
}

void test() {
	cls();
	cursorxy(42,22); fprintf(fd,"TEST1");
	cursorxy(27,12); fprintf(fd,"TEST2");
	cursorxy(4,9); fprintf(fd,"TEST3");
	for (int j=0; j<20; ++j) {
	  for (int i=0; i<80; ++i) {
		  if (i<40) {
		colorfg_rgb(i%3,j%5,i%6);
		colorbg_rgb(j%4,i%3+2,j%5);
		  } else {
			  colorfg(i%16);
			  colorbg(j%16);
		  }
		cursorxy(i,j);
		fprintf(fd,"X");
	  }
	}
	cursorxy(0,25);
}
void printlayout() {
	cls();
  for (int h=0; h<2; ++h) {
    for (int s=0;s<=21;s+=5) {  //extra one
	cursorxy(s+7+h*26,1); fprintf(fd,"%01d",s/10);
	cursorxy(s+7+h*26,2); fprintf(fd,"%01d",s%10);
    }
  }
  for (int t=0;t<35;++t) {
    for (int h=0; h<2; ++h) {
      for (int s=0;s<21-2*(t>=18-1)-(t>=25-1)-(t>=31-1);++s) {
	      if (s==0&&t+1<=35) {
	colorbg_rgb(0,0,0);
	cursorxy(0,t+3); colorfg_rgb(2,2,2);
	fprintf(fd,"T%02d:",t+1);
	      }
	cursorxy(7+s+h*26,t+3);
	colorfg_rgb(4,4,0);
	if (s%5==0) 
	  colorbg_rgb(0,0,1);
	else
	  colorbg_rgb(0,0,0);
	if (display_dummy) {
	  if (t>23) {
	    fprintf(fd,"?");
	  } else if (t==23 && s>5 && h==1) {
	    fprintf(fd,"?");
	  } else
	    fprintf(fd,".");
	} else {
	    fprintf(fd,"?");
	}
	colorbg_rgb(0,0,0); // only need to do this every fifth
      }
    }
  }

  // show where head is
  if (display_dummy) { int t=23;
	colorfg_rgb(2,5,2);
	cursorxy(30,t+3); 
	fprintf(fd,">"); 
	//fprintf(fd,"<"); 
  }
  display_info();
  fflush(fd);
}

char dts[64];
char * gettime() {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(dts, sizeof(dts), "%c", tm);
  //printf("%s\n", dts);
  return dts;
}
void
display_info_filename(char * str) {
	colorfg_rgb(3,3,3); 
	colorbg_rgb(0,0,0); 
  cursorxy(92,2); fprintf(fd,"File   :");
  fprintf(fd,str);
}
void
display_info_finish() {
	colorfg_rgb(3,3,3); 
	colorbg_rgb(0,0,0); 
cursorxy(92,10); fprintf(fd,"Finish :"); fprintf(fd,gettime());
}
void display_info_sig(long sig) {
	colorfg_rgb(3,3,3); 
	colorbg_rgb(0,0,0); 
cursorxy(92,5);  fprintf(fd,"Sig    :");  fprintf(fd,"%04lx",sig);
}

int infoline=0;
void 
display_info_add(char *head, char *info) {
	colorfg_rgb(3,3,4); 
	colorbg_rgb(0,0,0); 
cursorxy(92,infoline++);  fprintf(fd,"%-18s:",head);
                          fprintf(fd,"%s",info);
}

void display_flush() {
  fflush(fd);
}

void
display_info() {
	colorfg_rgb(3,3,3); 
	colorbg_rgb(0,0,0); 
cursorxy(92,2);  fprintf(fd,"File   :");
cursorxy(92,3);  fprintf(fd,"Type   :");
cursorxy(92,4);  fprintf(fd,"Label  :");
cursorxy(92,5);  fprintf(fd,"Sig    :");
cursorxy(92,6);  fprintf(fd,"Start  :"); fprintf(fd,gettime());
cursorxy(92,7);  fprintf(fd,"Elapsed:");
cursorxy(92,8);  fprintf(fd,"Good   :");
cursorxy(92,9);  fprintf(fd,"Errors :");
cursorxy(92,10); fprintf(fd,"Finish :");
infoline=12;
}

void
updatesector(int tc64, int h, int s, int show) {
	int t=tc64-1;
	cursorxy(t+7,h*19+s+3);
	cursorxy(7+s+h*26,t+3);
	colorfg_rgb(4,4,0);
	if (s%5==0) 
	  colorbg_rgb(0,0,1);
	else
	  colorbg_rgb(0,0,0);
	if (show>=200) {
	        colorfg_rgb(0,3,0); //different colour to reflect previous read
		fprintf(fd,"%c",show-200);
	} else if (show>=100) {
		// directly show char
		fprintf(fd,"%c",show-100);
	} else
	switch (show) {
          case 0: 
		  fprintf(fd,"?"); break; // not read not seen
          case 1: 
	          colorfg_rgb(0,4,0);
		  fprintf(fd,"."); break; // good
          case 2:
	          colorfg_rgb(4,4,0);
		  fprintf(fd,"C"); break; // ckm
          case 3: 
	          colorfg_rgb(5,5,0);
	          colorbg_rgb(4,0,0);
		  fprintf(fd,"X"); break;
	}
	colorbg_rgb(0,0,0); // only need to do this every fifth
  fflush(fd);
}

int last_display_head_track=0;
int last_display_head_side=0;
void
showhead(int t, int h) {
	// clear the old
	colorfg_rgb(2,5,2);
	colorbg_rgb(0,0,0);
	//cursorxy(last_display_head_track+7,19+3-1); 
	cursorxy(30,last_display_head_track+3); 
	fprintf(fd," ");

	colorbg_rgb(0,0,1);
	colorfg_rgb(2,5,2);
	//cursorxy(t+7,19+3-1); 
	cursorxy(30,t+3); 
	if (h==0) 
	  fprintf(fd,"<"); 
	else if (h==1)
	  fprintf(fd,">"); 
	else 
	  fprintf(fd,"*"); 
	//cursorxy(t+7,19+3-1); 
	cursorxy(30,t+3); 

	last_display_head_track=t;
	last_display_head_side=h;
  fflush(fd);
}

#include "interface1571.c"

void
display_hist_line(int line, char *str) {
	colorbg_rgb(0,0,1);
	// colorfg_rgb(2,5,2);
	colorfg_rgb(2,4,2);
	cursorxy(45+9,6+line+15+10*(1-last_display_head_side));
	fprintf(fd,str); 
}
void
display_hist_info(int line) {
	colorbg_rgb(0,0,0);
	colorfg_rgb(2,2,2);
	cursorxy(45+9,6+line+15+10*(1-last_display_head_side));
	fprintf(fd,"Track %02d Head %1d     |%10s%10s%10s%10s%10s",
			last_display_head_track+1,last_display_head_side,
			"|","|","|","|","|");
}

void display_init() {
  fd=fdopen(3,"a");
  if (fd==NULL) { fprintf(stderr,"no fd=3 available\n"); exit(1); }
	printlayout();
}

void display_finalize() {
  display_info_finish();
  resetscreen();
  fflush(fd);
}

void resetscreen() {
  defaultcolor();
  cursorxy(0,39);
}

void oldmain () {
	// run with : $ make display && ./display 3>&1 1>1
	// to switch stdout and 3
  fd=fdopen(3,"a");
  if (fd==NULL) exit(1);
  //test();
  printlayout();
  resetscreen();
  printf("this is normally stdout\n");
}

