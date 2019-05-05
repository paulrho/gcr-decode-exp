#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "hardware.h"
#include "diskstore.h"
#include "dfi.h"
#include "adfs.h"
#include "dfs.h"
#include "dos.h"
#include "fsd.h"
#include "rfi.h"
#include "mod.h"
#include "fm.h"
#include "mfm.h"

#include "display.h"

// For type of capture
#define DISKNONE 0
#define DISKCAT 1
#define DISKIMG 2
#define DISKRAW 3

// For type of output
#define IMAGENONE 0
#define IMAGERAW 1
#define IMAGESSD 2
#define IMAGEDSD 3
#define IMAGEFSD 4
#define IMAGEDFI 5
#define IMAGEIMG 6

// Used for values which can be overriden
#define AUTODETECT -1

// Capture retries when not in raw mode
#define RETRIES 5

// Number of rotations to cature per track
//#define ROTATIONS 3 // was 3 // temp 10
int ROTATIONS=3;

#ifdef NOPI
int hw_child=-1;
int trick_index=0;
#endif

int do_initialread=1;
int is_fasttry=0; // speeds track stepping, waiting on index, track settling time
int skiptracks=0;
int stoptracks=-1;
int halftracks=0;

int debug=0;
int summary=0;
int catalogue=0;
int sides=AUTODETECT;
unsigned int disktracks, drivetracks;
int capturetype=DISKNONE; // Default to no output
int outputtype=IMAGENONE; // Default to no image

unsigned char *samplebuffer=NULL;
unsigned char *flippybuffer=NULL;
unsigned long samplebuffsize;
int flippy=0;
int info=0;

// Processing position within the SPI buffer
unsigned long datapos=0;

// File handles
FILE *diskimage=NULL;
FILE *rawdata=NULL;

// Used for reversing bit order within a byte
static unsigned char revlookup[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe, 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};
unsigned char reverse(unsigned char n)
{
   // Reverse the top and bottom nibble then swap them.
   return (revlookup[n&0x0f]<<4) | revlookup[n>>4];
}

// Used for flipping the bits in a raw sample buffer
void fillflippybuffer(const unsigned char *rawdata, const unsigned long rawlen)
{
  if (flippybuffer==NULL)
    flippybuffer=malloc(rawlen);

  if (flippybuffer!=NULL)
  {
    unsigned long em;

    for (em=0; em<rawlen; em++)
      flippybuffer[rawlen-em]=reverse(rawdata[em]);
  }
}

void
signal_child() {
  printf("Signalling child\n");
  //signal(SIGTERM, SIG_IGN); // Termination request
  if (hw_child>0) kill(hw_child,SIGTERM);
  //raise(SIGTERM);
}

// Stop the motor and tidy up upon exit
void exitFunction()
{
  display_finalize();
  printf("Exit function\n");
  signal_child(); // actually dont need this but...
  hw_done();

  if (samplebuffer!=NULL)
  {
    free(samplebuffer);
    samplebuffer=NULL;
  }

  if (flippybuffer!=NULL)
  {
    free(flippybuffer);
    flippybuffer=NULL;
  }
}

// Handle signals by stopping motor and tidying up
void sig_handler(const int sig)
{
  if (hw_child>0) {
    display_finalize();
  }
  fprintf(stderr,"Got signal %d\n",sig);
  fflush(stderr);
  if (sig==SIGSEGV)
    printf("SEG FAULT\n");

  hw_done();
      signal_child(); // actually dont need this but...
  printf("Exiting from signal\n");
  exit(0);
}

void showargs(const char *exename)
{
  fprintf(stderr, "%s - Floppy disk raw flux capture and processor\n\n", exename);
  fprintf(stderr, "Syntax : ");
#ifdef NOPI
  fprintf(stderr, "[-i input_rfi_file] ");
#endif
  fprintf(stderr, "[[-c] | [-o output_file]] [-spidiv spi_divider] [[-ss]|[-ds]] [-r retries] [-sort] [-summary] [-tmax maxtracks] [-v]\n");
}

int main(int argc,char **argv)
{
  int argn=0;
  unsigned int i, j, rate;
           //int i         ; // allow neg!
  //unsigned int    j, rate;
  unsigned char retry, retries, side, drivestatus;
  int sortsectors=0;
  int missingsectors=0;
  char modulation=AUTODETECT;
#ifdef NOPI
  char *samplefile;
#endif

  // Check we have some arguments
  if (argc==1)
  {
    showargs(argv[0]);
    return 1;
  }

  // Set some defaults
  retries=RETRIES;
  rate=HW_SPIDIV32;
#ifdef NOPI
  samplefile=NULL;
#endif

  display_init(); // display

  // Process command line arguments
  while (argn<argc)
  {
    if (strcmp(argv[argn], "-v")==0)
    {
      debug=1;
      catalogue=1;
    }
    else
    if (strcmp(argv[argn], "-c")==0)
    {
      catalogue=1;

      if (capturetype==DISKNONE)
        capturetype=DISKCAT;
    }
    else
    if (strcmp(argv[argn], "-flipped")==0)
    {
      trick_index=3;
    }
    else
    if (strcmp(argv[argn], "-half")==0)
    {
      halftracks=1;
    }
    else
    if (strcmp(argv[argn], "-sort")==0)
    {
      sortsectors=1;
    }
    else
    if (strcmp(argv[argn], "-fast")==0)
    {
      is_fasttry=1;
    }
    else
    if (strcmp(argv[argn], "-noinit")==0)
    {
      do_initialread=0;
    }
    else
    if ((strcmp(argv[argn], "-skip")==0) && ((argn+1)<argc))
    {
      int retval;
      ++argn;
      if (sscanf(argv[argn], "%3d", &retval)==1)
        skiptracks=retval;
    }
    else
    if ((strcmp(argv[argn], "-stop")==0) && ((argn+1)<argc))
    {
      int retval;
      ++argn;
      if (sscanf(argv[argn], "%3d", &retval)==1)
        stoptracks=retval;
    }
    else
    if ((strcmp(argv[argn], "-rotations")==0) && ((argn+1)<argc))
    {
      int retval;
      ++argn;
      if (sscanf(argv[argn], "%3d", &retval)==1)
        ROTATIONS=retval;
      if (ROTATIONS<1 || ROTATIONS>10) ROTATIONS=3;
    }
    else
    if (strcmp(argv[argn], "-summary")==0)
    {
      summary=1;
    }
    else
    if (strcmp(argv[argn], "-ds")==0)
    {
      printf("Forced double-sided capture\n");

      // Request double-sided
      sides=2;
    }
    else
    if (strcmp(argv[argn], "-ss")==0)
    {
      printf("Forced single-sided capture\n");

      // Request single-sided
      sides=1;
    }
    else
    if ((strcmp(argv[argn], "-r")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      if (sscanf(argv[argn], "%3d", &retval)==1)
        retries=retval;
    }
    else
    if ((strcmp(argv[argn], "-tmax")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      // Override the maximum number of drive tracks
      if (sscanf(argv[argn], "%3d", &retval)==1)
      {
        hw_setmaxtracks(retval);
        printf("Override maximum number of drive tracks to %d\n", retval);
      }
    }
    else
    if ((strcmp(argv[argn], "-spidiv")==0) && ((argn+1)<argc))
    {
      int retval;

      ++argn;

      if (sscanf(argv[argn], "%4d", &retval)==1)
      {
        switch (retval)
        {
          case 16:
          case 32:
          case 64:
            rate=retval;
            printf("Setting SPI divider to %u\n", rate);
            break;

          default:
            fprintf(stderr, "Invalid SPI divider\n");
            return 7;
            break;
        }
      }
    }
    else
    if ((strcmp(argv[argn], "-o")==0) && ((argn+1)<argc))
    {
      ++argn;

      if (strstr(argv[argn], ".ssd")!=NULL)
      {
        // .SSD is single sided
        sides=1;

        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGESSD;
        }
        else
          printf("Unable to save ssd image\n");
      }
      else
      if (strstr(argv[argn], ".dsd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEDSD;
        }
        else
          printf("Unable to save dsd image\n");
      }
      else
      if (strstr(argv[argn], ".fsd")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEFSD;
        }
        else
          printf("Unable to save fsd image\n");
      }
      else
      if (strstr(argv[argn], ".img")!=NULL)
      {
        diskimage=fopen(argv[argn], "w+");
        if (diskimage!=NULL)
        {
          capturetype=DISKIMG;
          outputtype=IMAGEIMG;
          display_info_filename(argv[argn]);
        }
        else
          printf("Unable to save fsd image\n");
      }
      else
      if (strstr(argv[argn], ".rfi")!=NULL)
      {
        rawdata=fopen(argv[argn], "w+");
        if (rawdata!=NULL)
        {
          capturetype=DISKRAW;
          outputtype=IMAGERAW;
          display_info_filename(argv[argn]);
        }
        else
          printf("Unable to save rawdata\n");
      }
      else
      if (strstr(argv[argn], ".dfi")!=NULL)
      {
        rawdata=fopen(argv[argn], "w+");
        if (rawdata!=NULL)
        {
          capturetype=DISKRAW;
          outputtype=IMAGEDFI;
        }
        else
          printf("Unable to save dfi image\n");
      }
    }
#ifdef NOPI
    else
    if ((strcmp(argv[argn], "-i")==0) && ((argn+1)<argc))
    {
      ++argn;

      samplefile=argv[argn];

    }
#endif

    ++argn;
  }

#ifdef NOPI
    if ((samplefile==NULL) || (!hw_init(samplefile, rate)))
    {
      fprintf(stderr, "Failed virtual hardware init\n");
      return 4;
    }
#endif

  // Make sure we have something to do
  if (capturetype==DISKNONE)
  {
    showargs(argv[0]);
    return 1;
  }

  diskstore_init();

  mod_init(debug);

#ifndef NOPI
  if (geteuid() != 0)
  {
    fprintf(stderr,"Must be run as root\n");
    return 2;
  }
#endif

#ifndef NOPI
  if (!hw_init(rate))
  {
    fprintf(stderr, "Failed hardware init\n");
    return 4;
  }
#endif

  // Allocate memory for SPI buffer
  if (ROTATIONS==1) // give it a bit more 10%
    //samplebuffsize=(((hw_samplerate+hw_samplerate/10)/HW_ROTATIONSPERSEC)/BITSPERBYTE)*ROTATIONS;
    samplebuffsize=(((hw_samplerate+hw_samplerate/20)/HW_ROTATIONSPERSEC)/BITSPERBYTE)*ROTATIONS; // try 20%
  else
    samplebuffsize=((hw_samplerate/HW_ROTATIONSPERSEC)/BITSPERBYTE)*ROTATIONS;

  samplebuffer=malloc(samplebuffsize);
  if (samplebuffer==NULL)
  {
    fprintf(stderr, "\n");
    return 3;
  }

  printf("Start with %lu byte sample buffer\n", samplebuffsize);

  // Install signal handlers to make sure motor is stopped
  atexit(exitFunction);
  signal(SIGINT, sig_handler); // Ctrl-C
  signal(SIGSEGV, sig_handler); // Seg fault
  signal(SIGTERM, sig_handler); // Termination request

  drivestatus=hw_detectdisk();
  printf("returned from detect disk\n");

  if (drivestatus==HW_NODRIVE)
  {
    fprintf(stderr, "Failed to detect drive\n");
    return 5;
  }

  if (drivestatus==HW_NODISK)
  {
    fprintf(stderr, "Failed to detect disk in drive\n");
    return 6;
  }

  // Select drive, depending on jumper
  hw_driveselect();

  // Start MOTOR
  hw_startmotor();

  // Wait for motor to get up to speed
  hw_sleep(1);

  // Determine if head is at track 00
  //if (!notzero) {
    if (hw_attrackzero())
      printf("Starting at track zero\n");
    else
      printf("Starting not at track zero\n");
  //} else {
    //printf("Forcing not to start at zero\n");
  //}

  // Determine if disk in drive is write protected
  if (hw_writeprotected())
    printf("Disk is write-protected\n");
  else
    printf("Disk is writeable\n");

  // Start off assuming an 80 track disk in 80 track drive
  disktracks=hw_maxtracks;
  drivetracks=hw_maxtracks;

  // Try to determine what type of disk is in what type of drive

  // Seek to track 2
  hw_seektotrack(2);

  // Select lower side
  hw_sideselect(0); showhead(hw_currenttrack,0);

  // Wait for a bit after seek to allow drive speed to settle
  hw_sleep(1);

  if (do_initialread) {
  // Sample track
//printf("starting sampleraw data 1\n");
  hw_samplerawtrackdata((char *)samplebuffer, samplebuffsize);
//printf("finished sampleraw data 1\n");
  mod_process(samplebuffer, samplebuffsize, 99);
  }

  // Check readability
  if ((fm_lasttrack==-1) && (fm_lasthead==-1) && (fm_lastsector==-1) && (fm_lastlength==-1))
    printf("No FM sector IDs found\n");
  else
    modulation=MODFM;

  if ((mfm_lasttrack==-1) && (mfm_lasthead==-1) && (mfm_lastsector==-1) && (mfm_lastlength==-1))
    printf("No MFM sector IDs found\n");
  else
    modulation=MODMFM;

  if (modulation!=AUTODETECT)
  {
    int othertrack=-1; //
    int otherhead;

    // Check if it was FM sectors found
    if ((fm_lasttrack!=-1) && (fm_lasthead!=-1) && (fm_lastsector!=-1) && (fm_lastlength!=-1))
    {
      othertrack=fm_lasttrack;
      otherhead=fm_lasthead;
    }

    // Check if it was MFM sectors found
    if ((mfm_lasttrack!=-1) && (mfm_lasthead!=-1) && (mfm_lastsector!=-1) && (mfm_lastlength!=-1))
    {
      othertrack=mfm_lasttrack;
      otherhead=mfm_lasthead;
    }

    // Only look at other side if user hasn't specified number of sides
    if (sides==AUTODETECT)
    {
      // Select upper side
      hw_sideselect(1); showhead(hw_currenttrack,1);

      // Wait for a bit after head switch to allow drive to settle
      hw_sleep(1);

  //if (do_initialread) {
      // Sample track
//printf("starting sampleraw data 2\n");
      hw_samplerawtrackdata((char *)samplebuffer, samplebuffsize);
//printf("finished sampleraw data 2\n");
      mod_process(samplebuffer, samplebuffsize, 99);
  //}

      // Check for flippy disk
      if ((fm_lasttrack==-1) && (fm_lasthead==-1) && (fm_lastsector==-1) && (fm_lastlength==-1)
         && (mfm_lasttrack==-1) && (mfm_lasthead==-1) && (mfm_lastsector==-1) && (mfm_lastlength==-1))
      {
        fillflippybuffer(samplebuffer, samplebuffsize);

        if (flippybuffer!=NULL)
          mod_process(flippybuffer, samplebuffsize, 99);

        if ((fm_lasttrack!=-1) || (fm_lasthead!=-1) || (fm_lastsector!=-1) || (fm_lastlength!=-1)
           || (mfm_lasttrack!=-1) || (mfm_lasthead!=-1) || (mfm_lastsector!=-1) || (mfm_lastlength!=-1))
        {
          printf("Flippy disk detected\n");
          flippy=1;
        }
      }

      // Check readability
      if ((fm_lasttrack==-1) && (fm_lasthead==-1) && (fm_lastsector==-1) && (fm_lastlength==-1)
         && (mfm_lasttrack==-1) && (mfm_lasthead==-1) && (mfm_lastsector==-1) && (mfm_lastlength==-1))
      {
        // Only lower side was readable
        printf("Single-sided disk assumed, only found data on side 0\n");

        sides=1;
      }
      else
      {
        // If IDAM shows same head, then double-sided separate
        if ((fm_lasthead==otherhead) || (mfm_lasthead==otherhead))
          printf("Double-sided with separate sides disk detected\n");
        else
          printf("Double-sided disk detected\n");

        // Only mark as double-sided when not using single-sided output
        if (outputtype!=IMAGESSD)
          sides=2;
        else
          sides=1;
      }
    }

    // If IDAM cylinder shows 2 then correct stepping
    //if (1111111111111 || othertrack==2) //just for now PGS - switch this back off again
    if (othertrack==2) //back to normal
    {
      printf("Correct drive stepping for this disk and drive\n");
    }
    else
    {
      // If IDAM cylinder shows 1 then 40 track disk in 80 track drive
      if (othertrack==-1)
      {
        printf("40 track disk assumed (cant read MFM header), enabled double stepping\n");

	if (!halftracks) {
          // Enable double stepping
          hw_stepping=HW_DOUBLESTEPPING;
  
          disktracks=40;
          drivetracks=80;
  
          drivetracks=40; // really - 40 is the max!! (not sure why this was like this
	} else {
		// just for a special test
          hw_stepping=HW_NORMALSTEPPING;
          disktracks=40;
          drivetracks=80;
	}
      }
      if (othertrack==1)
      {
        printf("40 track disk detected in 80 track drive, enabled double stepping\n");

        // Enable double stepping
        hw_stepping=HW_DOUBLESTEPPING;

        disktracks=40;
        drivetracks=80;
      }

      // If IDAM cylinder shows 4 then 80 track in 40 track drive
      if (othertrack==4)
      {
        printf("80 track disk detected in 40 track drive\n*** Unable to fully image this disk in this drive ***\n");

        disktracks=80;
        drivetracks=40;
      }
    }
  }

  // Number of sides failed to autodetect and was not forced, so assume 1
  if (sides==AUTODETECT) sides=1;

  // Write RFI header when doing raw capture
  if (capturetype==DISKRAW)
  {
    if (outputtype==IMAGERAW)
      rfi_writeheader(rawdata, drivetracks, sides, hw_samplerate, hw_writeprotected());

    if (outputtype==IMAGEDFI)
      dfi_writeheader(rawdata);
  }

  printf("Got to zero track command\n");
  // Start at track 0
  hw_seektotrackzero();

  printf("about to start loop\n");
  if (skiptracks>0) printf("skipping %d tracks\n",skiptracks);
  // Loop through the tracks
  //for (i=-4; i<(int)drivetracks; i++)
  if (stoptracks>=0) { drivetracks=stoptracks+1; printf("will be stopping early\n"); }
  for (i=0+ skiptracks; i<drivetracks; i++) // normal
  {
    printf("about to seek to %d\n",i);
    hw_seektotrack(i);

    // Process all available disk sides (heads)
    for (side=0; side<sides; side++)
    {
      // Request a directory listing for this side of the disk
      if (i==0) info=0;

      // Select the correct side
      hw_sideselect(side); showhead(hw_currenttrack,side);

      // clear 
      system("interface-rclear");
      // Retry the capture if any sectors are missing
      printf("about to do retries = %d\n",retries);
      if (1) {
	      interface_display_read_sectors(1); // from previous runs (must be on this same disk - with valid rfi somewhere for the eventual read)
	      if (interface_track_is_good()) continue;  // just go to the next, we wont even try
      }
      for (retry=0; retry<retries; retry++)
      {
        // Wait for a bit after seek/head select to allow drive speed to settle
        if (!is_fasttry) hw_sleep(1); // FASTTRY
        else hw_usleep(18000); // track settling time 3.5 floppy

        if (retry==0)
          printf("Sampling data for track %.2X head %.2x\n", i, side);
        else if (retry>3+2) {
          //hw_seektotrack(0);
          //hw_seektotrack(i);
            //hw_usleep(500000); // 
            hw_sleep(1); // 
	  if (i==0) {
            hw_seektotrack(10);
          } else
            hw_seektotrack(0);
          //hw_seektotrack(i<10?10:i-10);
	    showhead(hw_currenttrack,1-side);
            //hw_usleep(500000); // 
            hw_sleep(1); // 
	  if (0) {
	    hw_sideselect(1-side); 
	      showhead(hw_currenttrack,1-side);
              hw_usleep(18000); // track settling time 3.5 floppy
	    hw_sideselect(side); 
	      showhead(hw_currenttrack,side);
              hw_usleep(18000); // track settling time 3.5 floppy
	  }
          hw_seektotrack(i);
	    showhead(hw_currenttrack,side);
            //hw_usleep(500000); //
            hw_sleep(1); // 
        }

        // Sampling data
//printf("starting sampleraw data 3\n");
        hw_samplerawtrackdata((char *)samplebuffer, samplebuffsize);
//printf("finished sampleraw data 3\n");
////PGS11111////
	// do this if not going to do it later - we want to do this if C64
        if (0) mod_findpeaks(samplebuffer, samplebuffsize);
        if (1) mod_findpeaks(samplebuffer, samplebuffsize);

        // Process the raw sample data to extract FM encoded data
        if (capturetype!=DISKRAW)
        {
//printf("not DISKRAW\n");
          if ((flippy==0) || (side==0))
          {
            mod_process(samplebuffer, samplebuffsize, retry);
          }
          else
          {
//printf("reading flippy\n");
            fillflippybuffer(samplebuffer, samplebuffsize);

            if (flippybuffer!=NULL)
              mod_process(flippybuffer, samplebuffsize, retry);
          }

#ifdef NOPI
          // No point in retrying when not using real hardware
          break;
#endif

          // Don't retry unless imaging DFS disks
          if ((outputtype!=IMAGEDSD) && (outputtype!=IMAGESSD)
          && (outputtype!=IMAGEIMG)
) {
//printf("not retrying because of outputtype %d\n",outputtype);
            break;
}

          int msg=0;
          for (j=1; j<=18; j++)
            if (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, j)==NULL) {
		    if (!msg) { 
			    msg=1;
                            printf("\nsee if we can retry: missing: ");
		    }
		    printf("%.2u ", j);
	    }
          if (!msg) printf("\nall sectors read successfully");
          printf("\n");
          // Determine if we have successfully read the whole track
          if (//(diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 0)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 1)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 2)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 3)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 4)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 5)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 6)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 7)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 8)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 9)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 10)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 11)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 12)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 13)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 14)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 15)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 16)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 17)!=NULL) &&
              (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, 18)!=NULL)) {
            fprintf(stderr,"all sectors 0-17 read\n");
            break;
          }

          printf("Retry attempt %d, sectors ", retry+1);
          for (j=0; j<DFS_SECTORSPERTRACK; j++)
            if (diskstore_findhybridsector(hw_currenttrack, hw_currenthead, j)==NULL) printf("%.2u ", j);
          printf("\n");
        }
        else
        {}; ///   YES now 20190316-1 //break; // No retries in RAW mode

      if (capturetype!=DISKRAW)
      {
        // Check if catalogue has been done
        if ((info==0) && (catalogue==1))
        {
          if (dfs_validcatalogue(hw_currenthead))
          {
            printf("\nDetected DFS, side : %d\n", hw_currenthead);
            dfs_showinfo(hw_currenthead);
            info++;
            printf("\n");
          }
          else
          if ((i==0) && (side==0))
          {
            int adfs_format;

            adfs_format=adfs_validate();

            if (adfs_format!=ADFS_UNKNOWN)
            {
              printf("\nDetected ADFS-");
              switch (adfs_format)
              {
                case ADFS_S:
                  printf("S");
                  break;

                case ADFS_M:
                  printf("M");
                  break;

                case ADFS_L:
                  printf("L");
                  break;

                case ADFS_D:
                  printf("D");
                  break;

                case ADFS_E:
                  printf("E");
                  break;

                case ADFS_F:
                  printf("F");
                  break;

                case ADFS_EX:
                  printf("E+");
                  break;

                case ADFS_FX:
                  printf("F+");
                  break;

                case ADFS_G:
                  printf("G");
                  break;

                default:
                  break;
              }
              printf("\n");
              adfs_showinfo(adfs_format, disktracks, debug);
              info++;
              printf("\n");
            }
            else
            {
              if (dos_validate()!=DOS_UNKNOWN)
              {
                printf("\nDetected DOS\n\n");
                dos_showinfo(disktracks, debug);
                info++;
              }
              else
                printf("\nUnknown disk format\n\n");
            }
          }
        }

        if (retry>=retries)
          printf("I/O error reading head %d track %u *****************\n", hw_currenthead, i);
      }
      else
      {
//printf("writing data \n");
        // Write the raw sample data if required
        if (rawdata!=NULL)
        {
//printf("writing rawdata \n");
          if (outputtype==IMAGERAW) {
              rfi_writetrack(rawdata, i, side, hw_measurerpm(), "rle", samplebuffer, samplebuffsize);
	      if (interface_track_is_good()) break;  // the retry loop
          }

//printf("at 2 writing rawdata \n");
          if (outputtype==IMAGEDFI)
            dfi_writetrack(rawdata, i, side, samplebuffer, samplebuffsize);
        }
//printf("done writing rawdata \n");

      }

      } // new retry loop

    } // side loop

    // If we're only doing a catalogue, then don't read any more tracks
    if (capturetype==DISKCAT)
      break;

    // If this is an 80 track disk in a 40 track drive, then don't go any further
    if ((drivetracks==40) && (disktracks==80))
   break;

    //if (i==0) { dos_showinfo(0,debug); fflush(stdout); }
    if (i==0) { dos_showinfo_display(0,debug); fflush(stdout); display_flush();}

  } // track loop

  // Return the disk head to track 0 following disk imaging
  hw_seektotrackzero(); showhead(hw_currenttrack,-1);

  printf("Finished\n");

  // Stop the drive motor
  hw_stopmotor();

  // Determine how many tracks we actually had data on
  if ((disktracks==80) && (diskstore_maxtrack<79))
    disktracks=(diskstore_maxtrack+1);

  // Check if sectors have been requested to be sorted
  if (sortsectors)
    diskstore_sortsectors();

  // Write the data to disk image file (if required)
  if (diskimage!=NULL)
  {
    if (outputtype==IMAGEFSD)
    {
      char title[100];

      title[0]=0;

      // If they were found and they appear to be DFS catalogue then extract title
      if (dfs_validcatalogue(0))
      {
        dfs_gettitle(0, title, sizeof(title));
      }
      else
      {
        int adfs_format;

        adfs_format=adfs_validate();

        if (adfs_format!=ADFS_UNKNOWN)
          adfs_gettitle(adfs_format, title, sizeof(title));
      }

      // If no title or blank title, then use default
      if (title[0]==0)
        strcpy(title, "NO TITLE");

      fsd_write(diskimage, disktracks, title);
    }
    else
    if ((outputtype==IMAGEDSD) || (outputtype==IMAGESSD))
    {
      Disk_Sector *sec;
      unsigned char blanksector[DFS_SECTORSIZE];
      int imgside;

      // Prepare a blank sector when no sector is found in store
      bzero(blanksector, sizeof(blanksector));

      for (i=0; (((int)i<hw_maxtracks) && (i<disktracks)); i++)
      {
        for (imgside=0; imgside<sides; imgside++)
        {
          for (j=0; j<DFS_SECTORSPERTRACK; j++)
          {
            // Write
            sec=diskstore_findhybridsector(i, imgside, j);

            if ((sec!=NULL) && (sec->data!=NULL))
            {
              fwrite(sec->data, 1, DFS_SECTORSIZE, diskimage);
            }
            else
            {
		    // try this - if it is missing - label it MISSING
              fwrite(blanksector, 1, DFS_SECTORSIZE, diskimage);
              missingsectors++;
            }
          }
        }
      }
    }
    else
    if (outputtype==IMAGEIMG)
    {
      Disk_Sector *sec;
      unsigned char blanksector[16384];
      int sectorsize;
      int imgside;

      // Prepare a blank sector when no sector is found in store
      bzero(blanksector, sizeof(blanksector));
      if (1) {
        // mark it with missing and EE
        strcpy((char *)&blanksector[16],"START MISSING SECTOR (offset 16)");
        for (int i=128; i<512-128; ++i) blanksector[i]=0xEE;
        strcpy((char *)&blanksector[512-64],"END MISSING SECTOR (offset 64)");
      }

      if ((diskstore_minsectorid!=-1) && (diskstore_maxsectorid!=-1))
      {
        if ((diskstore_minsectorsize!=-1) && (diskstore_maxsectorsize!=-1) && (diskstore_minsectorsize==diskstore_maxsectorsize))
          sectorsize=diskstore_minsectorsize;

        for (i=0; (((int)i<hw_maxtracks) && (i<disktracks)); i++)
        {
          for (imgside=0; imgside<sides; imgside++)
          {
            // Write sectors for this side
            for (j=(unsigned int)diskstore_minsectorid; j<=(unsigned int)diskstore_maxsectorid; j++)
            {
              sec=diskstore_findhybridsector(i, imgside, j);

              if ((sec!=NULL) && (sec->data!=NULL))
              {
                fwrite(sec->data, 1, sec->datasize, diskimage);
              }
              else
              {
                fwrite(blanksector, 1, sectorsize, diskimage);
                missingsectors++;
		fprintf(stderr,"  missing track %d sector %d head %d\n",i,j,imgside);
              }
            }
          }
        }
      }
      else
        printf("No sectors found to save\n");
    }
    else
      printf("Unknown output format\n");
  }

  // Close disk image files (if open)
  if (diskimage!=NULL) fclose(diskimage);
  if (rawdata!=NULL) fclose(rawdata);

  // Free memory allocated to SPI buffer
  if (samplebuffer!=NULL)
  {
    free(samplebuffer);
    samplebuffer=NULL;
  }

  // Free any memory allocated to flippy buffer
  if (flippybuffer!=NULL)
  {
    free(flippybuffer);
    flippybuffer=NULL;
  }

  // Dump a list of valid sectors
  if ((debug) || (summary))
  {
    unsigned int fmsectors=diskstore_countsectormod(MODFM);
    unsigned int mfmsectors=diskstore_countsectormod(MODMFM);

    diskstore_dumpsectorlist();

    printf("\nSummary: \n");

    if ((diskstore_mintrack!=AUTODETECT) && (diskstore_maxtrack!=AUTODETECT))
      printf("Disk tracks with data range from %d to %d\n", diskstore_mintrack, diskstore_maxtrack);

    printf("Drive tracks %u\n", drivetracks);

    if (sides==1)
      printf("Single sided capture\n");
    else
      printf("Double sided capture\n");

    printf("FM sectors found %u\n", fmsectors);
    printf("MFM sectors found %u\n", mfmsectors);

    printf("Detected density : ");
    if ((mod_density&MOD_DENSITYFMSD)!=0) printf("SD ");
    if ((mod_density&MOD_DENSITYMFMDD)!=0) printf("DD ");
    if ((mod_density&MOD_DENSITYMFMHD)!=0) printf("HD ");
    if ((mod_density&MOD_DENSITYMFMED)!=0) printf("ED ");
    if (mod_density==MOD_DENSITYAUTO) printf("Unknown density ");
    printf("\n");

    if ((diskstore_minsectorsize!=-1) && (diskstore_maxsectorsize!=-1))
      printf("Sector sizes range from %d to %d bytes\n", diskstore_minsectorsize, diskstore_maxsectorsize);

    if ((diskstore_minsectorid!=-1) && (diskstore_maxsectorid!=-1))
      printf("Sector ids range from %d to %d\n", diskstore_minsectorid, diskstore_maxsectorid);

    if ((diskstore_minsectorsize!=-1) && (diskstore_maxsectorsize!=-1) && (diskstore_minsectorid!=-1) && (diskstore_maxsectorid!=-1) && (diskstore_minsectorsize==diskstore_maxsectorsize))
    {
      long totalstorage;

      totalstorage=diskstore_maxsectorsize*((diskstore_maxsectorid-diskstore_minsectorid)+1);
      totalstorage*=disktracks;
      if (sides==2)
        totalstorage*=2;

      printf("Total storage is %ld bytes\n", totalstorage);
    }
  }

  if (missingsectors>0)
    printf("Missing %d sectors\n", missingsectors);

  display_finalize();
  return 0;
}


