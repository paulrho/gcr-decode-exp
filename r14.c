#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

// forward declare
void showflux();
void switchonraw();

#define MAXFS    10000000

int      rfi_trackdatalen;
char     rlebuff[MAXFS];
char     buf[MAXFS];
int      buflen=MAXFS;
int      nx;                                               // moved global just for now
int      ix;
int      xsize=1280;                                       //int xsize=6400;
int      resync=100;
int      found_track=-1;
int      keep_track=-1;
int      found_sector;
int      badcount=0;
int      monitor_track;
int      monitor_sector;
int      monitor_offset=0;
int      is_pixels_setting=1;
long int keep=0L;
int      adv_wrongbits=0;
int      adv_timing=0;
long     wraplen=0;
int      algmode=0;

#define ABSMAXPIP    10000                                 // was 300

int  MAXPIP=300;
int  pip[ABSMAXPIP];
int  buck[100];
int  pt=0;
int  setpeak[4];
int  is_pureflux=0;                                        //was 1
int  save_raw=0;
int  raw_starts=0;
char xlate[]={ '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','z','t','n','m','r','l','c','k','f','p','A','B','C','D','E','F' };
//char xlate[]={'0','1','2','3','4','5','6','7','8','9','o','i','z','m','
#define MAXRAW    (30000)                                  // done differently now
int      rawp=0;
char     rawc[MAXRAW];
long int rawkeep=0L;
int      rawbitc=0;
int      raw_gotbad=0;                                     // if you get a bad one - go round again
float    p1=22;
float    p2=42;
int      pk=0;
int      pki=0;

/////////////////////////////////////////////////////////////////////////////
// Track Sector tracker
//
//
// new feature - track which
// track we are on (we think - new parameter)
// (therefore use internal data to determine how many sectors we should have)
// the state of each
// return exit code?
//   of 0 for all good
//   of 1 for something missing/wrong
//
#define TOPSEC    30
int  sectormap_track=-1;                                   // passed by new parameter
char sectormap[TOPSEC];                                    // will do whole disk later
// at moment - just for 35 track single side
int sectors_per_track(int track)
{
   if (track>=1 && track<18) return 21;                    // 00..20
   else if (track>=18 && track <25) return 19;
   else if (track>=25 && track <31) return 18;
   else if (track>=31 && track <50) return 17;
   else return -1;
}

enum { SM_MISSING,SM_GOOD,SM_GOODISH,SM_CKM,SM_BAD };
void init_sectormap()
{
   for (int i=0; i<TOPSEC; ++i) sectormap[i]=SM_MISSING;
   // load a bitmap that saves state between runs - needs to be invalidated for different tracks
   FILE *fp=fopen("bitmap.dat","rb");
   if (fp==NULL) return;
   int n=fread(sectormap, 1, TOPSEC, fp);
   fprintf(stderr,"track %02d bitmap: ",sectormap_track);
   for (int i=0; i<n; ++i) fprintf(stderr,"%d ",sectormap[i]);
   fprintf(stderr,"\n");
   fclose(fp);
}

int save_sectormap()
{
   FILE *fp=fopen("bitmap.dat","wb");

   if (fp==NULL) {
      fprintf(stderr,"could not save bitmap\n");
      return -1;
   }
   for (int i=0; i<TOPSEC; ++i) fprintf(fp,"%c",sectormap[i]);
   fclose(fp);
   return 0;
}

void update_sectormap(int track, int sector, int state)
{
   if (track==sectormap_track && sector>=0 && sector<sectors_per_track(track)) {
      switch (sectormap[sector]) {
        case SM_MISSING:
        case SM_BAD:
           sectormap[sector]=state; break;
        case SM_CKM:
           if (state==SM_GOOD || state==SM_GOODISH) sectormap[sector]=state;
           break;
        case SM_GOODISH:
           if (state==SM_GOOD) sectormap[sector]=state;
           break;
      }
   }
}

int is_good_sectormap()
{
   for (int i=0; i<sectors_per_track(sectormap_track); ++i) if (sectormap[i]!=SM_GOOD) return 0;
   return 1;
}

/////////////////////////////////////////////////////////////////////////////
#define MAXBUCK    100

void printbucketedges()
{
   fprintf(stderr,"\nbucket edges <%.1f <%.1f <%.1f(1) <%.1f(01) <%.1f(001)\n",
           p1*0.5,
           p1*(0.75-0.05*adv_timing),
           p2*0.5+p1*(0.50+0.05*adv_timing),
           p2    +p1*(0.50+0.05*adv_timing),
           p2    +p1*2.0);
}

void bitpipe_clear()
{
   pk=0;
   pki=0;
   pt=0;
   for (int i=0; i<MAXBUCK; ++i) { buck[i]=0; }
}

void bitpipe_histogram()
{
   // show map
   //fprintf(stderr,"\nHistogram: Track %02d Sector %02d\n",found_track,found_sector); // we dont know yet
   fprintf(stderr,"\nHistogram:\n");
   for (int l=10; l>=0; l--) {                             // for each line
      for (int i=1; i<MAXBUCK; ++i) {
         //instead of pk could use 10000
         if (l==0 && buck[i]==1) fprintf(stderr,".");
         else if (l==0 && buck[i]<=pk/10/2*l && buck[i]!=0) fprintf(stderr,":");
         else if (buck[i]>pk/10*l) fprintf(stderr,"*"); else fprintf(stderr," ");
      }
      fprintf(stderr,"\n");
      if (l==0) {
         for (int i=1; i<MAXBUCK; i+=10) fprintf(stderr,"         0");
         fprintf(stderr,"\n");
      }
   }
   for (int i=1; i<MAXBUCK; i++) {
      if (i==5 || i==8 || i==30 || i==47 || i==81)
         fprintf(stderr,"L");
      else fprintf(stderr,"_");
   }
   fprintf(stderr,"\n");
   for (int i=1; i<MAXBUCK; i++) {
      if (i==1+(int)(p1*0.5) || i==1+(int)(p1*(0.75)) || i==1+(int)(p2*0.5+p1*(0.5)) || i==1+(int)(p2+p1*0.5) || i==1+(int)(p2+p1*2.0))
         fprintf(stderr,"*");
      else fprintf(stderr," ");
   }
   fprintf(stderr,"\n");
   // ct <5 <8 <30 <47 <81 for 1000 (track1-18)
}

void bitpipe_new(int c)
{
   //           fprintf(stderr,"c=%d\n",c);
   // for now - just accumulate!
   if (0) if (pip[pt]>0) buck[pip[pt]]--;                  // revolving - take old off
   if (c>0 && c<MAXBUCK) buck[c]++;
   pip[pt++]=c;
   if (c<MAXBUCK)
      if (buck[c]>pk && buck[c]>5 && c>15) { pk=buck[c]; pki=c; } // keep a running peak now

   if (pt==MAXPIP) pt=0;
   if (pt==0) {
      int i;
      int save_pk=pk;
      pk=0; pki=0;
      int lastpk=0;
      int lw=0;
      int lwi=0;
      if (1) for (int i=1; i<MAXBUCK; ++i) {
            if (buck[i]>pk && buck[i]>5 && i>lwi-3 && buck[i]>save_pk/20) { pk=buck[i]; pki=i; lastpk=pk; lw=buck[i]; }
            if (buck[i]<lw && i>pki-3) { lw=buck[i]; lwi=i; pk=buck[i]; }
            //if (buck[i]<lastpk/5) { pk=buck[i]; /* reset */ }
         }
      // pk is now the last one, divide it by three
      if (pki<50) pki/=2;                                  // special work around - we didn't get the top value - too few
      else pki/=3;

      fprintf(stderr,"\nPeak of %d at %d\n",pk,pki);
      p1=pki+3-3   +1;                                     // one less and no good  +0 few errs, +1,+2 good   +3 very bad
      if (p1<10) p1=10;
      p2=p1*2+3-3  +1;                                     // one less and no good

      if (111) fprintf(stderr,"\nPeak %f %f\n",p1,p2);
      pk=save_pk;

      if (00) {
         bitpipe_histogram();
      }
   }
}

void dotimingprofile(/*int fromi*/)
{
   // disarm this by saving p1 p2
   int           save_p1=p1;
   int           save_p2=p2;
   unsigned char lastc=0;
   unsigned char c, s;
   long int      tot=0;
   int           fromi=ix+1;                               // rename the global locally // move past +1 the blip

   s=0;
   bitpipe_clear();
   fprintf(stderr,"fromi=%d\n",ix);
   for (int i=fromi; i<nx; i++) {                          // dont need to worry about reverse - same result
      c=rlebuff[i]; tot+=c;
      if (s==1) if (c!=0 && c!=255) bitpipe_new(c+lastc);
      //if (s==1) if (c!=0 && c!=255) bitpipe_new(c);
      s=1-s;
      lastc=c;
      if (tot> 20 *10*(256+80)) break;
   }
   fprintf(stderr,"Block preview done Track %02d Sector %02d\n",found_track,found_sector);
   if (1) {                                                // 0 = keep these values!
      p1=save_p1;
      p2=save_p2;
   }
   bitpipe_histogram();
}

#define uint16_t    int
#define uint8_t     int
uint16_t Fletcher16(uint8_t *data, int count)
{
   uint16_t sum1=0;
   uint16_t sum2=0;
   int      index;

   for (index=0; index < count; ++index) {
      sum1=(sum1 + data[index]) % 255;
      sum2=(sum2 + sum1) % 255;
   }
   return (sum2 << 8) | sum1;
}

int is_peaker=1;

// 4-bit value	GCR code[29]
// hex	bin	bin	hex
// 0x0	0000	0.1010	0x0A
// 0x1	0001	0.1011	0x0B
// 0x2	0010	1.0010	0x12
// 0x3	0011	1.0011	0x13
// 0x4	0100	0.1110	0x0E
// 0x5	0101	0.1111	0x0F
// 0x6	0110	1.0110	0x16
// 0x7	0111	1.0111	0x17
// 0x8	1000	0.1001	0x09
// 0x9	1001	1.1001	0x19
// 0xA	1010	1.1010	0x1A
// 0xB	1011	1.1011	0x1B
// 0xC	1100	0.1101	0x0D
// 0xD	1101	1.1101	0x1D
// 0xE	1110	1.1110	0x1E
// 0xF	1111	1.0101	0x15
//
// btw, not valids
//                    0.0xxx  white or yellow
//                    x.xx00  white


// 4-bit value	GCR code[29]
// hex	bin	bin	hex
// 0x0	0000	0.1010	0x0A    a
// 0x1	0001	0.1011	0x0B    b
// 0x2	0010	1.0010	0x12    n  hi 2
// 0x3	0011	1.0011	0x13    m  hi 3
// 0x4	0100	0.1110	0x0E    e
// 0x5	0101	0.1111	0x0F    f
// 0x6	0110	1.0110	0x16    c  hi 6
// 0x7	0111	1.0111	0x17    k  hi 7
// 0x8	1000	0.1001	0x09    9
// 0x9	1001	1.1001	0x19    p  hi 9
// 0xA	1010	1.1010	0x1A    A  hi a
// 0xB	1011	1.1011	0x1B    B  hi b
// 0xC	1100	0.1101	0x0D    d
// 0xD	1101	1.1101	0x1D    D  hi d
// 0xE	1110	1.1110	0x1E    E  hi e
// 0xF	1111	1.0101	0x15    l  hi 5
//
int decode(long in)
{
   // give me 5 bits
   switch (in) {
     case 0x0A: return 0x00;
     case 0x0B: return 0x01;
     case 0x12: return 0x02;
     case 0x13: return 0x03;
     case 0x0E: return 0x04;
     case 0x0F: return 0x05;
     case 0x16: return 0x06;
     case 0x17: return 0x07;
     case 0x09: return 0x08;
     case 0x19: return 0x09;
     case 0x1A: return 0x0A;
     case 0x1B: return 0x0B;
     case 0x0D: return 0x0C;
     case 0x1D: return 0x0D;
     case 0x1E: return 0x0E;
     case 0x15: return 0x0F;

     default: fprintf(stderr," (NON-GCR-CODE %s%s%s%s%s)",
                      in&0x10 ? "1" : "0",
                      in&0x08 ? "1" : "0",
                      in&0x04 ? "1" : "0",
                      in&0x02 ? "1" : "0",
                      in&0x01 ? "1" : "0"
                      );
        return -1;
        //return 0x00;
   }
}

#define DECODEBYTE    decb=(decode((keep&0x3F8)>>5)<<4)+decode(keep&0x1F)

#define MAXBUF        1000
enum { UNSYNC,SYNC,HEADERI,POSTHEADER,DATA,GAP } m=UNSYNC; // marks it is NOW in this mode
int      bitc=0;
int      dc=0;
int      databuf[MAXBUF];
long int lastset=0;                                        // to see how far back in bitcount we have come (for invalidating)
long int bitcount=0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Visualisation - picture
int is_colout=0;
int is_fluxout=0;
int is_binout=0;
int pxc=0;
int colout_notvalidGCR=0;                                  // only for display purposes

void pixel(int r, int g, int b)
{
   printf("%d %d %d  ",r,g,b);
   pxc++;
   if (pxc>=xsize) { fprintf(stdout,"\n"); pxc-=xsize; }   // for is_colout
}

void advanceline()
{
   for (int i=pxc; i<resync*(1+2*(is_fluxout)); ++i) pixel(220,220,220);
}

void writeoutrestofline()
{
   fprintf(stderr,"writing out %d to %d\n",pxc,xsize);
   for (int i=pxc; i<xsize; ++i) pixel(255,255,255);
}

void display_quintel_as_twopixels()
{
   int r,g,b;

   switch ((keep&28)>>2) {
     case 0x00: r=255; g=255; b=255;  fprintf(stderr,"(Top000) "); break; // white
     case 0x01: r=255; g=255; b=0; break;                                 //yellow
     case 0x02: r=127; g=127; b=190; break;                               // see if this is clearer -  just grey
     case 0x03: r=127; g=0; b=127; break;                                 // dk purple
     case 0x04: r=255; g=0; b=255; break;                                 // pink/magenta
     case 0x05: r=0; g=255; b=0; break;                                   //green
     case 0x06: r=0; g=0; b=127; break;                                   // dk blue
     case 0x07: r=0; g=0; b=0; break;                                     //black
   }
   if (colout_notvalidGCR && r==255 && g==255 && b==255) { r=160, b=90, g=90; }
   printf("%d %d %d  ",r,g,b);
   pxc++;
   if (pxc>=xsize) { fprintf(stdout,"\n"); pxc-=xsize; }
   switch ((keep&0x03)) {
     case 0x00: r=255; g=255; b=255;  fprintf(stderr,"(Bot00) "); break; // white
     case 0x01: r=0; g=255; b=0; break;
     case 0x02: r=0; g=127; b=255; break;                                // but this is lt blue
     case 0x03: r=0; g=0; b=0; break;
   }
   if (colout_notvalidGCR && r==255 && g==255 && b==255) { r=160, b=90, g=90; }
   printf("%d %d %d  ",r,g,b);
   pxc++;
   if (pxc>=xsize) { fprintf(stdout,"\n"); pxc-=xsize; }
}

/////////////////////////////////////////////////////////
// monitoring specific sectors

void possibly_switch_pixel_monitor_at_sync()
{
   // start at the actual data
   if ((m==HEADERI || m==POSTHEADER) && is_pixels_setting>=2 && found_track>=0 && (monitor_track==999 || found_track==monitor_track && found_sector==monitor_sector) && monitor_offset==0) {
      if (is_pixels_setting==2) is_colout=1;               // switch on for specific track/sec monitor
      if (is_pixels_setting==3) is_fluxout=1;              // switch on for specific track/sec monitor
      fprintf(stderr,"Started pixel monitoring of this track sector\n");
   }
   else
   // switch it off
   if (is_pixels_setting>=2 && (monitor_track==999 || found_track!=monitor_track || found_sector!=monitor_sector)) {
      if (is_pixels_setting==2) {
         is_colout=0;                                      // switch off for track/sec monitor
         if (m!=DATA && (is_colout || is_fluxout)) writeoutrestofline();
      }
      if (is_pixels_setting==3) {
         if (monitor_offset<0) {
            // keep it on - but will advance for data an NO new line for post header
         }
         else {
            is_fluxout=0;                                  // switch off for track/sec monitor
            if (m!=DATA && (is_colout || is_fluxout)) writeoutrestofline();
         }
      }
   }
   //if (is_colout || is_fluxout) writeoutrestofline();
   //if (is_pixels_setting>=2) { is_colout=0; is_fluxout=0; } // switch off for track/sec monitor
}

void possibly_start_pixel_monitor()
{
   if (is_pixels_setting>=2 && found_track>=0 && (monitor_track==999 || found_track==monitor_track && found_sector==monitor_sector) && monitor_offset==(dc+1)) {
      if (is_pixels_setting==2) is_colout=1;               // switch on for specific track/sec monitor
      if (is_pixels_setting==3) is_fluxout=1;              // switch on for specific track/sec monitor
      fprintf(stderr,"Started pixel monitoring of this track sector from this offset %d\n",dc);
   }
}

/////////////////////////////////////////////////////////
// raw track dumping for G64

void switchoffraw(int rew)
{
   if (keep_track==35) return;                             // just for now
   if (raw_gotbad) {
      // go round again
      raw_gotbad=0;
      save_raw=1;
      switchonraw(20);
      fprintf(stderr,"going around again... ");
   }
   else
   if (save_raw==2) {
      fprintf(stderr,"stopping\n");
      save_raw=-1;
      rawp=rawp-rew;
   }
}

void switchonraw(int rew)
{
   if (save_raw==1) {                                      // only start recording if we didnt get it
      raw_gotbad=0;
      fprintf(stderr,"rewinding\n");
      save_raw=2;                                          // first time header
      raw_starts=rawp-rew;
      // shuffle  - rewind only to start (just before, so we get the sync)
      rawp=rawp-raw_starts;                                // should be 10
      if (raw_starts>0) for (int i=0; i<rawp; ++i) { rawc[i]=rawc[i+raw_starts]; }
   }
   if (save_raw>0) raw_starts=rawp;                        // we start at the beginning (used for undo)
}

/////////////////////////////////////////////////////////
// write the datablock out to a file - the filename depends on the quality
int noncode;
int ckm;

void write_datablock_to_file()
{
   // write data block to file
   char filename[80];
   char suffix[80];
   int  state=-1;

   if (found_sector>=0 && found_sector<=20) {
      keep_track=found_track;                              // just for raw
      fprintf(stderr,"keep_track=%d\n",keep_track);
   }
   if ((ckm&0xFF)==databuf[256] && noncode==0) {
      //strcpy(suffix,".dat"); // original plain
      //sprintf(suffix,"-%03d.dat",badcount++); /// just temp
      // try this - add in a better checksum
      // the xcc16 "x" is to get the sort order toward the bottom
      // except dont do this if we already have the same file with same ck16 with good post
      // check if file already exists
      state=SM_GOOD;
      sprintf(suffix,"-%04x.dat",Fletcher16(databuf,256));
      sprintf(filename,"data/track%02dsec%02d%s",found_track,found_sector,suffix);
      FILE *fp=fopen(filename,"rb");
      if (fp!=NULL) {                                      // exists
         // so it exists in a good form - just do nothing - we have the same result
         fprintf(stderr,"block already saved as good with this ck16\n");
         fclose(fp);
         // we'll just rewrite it so as not to touch the code far below
         // of course, it depends on the order whether the file below gets written first
      }
      else {
         state=SM_GOODISH;
         if (noncode || adv_wrongbits>0 || (((char)databuf[256+1])&0xFFF)!=0x00 || (((char)databuf[256+2])&0xFFF)!=0x00)
            sprintf(suffix,"x%04x-w%02d-p%02x%02x%s.dat",Fletcher16(databuf,256),
                    adv_wrongbits,(((char)databuf[256+1])&0xFFF), (((char)databuf[256+2])&0xFFF)!=0x00, (noncode) ? "-nonc" : ""
                    );
         //else
         //sprintf(suffix,"-%04x.dat",Fletcher16(databuf,256));
         // otherwise - just use what we've already setup
      }
   }
   else if (databuf[256+1]==0 && (databuf[256+2]==0 || databuf[256+2]==1)) {
      //sprintf(suffix,"-%03d.ckm",badcount++); // checksum bad but post good
      sprintf(suffix,"-%04x-%03d.ckm",Fletcher16(databuf,256),badcount++); // do this now
      state=SM_CKM;
   }
   else {
      sprintf(suffix,"-%03d.bad",badcount++);
      raw_gotbad++;
      state=SM_BAD;
   }

   fprintf(stderr,"Writing block data/track%02dsec%02d%s\n",found_track,found_sector,suffix);
   sprintf(filename,"data/track%02dsec%02d%s",found_track,found_sector,suffix);
   FILE *fp=fopen(filename,"wb");
   if (fp!=NULL) {
      for (int i=0; i<256; ++i) fprintf(fp,"%c",databuf[i]);
      fclose(fp);
   }
   if (sectormap_track>0) update_sectormap(found_track,found_sector,state);
}

/////////////////////////////////////////////////////////
// the guts - a (flux) bit at a time

void addbit(int bit)
{
   int decb;                                               // decoded byte

   if (0 && is_fluxout && pxc<xsize-10) {
      if (bit<0) pixel(255,0,0);
      if (bit==0) pixel(0,255,0);
      if (bit==1) pixel(0,0,255);
   }
   if (bit<0) {
      keep=0L; fprintf(stderr,"%s","?");
      if (m!=UNSYNC && m!=POSTHEADER) {
         if ((is_colout || (is_pureflux && is_fluxout && pxc<xsize-10)))
            pixel(255,0,0);
      }
      return;
   }
   bitcount++;                                             // only for de-syncing

   if ((!is_pureflux||1) && is_fluxout && pxc<xsize-10) showflux(bit);

   keep=((keep<<1)&0x7FF)|bit; bitc++;

   if (save_raw>0) {
      rawkeep=((rawkeep<<1)&0x7FF)|bit; rawbitc++;
      if (rawkeep==0x7FE) { if (rawp<MAXRAW-1) { rawc[rawp++]=0xFF; rawc[rawp++]=0xFF; }  rawbitc=1; } //just for now
      if (rawbitc==8) {
         rawbitc=0;
         if (rawp<MAXRAW) rawc[rawp++]=rawkeep&0xFF;
      }
   }

   if ((is_colout || (is_fluxout && pxc<xsize-10)) && m!=UNSYNC && bitc%5==0) // add is flux out for now
      display_quintel_as_twopixels();

   //
   // SYNC
   //
   if (keep==0x7FE) {                                      // try 10 bits //if ((keep&0x3FF)==0x3FE) { // 11 bits
      if (is_colout || is_fluxout) {
         if (m!=HEADERI && m!=POSTHEADER) {
            //if (m==DATA) writeoutrestofline();
         }
         else advanceline();
      }
      possibly_switch_pixel_monitor_at_sync();
      fprintf(stderr,"\n[SYNC!*****]\n"); wraplen=0; bitc=1; m=SYNC;
   }
   if (m!=DATA && m!=HEADERI && m!=POSTHEADER) fprintf(stderr,"%d",bit);  //for now, we are just translating them

   // also, if waiting for data, and it takes too long -> invalidate everything!
   if (m!=DATA && bitcount-lastset>5*50 && found_track>0) { fprintf(stderr,"resetting found t/s to invalidate\n"); found_sector=-1; found_track=-1; }

   if (m==SYNC && bitc==10) {
      //
      // Got First Byte after SYNC
      //
      DECODEBYTE;

      //if ((keep & 0xFF) == 0x52) { fprintf(stderr,"[HEADERINFO] "); m=HEADERI;       }
      if (decb==0x08) {
         fprintf(stderr,"[HEADERINFO] "); m=HEADERI;    dc=0;    lastset=bitcount;
         // switchon raw
         //else if ((keep & 0xFF ) == 0x55) { fprintf(stderr,"[HDATA] "); m=DATA; bitc=0; dc=0; }
         if (is_colout || is_fluxout) writeoutrestofline();  // note- it would have already done two four pixels for the header byte
         if (monitor_offset<0 &&  is_pixels_setting>=2 && (monitor_track==999 || found_track!=monitor_track || found_sector!=monitor_sector)) {
            //if (is_pixels_setting==2) is_colout=1; // switch on for specific track/sec monitor
            if (is_pixels_setting==3) {
               is_fluxout=1;                               // just grab all from header byte
               fprintf(stderr,"Started pixel monitoring of this track \n");
            }
         }
         // try this - assume we were close enough with timing to receive the header byte after a sync (sync easy with timing)
         // now - do a timing profile of just this block
         //dotimingprofile(/*i,n*/);
      }
      else if (decb==0x07) {
         fprintf(stderr,"[HDATA]\n"); m=DATA; bitc=0; dc=0;  lastset=bitcount; wraplen=0;
         adv_wrongbits=0;
         // maybe not the best place
         if (save_raw==1 && found_sector==0 && found_track>0) switchonraw(20+20);  // start only at sec 0 // was 20
         dotimingprofile(/*i,n*/);                                                 // this wont interfere - as we only do one at start
      }
      else {
         // the byte was something we don't know about - switch it all off
         m=UNSYNC;                                         // this was set wrong
         if (is_pixels_setting>=2) if (is_colout || is_fluxout) { pixel(255,0,0); pixel(0,0,0); pixel(255,0,0); writeoutrestofline(); }
         if (is_pixels_setting==2) is_colout=0;            // switch off for track/sec monitor
         if (is_pixels_setting==3) is_fluxout=0;           // switch off for track/sec monitor
         // invalidate everything!
         // only if it takes too long!
         if (bitcount-lastset>5*50) { fprintf(stderr,"resetting found t/s to invalidate\n"); found_sector=-1; found_track=-1; }
      }
      // not valid
   }

   if (m!=UNSYNC && (bitc==10 || dc==256+2 && bitc==5)) {  // we only read 2.5 bytes more
      if (m==HEADERI || m==POSTHEADER) {
         DECODEBYTE;
         databuf[dc++]=decb;
         bitc=0;
         //fprintf(stderr,"[%2X%2X=%02X]",(keep&0x3FC)>>5,keep&0x1F,decb); // old way
         fprintf(stderr,"[%c%c=%02X]",xlate[(keep&0x3FC)>>5],xlate[keep&0x1F],decb); // try this new way wrong spot
         if (dc%16==0) { fprintf(stderr,"\n"); wraplen=0; }

         if (dc>20) /* way too many */ { m=UNSYNC; dc=0; } // dc so it never exceeds MAXBUF
         if (dc==8) {
            //
            // END OF HEADER BLOCK
            //
            fprintf(stderr,"at end of HEADER!!!\n");
            printbucketedges();
            fprintf(stderr,"\nHEADERID %02X CKSM %02X(%02X %s) SECTORx%02X( %2d ) TRACKx%02X( %2d ) ID %02X%02X END %02X%02X\n",
                    databuf[0],
                    databuf[1], (databuf[2]^databuf[3]^databuf[4]^databuf[5])&0xFF,
                    (databuf[1]==((databuf[2]^databuf[3]^databuf[4]^databuf[5])&0xFF)) ? "OK  " : "BAD ",
                    databuf[2], databuf[2],
                    databuf[3], databuf[3],
                    databuf[4], databuf[5],
                    databuf[6], databuf[7]
                    );
            if ((databuf[1]==((databuf[2]^databuf[3]^databuf[4]^databuf[5])&0xFF))) { // hdr ckm good
               found_track=(int)databuf[3];
               found_sector=(int)databuf[2];
               m=POSTHEADER;                               // ignore dodgy bytes - we are not reading them any way (gap)
            }
            else found_track=(-1);
            if (save_raw==2 && found_sector==0) switchoffraw(20);
            wraplen=0;
         }
      }
      else if (m==DATA) {
         possibly_start_pixel_monitor();
         // 0x07 256Data (two next block + 254 data) CKM 00 00
         databuf[dc++]=DECODEBYTE;
         bitc=0;
         //fprintf(stderr,"[%2X%2X=%02X]",(keep&0x3FC)>>5,keep&0x1F,decb); // old way
         fprintf(stderr,"[%c%c=%02X]",xlate[(keep&0x3FC)>>5],xlate[keep&0x1F],decb); // try this new way wrong spot
         if (dc%16==0) { fprintf(stderr,"\n"); wraplen=0; }
         //fprintf(stderr,"[%02X]",decb);

         if (dc>300) /* way too many */ { m=UNSYNC; dc=0; } // dc so it never exceeds MAXBUF
         if (dc==256+2+1) {
            //
            // END OF DATA BLOCK
            //
            if (is_binout) for (int i=2; i<256; ++i) printf("%c",databuf[i]);
            ckm=0; noncode=0;
            for (int i=0; i<256; ++i) { ckm=ckm^databuf[i]; if (databuf[i]<0) noncode=1; } // calculate simple ckm
            if (databuf[256]<0) noncode=1;
            fprintf(stderr,"\n  END OF BLOCK! ckm=%02X %s%s THIS t=%d s=%d NEXT t=%d s=%d post:%02x%02x wrongbits=%d\n"
                    ,ckm&0xFF,((ckm&0xFF)==databuf[256]&&found_track>=0) ? "GOOD" : "***BAD!***"
                    ,noncode ? " NONCODE DETECTED" : ""
                    ,found_track,found_sector
                    ,databuf[0],databuf[1]
                    ,((char)databuf[256+1])&0xFFF,((char)databuf[256+2])&0xFFF
                    ,adv_wrongbits
                    );
            wraplen=0;                                     // for debug text out
            if (found_track>=0 && found_track<100) {
               write_datablock_to_file();
               found_track=-1;                             // we are done
               if (is_colout || is_fluxout) {
                  // write ckm quality indicator (3 pixels)
                  if ((ckm&0xFF)==databuf[256]) for (int j=0; j<3; ++j) pixel(0,160,0);
                  else if (databuf[256+1]==0 && (databuf[256+2]==0 || databuf[256+2]==1)) for (int j=0; j<3; ++j) pixel(255,255,0);
                  else for (int j=0; j<3; ++j) pixel(255,0,0);
               }
            }
         }
      }
   }
}

// this is for debuging via image output only
int keepr=0;
int keepg=0;
int keepb=0;

void showflux(int b)
{
   if (b==0) pixel(255,255,255);
   else pixel(keepr,keepg,keepb);
}

void noteflux(int ct, int a)
{
   if (1) {
      if ((!is_pureflux||1) && is_fluxout && pxc<xsize-10)
         if (0)
            switch (a) {
              case -1: keepr=255; keepg=0; keepg=0;  break;
              case 0:  keepr=255; keepg=0; keepg=0;  break;
              case 1:  keepr=0; keepg=0;   keepb=0; break;
              case 2:  keepr=0; keepg=127; keepb=255; break;
              case 3:  keepr=0; keepg=255; keepb=127; break;
              case 4: keepr=205; keepg=205; keepb=205; break;
            }
      if (1)
         switch (a) {
           case -1: keepr=255; keepg=0; keepg=0;  break;
           case 0:  keepr=255; keepg=0; keepg=0;  break;
           case 1:  keepr=200; keepg=200; keepb=200; break;
           case 2:  keepr=100; keepg=200; keepb=255; break;
           case 3:  keepr=100; keepg=255; keepb=200; break;
           case 4:  keepr=205; keepg=205; keepb=205; break;
         }
   }
   else {
      if ((!is_pureflux||1) && is_fluxout && pxc<xsize-10)
         switch (a) {
           case -1: pixel(127+64,0,0); break;
           case 0:  pixel(255,0,0); break;
           case 1:  pixel(0,0,0); break;
           case 2:  pixel(255,255,255); pixel(0,127,255); break;
           case 3:  pixel(255,255,255); pixel(255,255,255); pixel(0,255,127); break;
           case 4:
              for (int i=0; (i<(int)(ct/p1)-2) && pxc<xsize-10; ++i) pixel(205,205,205);
              pixel(255,127,127); pixel(255,0,0);
              break;
         }
   }
}

int main(int argc, char *argv[])
{

   FILE *rfifile;
   //char * rawfile="data.dat";
   char *rawfile;
   //int n;
   long rlen=0;
   //int i;
   unsigned char lastc=0;
   unsigned char c, b, blen, s;
   float         tt=0.0;
   int           insync=0;
   char *        td="";

   //
   // Read args
   //
   if (argc<2) { fprintf(stderr,"%s filename track [mode track sector [offset]]\n",argv[0]); exit(1); }
   rawfile=argv[1];
   if (argc>=3) { algmode=atoi(argv[2]); }
   rfifile=fopen(rawfile, "rb");
   if (argc>=6) {
      // 2 t s colout
      // 3 t s fluxout
      // 4     fluxout and pureflux=1 (will then change fluxout to be 3)
      // select only a certain track and sector for
      is_pixels_setting=atoi(argv[3]);                     // only a certain track and sector once detected
      //if (is_pixels_setting==3) xsize=4096;
      if (is_pixels_setting==4) {
         is_pixels_setting=3;
         is_pureflux=1;
      }
      if (is_pixels_setting==3) xsize=8192;
      if (is_pixels_setting==0) {

      }
      fprintf(stderr,"is_pixels_setting=%d xsize=%d\n",is_pixels_setting,xsize);
      monitor_track=atoi(argv[4]);
      monitor_sector=atoi(argv[5]);
      if (argc>=7) {
         monitor_offset=atoi(argv[6]);
         //fprintf(stderr,"Setting offset to %d\n",monitor_offset);
      }
      if (argc>=9) {
         MAXPIP=atoi(argv[7]);
         adv_timing=atoi(argv[8]);
      }
      if (argc>=10) {
         save_raw=atoi(argv[9]);
      }
      if (argc>=11) {
         sectormap_track=atoi(argv[10]);
      }
   }

   if (is_pixels_setting==1 || is_pixels_setting==2) is_colout=1;  // switch on if needs permanent on
   if (is_pixels_setting==3) is_fluxout=1;                         // switch on if needs permanent on
   if (is_pixels_setting==2) is_colout=0;
   if (is_pixels_setting==3) is_fluxout=0;

   if (sectormap_track>0) init_sectormap();
   //
   // Read track
   //
   nx=fread(rlebuff, 1, MAXFS, rfifile);
   fprintf(stderr,"Read %d bytes from FILE %s\n",nx,rawfile);

   //
   // Start processing
   //
   // peaker preview -> feed in quite a few bits to make sure we have the right averages
   // basically this plays the bits twice
   // actually - it seems I don't need this - just accumulate the buckets on the fly
   {
      blen=0; s=0;
      lastc=0;
      // read the whole thing (pre-pass)
      for (ix=0; ix<nx; ix++) {                            // dont need to worry about reverse - same result
         c=rlebuff[ix];
         //if (s==1) if (c!=0 && c!=255) bitpipe_new(c);
         if (s==1) if (c!=0 && c!=255) bitpipe_new(c+lastc);
         //if (s==1) bitpipe_new(c);
         lastc=c;
         s=1-s;
      }
      fprintf(stderr,"Peaking done\n");
      bitpipe_histogram();
   }

   //
   // Main processing loop
   //
   blen=0; s=0;
   lastc=0;
   int reversed=0;                                                                               // allow reverse read (hardcode)
   for (ix=reversed ? nx-1 : 0; (ix<nx)&&!reversed||reversed&&(ix>=0); reversed ? ix-- : ix++) { // normal
      if (save_raw) if (rawp==MAXRAW) break;                                                     // for now
      if (save_raw<0) { fprintf(stderr,"stopping reading early\n"); break; }                     // for now

      // Extract next RLE value
      c=rlebuff[ix];

      if (is_pureflux && is_fluxout && pxc<xsize-10) {     // truncate at width
         for (int j=0; j<c/2+1 && pxc<xsize-10; ++j) {
            int r,b,g;
            if (s==0) { r=0; g=0; b=0; }
            if (s==1) { r=127; g=255; b=255; }
            pixel(r,g,b);
         }
      }
      if (s==1) {
         int ct=c+lastc;
         int cb;
         if (algmode==1) {                                 //old
            td="-";
            if (ct<7) { td="."; /* ignore */ fprintf(stderr,"."); }
            if (ct>=7 && ct<=14) { td="_"; addbit(-1); fprintf(stderr,"_%d ",ct); }
            //if (ct>=7 && ct<=12) { td="_"; addbit(-1); fprintf(stderr,"_%d ",ct); }
            if (ct>=13 && ct<=26) { td="1"; addbit(1); }
            if (ct>=30 && ct<=42) { td="01"; addbit(0); addbit(1); }
            if (ct>=47 && ct<=55) { td="001"; addbit(0); addbit(0); addbit(1); }
            if (ct>55) { td=">>>>"; addbit(-1); fprintf(stderr,">%d ",ct); }
         }
         else if (algmode==0) {                            //old
            td="-";
            if (ct<7) { td="."; /* ignore */ fprintf(stderr,"."); }
            if (ct>=7 && ct<14) { td="_"; addbit(-1); fprintf(stderr,"_%d ",ct); }
            if (ct>=14 && ct<32) { td="1"; addbit(1); }
            if (ct>=32 && ct<49) { td="01"; addbit(0); addbit(1); }
            if (ct>=50 && ct<=60) { td="001"; addbit(0); addbit(0); addbit(1); }
            if (ct>60) { td=">>>>"; addbit(-1); fprintf(stderr,">%d ",ct); }
         }
         else if (algmode>100) {                           // its a frequency factor
            //
            // Timed mode ~600->1300
            //
            td="-";
            float sc=((float)algmode)/1000.0;
            if (ct<5.0*sc) { noteflux(ct,-1); td="."; /* ignore */ fprintf(stderr,"."); }
            else if (ct<8.0*sc) { noteflux(ct,0); td="_"; addbit(-1); fprintf(stderr,"_%d ",ct); }
            else if (ct<30.0*sc) { noteflux(ct,1); td="1"; addbit(1); }
            else if (ct<(47.0)*sc) { noteflux(ct,2); td="01"; addbit(0); addbit(1); }
            else if (ct<=80.0*sc) { noteflux(ct,3); td="001"; addbit(0); addbit(0); addbit(1); }
            else {
               noteflux(ct,4); td=">>>>";

               if (1) {
                  colout_notvalidGCR=1;
                  for (int i=0; i<(int)(ct/(20.0*sc)); ++i) {
                     // not showing this will hide the fact it isn't valid GCR... think about
                     ///ALREADYDISPLAYING so dont do this now //if (is_colout||is_pureflux&&is_fluxout) pixel(160,90,90);
                     if (i>=4) addbit(0);
                  }
                  addbit(0); addbit(0); addbit(0); addbit(1); // adding this!!
                  addbit(-1);
                  colout_notvalidGCR=0;
                  fprintf(stderr,">%d ",ct);                                   // act like two zeros at least
               }
               else {                                                          // old way
                  addbit(0); addbit(0); addbit(-1); fprintf(stderr,">%d ",ct); // act like two zeros at least
                  if (is_colout) for (int i=0; i<ct/40.0; ++i) {
                        int r=160,g=90,b=90;
                        pixel(r,g,b);
                     }
               }
            }
         }
         else {                                            // say 2
            // Dynamic Mode
            td="-";
            if (ct<       p1*0.5) { noteflux(ct,-1); td="."; /* ignore */ fprintf(stderr,"."); }
            else if (ct<       p1*(0.75-0.05*adv_timing)+2*0) { noteflux(ct,0); td="_"; addbit(-1); fprintf(stderr,"_%d ",ct); } //55 good
            else if (ct<p2*0.5+p1*(0.50+0.05*adv_timing)) { noteflux(ct,1); td="1"; addbit(1); }
            else if (ct<p2    +p1*(0.50+0.05*adv_timing)-3*0) { noteflux(ct,2); td="01"; addbit(0); addbit(1); }                 // changing to 7 red SOME things better 0.65 good 0.60 0.611 switches
            else if (ct<p2    +p1*2.0) { noteflux(ct,3); td="001"; addbit(0); addbit(0); addbit(1); }
            else {
               noteflux(ct,4); td=">>>>";
               colout_notvalidGCR=1;
               int ab=0;
               for (int i=0; i<(int)(ct/p1)-1; ++i) {      // was 40
                  if (i>2) { addbit(0); ab++; }
               }
               ab+=4;
               addbit(0); addbit(0); addbit(0); addbit(1); // adding this!!
               addbit(-1);
               colout_notvalidGCR=1;
               fprintf(stderr,">%d(added:%d) ",ct,ab);     // act like two zeros at least
            }
         }
      }

      wraplen++; if (wraplen%384==0) fprintf(stderr,"\n");  // text debug
      lastc=c;
      s=1-s;                                                // Switch states
   }
   //fprintf(stderr,"converted %d\n",rlen);
   // finalise picture
   if (is_colout || is_fluxout) writeoutrestofline();
   if (is_colout || is_fluxout) { printf(" "); writeoutrestofline(); } // and just a blank line - space in front for sorting
   if (is_pixels_setting==1) for (int i=0; i<3; ++i) writeoutrestofline();

   if (save_raw) {                                         // -ve or +ve
      char filename[80];
      sprintf(filename,"data/track%02d.raw",keep_track);
      FILE *fp=fopen(filename,"wb");
      if (fp!=NULL) {
         if (keep_track<=17) rawp=7692+50;
         else if (keep_track<=24) rawp=7142+50;
         else if (keep_track<=30) rawp=6666+50;
         else rawp=6250+50;
         for (int i=0; i<rawp; ++i) fprintf(fp,"%c",rawc[i]);
         fclose(fp);
      }
      else fprintf(stderr,"write to raw track failed\n");
   }
   fprintf(stderr,"\nFinished r14 run ix=%d nx=%d\n",ix,nx);
   if (sectormap_track>0) {
      save_sectormap();
      exit(is_good_sectormap()!=1);                        //reverse it
   }
   exit(0);
}

/*******/
/* END */
/*******/
