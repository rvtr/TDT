#ifndef TAD_H
#define TAD_H

#include <nds/ndstypes.h>
#include <nds/memory.h>

char* openTad(char const* src);
bool decryptTad(unsigned char* commonKey,
                unsigned char* title_key_iv,
                unsigned char* title_key_enc,
                unsigned char* content_iv,
                int srlSize,
                unsigned char* srlTidLow,
                bool dataTitle,
                unsigned char* contentHash);
void printTadInfo(char const* fpath);
extern bool dataTitle;
extern unsigned char srlTidLow[4];
extern unsigned char srlTidHigh[4];

#endif