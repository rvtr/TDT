/*
    A lot of this is heavily "inspired" by remaketad.pl in TwlIPL (/tools/bin/remaketad.pl)
    That script was made to decrypt + unpack a dev TAD and and rebuild for SystemUpdaters.
    
    https://github.com/rvtr/TwlIPL/blob/trunk/tools/bin/remaketad.pl
*/

#include "tad.h"
#include "storage.h"
#include "rom.h"
#include "main.h"
#include "nand/twltool/dsi.h"
#include <nds/ndstypes.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <dirent.h>

/*
    The common keys for decrypting TADs.

    DEV: Used for most TADs. Anything created with the standard maketad will be dev.
    PROD: Used for some TADs in factory tools like PRE_IMPORT and IMPORT. They can be created from the NUS or manually from NAND.
    DEBUGGER: Used for TwlSystemUpdater TADs. Created with maketad_updater, not really common to see.
    
    If for whatever reason you want to make TADs, see here:
    https://randommeaninglesscharacters.com/dsidev/man/maketad.html
*/
const unsigned char devKey[] = {
    0xA1, 0x60, 0x4A, 0x6A, 0x71, 0x23, 0xB5, 0x29,
    0xAE, 0x8B, 0xEC, 0x32, 0xC8, 0x16, 0xFC, 0xAA
};
const unsigned char prodKey[] = {
    0xAF, 0x1B, 0xF5, 0x16, 0xA8, 0x07, 0xD2, 0x1A,
    0xEA, 0x45, 0x98, 0x4F, 0x04, 0x74, 0x28, 0x61
};
const unsigned char debuggerKey[] = {
    0xA2, 0xFD, 0xDD, 0xF2 ,0xE4, 0x23, 0x57, 0x4A,
    0xE7, 0xED, 0x86, 0x57, 0xB5, 0xAB, 0x19, 0xD3
};
const unsigned char customKey[] = {
    0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00
};
// Content IV be fine as a hardcoded string. Content IV is based off of the content index. (index # with zerobyte padding) 
// All TADs I've seen only ever had a single content. It might be a good idea to add something down the line in case a
// weird TAD pops up, but until then this should do.
unsigned char content_iv[] = {
    0x00, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
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
unsigned char srlCompany[2];
unsigned char srlVerLow[1];
unsigned char srlVerHigh[1];
unsigned char contentHash[20];
unsigned char srlTidLow[4];
unsigned char srlTidHigh[4];
uint32_t srlTrueSize;
bool dataTitle;


uint32_t swap_endian_u32(uint32_t x) {
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

uint16_t swap_endian_u16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

uint32_t round_up( const u32 v, const u32 align ) {
    u32 r = ((v + align - 1) / align) * align;
    return r;
}

void decrypt_cbc(const unsigned char* key, const unsigned char* iv, const unsigned char* encryptedData, size_t dataSize, size_t keySize, unsigned char* decryptedData) {
    aes_context ctx;
    aes_setkey_dec(&ctx, key, 128);
    aes_crypt_cbc(&ctx, AES_DECRYPT, dataSize, iv, encryptedData, decryptedData);
}

char* openTad(char const* src) {
	if (!src) return "ERROR";

    FILE *file = fopen(src, "rb");
    if (file == NULL) {
        printf("ERROR: fopen()");
        return "ERROR";
    }

    // idk how to create folders recursively
    mkdir("sd:/_nds", 0777);
    mkdir("sd:/_nds/TADDeliveryTool", 0777);
    mkdir("sd:/_nds/TADDeliveryTool/tmp", 0777);

    /*
    The code below is determining the file offsets and sizes within the TAD.
    This is done using the 32 byte header.

    Example header from "KART_K04.tad"

    00000020 49730000 00000E80 00000000
    000002A4 00000208 000DFC00 00000000

    Breaking it down...

     Hex       | Dec    | Meaning
    -----------+--------+------------
    0x00000020 | 32     | Header size
    0x4973     | 18803  | TAD type (always "Is" in ASCII)
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

    Header header;
    fread(&header, sizeof(Header), 1, file);
    iprintf("Parsing TAD header...\n");
    Tad tad;
    tad.hdrOffset = 0;

    // 18803 = "Is". This is the standard TAD type.
    // Others exist, but they are for Wii boot2 (ib) and netcard (NULL)
    if (swap_endian_u16(header.tadType) == 18803) {
        //iprintf("  tadType:      'Is'\n");
    } else {
        iprintf("  tadType:      UNKNOWN\nERROR: unexpected TAD type\n");
        return "ERROR";
    }

    // All offsets in the TAD are aligned to 64 bytes.
    // TODO: Make sure offset calculation and alignment is correct by comparing that to total size
    tad.certOffset = round_up(swap_endian_u32(header.hdrSize), 64);
    tad.crlOffset = round_up(tad.certOffset + swap_endian_u32(header.certSize), 64);
    tad.ticketOffset = round_up(tad.crlOffset + swap_endian_u32(header.crlSize), 64);
    tad.tmdOffset = round_up(tad.ticketOffset + swap_endian_u32(header.ticketSize), 64);
    tad.srlOffset = round_up(tad.tmdOffset + swap_endian_u32(header.tmdSize), 64);
    tad.metaOffset = round_up(tad.srlOffset + swap_endian_u32(header.srlSize), 64);
    /*
    Okay sooo this is stupid. Content size defined in header != true content size
    
    sysmenuVersion has the header content size aligned to 64 bytes
    The TMD content size + hash is for an unpadded content
    
    As such I think that the TMD size should always be the default.
    */
    fseek(file, tad.tmdOffset+496, SEEK_SET);
    fread(&srlTrueSize, 1, 4, file);
    fread(contentHash, 1, 20, file);
	
    fseek(file, tad.tmdOffset+396, SEEK_SET);
    fread(srlTidHigh, 1, 4, file);
    fread(srlTidLow, 1, 4, file);

    fclose(file);

    /*
    Copy the contents of the TAD to the SD card.

    For installing we only need the TMD, ticket, and SRL.

    We can skip the cert since that already exists in NAND, and the TADs cert might not match the signing on the DSi.
    */

    iprintf("Copying output files...\n");

    iprintf("  Copying TMD...\n"); 
    copyFilePart(src, tad.tmdOffset, swap_endian_u32(header.tmdSize), "sd:/_nds/TADDeliveryTool/tmp/temp.tmd");

    iprintf("  Copying ticket...\n");
    copyFilePart(src, tad.ticketOffset, swap_endian_u32(header.ticketSize), "sd:/_nds/TADDeliveryTool/tmp/temp.tik");
    
    iprintf("  Copying SRL...\n"); 
    copyFilePart(src, tad.srlOffset, swap_endian_u32(srlTrueSize), "sd:/_nds/TADDeliveryTool/tmp/temp.srl.enc");

    /*
    Get the title key + IV from the ticket.
    */

    iprintf("Decrypting SRL...\n");
    //iprintf("  Finding title key...\n");
    FILE *ticket = fopen("sd:/_nds/TADDeliveryTool/tmp/temp.tik", "rb");
    unsigned char title_key_enc[16];
    fseek(ticket, 447, SEEK_SET);
    fread(title_key_enc, 1, 16, ticket);
    //iprintf("  Title key found!\n");

    //iprintf("  Finding title key IV...\n");
    unsigned char title_key_iv[16];
    fseek(ticket, 476, SEEK_SET);
    fread(title_key_iv, 1, 8, ticket);
    memset(title_key_iv + 8, 0, 8);
    //iprintf("  Title key IV found!\n");

    fclose(ticket);

    /*
    This is SRL decryption (AES-CBC).
    
        Common key + title key IV to decrypt title key
        Title key + content IV to decrypt content

    We have to try each possible common key until we find one that works. I don't know a better way to do this
    (nothing in the TAD would specify the key needed) so we'll try keys in the order of which ones are more common:

        DEV --> PROD --> DEBUGGER
    */

    if (srlTidHigh[3] == 0x0f) {
        dataTitle = TRUE;
    } else {
        dataTitle = FALSE;
    }

    bool keyFail;
    iprintf("Trying dev common key...\n");
    keyFail = decryptTad(devKey, title_key_iv, title_key_enc, content_iv, swap_endian_u32(srlTrueSize), srlTidLow, dataTitle, contentHash);

    if (keyFail == TRUE) {
        remove("sd:/_nds/TADDeliveryTool/tmp/temp.srl");
        iprintf("Key fail!\n\nTrying prod common key...\n");
        keyFail = decryptTad(prodKey, title_key_iv, title_key_enc, content_iv, swap_endian_u32(srlTrueSize), srlTidLow, dataTitle, contentHash);
    }
    if (keyFail == TRUE) {
        remove("sd:/_nds/TADDeliveryTool/tmp/temp.srl");
        iprintf("Key fail!\n\nTrying debugger common key...\n");
        keyFail = decryptTad(debuggerKey, title_key_iv, title_key_enc, content_iv, swap_endian_u32(srlTrueSize), srlTidLow, dataTitle, contentHash);
    }
    if (keyFail == TRUE) {
        remove("sd:/_nds/TADDeliveryTool/tmp/temp.srl");
        iprintf("Key fail!\n\nTrying custom key...\n");
        keyFail = decryptTad(customKey, title_key_iv, title_key_enc, content_iv, swap_endian_u32(srlTrueSize), srlTidLow, dataTitle, contentHash);
    }
    if (keyFail == TRUE) {
        remove("sd:/_nds/TADDeliveryTool/tmp/temp.srl");
        iprintf("All keys failed!\n");
        return "ERROR";
    }
    return "sd:/_nds/TADDeliveryTool/tmp/temp.srl";

}

bool decryptTad(unsigned char* commonKey,
                unsigned char* title_key_iv,
                unsigned char* title_key_enc,
                unsigned char* content_iv,
                int srlSize,
                unsigned char* srlTidLow,
                bool dataTitle,
                unsigned char* contentHash) {
    unsigned char title_key_dec[16];
    unsigned char title_key_iv_bak[16];
    unsigned char content_iv_bak[16];
    unsigned char srl_buffer_enc[16];
    unsigned char srl_buffer_dec[16];

    // Backup IVs because PolarSSL will overwrite it
    memcpy( title_key_iv_bak, title_key_iv, 16 );
    memcpy( content_iv_bak, content_iv, 16 );

    FILE *srlFile_enc = fopen("sd:/_nds/TADDeliveryTool/tmp/temp.srl.enc", "rb");
    fseek(srlFile_enc, 0, SEEK_SET);
    FILE *srlFile_dec = fopen("sd:/_nds/TADDeliveryTool/tmp/temp.srl", "wb");
    fseek(srlFile_dec, 0, SEEK_SET);

    iprintf("  Decrypting SRL in chunks..\n");
    decrypt_cbc(commonKey, title_key_iv, title_key_enc, 16, 16, title_key_dec);
    int i=0;
    bool keyFail = FALSE;

    /*
    Why have two methods of decrypting for data and normal titles?
    
    Normal titles can be massive (16mb)! Decrpyting and calculating SHA1 multiple times to test keys
    is painfully slow. We can quickly test the SRL header for the TID to make sure the key works.
    
    Data titles can't be tested the same way. Since they're just data, they don't have a header to read.
    Luckily they tend to be small (10-300kb) so completely decrypting and checking a SHA1 hash is fast.
    */

    if (dataTitle == TRUE) {
        // Copied SHA1 stuff from here.
        // https://github.com/DS-Homebrew/SafeNANDManager/blob/master/arm9/source/arm9.c#L96-L152
        swiSHA1context_t ctx;
        ctx.sha_block=0;
        u8 sha1[20]={0};
        swiSHA1Init(&ctx);

        while (i < srlSize) {
            fread(srl_buffer_enc, 1, 16, srlFile_enc);
            decrypt_cbc(title_key_dec, content_iv, srl_buffer_enc, 16, 16, srl_buffer_dec);
            fwrite(srl_buffer_dec, 1, 16, srlFile_dec);
            printProgressBar( ((float)i / (float)srlSize) );
            swiSHA1Update(&ctx, srl_buffer_dec, 16);
            i=i+16;

        }
        swiSHA1Final(sha1, &ctx);

        // Compare SHA1 hash of file to TMD
        for (int i = 0; i < 20; i++) {
            if (contentHash[i] != sha1[i]) {
                keyFail = TRUE;
            }
        }
        iprintf("\n");

    } else {
        while (i < srlSize && keyFail == FALSE) {
            fread(srl_buffer_enc, 1, 16, srlFile_enc);
            decrypt_cbc(title_key_dec, content_iv, srl_buffer_enc, 16, 16, srl_buffer_dec);
            fwrite(srl_buffer_dec, 1, 16, srlFile_dec);
            printProgressBar( ((float)i / (float)srlSize) );
    	   // Executable SRLs will always have a reverse order TID low at 0x230. 
    	   // Use this to check if the current common key works.
            if (i == 560) {
                if (srl_buffer_dec[3] != srlTidLow[0] ||
                    srl_buffer_dec[2] != srlTidLow[1] ||
                    srl_buffer_dec[1] != srlTidLow[2] ||
                    srl_buffer_dec[0] != srlTidLow[3] ) {
                    keyFail = TRUE;
                }
            }
            i=i+16;
        }
    }
    fclose(srlFile_dec);
    fclose(srlFile_enc);
    // Restore IVs
    memcpy( title_key_iv, title_key_iv_bak, 16 );
    memcpy( content_iv, content_iv_bak, 16 );
    return keyFail;
}

void printTadInfo(char const* fpath)
{
    clearScreen(&topScreen);
    if (!fpath) return;

    FILE *file = fopen(fpath, "rb");
    Header header;
    fread(&header, sizeof(Header), 1, file);
    Tad tad;
    tad.hdrOffset = 0;
    // All offsets in the TAD are aligned to 64 bytes.
    tad.certOffset = round_up(swap_endian_u32(header.hdrSize), 64);
    tad.crlOffset = round_up(tad.certOffset + swap_endian_u32(header.certSize), 64);
    tad.ticketOffset = round_up(tad.crlOffset + swap_endian_u32(header.crlSize), 64);
    tad.tmdOffset = round_up(tad.ticketOffset + swap_endian_u32(header.ticketSize), 64);
    tad.srlOffset = round_up(tad.tmdOffset + swap_endian_u32(header.tmdSize), 64);
    tad.metaOffset = round_up(tad.srlOffset + swap_endian_u32(header.srlSize), 64);
    // Get info from TMD.
    fseek(file, tad.tmdOffset+396, SEEK_SET);
    fread(srlTidHigh, 1, 4, file);
    fread(srlTidLow, 1, 4, file);
    fseek(file, tad.tmdOffset+408, SEEK_SET);
    fread(srlCompany, 1, 2, file);
    fseek(file, tad.tmdOffset+476, SEEK_SET);
    fread(srlVerHigh, 1, 1, file);
    fread(srlVerLow, 1, 1, file);

    // I am so sorry for this mess.
    iprintf("\nSize:\n  ");
    fseek(file, 0, SEEK_END);
    unsigned long long romSize = ftell(file);
    iprintf("\x1B[42m");    //green
    printBytes(romSize);
    iprintf("\x1B[47m");    //white
    iprintf(" (\x1B[42m%ld blocks\x1B[47m)\n", ((swap_endian_u32(header.srlSize) / BYTES_PER_BLOCK) * BYTES_PER_BLOCK + BYTES_PER_BLOCK) / BYTES_PER_BLOCK);

    iprintf("Game Code:\n  ");
    iprintf("\x1B[42m");    //green
    for (int i = 0; i < 4; i++) {printf("%c", srlTidLow[i]);}
    iprintf("\x1B[47m");    //white

    iprintf("\nGame Version:\n  \x1B[42m%d.%d\x1B[47m (NUS: \x1B[42mv%d\x1B[47m)\n", (int)srlVerHigh[0], (int)srlVerLow[0], ((int)srlVerHigh[0] * 256) + (int)srlVerLow[0]);

    iprintf("Company Code:\n  \x1B[42m%c%c\x1B[47m (\x1B[42m%02x%02x\x1B[47m)\n", srlCompany[0], srlCompany[1], srlCompany[0], srlCompany[1]);
    
    // Print program type based on TID high?
    iprintf("Title ID: \n  ");
    iprintf("\x1B[42m");    //green
    for (int i = 0; i < 4; i++) {printf("%02x", srlTidHigh[i]);}
    iprintf(" ");
    for (int i = 0; i < 4; i++) {printf("%02x", srlTidLow[i]);}
    iprintf("\x1B[47m");    //white

    //print full file path
    iprintf("\n\n%s\n", fpath);
    fclose(file);
}
