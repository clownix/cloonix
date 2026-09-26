/** @file
   This file is derived from sober128 implementation in corosync
   cluster engine. corosync cluster engine borrows the implementation
   from LibTomCrypt.

   The latest version of the original code can be found at
   http://www.libtom.net/LibTomCrypt/ according to which this code is in the
   Public Domain
*/

#ifndef _SOBER127_H
#define _SOBER127_H

typedef struct _sober128_prng {
    unsigned long      R[17],          /* Working storage for the shift register */
                 initR[17],      /* saved register contents */
                 konst,          /* key dependent constant */
                 sbuf;           /* partial word encryption buffer */

    int          nbuf,           /* number of part-word stream bits buffered */
                 flag,           /* first add_entropy call or not? */
                 set;            /* did we call add_entropy to set key? */

} sober128_prng;

int sober128_start(sober128_prng *prng);
int sober128_add_entropy(const unsigned char *buf, unsigned long len, sober128_prng *prng);
unsigned long sober128_read(unsigned char *buf, unsigned long len, sober128_prng *prng);

#endif	/* sober128.h */
