
extern int sha256_crypto_hash(unsigned char *out,const unsigned char *in,unsigned long long inlen);
/* defines to make NaCl's sha256 look like openssl's... */
#define SHA256_CTX           struct sha256digest { unsigned char digest[32]; }
#define SHA256_Init(x)       memset((x)->digest,0,32)
#define SHA256_Update(x,a,b) sha256_crypto_hash((x)->digest, (const unsigned char*)a, b)
#define SHA256_Final(x,y)    memcpy((x), (y)->digest, 32)
