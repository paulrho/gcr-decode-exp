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
extern int is_one_sided;
#define TOPSEC    30
int  sectormap_track=-1;                                   // passed by new parameter
char sectormap[TOPSEC];                                    // will do whole disk later
// at moment - just for 35 track single side
int sectors_per_track(int track)
{
   if (is_one_sided && track>35) track=35;
   if (track>=1 && track<18) return 21;                    // 00..20
   else if (track>=18 && track <25) return 19;
   else if (track>=25 && track <31) return 18;
   else if (track>=31 && track <=35) return 17;
   else if (track>=1+35 && track<18+35) return 21;         // 00..20
   else if (track>=18+35 && track <25+35) return 19;
   else if (track>=25+35 && track <31+35) return 18;
   else if (track>=31+35 && track <=35+35) return 17;
   else return -1;
}

/**
 *         /HEADER.*BAD . SECTOR/ { if (v[$12][$9]<10-2) { t[$12]=1; v[$12][$9]++; }; next; }  'b' current 'x'
 *         /BAD!.*post:000[01]/    { if (v[$12][$9]<10-1) { t[$12]=1; v[$12][$9]=9; }; next; } 'c'
 *         /BAD/                   { if (v[$12][$9]<10-2) { t[$12]=1; v[$12][$9]++; }; next; } 'X' bad
 *         /GOOD.*NONCODE/         { if (v[$12][$9]<10-1) { t[$12]=1; v[$12][$9]=10; }; next; } 'C' - odd : good but with a noncode
 *         /GOOD/                  { if (v[$12][$9]<30)   { t[$12]=1; v[$12][$9]=20; } }       'P' - post >1
 *         /GOOD.*post:0001/       { if (v[$12][$9]<40)   { t[$12]=1; v[$12][$9]=30; } }       ':' = post=1
 *         /GOOD.*post:0000/       { t[$12]=1; v[$12][$9]=40; }                                '.' = good post=0
 **/

//     ?           .        D            :         P         N         C       X       H             B              b
enum { SM_MISSING, SM_GOOD, SM_GOOD_DUP, SM_GOOD1, SM_GOODP, SM_GOODN, SM_CKM, SM_BAD, SM_HEADER_OK, SM_HEADER_ODD, SM_HEADER_BAD };
char *sectormap_char="?.D:PNCXHBb";
void init_sectormap()
{
   for (int i=0; i<TOPSEC; ++i) sectormap[i]=SM_MISSING;
   // load a bitmap that saves state between runs - needs to be invalidated for different tracks
   FILE *fp=fopen("bitmap.dat","rb");
   if (fp==NULL) return;
   int n=fread(sectormap, 1, TOPSEC, fp);
   dbgprintf(stderr,"track %02d bitmap_d: ",sectormap_track);
   for (int i=0; i<n; ++i) dbgprintf(stderr,"%d ",sectormap[i]);
   dbgprintf(stderr,"\n");
   fclose(fp);
}

int save_sectormap()
{
   FILE *fp=fopen("bitmap.dat","wb");

   if (fp==NULL) {
      dbgprintf(stderr,"could not save bitmap\n");
      return -1;
   }
   for (int i=0; i<TOPSEC; ++i) fprintf(fp,"%c",sectormap[i]);
   fclose(fp);
   dbgprintf(stderr,"track %02d bitmap_c: ",sectormap_track);
   for (int i=0; i<TOPSEC; ++i) dbgprintf(stderr,"%c",i<sectors_per_track(sectormap_track) ? sectormap_char[sectormap[i]] : ' ');
   dbgprintf(stderr,"\n");

   // last line
   fprintf(stderr,"TRACK %2d ",sectormap_track);
   for (int i=0; i<TOPSEC; ++i) fprintf(stderr,"%c",i<sectors_per_track(sectormap_track) ? sectormap_char[sectormap[i]] : ' ');
   fprintf(stderr,"\n");
   return 0;
}

void update_sectormap(int track, int sector, int state)
{
   if (track==sectormap_track && sector>=0 && sector<TOPSEC && sector<sectors_per_track(track)) {
      if (state>0 && state<sectormap[sector] || sectormap[sector]==0) sectormap[sector]=state;
   }
}

int is_good_sectormap()
{
   // allow GOOD1 too
   for (int i=0; i<sectors_per_track(sectormap_track); ++i) if (sectormap[i]!=SM_GOOD && sectormap[i]!=SM_GOOD1) return 0;
   return 1;
}

/////////////////////////////////////////////////////////////////////////////
