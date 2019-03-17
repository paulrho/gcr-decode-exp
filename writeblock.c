void write_datablock_to_file()
{
   // write data block to file
   char filename[80];
   char suffix[80];
   int  state=SM_MISSING;

   if (found_sector>=0 && found_sector<=20) {
      keep_track=found_track;                              // just for raw
      dbgprintf(stderr,"keep_track=%d\n",keep_track);
   }
   sprintf(filename,"data/track%02dsec%02d%s",found_track,found_sector,suffix);
   if ((ckm&0xFF)==databuf[256] && noncode==0) {
      //strcpy(suffix,".dat"); // original plain
      //sprintf(suffix,"-%03d.dat",badcount++); /// just temp
      // try this - add in a better checksum
      // the xcc16 "x" is to get the sort order toward the bottom
      // except dont do this if we already have the same file with same ck16 with good post
      // check if file already exists
      state=SM_GOOD;
      sprintf(suffix,"-%04x.dat",Fletcher16(databuf,256));
      FILE *fp=fopen(filename,"rb");
      if (fp!=NULL) {                                      // exists
         // so it exists in a good form - just do nothing - we have the same result
         dbgprintf(stderr,"block already saved as good with this ck16\n");
         fclose(fp);
         sprintf(filename,"");                             // lets flag it not to write
         // we'll just rewrite it so as not to touch the code far below
         // of course, it depends on the order whether the file below gets written first
      }
      else {
         // dont try and display the -1 in p - just make it pffff
         if (noncode || adv_wrongbits>0 || (((char)databuf[256+1])&0xFFF)!=0x00 || (((char)databuf[256+2])&0xFFF)!=0x00) {
            sprintf(suffix,"x%04x-w%02d-p%02x%02x%s.dat",Fletcher16(databuf,256),
                    adv_wrongbits,(((char)databuf[256+1])&0xFF), (((char)databuf[256+2])&0xFF)!=0x00, (noncode) ? "-nonc" : ""
                                                           //adv_wrongbits,(((char)databuf[256+1])&0xFFF), (((char)databuf[256+2])&0xFFF)!=0x00, (noncode) ? "-nonc" : ""
                    );
            int post=  (((char)databuf[256+1])&0xFF)<<8 || (((char)databuf[256+2])&0xFF);
            if (post>1) state=SM_GOODP;
            else if (post==1) state=SM_GOOD1;
            else state=SM_GOODN;                           // dont think wrongbits used (noncode)
         }
         else if (1) {                                     // actually - this is expensive
            // ... we are going to write a good one - so delete any sub-good ones
            //#include <stdio.h> //#include <dirent.h> //#include <string.h>

            struct dirent *de;                             // Pointer for directory entry
            DIR *          dr = opendir("data");           // opendir() returns a pointer of DIR type.

            if (dr != NULL) {                              // opendir returns NULL if couldn't open directory
               char comparename[80];
               char unlinkname[120];
               sprintf(comparename,"track%02dsec%02d%s",found_track,found_sector,suffix);
               comparename[12]='x';
               // data/track57sec00x0cf7-w00-p0001.dat
               //      0123456789012345678901234567,,,
               while ((de = readdir(dr)) != NULL) {
                  //dbgprintf(stderr,"Checking %s\n", de->d_name);
                  if (strncmp(comparename,de->d_name,12+5)==0 &&
                      strncmp(".dat",&de->d_name[27],4)==0) {
                     // get rid of this one
                     //dbgprintf(stderr,"Need to delete: %s\n", de->d_name);
                     sprintf(unlinkname,"data/%s",de->d_name);
                     dbgprintf2(stderr,"Need to del : %s\n", unlinkname);
                     unlink(unlinkname);
                  }
               }
               closedir(dr);
            }
         }
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

   if (filename[0]!=0) {
      dbgprintf2(stderr,"Writing block data/track%02dsec%02d%s\n",found_track,found_sector,suffix);
      sprintf(filename,"data/track%02dsec%02d%s",found_track,found_sector,suffix);
      FILE *fp=fopen(filename,"wb");
      if (fp!=NULL) {
         for (int i=0; i<256; ++i) fprintf(fp,"%c",databuf[i]);
         fclose(fp);
      }
   }
   if (sectormap_track>0) update_sectormap(found_track,found_sector,state);
}
