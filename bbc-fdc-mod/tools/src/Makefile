# BBC-FDC tools

CC = gcc

HARDWARE = $(shell bash ./hw.sh Hardware)
REVISION = $(shell bash ./hw.sh Revision)

DEBUGFLAGS = -g -W -Wall
BUILDFLAGS = $(DEBUGFLAGS) -D$(HARDWARE) -D$(REVISION)

all: drivetest bbcfdc checkfsd

drivetest: drivetest.o hardware.o
	$(CC) $(BUILDFLAGS) -o drivetest drivetest.o hardware.o -lbcm2835

# additions
display1571.c: ;
interface1571.c: ;
display3p5.c: ;

# additions

display.o: display.c display.h display3p5.c display1571.c interface1571.c
	$(CC) $(BUILDFLAGS) -c -o display.o display.c

#special
display_pc.o: display.h display3p5.c display1571.c interface1571.c
	$(CC) $(BUILDFLAGS) -c -o display_pc.o display3p5.c

drivetest.o: drivetest.c hardware.h
	$(CC) $(BUILDFLAGS) -c -o drivetest.o drivetest.c

checkfsd: checkfsd.o
	$(CC) $(BUILDFLAGS) -o checkfsd checkfsd.o

checkfsd.o: checkfsd.c fsd.h
	$(CC) $(BUILDFLAGS) -c -o checkfsd.o checkfsd.c

bbcfdc: bbcfdc.o adfs.o crc.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o hardware.o jsmn.o mfm.o mod.o rfi.o display.o
	$(CC) $(BUILDFLAGS) -o bbcfdc adfs.o bbcfdc.o crc.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o hardware.o jsmn.o mfm.o mod.o rfi.o display.o -lbcm2835

bbcfdc.o: bbcfdc.c adfs.h dfi.h dfs.h diskstore.h dos.h fm.h fsd.h hardware.h jsmn.h mfm.h mod.h rfi.h display.h
	$(CC) $(BUILDFLAGS) -c -o bbcfdc.o bbcfdc.c

#special
bbcfdc_pc: bbcfdc_pc.o adfs.o crc.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o hardware.o jsmn.o mfm.o mod.o rfi.o display_pc.o
	$(CC) $(BUILDFLAGS) -o bbcfdc_pc adfs.o bbcfdc.o crc.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o hardware.o jsmn.o mfm.o mod.o rfi.o display_pc.o -lbcm2835

bbcfdc_pc.o: bbcfdc.c adfs.h dfi.h dfs.h diskstore.h dos.h fm.h fsd.h hardware.h jsmn.h mfm.h mod.h rfi.h display.h
	$(CC) $(BUILDFLAGS) -c -o bbcfdc_pc.o bbcfdc.c

##########################

bbcfdc-nopi: bbcfdc-nopi.o adfs.o crc.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o jsmn.o mfm.o mod.o nopi.o rfi.o display.o
	$(CC) $(BUILDFLAGS) -DNOPI -o bbcfdc-nopi bbcfdc-nopi.o adfs.o crc.o dfi.o dfs.o diskstore.o dos.o fm.o fsd.o jsmn.o mfm.o mod.o nopi.o rfi.o

bbcfdc-nopi.o: bbcfdc.c adfs.h dfi.h dfs.h diskstore.h dos.h fm.h fsd.h hardware.h jsmn.h mfm.h mod.h rfi.h
	$(CC) $(BUILDFLAGS) -DNOPI -c -o bbcfdc-nopi.o bbcfdc.c

nopi.o: nopi.c hardware.h jsmn.h rfi.h
	$(CC) $(BUILDFLAGS) -DNOPI -c -o nopi.o nopi.c

##########################

adfs.o: adfs.c adfs.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o adfs.o adfs.c

crc.o: crc.c crc.h
	$(CC) $(BUILDFLAGS) -c -o crc.o crc.c

dfi.o: dfi.c dfi.h
	$(CC) $(BUILDFLAGS) -c -o dfi.o dfi.c

dfs.o: dfs.c dfs.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o dfs.o dfs.c

dos.o: dos.c dos.h diskstore.h
	$(CC) $(BUILDFLAGS) -c -o dos.o dos.c

diskstore.o: diskstore.c diskstore.h hardware.h mod.h
	$(CC) $(BUILDFLAGS) -c -o diskstore.o diskstore.c

fm.o: fm.c crc.h diskstore.h dfs.h fm.h hardware.h mod.h
	$(CC) $(BUILDFLAGS) -c -o fm.o fm.c

mfm.o: mfm.c crc.h diskstore.h mfm.h hardware.h mod.h
	$(CC) $(BUILDFLAGS) -c -o mfm.o mfm.c

mod.o: mod.c fm.h mfm.h hardware.h
	$(CC) $(BUILDFLAGS) -c -o mod.o mod.c

fsd.o: fsd.c diskstore.h fsd.h
	$(CC) $(BUILDFLAGS) -c -o fsd.o fsd.c

jsmn.o: jsmn.c jsmn.h
	$(CC) $(BUILDFLAGS) -c -o jsmn.o jsmn.c

rfi.o: rfi.c hardware.h jsmn.h rfi.h
	$(CC) $(BUILDFLAGS) -c -o rfi.o rfi.c

hardware.o: hardware.c hardware.h pins.h
	$(CC) $(BUILDFLAGS) -c -o hardware.o hardware.c

clean:
	rm -f *.o
	rm -f drivetest
	rm -f bbcfdc
	rm -f checkfsd
	rm -f bbcfdc-nopi
