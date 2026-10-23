#ifndef HMBTN_SHA256_H
#define HMBTN_SHA256_H
#include <stdint.h>
#include <stddef.h>
typedef struct HmbtnSHA256 { uint32_t state[8]; uint64_t bytes; uint8_t block[64]; } HmbtnSHA256;
void hmbtn_sha256_init(HmbtnSHA256 *s);
void hmbtn_sha256_update(HmbtnSHA256 *s,const void *data,size_t size);
void hmbtn_sha256_finish(HmbtnSHA256 *s,char hex[65]);
#endif
