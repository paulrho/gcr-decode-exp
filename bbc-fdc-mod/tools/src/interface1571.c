// this is duplicate code with sectormap.c (part of r15)
#define TOPSEC 30
enum { SM_MISSING, SM_GOOD, SM_GOOD_DUP, SM_GOOD1, SM_GOODP, SM_GOODN, SM_CKM, SM_BAD, SM_HEADER_OK, SM_HEADER_ODD, SM_HEADER_BAD };
char *sectormap_char="?.D:PNCXHBb";
char sectormap[TOPSEC];                                    // will do whole disk later

int sectors_per_track(int track)
{
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
// duplicate code

// extended code
void
interface_display_read_sectors(int is_from_previous) // will look at bitmap.dat and display it on screen
{
	// is_from_previous 
  for (int i=0; i<TOPSEC; ++i) sectormap[i]=SM_MISSING;
  // load a bitmap that saves state between runs - needs to be invalidated for different tracks
  //FILE *fp=fopen("bitmap.dat","rb");
  char filename[80];
  //if (111) { // one sided but reading the reverse 
	  //sprintf(filename,"bitmapt%02d.dat",last_display_head_track+1+4*last_display_head_side/* *2+1+h*35 offset*/);
  //} else {
	  sprintf(filename,"bitmapt%02d.dat",last_display_head_track+1+last_display_head_side*35/* *2+1+h*35 offset*/);
  //}
  FILE *fp=fopen(filename,"rb");
  if (fp==NULL) return;
  int n=fread(sectormap, 1, TOPSEC, fp);
  fclose(fp);
  for (int i=0; i<sectors_per_track(last_display_head_track+1) && i<n; ++i) 
    updatesector(last_display_head_track+1,last_display_head_side,i,sectormap_char[(int)sectormap[i]]+100+100*is_from_previous);
}

int interface_track_is_good() {
  for (int i=0; i<sectors_per_track(last_display_head_track+1); ++i) 
    if (!sectormap[i] || sectormap[i]>SM_GOODN) return 0;
  return 1;
}

