
#include "shared.h"

// 4-bit value	GCR code[29]
// hex	bin	bin	hex
// 0x0	0000	0.1010	0x0A
// 0x1	0001	0.1011	0x0B
// 0x2	0010	1.0010	0x12
// 0x3	0011	1.0011	0x13
// 0x4	0100	0.1110	0x0E
// 0x5	0101	0.1111	0x0F
// 0x6	0110	1.0110	0x16
// 0x7	0111	1.0111	0x17
// 0x8	1000	0.1001	0x09
// 0x9	1001	1.1001	0x19
// 0xA	1010	1.1010	0x1A
// 0xB	1011	1.1011	0x1B
// 0xC	1100	0.1101	0x0D
// 0xD	1101	1.1101	0x1D
// 0xE	1110	1.1110	0x1E
// 0xF	1111	1.0101	0x15
//
// btw, not valids
//                    0.0xxx  white or yellow
//                    x.xx00  white


// 4-bit value	GCR code[29]
// hex	bin	bin	hex
// 0x0	0000	0.1010	0x0A    a
// 0x1	0001	0.1011	0x0B    b
// 0x2	0010	1.0010	0x12    n  hi 2
// 0x3	0011	1.0011	0x13    m  hi 3
// 0x4	0100	0.1110	0x0E    e
// 0x5	0101	0.1111	0x0F    f
// 0x6	0110	1.0110	0x16    c  hi 6
// 0x7	0111	1.0111	0x17    k  hi 7
// 0x8	1000	0.1001	0x09    9
// 0x9	1001	1.1001	0x19    p  hi 9
// 0xA	1010	1.1010	0x1A    A  hi a
// 0xB	1011	1.1011	0x1B    B  hi b
// 0xC	1100	0.1101	0x0D    d
// 0xD	1101	1.1101	0x1D    D  hi d
// 0xE	1110	1.1110	0x1E    E  hi e
// 0xF	1111	1.0101	0x15    l  hi 5
//
int decode(long in)
{
   // give me 5 bits
   switch (in) {
     case 0x0A: return 0x00;
     case 0x0B: return 0x01;
     case 0x12: return 0x02;
     case 0x13: return 0x03;
     case 0x0E: return 0x04;
     case 0x0F: return 0x05;
     case 0x16: return 0x06;
     case 0x17: return 0x07;
     case 0x09: return 0x08;
     case 0x19: return 0x09;
     case 0x1A: return 0x0A;
     case 0x1B: return 0x0B;
     case 0x0D: return 0x0C;
     case 0x1D: return 0x0D;
     case 0x1E: return 0x0E;
     case 0x15: return 0x0F;

     default: dbgprintf(stderr," (NON-GCR-CODE %s%s%s%s%s)",
                        in&0x10 ? "1" : "0",
                        in&0x08 ? "1" : "0",
                        in&0x04 ? "1" : "0",
                        in&0x02 ? "1" : "0",
                        in&0x01 ? "1" : "0"
                        );
        return -1;
        //return 0x00;
   }
}
