#include "tad.h"
#include "storage.h"
#include <nds/ndstypes.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <dirent.h>
/*
    The common keys for decrypting TADs.

    DEV: Used in most TADs. Anything created with the standard maketad will be dev.
    PROD: Used in some TADs for factory tools like PRE_IMPORT and IMPORT. Really uncommon. I only know of 24 prod TADs to have ever been found, and 19 of those haven't ever been released (pleeeeeaaaaase release IMPORT soon). All retail signed and can't be created with any leaked maketads.
    DEBUGGER: Used in TwlSystemUpdater TADs. Created with maketad_updater.
    
    If for whatever reason you want to make TADs, see here:
    https://randommeaninglesscharacters.com/dsidev/man/maketad.html
*/
unsigned char devKey[] = {0xA1, 0x60, 0x4A, 0x6A, 0x71, 0x23, 0xB5, 0x29, 0xAE, 0x8B, 0xEC, 0x32, 0xC8, 0x16, 0xFC, 0xAA};
unsigned char prodKey[] = {0xAF, 0x1B, 0xF5, 0x16, 0xA8, 0x07, 0xD2, 0x1A, 0xEA, 0x45, 0x98, 0x4F, 0x04, 0x74, 0x28, 0x61};
unsigned char debuggerKey[] = {0xA2, 0xFD, 0xDD, 0xF2 ,0xE4, 0x23, 0x57, 0x4A, 0xE7, 0xED, 0x86, 0x57, 0xB5, 0xAB, 0x19, 0xD3};
typedef struct {
    uint32_t hdrSize;
    uint16_t tadType;
    uint16_t tadVersion;
    uint32_t certSize;
    uint32_t crlSize;
    uint32_t ticketSize;
    uint32_t tmdSize;
    uint32_t srlSize;
    uint32_t metaSize;
} Header;
typedef struct {
    uint32_t hdrOffset;
    uint32_t certOffset;
    uint32_t crlOffset;
    uint32_t ticketOffset;
    uint32_t tmdOffset;
    uint32_t srlOffset;
    uint32_t metaOffset;
} Tad;
bool tadSuccess;

uint32_t swap_endian_u32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

uint16_t swap_endian_u16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

uint32_t round_up( const u32 v, const u32 align )
{
    u32 r = ((v + align - 1) / align) * align;
    return r;
}

int decryptTad(char const* src)
{
	if (!src) return 1;

    FILE *file = fopen(src, "rb");
    if (file == NULL) {
        printf("ERROR: fopen()");
        return 1;
    }

    mkdir("sd:/tadtests", 0777);

    // A lot of this is heavily """inspired""" by remaketad.pl in TwlIPL (/tools/bin/remaketad.pl)
    // This is made to decrypt + unpack a dev TAD and and rebuild for SystemUpdaters.
    //
    // https://github.com/rvtr/TwlIPL/blob/trunk/tools/bin/remaketad.pl

    Header header;
    fread(&header, sizeof(Header), 1, file);
    iprintf("Parsing TAD header...\n");
    Tad tad;
    tad.hdrOffset = 0;
    tad.certOffset = round_up(swap_endian_u32(header.hdrSize), 64);
    tad.crlOffset = round_up(tad.certOffset + swap_endian_u32(header.certSize), 64);
    tad.ticketOffset = round_up(tad.crlOffset + swap_endian_u32(header.crlSize), 64);
    tad.tmdOffset = round_up(tad.ticketOffset + swap_endian_u32(header.ticketSize), 64);
    tad.srlOffset = round_up(tad.tmdOffset + swap_endian_u32(header.tmdSize), 64);
    tad.metaOffset = round_up(tad.srlOffset + swap_endian_u32(header.srlSize), 64);
    // TODO: Make sure offset calculation and alignment is correct by comparing that to total size
    iprintf("  hdrSize:      %lu\n", swap_endian_u32(header.hdrSize));
    iprintf("  hdrOffset:    %lu\n", tad.hdrOffset);
    // 18803 = "Is". This is the standard TAD type.
    if (swap_endian_u16(header.tadType) == 18803) {
        iprintf("  tadType:      'Is'\n");
    } else {
        iprintf("  tadType:      UNKNOWN\nERROR: unexpected TAD type\n");
        return 1;
    }
    iprintf("  tadVersion:   %u\n", swap_endian_u16(header.tadVersion));
    iprintf("  certSize:     %lu\n", swap_endian_u32(header.certSize));
    iprintf("  certOffset:   %lu\n", tad.certOffset);
    iprintf("  crlSize:      %lu\n", swap_endian_u32(header.crlSize));
    iprintf("  crlOffset:    %lu\n", tad.crlOffset);
    iprintf("  ticketSize:   %lu\n", swap_endian_u32(header.ticketSize));
    iprintf("  ticketOffset: %lu\n", tad.ticketOffset);
    iprintf("  tmdSize:      %lu\n", swap_endian_u32(header.tmdSize));
    iprintf("  tmdOffset:    %lu\n", tad.tmdOffset);
    iprintf("  srlSize:      %lu\n", swap_endian_u32(header.srlSize));
    iprintf("  srlOffset:    %lu\n", tad.srlOffset);
    iprintf("  metaSize:     %lu\n", swap_endian_u32(header.metaSize));
    iprintf("  metaOffset:   %lu\n", tad.metaOffset);
    /*
    Please excuse my terrible copy paste coding. I do not know C and I'm translating from other languages
    that I don't know (python, perl). 

    Anyways, the code above is determining the file offsets and sizes within the TAD.
    This is done using the 32 byte header.

    Example header from "KART_K04.tad"

    00000020 49730000 00000E80 00000000
    000002A4 00000208 000DFC00 00000000

    Breaking it down...

     Hex       | Dec    | Meaning
    -----------+--------+------------
    0x00000020 | 32     | Header size
    0x4973     | Is     | TAD type
    0x0000     | 0      | TAD version
    0x00000E80 | 3712   | Cert size
    0x00000000 | 0      | Crl size
    0x000002A4 | 676    | Ticket size
    0x00000208 | 520    | TMD size
    0x000DFC00 | 916480 | SRL size
    0x00000000 | 0      | Meta size

    Gee, looks awfully like a WAD header, doesn't it? Turns out TADs are just renamed WADs. Not even changed one bit.
    There's literally a commit replacing every instance of WAD with TAD in TwlIPL...
    https://github.com/rvtr/TwlIPL/commit/baca65d35d5d62d815c88e6374b895d5b0755277
    */

    iprintf("================================\nCopying output files...\n");
    // ChatGPT ahh code
    // Also more copy pasting because I am a silly idiot!
    iprintf("  Copying TMD...\n"); 
    FILE *tmdFile = fopen("sd:/tadtests/tmd.bin", "wb");
    if (tmdFile == NULL) {
        iprintf("ERROR: fopen()\n");
    }
    fseek(file, tad.tmdOffset, SEEK_SET);

    for (int i = 0; i < swap_endian_u32(header.tmdSize); i++) {
        char ch = fgetc(file);
        fputc(ch, tmdFile);
    }
    fclose(tmdFile);

    iprintf("  Copying ticket...\n");
    FILE *ticketFile = fopen("sd:/tadtests/ticket.bin", "wb");
    if (ticketFile == NULL) {
        iprintf("ERROR: fopen()\n");
    }
    fseek(file, tad.ticketOffset, SEEK_SET);

    for (int i = 0; i < swap_endian_u32(header.ticketSize); i++) {
        char ch = fgetc(file);
        fputc(ch, ticketFile);
    }
    fclose(ticketFile);

    iprintf("  Copying SRL...\n"); 
    FILE *srlFile = fopen("sd:/tadtests/srl_enc.bin", "wb");
    if (srlFile == NULL) {
        iprintf("ERROR: fopen()\n");
    }
    fseek(file, tad.srlOffset, SEEK_SET);

    for (int i = 0; i < swap_endian_u32(header.srlSize); i++) {
        char ch = fgetc(file);
        fputc(ch, srlFile);
    }
    fclose(srlFile);
    fclose(file);

    iprintf("================================\nDone!\n");

    /*
    Try to decrypt the SRL with each key until one works. I don't know a better way to do this (nothing in the TAD would
    specify the key needed) so we'll try keys in the order of which ones are more common:
        DEV --> DEBUGGER --> PROD

    We check for only zerobytes at 0x15-1B to see if the SRL is decrypted properly. That region should always be blank.
    
    https://problemkaputt.de/gbatek.htm#dscartridgeheader
    https://gist.github.com/rvtr/f1069530129b7a57967e3fc4b30866b4#file-decrypt_tad-py-L84
    */

	//return copyFilePart(src, 0, size, dst);
	return 0; 

}