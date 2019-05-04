int findblocks(int track, int sector, int change);

#include "tracker.h"

void scanblocks()
{
   int st=0;
   int worst=SM_GOOD;

   for (int track=1; track<=70; ++track) {
      //fprintf(stderr,"track=%d\n",track);
      if (!is_one_sided && track==36) {
         for (int sector=0; sector<sectors_per_track(track); ++sector) {
            st=findblocks(track,sector,0);
            if (st<SM_GOOD || st>SM_BAD) break;
         }
         if (st<SM_GOOD || st>SM_BAD) break;               // note - will still kick over on a "bad" - but existing sector
      }
      else if (is_one_sided && track>=36) {
         // if anything good - go, otherwise finish
         st=findblocks(track,/*sector*/0,0);
         if (st<SM_GOOD || st>SM_BAD) break;
      }
      fprintf(stdout,"TRACK %2d ",track);
      for (int sector=0; sector<sectors_per_track(track); ++sector) {
         int st=findblocks(track,sector,1);
         fprintf(stdout,"%c",sectormap_char[st]);
         if (!st || st>worst && worst) worst=st;
      }
      fprintf(stdout,"\n");
   }
   switch (worst) {
     case SM_MISSING: fprintf(stdout,"miss"); break;
     case SM_GOODN:
     case SM_GOODP:
     case SM_GOOD1:
     case SM_GOOD:    fprintf(stdout,"good"); break;
     case SM_GOOD_DUP: fprintf(stdout,"dupe"); break;
     case SM_CKM:     fprintf(stdout,"ckm"); break;
     case SM_BAD:
     default:         fprintf(stdout,"bad"); break;
   }
   fprintf(stdout,"\n");
}

#define UPDSTATUS(a)    if (!status || a<status) status=a

int findblocks(int track, int sector, int change)
{
   int            status=SM_MISSING;
   struct dirent *de;                                      // Pointer for directory entry
   DIR *          dr;                                      // opendir() returns a pointer of DIR type.

   char comparename[80];
   char newname[80];
   char oldname[80];

   sprintf(comparename,"track%02dsec%02d",track,sector);

   int good_count=0;
   int goodish_count=0;
   int ckm_count=0;
   int bad_count=0;

   dr = opendir("data");                                   // opendir() returns a pointer of DIR type.
   if (dr != NULL) {                                       // opendir returns NULL if couldn't open directory
      // data/track57sec00-aaaa.dat
      // data/track57sec00x0cf7-w00-p0001.dat
      //      0123456789012345678901234567,,,
      while ((de = readdir(dr)) != NULL) {
         //fprintf(stderr,"Checking %s\n", de->d_name); fflush(stderr);
         int l=strlen(de->d_name);
         if (l>=12 && strncmp(".missing.dat",&de->d_name[l-12],12)==0) {

            strcpy(oldname,"data/"); strcat(oldname,de->d_name);
            unlink(oldname);
            fprintf(stderr,"removing %s\n",oldname);
            continue;
         }
         else if (l>=12 && strncmp(comparename,de->d_name,12)==0 &&
                  !(l>=8 && strncmp(".ckm.dat",&de->d_name[l-8],8)==0) &&
                  !(l>=8 && strncmp(".bad.dat",&de->d_name[l-8],8)==0)) {
            // found a matching track/sector
            if (l>=4 && strncmp(".dat",&de->d_name[l-4],4)==0) {
               // this is one of the blocks with a .dat ending
               if (l==21) {
// .   this is a good block
                  // but if we get a second one - move it to ignore...
                  UPDSTATUS(SM_GOOD);
                  good_count++;
                  if (good_count>1) {
                     // rename it to ignore
                     UPDSTATUS(SM_GOOD_DUP);               // make a duplicate status (perhaps)
                  }
               }
               else if (l>21) {
                  // this is a block that is a little bit not -> but still good -> check the p000? status...
                  char pxxx[5];
                  strncpy(pxxx,&de->d_name[23],4); pxxx[4]=0;
                  if (strcmp(pxxx,"0000")==0) {
// w  w!= but p=0000
                     UPDSTATUS(SM_GOODN);
                  }
                  else if (strcmp(pxxx,"0001")==0) {
// :   p=0001
                     UPDSTATUS(SM_GOOD1);
                  }
                  else {
// P   p=nnnn >1
                     UPDSTATUS(SM_GOODP);
                  }
                  goodish_count++;
               }
            }
            else if (l>=8 && strncmp(".ckm.dat",&de->d_name[l-8],8)==0 ||
                     l>=8 && strncmp(".bad.dat",&de->d_name[l-8],8)==0) {

               strcpy(oldname,"data/"); strcat(oldname,de->d_name);
               unlink(oldname);
               //otherwise ignore - there is a copy elsewhere
            }
            else if (l>=4 && strncmp(".ckm",&de->d_name[l-4],4)==0) {
// C   checksum incorrect
               UPDSTATUS(SM_CKM);
               ckm_count++;
            }
            else
            if (l>=4 && strncmp(".bad",&de->d_name[l-4],4)==0) {
// X   bad block - something wrong with it
               UPDSTATUS(SM_BAD);
               bad_count++;
            }
         }
      }                                                    // while read dir
      closedir(dr);
   }                                                       // if dir
   //if (good_count==1) return status;

   if (status==SM_MISSING) {
      if (change) {
         // create empty
         int  n;
         char buf[256];
         for (int i=0; i<256; ++i) buf[i]=0;
         sprintf(newname,"data/track%02dsec%02d.missing.dat",track,sector);
         //sprintf(newname,"data/trackxxsecxx.missing.dat",track,sector);
         FILE *fp=fopen(newname,"wb");
         if (fp!=NULL) {
            //fprintf(stderr,"writing %s to t%d/s%d\n",newname,track,sector);
            fwrite(buf,1,256,fp);
            fclose(fp);
         }
      }
      return status;
   }

   int good_recount=0;
   int goodish_recount=0;
   dr = opendir("data");                                   // opendir() returns a pointer of DIR type.
   if (dr != NULL) {                                       // opendir returns NULL if couldn't open directory
      while ((de = readdir(dr)) != NULL) {
         //fprintf(stderr,"Checking %s\n", de->d_name); fflush(stderr);
         int l=strlen(de->d_name);
         if (l>=12 && strncmp(comparename,de->d_name,12)==0) {
            // found a matching track/sector
            if (l>=4 && strncmp(".dat",&de->d_name[l-4],4)==0) {
               // this is one of the blocks with a .dat ending
               if (l==21) {
// .   this is a good block
                  // but if we get a second one - move it to ignore...
                  good_recount++;
                  if (good_recount>1) {
                     // rename to add .ignore
                     strcpy(newname,de->d_name); strcat(newname,".ignore"); rename(de->d_name,newname);
                     fprintf(stderr,"t%d s%drename to %s\n",track,sector,newname);
                     strcpy(oldname,"data/"); strcat(oldname,de->d_name);
                     strcpy(newname,"data/"); strcat(newname,de->d_name); strcat(newname,".ignore");
                     rename(oldname,newname);
                     fprintf(stderr,"t%d s%drename to %s\n",track,sector,newname);
                  }
               }
               else if (l>21) {
                  // this is a block that is a little bit not -> but still good -> check the p000? status...
                  if (good_count>0) {
                     // rename to add .ignore
                     strcpy(oldname,"data/"); strcat(oldname,de->d_name);
                     strcpy(newname,"data/"); strcat(newname,de->d_name); strcat(newname,".ignore");
                     rename(oldname,newname);
                     fprintf(stderr,"t%d s%drename to %s\n",track,sector,newname);
                  }
                  else if (goodish_count>0) {
                     goodish_recount++;
                     if (goodish_recount>1) {
                        // rename to add .ignore
                        strcpy(oldname,"data/"); strcat(oldname,de->d_name);
                        strcpy(newname,"data/"); strcat(newname,de->d_name); strcat(newname,".ignore");
                        rename(oldname,newname);
                        fprintf(stderr,"t%d s%drename to %s\n",track,sector,newname);
                     }
                  }
               }
            }
            else if (l>=4 && strncmp(".ckm",&de->d_name[l-4],4)==0) {
// C   checksum incorrect
               if (good_count==0 && goodish_count==0) {
                  // COPY .dat on the end and exit!
                  strcpy(oldname,"data/"); strcat(oldname,de->d_name);
                  strcpy(newname,"data/"); strcat(newname,de->d_name); strcat(newname,".dat");
                  link(oldname,newname);
                  fprintf(stderr,"copying to *.dat\n");
                  break;
               }
            }
            else if (l>=4 && strncmp(".bad",&de->d_name[l-4],4)==0) {
// X   bad block - something wrong with it
               if (good_count==0 && goodish_count==0 && ckm_count==0) {
                  // COPY .dat on the end and exit!
                  strcpy(oldname,"data/"); strcat(oldname,de->d_name);
                  strcpy(newname,"data/"); strcat(newname,de->d_name); strcat(newname,".dat");
                  link(oldname,newname);
                  fprintf(stderr,"copying to *.dat\n");
                  break;
               }
            }
         }
      }                                                    // while read dir
      closedir(dr);
   }                                                       // if dir
   return status;
}
