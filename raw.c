
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
      dbgprintf(stderr,"going around again... ");
   }
   else
   if (save_raw==2) {
      dbgprintf(stderr,"stopping\n");
      save_raw=-1;
      rawp=rawp-rew;
   }
}

void switchonraw(int rew)
{
   if (save_raw==1) {                                      // only start recording if we didnt get it
      raw_gotbad=0;
      dbgprintf(stderr,"rewinding\n");
      save_raw=2;                                          // first time header
      raw_starts=rawp-rew;
      // shuffle  - rewind only to start (just before, so we get the sync)
      rawp=rawp-raw_starts;                                // should be 10
      if (raw_starts>0) for (int i=0; i<rawp; ++i) { rawc[i]=rawc[i+raw_starts]; }
   }
   if (save_raw>0) raw_starts=rawp;                        // we start at the beginning (used for undo)
}

