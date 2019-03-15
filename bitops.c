
// forward declare
void addbit(int bit);
void noteflux(int ct, int a);
void noteflux(int ct, int a);
extern int colout_notvalidGCR;                                  // only for display purposes
extern int is_colout;
void pixel(int r, int g, int b);



// All about reading the bits themselves (thus the timing)
#define ABSMAXPIP    10000                                 // was 300

int  MAXPIP=300;
int  pip[ABSMAXPIP];
int  buck[100];
int  pt=0;
int  setpeak[4];
float    p1=22;
float    p2=42;
int      pk=0;
int      pki=0;

/////////////////////////////////////////////////////////////////////////////
#define MAXBUCK    100

void printbucketedges()
{
   dbgprintf(stderr,"\nbucket edges <%.1f <%.1f <%.1f(1) <%.1f(01) <%.1f(001)\n",
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
   //dbgprintf(stderr,"\nHistogram: Track %02d Sector %02d\n",found_track,found_sector); // we dont know yet
   dbgprintf(stderr,"\nHistogram:\n");
   for (int l=10; l>=0; l--) {                             // for each line
      for (int i=1; i<MAXBUCK; ++i) {
         //instead of pk could use 10000
         if (l==0 && buck[i]==1) dbgprintf(stderr,".");
         else if (l==0 && buck[i]<=pk/10/2*l && buck[i]!=0) dbgprintf(stderr,":");
         else if (buck[i]>pk/10*l) dbgprintf(stderr,"*"); else dbgprintf(stderr," ");
      }
      dbgprintf(stderr,"\n");
      if (l==0) {
         for (int i=1; i<MAXBUCK; i+=10) dbgprintf(stderr,"         0");
         dbgprintf(stderr,"\n");
      }
   }
   for (int i=1; i<MAXBUCK; i++) {
      if (i==5 || i==8 || i==30 || i==47 || i==81)
         dbgprintf(stderr,"L");
      else dbgprintf(stderr,"_");
   }
   dbgprintf(stderr,"\n");
   for (int i=1; i<MAXBUCK; i++) {
      if (i==1+(int)(p1*0.5) || i==1+(int)(p1*(0.75)) || i==1+(int)(p2*0.5+p1*(0.5)) || i==1+(int)(p2+p1*0.5) || i==1+(int)(p2+p1*2.0))
         dbgprintf(stderr,"*");
      else dbgprintf(stderr," ");
   }
   dbgprintf(stderr,"\n");
   // ct <5 <8 <30 <47 <81 for 1000 (track1-18)
}

void bitpipe_new(int c)
{
   //           dbgprintf(stderr,"c=%d\n",c);
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

      dbgprintf(stderr,"\nPeak of %d at %d\n",pk,pki);
      p1=pki+3-3   +1;                                     // one less and no good  +0 few errs, +1,+2 good   +3 very bad
      if (p1<10) p1=10;
      p2=p1*2+3-3  +1;                                     // one less and no good

      if (111) dbgprintf(stderr,"\nPeak %f %f\n",p1,p2);
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
   dbgprintf(stderr,"fromi=%d\n",ix);
   for (int i=fromi; i<nx; i++) {                          // dont need to worry about reverse - same result
      c=rlebuff[i]; tot+=c;
      if (s==1) if (c!=0 && c!=255) bitpipe_new(c+lastc);
      //if (s==1) if (c!=0 && c!=255) bitpipe_new(c);
      s=1-s;
      lastc=c;
      if (tot> 20 *10*(256+80)) break;
   }
   dbgprintf(stderr,"Block preview done Track %02d Sector %02d\n",found_track,found_sector);
   if (1) {                                                // 0 = keep these values!
      p1=save_p1;
      p2=save_p2;
   }
   bitpipe_histogram();
}

int is_peaker=1;

void fluxspan(int algmode, int c, int lastc) {
         int ct=c+lastc;
         int cb;
         if (algmode==1) {                                 //old
            /*td="-";*/
            if (ct<7) { /*td=".";*/ /* ignore */ dbgprintf(stderr,"."); }
            if (ct>=7 && ct<=14) { /*td="_";*/ addbit(-1); dbgprintf(stderr,"_%d ",ct); }
            //if (ct>=7 && ct<=12) { /*td="_";*/ addbit(-1); dbgprintf(stderr,"_%d ",ct); }
            if (ct>=13 && ct<=26) { /*td="1";*/ addbit(1); }
            if (ct>=30 && ct<=42) { /*td="01";*/ addbit(0); addbit(1); }
            if (ct>=47 && ct<=55) { /*td="001";*/ addbit(0); addbit(0); addbit(1); }
            if (ct>55) { /*td=">>>>";*/ addbit(-1); dbgprintf(stderr,">%d ",ct); }
         }
         else if (algmode==0) {                            //old
            /*td="-";
            if (ct<7) { /*td="."; /* ignore */ dbgprintf(stderr,"."); }
            if (ct>=7 && ct<14) { /*td="_"; addbit(-1); dbgprintf(stderr,"_%d ",ct); }
            if (ct>=14 && ct<32) { /*td="1"; addbit(1); }
            if (ct>=32 && ct<49) { /*td="01"; addbit(0); addbit(1); }
            if (ct>=50 && ct<=60) { /*td="001"; addbit(0); addbit(0); addbit(1); }
            if (ct>60) { /*td=">>>>"; addbit(-1); dbgprintf(stderr,">%d ",ct); }
         }
         else if (algmode>100) {                           // its a frequency factor
            //
            // Timed mode ~600->1300
            //
            /*td="-";*/
            float sc=((float)algmode)/1000.0;
            if (ct<5.0*sc) { noteflux(ct,-1); /*td=".";*/ /* ignore */ dbgprintf(stderr,"."); }
            else if (ct<8.0*sc) { noteflux(ct,0); /*td="_";*/ addbit(-1); dbgprintf(stderr,"_%d ",ct); }
            else if (ct<30.0*sc) { noteflux(ct,1); /*td="1";*/ addbit(1); }
            else if (ct<(47.0)*sc) { noteflux(ct,2); /*td="01";*/ addbit(0); addbit(1); }
            else if (ct<=80.0*sc) { noteflux(ct,3); /*td="001";*/ addbit(0); addbit(0); addbit(1); }
            else {
               noteflux(ct,4); /*td=">>>>";*/

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
                  dbgprintf(stderr,">%d ",ct);                                   // act like two zeros at least
               }
               else {                                                            // old way
                  addbit(0); addbit(0); addbit(-1); dbgprintf(stderr,">%d ",ct); // act like two zeros at least
                  if (is_colout) for (int i=0; i<ct/40.0; ++i) {
                        int r=160,g=90,b=90;
                        pixel(r,g,b);
                     }
               }
            }
         }
         else {                                            // say 2
            // Dynamic Mode
            /*td="-";*/
            if (ct<       p1*0.5) { noteflux(ct,-1); /*td=".";*/ /* ignore */ dbgprintf(stderr,"."); }
            else if (ct<       p1*(0.75-0.05*adv_timing)+2*0) { noteflux(ct,0); /*td="_";*/ addbit(-1); dbgprintf(stderr,"_%d ",ct); } //55 good
            else if (ct<p2*0.5+p1*(0.50+0.05*adv_timing)) { noteflux(ct,1); /*td="1";*/ addbit(1); }
            else if (ct<p2    +p1*(0.50+0.05*adv_timing)-3*0) { noteflux(ct,2); /*td="01";*/ addbit(0); addbit(1); }                 // changing to 7 red SOME things better 0.65 good 0.60 0.611 switches
            else if (ct<p2    +p1*2.0) { noteflux(ct,3); /*td="001";*/ addbit(0); addbit(0); addbit(1); }
            else {
               noteflux(ct,4); /*td=">>>>";*/
               colout_notvalidGCR=1;
               int ab=0;
               for (int i=0; i<(int)(ct/p1)-1; ++i) {      // was 40
                  if (i>2) { addbit(0); ab++; }
               }
               ab+=4;
               addbit(0); addbit(0); addbit(0); addbit(1); // adding this!!
               addbit(-1);
               colout_notvalidGCR=1;
               dbgprintf(stderr,">%d(added:%d) ",ct,ab);   // act like two zeros at least
            }
         }

}

/*******/
/* END */
/*******/
