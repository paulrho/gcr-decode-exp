#include <stdio.h>

#include "crc.h"
#include "hardware.h"
#include "diskstore.h"
#include "mod.h"
#include "mfm.h"

int mfm_state=MFM_SYNC; // state machine
unsigned int mfm_datacells=0; // 16 bit sliding buffer
int mfm_bits=0; // Number of used bits within sliding buffer
unsigned int mfm_p1, mfm_p2, mfm_p3=0; // bit history

// Most recent address mark
unsigned long mfm_idpos, mfm_blockpos;
int mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength; // IDAM values
int mfm_lasttrack, mfm_lasthead, mfm_lastsector, mfm_lastlength;
unsigned char mfm_blocktype;
unsigned int mfm_blocksize;
unsigned int mfm_idblockcrc, mfm_datablockcrc, mfm_bitstreamcrc;

// Output block data buffer, for a single sector
unsigned char mfm_bitstream[MFM_BLOCKSIZE];
unsigned int mfm_bitlen=0;

unsigned long pgs_save_when_addr=0;
unsigned long pgs_bitcounter=0; // perhaps overhead

// MFM timings
float mfm_defaultwindow;
float mfm_bucket01, mfm_bucket001, mfm_bucket0001;

int mfm_debug=0;

// Add a bit to the 16-bit accumulator, when full - attempt to process (clock + data)
void mfm_addbit(const unsigned char bit, const unsigned long datapos)
{
  unsigned char clock, data;
  unsigned char dataCRC; // EDC

  // Maintain previous 48 bits of data
  mfm_p1=((mfm_p1<<1)|((mfm_p2&0x8000)>>15))&0xffff;
  mfm_p2=((mfm_p2<<1)|((mfm_p3&0x8000)>>15))&0xffff;
  mfm_p3=((mfm_p3<<1)|((mfm_datacells&0x8000)>>15))&0xffff;

  mfm_datacells=((mfm_datacells<<1)&0xffff);
  mfm_datacells|=bit;
  mfm_bits++;
  pgs_bitcounter++;

if (0) fprintf(stderr,"%s",bit?"1":"-");

  if (mfm_bits>=16)
  {
    // Extract clock byte
    clock=mod_getclock(mfm_datacells);

    // Extract data byte
    data=mod_getdata(mfm_datacells);

    switch (mfm_state)
    {
      case MFM_SYNC:
        if (mfm_datacells==0x5224)
        {
          if (mfm_debug)
            fprintf(stderr, "[%lx] ==MFM IAM SYNC 5224==\n", datapos);

          mfm_bits=16; // Keep looking for sync (preventing overflow)
        }
        else
        if (mfm_datacells==0x4489)
        {
          if (mfm_debug)
            fprintf(stderr, "[%lx] ==MFM IDAM/DAM SYNC 4489==\n", datapos);

          mfm_bits=0;
          mfm_bitlen=0; // Clear output buffer

          mfm_state=MFM_MARK; // Move on to look for MFM address mark
        }
        else
          mfm_bits=16; // Keep looking for sync (preventing overflow)
        break;

      case MFM_MARK:
        switch (data)
        {
          case MFM_BLOCKADDR: // fe - IDAM
          case MFM_ALTBLOCKADDR: // ff - Alternative IDAM
          case M2FM_BLOCKADDR: // 0e - Intel M2FM IDAM
          case M2FM_HPBLOCKADDR: // 70 - HP M2FM IDAM
            if (mfm_debug)
              fprintf(stderr, "[%lx] ID Address Mark %.2x\n", datapos, data);

            mfm_bits=0;
            mfm_blocktype=data;

            mfm_bitlen=0;
            mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p1);
            mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p2);
            mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p3);
            mfm_bitstream[mfm_bitlen++]=data;

            mfm_blocksize=3+1+4+2;

            // Clear IDAM cache incase previous was good and this one is bad
            mfm_idamtrack=-1;
            mfm_idamhead=-1;
            mfm_idamsector=-1;
            mfm_idamlength=-1;

            mfm_state=MFM_ADDR;
	    pgs_save_when_addr=pgs_bitcounter; // note - we shouldn't wait too long before invalidating the address
            break;

          case MFM_BLOCKDATA: // fb - DAM
          case MFM_ALTBLOCKDATA: // fa - Alternative DAM
          case MFM_RX02BLOCKDATA: // fd - RX02 M2FM DAM
          case M2FM_BLOCKDATA: // 0b - Intel M2FM DAM
          case M2FM_HPBLOCKDATA: // 50 - HP M2FM DAM

	    if (mfm_debug) fprintf(stderr,"bits since address %lu\n",pgs_bitcounter-pgs_save_when_addr); // note - we shouldn't wait too long before invalidating the address
	    if (pgs_bitcounter-pgs_save_when_addr>800) { /* this needs to be calculated */
	      fprintf(stderr,"took too long to get data header - invalidating (ignoring)\n");
              mfm_blocktype=MFM_BLOCKNULL;
              mfm_bitlen=0;
              mfm_state=MFM_SYNC;
	      break; // really need to get to code below rather than repeat
	    }

            if (mfm_debug)
              fprintf(stderr, "[%lx] Data Address Mark %.2x\n", datapos, data);

            // Don't process if don't have a valid preceding IDAM
            if ((mfm_idamtrack!=-1) && (mfm_idamhead!=-1) && (mfm_idamsector!=-1) && (mfm_idamlength!=-1))
            {
              mfm_bits=0;
              mfm_blocktype=data;

              mfm_bitlen=0;
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p1);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p2);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p3);
              mfm_bitstream[mfm_bitlen++]=data;

              mfm_state=MFM_DATA;
            }
            else
            {
              mfm_blocktype=MFM_BLOCKNULL;
              mfm_bitlen=0;
              mfm_state=MFM_SYNC;
            }
            break;

          case MFM_BLOCKDELDATA: // f8 - DDAM
          case MFM_ALTBLOCKDELDATA: // f9 - Alternative DDAM
          case M2FM_BLOCKDELDATA: // 08 - Intel M2FM DDAM
            if (mfm_debug)
              fprintf(stderr, "[%lx] Deleted Data Address Mark %.2x\n", datapos, data);

            // Don't process if don't have a valid preceding IDAM
            if ((mfm_idamtrack!=-1) && (mfm_idamhead!=-1) && (mfm_idamsector!=-1) && (mfm_idamlength!=-1))
            {
              mfm_bits=0;
              mfm_blocktype=data;

              mfm_bitlen=0;
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p1);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p2);
              mfm_bitstream[mfm_bitlen++]=mod_getdata(mfm_p3);
              mfm_bitstream[mfm_bitlen++]=data;

              mfm_state=MFM_DATA;
            }
            else
            {
              mfm_blocktype=MFM_BLOCKNULL;
              mfm_bitlen=0;
              mfm_state=MFM_SYNC;
            }
            break;

          default:
            break;
        }
        mfm_bits=0;
        break;

      case MFM_ADDR:
        if (mfm_bitlen<mfm_blocksize)
        {
          mfm_bitstream[mfm_bitlen++]=data;
          mfm_bits=0;
        }
        else
        {
          mfm_idblockcrc=calc_crc(&mfm_bitstream[0], mfm_bitlen-2);
          mfm_bitstreamcrc=(((unsigned int)mfm_bitstream[mfm_bitlen-2]<<8)|mfm_bitstream[mfm_bitlen-1]);
          dataCRC=(mfm_idblockcrc==mfm_bitstreamcrc)?GOODDATA:BADDATA;

          if (mfm_debug)
          {
            fprintf(stderr, "Track %.02d ", mfm_bitstream[4]);
            fprintf(stderr, "Head %d ", mfm_bitstream[5]);
            fprintf(stderr, "Sector %.02d ", mfm_bitstream[6]);
            fprintf(stderr, "Data size %d ", mfm_bitstream[7]);
            fprintf(stderr, "CRC %.2x%.2x ", mfm_bitstream[mfm_bitlen-2], mfm_bitstream[mfm_bitlen-1]);

            if (dataCRC==GOODDATA)
              fprintf(stderr, "OK\n");
            else
              fprintf(stderr, "BAD (%.4x)\n", mfm_idblockcrc);
          }

          if (dataCRC==GOODDATA)
          {
            // Record IDAM values
            mfm_idamtrack=mfm_bitstream[4];
            mfm_idamhead=mfm_bitstream[5];
            mfm_idamsector=mfm_bitstream[6];
            mfm_idamlength=mfm_bitstream[7];

            // Record last known good IDAM values for this track
            mfm_lasttrack=mfm_idamtrack;
            mfm_lasthead=mfm_idamhead;
            mfm_lastsector=mfm_idamsector;
            mfm_lastlength=mfm_idamlength;

            // Sanitise data block length
            switch(mfm_idamlength)
            {
              case 0x00: // 128
              case 0x01: // 256
              case 0x02: // 512
              case 0x03: // 1024
              case 0x04: // 2048
              case 0x05: // 4096
              case 0x06: // 8192
              case 0x07: // 16384
                mfm_blocksize=3+1+(128<<mfm_idamlength)+2;
                break;

              default:
                if (mfm_debug)
                  fprintf(stderr, "Invalid record length %.2x\n", mfm_idamlength);

                mfm_state=MFM_SYNC;
                break;
            }
          }

          mfm_state=MFM_SYNC;
        }
        break;

      case MFM_DATA:
        if (mfm_bitlen<mfm_blocksize)
        {
          mfm_bitstream[mfm_bitlen++]=data;
          mfm_bits=0;
        }
        else
        {
          mfm_datablockcrc=calc_crc(&mfm_bitstream[0], mfm_bitlen-2);
          mfm_bitstreamcrc=(((unsigned int)mfm_bitstream[mfm_bitlen-2]<<8)|mfm_bitstream[mfm_bitlen-1]);
          dataCRC=(mfm_datablockcrc==mfm_bitstreamcrc)?GOODDATA:BADDATA;

  if (000000) fprintf(stderr,"NOTE ONLY MFM block data size=%d Block header = %02X%02X%02X%02X  CRC=%02X%02X %4s dump=%d\n",
  mfm_bitlen,
  mfm_bitstream[0],mfm_bitstream[1],mfm_bitstream[2],mfm_bitstream[3],
  mfm_bitstream[mfm_bitlen-2],mfm_bitstream[mfm_bitlen-1],
  (dataCRC==GOODDATA)?"GOOD":"BAD",
  mfm_bitlen-2-4);
if (0) {
  int i;
  FILE * fd;
  fprintf(stderr,"MFM block data size=%d Block header = %02X%02X%02X%02X  CRC=%02X%02X %4s dump=%d\n",
  mfm_bitlen,
  mfm_bitstream[0],mfm_bitstream[1],mfm_bitstream[2],mfm_bitstream[3],
  mfm_bitstream[mfm_bitlen-2],mfm_bitstream[mfm_bitlen-1],
  (dataCRC==GOODDATA)?"GOOD":"BAD",
  mfm_bitlen-2-4);

  fd=fdopen(3,"a");
  for (i=4; i<mfm_bitlen-2; ++i) {
    //fprintf(stdout,"%2x ",mfm_bitstream[i]);
    fprintf(fd,"%c",mfm_bitstream[i]); /* &127); */
  }
  //fprintf(stdout,"\n");
}
          if (mfm_debug)
          {
            fprintf(stderr, "DATA block %.2x ", mfm_blocktype);
            fprintf(stderr, "CRC %.2x%.2x ", mfm_bitstream[mfm_bitlen-2], mfm_bitstream[mfm_bitlen-1]); // fix

            if (dataCRC==GOODDATA)
              fprintf(stderr, "OK\n");
            else
              fprintf(stderr, "BAD (%.4x)\n", mfm_datablockcrc);
          }

          if (dataCRC==GOODDATA) // || 1) // ignore for now
          {
            if (diskstore_addsector(MODMFM, hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idpos, mfm_idblockcrc, mfm_blockpos, mfm_blocktype, mfm_blocksize-3-1-2, &mfm_bitstream[4], mfm_datablockcrc)==1)
            {
              if (mfm_debug)
                fprintf(stderr, "** MFM new sector T%d H%d - C%d H%d R%d N%d - IDCRC %.4x DATACRC %.4x **\n", hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idblockcrc, mfm_datablockcrc);
                            if (1) {
                              fprintf(stderr,"C%dH%dR%d-%4s ", mfm_idamtrack, mfm_idamhead, mfm_idamsector, (dataCRC==GOODDATA)?"GOOD":"BAD");
                            }
  updatesector(mfm_idamtrack,mfm_idamhead,mfm_idamsector,(dataCRC==GOODDATA)?1:3);
  if (mfm_idamtrack==0 && mfm_idamhead==0 && mfm_idamsector==1) {
	  // show a sig - just the datacrc
    display_info_sig( mfm_bitstream[mfm_bitlen-2]<<8 | mfm_bitstream[mfm_bitlen-1]);
  }
                                                    if (0) {
                                                      int i;
                                                      FILE * fd;
                                                      fprintf(stderr,"MFM block data size=%d Block header = %02X%02X%02X%02X  CRC=%02X%02X %4s dump=%d ",
                                                      mfm_bitlen,
                                                      mfm_bitstream[0],mfm_bitstream[1],mfm_bitstream[2],mfm_bitstream[3],
                                                      mfm_bitstream[mfm_bitlen-2],mfm_bitstream[mfm_bitlen-1],
                                                      (dataCRC==GOODDATA)?"GOOD":"BAD",
                                                      mfm_bitlen-2-4);
                                                                    fprintf(stderr, "T%d H%d - C%d H%d R%d N%d - IDCRC %.4x DATACRC %.4x \n", hw_currenttrack, hw_currenthead, mfm_idamtrack, mfm_idamhead, mfm_idamsector, mfm_idamlength, mfm_idblockcrc, mfm_datablockcrc);
                                                    
                                                      fd=fdopen(3,"a");
                                                      for (i=4; i<mfm_bitlen-2; ++i) {
                                                        //fprintf(stdout,"%2x ",mfm_bitstream[i]);
                                                        fprintf(fd,"%c",mfm_bitstream[i]); /* &127); */
                                                      }
                                                      //fprintf(stdout,"\n");
                                                    }
            }
          } else { // bad
  		if (diskstore_findlogicalsector(mfm_idamtrack, mfm_idamhead, mfm_idamsector)!=NULL) {
		} else { // dont have it
                  updatesector(mfm_idamtrack,mfm_idamhead,mfm_idamsector,(dataCRC==GOODDATA)?1:3);
		}

	  }

// should have been here
// this shouldnt now be required because of the "too long" detection
            // Clear IDAM cache incase we get just another datablock with no header (that doesn't belong to us) //PGS
            mfm_idamtrack=-1;
            mfm_idamhead=-1;
            mfm_idamsector=-1;
            mfm_idamlength=-1;

          mfm_state=MFM_SYNC;
        }
        break;

      default:
        // Unknown state, put it back to SYNC
        mfm_p1=0;
        mfm_p2=0;
        mfm_p3=0;
        mfm_bits=0;

        mfm_state=MFM_SYNC;
        break;
    }
  }
}

void mfm_addsample(const unsigned long samples, const unsigned long datapos)
{
  /////debug//fprintf(stderr,"ST%dH%d %d\n",hw_currenttrack,hw_currenthead,samples);
  // Does number of samples fit within "01" bucket ..
  if (samples<=mfm_bucket01)
  {
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "001" bucket ..
  if (samples<=mfm_bucket001)
  {
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
  else // .. does number of samples fit within "0001" bucket ..
  if (samples<=mfm_bucket0001)
  {
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
  else
  {
    // TODO This shouldn't happen in MFM encoding
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(0, datapos);
    mfm_addbit(1, datapos);
  }
}

void mfm_init(const int debug, const char density)
{
  char bitcell=MFM_BITCELLDD;
  float diff;

  mfm_debug=debug;

  if ((density&MOD_DENSITYMFMED)!=0)
    bitcell=MFM_BITCELLED;

  if ((density&MOD_DENSITYMFMHD)!=0)
    bitcell=MFM_BITCELLHD;

  // Determine number of samples between "1" pulses (default window)
  mfm_defaultwindow=((float)hw_samplerate/(float)USINSECOND)*(float)bitcell;

  // From default window, determine ideal sample times for assigning bits "01", "001" or "0001"
  mfm_bucket01=mfm_defaultwindow;
  mfm_bucket001=(mfm_defaultwindow/2)*3;
  mfm_bucket0001=(mfm_defaultwindow/2)*4;

  // Increase bucket sizes to halfway between peaks
  diff=mfm_bucket001-mfm_bucket01;
  mfm_bucket01+=(diff/2);
  mfm_bucket001+=(diff/2);
  mfm_bucket0001+=(diff/2);

  // Set up MFM parser
  mfm_state=MFM_SYNC;
  mfm_datacells=0;
  mfm_bits=0;

  mfm_idpos=0;
  mfm_blockpos=0;

  mfm_blocktype=MFM_BLOCKNULL;
  mfm_blocksize=0;

  mfm_idblockcrc=0;
  mfm_datablockcrc=0;
  mfm_bitstreamcrc=0;

  mfm_bitlen=0;

  // Initialise previous data cache
  mfm_p1=0;
  mfm_p2=0;
  mfm_p3=0;

  // Initialise last found sector IDAM to invalid
  mfm_idamtrack=-1;
  mfm_idamhead=-1;
  mfm_idamsector=-1;
  mfm_idamlength=-1;

  // Initialise last known good sector IDAM to invalid
  mfm_lasttrack=-1;
  mfm_lasthead=-1;
  mfm_lastsector=-1;
  mfm_lastlength=-1;
}
