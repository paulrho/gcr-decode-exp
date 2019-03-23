#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <unistd.h>

#include "checksum.h"
#include "gcr_decode.h"
#include "shared.h"

void donothing()
{
};

// forward declare
void showflux();
void switchonraw();

#define MAXFS    10000000

int      reversed=0;
int      is_one_sided=0;                                   // this allows more than 35 tracks on one side - with correct timings // you need to switch this to 1 if you know there is more than 35 tracks (like viatel)
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

#include "bitops.c"
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

#include "tracker.c"
#include "blocks2d64.c"


#define MAXBUF    1000
enum { UNSYNC,SYNC,HEADERI,POSTHEADER,DATA,GAP } m=UNSYNC; // marks it is NOW in this mode
int      bitc=0;
int      dc=0;
int      databuf[MAXBUF];
long int lastset=0;                                        // to see how far back in bitcount we have come (for invalidating)
long int bitcount=0;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Visualisation - picture
/////////////////////////////////////////////////////////
// monitoring specific sectors
#include "visualise_pic.c"

/////////////////////////////////////////////////////////
// raw track dumping for G64
#include "raw.c"

/////////////////////////////////////////////////////////
// write the datablock out to a file - the filename depends on the quality
int noncode;
int ckm;
#include "writeblock.c"

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
      keep=0L; dbgprintf(stderr,"%s","?");
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
      dbgprintf(stderr,"\n[SYNC!*****]\n"); wraplen=0; bitc=1; m=SYNC;
   }
   if (m!=DATA && m!=HEADERI && m!=POSTHEADER) dbgprintf(stderr,"%d",bit);  //for now, we are just translating them

   // also, if waiting for data, and it takes too long -> invalidate everything!
   if (m!=DATA && bitcount-lastset>5*50 && found_track>0) {
      dbgprintf(stderr,"resetting found t/s to invalidate\n"); found_sector=-1; found_track=-1;
      raw_gotbad++;                                        // just for a test
   }

   if (m==SYNC && bitc==10) {
      //
      // Got First Byte after SYNC
      //
      DECODEBYTE;

      //if ((keep & 0xFF) == 0x52) { dbgprintf(stderr,"[HEADERINFO] "); m=HEADERI;       }
      if (decb==0x08) {
         dbgprintf(stderr,"[HEADERINFO] "); m=HEADERI;    dc=0;    lastset=bitcount;
         // switchon raw
         //else if ((keep & 0xFF ) == 0x55) { dbgprintf(stderr,"[HDATA] "); m=DATA; bitc=0; dc=0; }
         if (is_colout || is_fluxout) writeoutrestofline();  // note- it would have already done two four pixels for the header byte
         if (monitor_offset<0 &&  is_pixels_setting>=2 && (monitor_track==999 || found_track!=monitor_track || found_sector!=monitor_sector)) {
            //if (is_pixels_setting==2) is_colout=1; // switch on for specific track/sec monitor
            if (is_pixels_setting==3) {
               is_fluxout=1;                               // just grab all from header byte
               dbgprintf(stderr,"Started pixel monitoring of this track \n");
            }
         }
         // try this - assume we were close enough with timing to receive the header byte after a sync (sync easy with timing)
         // now - do a timing profile of just this block
         //dotimingprofile(/*i,n*/);
      }
      else if (decb==0x07) {
         dbgprintf(stderr,"[HDATA]\n"); m=DATA; bitc=0; dc=0;  lastset=bitcount; wraplen=0;
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
         if (bitcount-lastset>5*50) {
            dbgprintf(stderr,"resetting found t/s to invalidate\n"); found_sector=-1; found_track=-1;
            //not here //raw_gotbad++;                                  // just for a test
         }
      }
      // not valid
   }

   if (m!=UNSYNC && (bitc==10 || dc==256+2 && bitc==5)) {  // we only read 2.5 bytes more
      if (m==HEADERI || m==POSTHEADER) {
         DECODEBYTE;
         databuf[dc++]=decb;
         bitc=0;
         //dbgprintf(stderr,"[%2X%2X=%02X]",(keep&0x3FC)>>5,keep&0x1F,decb); // old way
         dbgprintf(stderr,"[%c%c=%02X]",xlate[(keep&0x3FC)>>5],xlate[keep&0x1F],decb); // try this new way wrong spot
         if (dc%16==0) { dbgprintf(stderr,"\n"); wraplen=0; }

         if (dc>20) /* way too many */ { m=UNSYNC; dc=0; } // dc so it never exceeds MAXBUF
         if (dc==8) {
            //
            // END OF HEADER BLOCK
            //
            dbgprintf(stderr,"at end of HEADER!!!\n");
            printbucketedges();
            dbgprintf(stderr,"\nHEADERID %02X CKSM %02X(%02X %s) SECTORx%02X( %2d ) TRACKx%02X( %2d ) ID %02X%02X END %02X%02X\n",
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
            if (sectormap_track>0) {
               int state;
               state=SM_HEADER_BAD;
               if (databuf[1]==((databuf[2]^databuf[3]^databuf[4]^databuf[5])&0xFF)) {
                  if (databuf[6]==0x0F && databuf[7]==0x0F)
                     state=SM_HEADER_OK;
                  else
                     state=SM_HEADER_ODD;
               }
               update_sectormap(found_track,found_sector,state);
            }
            wraplen=0;
         }
      }
      else if (m==DATA) {
         possibly_start_pixel_monitor();
         // 0x07 256Data (two next block + 254 data) CKM 00 00
         databuf[dc++]=DECODEBYTE;
         bitc=0;
         //dbgprintf(stderr,"[%2X%2X=%02X]",(keep&0x3FC)>>5,keep&0x1F,decb); // old way
         dbgprintf(stderr,"[%c%c=%02X]",xlate[(keep&0x3FC)>>5],xlate[keep&0x1F],decb); // try this new way wrong spot
         if (dc%16==0) { dbgprintf(stderr,"\n"); wraplen=0; }
         //dbgprintf(stderr,"[%02X]",decb);

         if (dc>300) /* way too many */ { m=UNSYNC; dc=0; } // dc so it never exceeds MAXBUF
         if (dc==256+2+1) {
            //
            // END OF DATA BLOCK
            //
            if (is_binout) for (int i=2; i<256; ++i) printf("%c",databuf[i]);
            ckm=0; noncode=0;
            for (int i=0; i<256; ++i) { ckm=ckm^databuf[i]; if (databuf[i]<0) noncode=1; } // calculate simple ckm
            if (databuf[256]<0) noncode=1;
            dbgprintf(stderr,"\n  END OF BLOCK! ckm=%02X %s%s THIS t=%d s=%d NEXT t=%d s=%d post:%02x%02x wrongbits=%d\n"
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

int setting_f=0;
void set_settings(int set, int track)
{
   if (is_one_sided && track>35) track=35;                 // jut for here
   if (set==3) {
      if (track >=0 && track<=17) setting_f=1000;          // speed 3
      if (track >=18 && track<=24) setting_f=1080;         // speed 2
      if (track >=25 && track<=30) setting_f=1160;         // speed 1
      if (track >=31 && track<=35) setting_f=1240;         // speed 0
      if (track >=1+35 && track<=17+35) setting_f=1000;    // speed 3
      if (track >=18+35 && track<=24+35) setting_f=1080;   // speed 2
      if (track >=25+35 && track<=30+35) setting_f=1160;   // speed 1
      if (track >=31+35 && track<=35+35) setting_f=1240;   // speed 0
   }
   else if (set==4) {
      // old way
      if (track >=0 && track<=17) setting_f=1050;          // speed 3
      if (track >=18 && track<=24) setting_f=1130;         // speed 2
      if (track >=25 && track<=30) setting_f=1210;         // speed 1
      if (track >=31 && track<=35) setting_f=1290;         // speed 0
      if (track >=1+35 && track<=17+35) setting_f=1050;    // speed 3
      if (track >=18+35 && track<=24+35) setting_f=1130;   // speed 2
      if (track >=25+35 && track<=30+35) setting_f=1210;   // speed 1
      if (track >=31+35 && track<=35+35) setting_f=1290;   // speed 0
   }
   else if (set==5) {
      if (track >=0 && track<=17) setting_f=1000*4/5;         // speed 3
      if (track >=18 && track<=24) setting_f=1080*4/5;        // speed 2
      if (track >=25 && track<=30) setting_f=1160*4/5;        // speed 1
      if (track >=31 && track<=35) setting_f=1240*4/5;        // speed 0
      if (track >=1+35 && track<=17+35) setting_f=1000*4/5;   // speed 3
      if (track >=18+35 && track<=24+35) setting_f=1080*4/5;  // speed 2
      if (track >=25+35 && track<=30+35) setting_f=1160*4/5;  // speed 1
      if (track >=31+35 && track<=35+35) setting_f=1240*4/5;  // speed 0
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
   if (argc<2) { dbgprintf(stderr,"%s [filename|-scan] track [mode track sector [offset]]\n",argv[0]); exit(1); }
   rawfile=argv[1];
   if (strcmp(rawfile,"-scan")==0) {
      scanblocks();
      exit(0);
   }
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
      dbgprintf(stderr,"is_pixels_setting=%d xsize=%d\n",is_pixels_setting,xsize);
      monitor_track=atoi(argv[4]);
      monitor_sector=atoi(argv[5]);
      if (argc>=7) {
         monitor_offset=atoi(argv[6]);
         //dbgprintf(stderr,"Setting offset to %d\n",monitor_offset);
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
      if (argc>=12) {
         reversed=atoi(argv[11]);
      }
   }

   if (is_pixels_setting==1 || is_pixels_setting==2) is_colout=1;  // switch on if needs permanent on
   if (is_pixels_setting==3) is_fluxout=1;                         // switch on if needs permanent on
   if (is_pixels_setting==2) is_colout=0;
   if (is_pixels_setting==3) is_fluxout=0;
   if (algmode>=3 && algmode<=5) { set_settings(algmode,sectormap_track); if (setting_f>100) algmode=setting_f; }

   if (sectormap_track>0) init_sectormap();
   //
   // Read track
   //
   nx=fread(rlebuff, 1, MAXFS, rfifile);
   dbgprintf(stderr,"Read %d bytes from FILE %s\n",nx,rawfile);

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
      //for (ix=0; ix<nx; ix++) {                            // dont need to worry about reverse - same result ?
      for (ix=reversed ? nx-1 : 0; (ix<nx)&&!reversed||reversed&&(ix>=0); reversed ? ix-- : ix++) { // normal
         c=rlebuff[ix];
         //if (s==1) if (c!=0 && c!=255) bitpipe_new(c);
         if (s==1) if (c!=0 && c!=255) bitpipe_new(c+lastc);
         //if (s==1) bitpipe_new(c);
         lastc=c;
         s=1-s;
      }
      dbgprintf(stderr,"Peaking done\n");
      bitpipe_histogram();
   }

   //
   // Main processing loop
   //
   blen=0; s=0;
   lastc=0;
   for (ix=reversed ? nx-1 : 0; (ix<nx)&&!reversed||reversed&&(ix>=0); reversed ? ix-- : ix++) { // normal
      if (save_raw) if (rawp==MAXRAW) break;                                                     // for now
      if (save_raw<0) { dbgprintf(stderr,"stopping reading early\n"); break; }                   // for now

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
      if (s==1) fluxspan(algmode, c, lastc);

      wraplen++; if (wraplen%384==0) dbgprintf(stderr,"\n");  // text debug
      lastc=c;
      s=1-s;                                                  // Switch states
   }
   //dbgprintf(stderr,"converted %d\n",rlen);
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
      else dbgprintf(stderr,"write to raw track failed\n");
   }
   dbgprintf(stderr,"\nFinished r14 run ix=%d nx=%d\n",ix,nx);
   if (sectormap_track>0) {
      save_sectormap();
      exit(is_good_sectormap()!=1);                        //reverse it
   }
   exit(0);
}

/*******/
/* END */
/*******/
