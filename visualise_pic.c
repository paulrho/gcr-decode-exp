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
   dbgprintf(stderr,"writing out %d to %d\n",pxc,xsize);
   for (int i=pxc; i<xsize; ++i) pixel(255,255,255);
}

void display_quintel_as_twopixels()
{
   int r,g,b;

   switch ((keep&28)>>2) {
     case 0x00: r=255; g=255; b=255;  dbgprintf(stderr,"(Top000) "); break; // white
     case 0x01: r=255; g=255; b=0; break;                                   //yellow
     case 0x02: r=127; g=127; b=190; break;                                 // see if this is clearer -  just grey
     case 0x03: r=127; g=0; b=127; break;                                   // dk purple
     case 0x04: r=255; g=0; b=255; break;                                   // pink/magenta
     case 0x05: r=0; g=255; b=0; break;                                     //green
     case 0x06: r=0; g=0; b=127; break;                                     // dk blue
     case 0x07: r=0; g=0; b=0; break;                                       //black
   }
   if (colout_notvalidGCR && r==255 && g==255 && b==255) { r=160, b=90, g=90; }
   printf("%d %d %d  ",r,g,b);
   pxc++;
   if (pxc>=xsize) { fprintf(stdout,"\n"); pxc-=xsize; }
   switch ((keep&0x03)) {
     case 0x00: r=255; g=255; b=255;  dbgprintf(stderr,"(Bot00) "); break; // white
     case 0x01: r=0; g=255; b=0; break;
     case 0x02: r=0; g=127; b=255; break;                                  // but this is lt blue
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
      dbgprintf(stderr,"Started pixel monitoring of this track sector\n");
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
      dbgprintf(stderr,"Started pixel monitoring of this track sector from this offset %d\n",dc);
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
