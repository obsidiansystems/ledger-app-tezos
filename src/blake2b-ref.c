/*
   BLAKE2 reference source code package - reference C implementations

   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:

   - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0

   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
*/

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "blake2.h"
#include "blake2-impl.h"

static const uint64_t b2b_IV[8] =
{
  0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
  0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
  0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
  0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t b2b_sigma[12][16] =
{
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
  { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
  {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
  {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
  {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
  { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
  { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
  {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
  { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};


static void b2b_set_lastnode( b2b_state *S )
{
  S->f[1] = (uint64_t)-1;
}

/* Some helper functions, not necessarily useful */
static int b2b_is_lastblock( const b2b_state *S )
{
  return S->f[0] != 0;
}

static void b2b_set_lastblock( b2b_state *S )
{
  if( S->last_node ) b2b_set_lastnode( S );

  S->f[0] = (uint64_t)-1;
}

static void b2b_increment_counter( b2b_state *S, const uint64_t inc )
{
  S->t[0] += inc;
  S->t[1] += ( S->t[0] < inc );
}

static void b2b_init0( b2b_state *S )
{
  size_t i;
  memset( S, 0, sizeof( b2b_state ) );

  for( i = 0; i < 8; ++i ) S->h[i] = b2b_IV[i];
}

/* init xors IV with input parameter block */
int b2b_init_param( b2b_state *S, const b2b_param *P )
{
  const uint8_t *p = ( const uint8_t * )( P );
  size_t i;

  b2b_init0( S );

  /* IV XOR ParamBlock */
  for( i = 0; i < 8; ++i )
    S->h[i] ^= load64( p + sizeof( S->h[i] ) * i );

  S->outlen = P->digest_length;
  return 0;
}



int b2b_init( b2b_state *S, size_t outlen )
{
  b2b_param P[1];

  if ( ( !outlen ) || ( outlen > B2B_OUTBYTES ) ) return -1;

  P->digest_length = (uint8_t)outlen;
  P->key_length    = 0;
  P->fanout        = 1;
  P->depth         = 1;
  store32( &P->leaf_length, 0 );
  store32( &P->node_offset, 0 );
  store32( &P->xof_length, 0 );
  P->node_depth    = 0;
  P->inner_length  = 0;
  memset( P->reserved, 0, sizeof( P->reserved ) );
  memset( P->salt,     0, sizeof( P->salt ) );
  memset( P->personal, 0, sizeof( P->personal ) );
  return b2b_init_param( S, P );
}


int b2b_init_key( b2b_state *S, size_t outlen, const void *key, size_t keylen )
{
  b2b_param P[1];

  if ( ( !outlen ) || ( outlen > B2B_OUTBYTES ) ) return -1;

  if ( !key || !keylen || keylen > B2B_KEYBYTES ) return -1;

  P->digest_length = (uint8_t)outlen;
  P->key_length    = (uint8_t)keylen;
  P->fanout        = 1;
  P->depth         = 1;
  store32( &P->leaf_length, 0 );
  store32( &P->node_offset, 0 );
  store32( &P->xof_length, 0 );
  P->node_depth    = 0;
  P->inner_length  = 0;
  memset( P->reserved, 0, sizeof( P->reserved ) );
  memset( P->salt,     0, sizeof( P->salt ) );
  memset( P->personal, 0, sizeof( P->personal ) );

  if( b2b_init_param( S, P ) < 0 ) return -1;

  {
    uint8_t block[B2B_BLOCKBYTES];
    memset( block, 0, B2B_BLOCKBYTES );
    memcpy( block, key, keylen );
    b2b_update( S, block, B2B_BLOCKBYTES );
    secure_zero_memory( block, B2B_BLOCKBYTES ); /* Burn the key from stack */
  }
  return 0;
}

#define G(r,i,a,b,c,d)                      \
  do {                                      \
    a = a + b + m[b2b_sigma[r][2*i+0]]; \
    d = rotr64(d ^ a, 32);                  \
    c = c + d;                              \
    b = rotr64(b ^ c, 24);                  \
    a = a + b + m[b2b_sigma[r][2*i+1]]; \
    d = rotr64(d ^ a, 16);                  \
    c = c + d;                              \
    b = rotr64(b ^ c, 63);                  \
  } while(0)

#define ROUND(r)                    \
  do {                              \
    G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
    G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
    G(r,2,v[ 2],v[ 6],v[10],v[14]); \
    G(r,3,v[ 3],v[ 7],v[11],v[15]); \
    G(r,4,v[ 0],v[ 5],v[10],v[15]); \
    G(r,5,v[ 1],v[ 6],v[11],v[12]); \
    G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
    G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
  } while(0)

static void b2b_compress( b2b_state *S, const uint8_t block[B2B_BLOCKBYTES] )
{
  uint64_t m[16];
  uint64_t v[16];
  size_t i;

  for( i = 0; i < 16; ++i ) {
    m[i] = load64( block + i * sizeof( m[i] ) );
  }

  for( i = 0; i < 8; ++i ) {
    v[i] = S->h[i];
  }

  v[ 8] = b2b_IV[0];
  v[ 9] = b2b_IV[1];
  v[10] = b2b_IV[2];
  v[11] = b2b_IV[3];
  v[12] = b2b_IV[4] ^ S->t[0];
  v[13] = b2b_IV[5] ^ S->t[1];
  v[14] = b2b_IV[6] ^ S->f[0];
  v[15] = b2b_IV[7] ^ S->f[1];

  ROUND( 0 );
  ROUND( 1 );
  ROUND( 2 );
  ROUND( 3 );
  ROUND( 4 );
  ROUND( 5 );
  ROUND( 6 );
  ROUND( 7 );
  ROUND( 8 );
  ROUND( 9 );
  ROUND( 10 );
  ROUND( 11 );

  for( i = 0; i < 8; ++i ) {
    S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
  }
}

#undef G
#undef ROUND

int b2b_update( b2b_state *S, const void *pin, size_t inlen )
{
  const unsigned char * in = (const unsigned char *)pin;
  if( inlen > 0 )
  {
    size_t left = S->buflen;
    size_t fill = B2B_BLOCKBYTES - left;
    if( inlen > fill )
    {
      S->buflen = 0;
      memcpy( S->buf + left, in, fill ); /* Fill buffer */
      b2b_increment_counter( S, B2B_BLOCKBYTES );
      b2b_compress( S, S->buf ); /* Compress */
      in += fill; inlen -= fill;
      while(inlen > B2B_BLOCKBYTES) {
        b2b_increment_counter(S, B2B_BLOCKBYTES);
        b2b_compress( S, in );
        in += B2B_BLOCKBYTES;
        inlen -= B2B_BLOCKBYTES;
      }
    }
    memcpy( S->buf + S->buflen, in, inlen );
    S->buflen += inlen;
  }
  return 0;
}

int b2b_final( b2b_state *S, void *out, size_t outlen )
{
  uint8_t buffer[B2B_OUTBYTES] = {0};
  size_t i;

  if( out == NULL || outlen < S->outlen )
    return -1;

  if( b2b_is_lastblock( S ) )
    return -1;

  b2b_increment_counter( S, S->buflen );
  b2b_set_lastblock( S );
  memset( S->buf + S->buflen, 0, B2B_BLOCKBYTES - S->buflen ); /* Padding */
  b2b_compress( S, S->buf );

  for( i = 0; i < 8; ++i ) /* Output full hash to temp buffer */
    store64( buffer + sizeof( S->h[i] ) * i, S->h[i] );

  memcpy( out, buffer, S->outlen );
  secure_zero_memory(buffer, sizeof(buffer));
  return 0;
}

#if defined(SUPERCOP)
int crypto_hash( unsigned char *out, unsigned char *in, unsigned long long inlen )
{
  return b2b( out, B2B_OUTBYTES, in, inlen, NULL, 0 );
}
#endif

#if defined(B2B_SELFTEST)
#include <string.h>
#include "blake2-kat.h"
int main( void )
{
  uint8_t key[B2B_KEYBYTES];
  uint8_t buf[BLAKE2_KAT_LENGTH];
  size_t i, step;

  for( i = 0; i < B2B_KEYBYTES; ++i )
    key[i] = ( uint8_t )i;

  for( i = 0; i < BLAKE2_KAT_LENGTH; ++i )
    buf[i] = ( uint8_t )i;

  /* Test simple API */
  for( i = 0; i < BLAKE2_KAT_LENGTH; ++i )
  {
    uint8_t hash[B2B_OUTBYTES];
    b2b( hash, B2B_OUTBYTES, buf, i, key, B2B_KEYBYTES );

    if( 0 != memcmp( hash, b2b_keyed_kat[i], B2B_OUTBYTES ) )
    {
      goto fail;
    }
  }

  /* Test streaming API */
  for(step = 1; step < B2B_BLOCKBYTES; ++step) {
    for (i = 0; i < BLAKE2_KAT_LENGTH; ++i) {
      uint8_t hash[B2B_OUTBYTES];
      b2b_state S;
      uint8_t * p = buf;
      size_t mlen = i;
      int err = 0;

      if( (err = b2b_init_key(&S, B2B_OUTBYTES, key, B2B_KEYBYTES)) < 0 ) {
        goto fail;
      }

      while (mlen >= step) {
        if ( (err = b2b_update(&S, p, step)) < 0 ) {
          goto fail;
        }
        mlen -= step;
        p += step;
      }
      if ( (err = b2b_update(&S, p, mlen)) < 0) {
        goto fail;
      }
      if ( (err = b2b_final(&S, hash, B2B_OUTBYTES)) < 0) {
        goto fail;
      }

      if (0 != memcmp(hash, b2b_keyed_kat[i], B2B_OUTBYTES)) {
        goto fail;
      }
    }
  }

  puts( "ok" );
  return 0;
fail:
  puts("error");
  return -1;
}
#endif
