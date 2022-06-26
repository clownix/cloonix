/* This is a public domain base64 implementation written by WEI Zhicheng. */

#define BASE64_ENCODE_OUT_SIZE(s) ((unsigned int)((((s) + 2) / 3) * 4 + 1))
#define BASE64_DECODE_OUT_SIZE(s) ((unsigned int)(((s) / 4) * 3))

/*
 * out is null-terminated encode string.
 * return values is out length, exclusive terminating `\0'
 */
int
base64_encode(char *in, int inlen, char *out);

/*
 * return values is out length
 */
int
base64_decode(char *in, int inlen, char *out);

