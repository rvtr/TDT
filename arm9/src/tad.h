#ifndef TAD_H
#define TAD_H

#include <nds/ndstypes.h>
#include <nds/memory.h>

int openTad(char const* src);
bool decryptTad(unsigned char* commonKey,
                unsigned char* title_key_iv,
                unsigned char* title_key_enc,
                unsigned char* content_iv,
                int srlSize,
                unsigned char* srlTidLow);

#endif