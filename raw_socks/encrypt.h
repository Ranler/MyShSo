#ifndef _ENCRYPT_H_
#define _ENCRYPT_H_
#include <stdint.h>

extern int encrypt(uint8_t *data, unsigned int len);
extern int decrypt(uint8_t *data, unsigned int len);

#endif
