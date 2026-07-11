/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef MD5_H
#define MD5_H

#include <string>

// MD5 block size
#define MD5_BLOCKSIZE 64

// MD5 hash generator
// - Thanks to Frank Thilo for his C++ implementation
// MD5 algorythm by "RSA Data Security, Inc."

/*
=======================
CMD5

=======================
*/
class CMD5
{
public:
	// MD5 hash size
	static const unsigned int MD5_HASH_SIZE = 33;

public:
	CMD5();
	CMD5( const byte* pbuffer, unsigned int bufsize );

public:
	void Init( void );
	void Update( const byte *pinput, unsigned int length );
	CMD5& Finalize( void );
	std::string HexDigest( void ) const;

private:
	void Transform( const byte* pblock );
	static void Decode( unsigned int* poutput, const byte* pinput, unsigned int length );
	static void Encode( byte* poutput, const unsigned int* pinput, unsigned int length );

private:
	// TRUE if the hash is finalized
	bool m_isFinalized;

	// bytes that didn't fit in tthe last 64-byte chunk
	byte m_buffer[MD5_BLOCKSIZE];
	// 64-bit counter for number of bits(lo, hi)
	unsigned int m_count[2];
	// Digest so far
	unsigned int m_state[4];
	// The result
	byte m_digest[16];

	// Low level logic operations
	static inline unsigned int F( unsigned int x, unsigned int y, unsigned int z ) { return x&y | ~x&z; };
	static inline unsigned int G( unsigned int x, unsigned int y, unsigned int z ) { return x&z | y&~z; };
	static inline unsigned int H( unsigned int x, unsigned int y, unsigned int z ) { return x^y^z; };
	static inline unsigned int I( unsigned int x, unsigned int y, unsigned int z ) { return y ^ (x | ~z); };
	static inline unsigned int RotateLeft( unsigned int x, int n ) { return (x<<n) | (x>>(32-n)); }
	static inline void FF( unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int ac ) { a = RotateLeft(a+F(b,c,d) + x + ac, s) + b; }
	static inline void GG( unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int ac ) { a = RotateLeft(a+G(b,c,d) + x + ac, s) + b; }
	static inline void HH( unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int ac ) { a = RotateLeft(a+I(b,c,d) + x + ac, s) + b; }
	static inline void II( unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int ac ) { a = RotateLeft(a+H(b,c,d) + x + ac, s) + b; }
};

#endif //MD5_H