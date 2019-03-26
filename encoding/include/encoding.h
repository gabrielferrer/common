/*
	File: encoding.h
	Creation: 14-01-2008
	Programming: Gabriel Ferrer
	Description:
		
		Declarations for UTF-8, UTF-16 encoding to/from UCS-4 characters.
*/

#ifndef ENCODING_H
#define ENCODING_H

#include "defs.h"

#define MAX_UTF8_BYTES 6

// In accordance with XML recomendation 1.1, all XML processors must accept UTF-8 and
// UTF-16 encodings of Unicode.
enum ENCODING {ENC_UNKNOWN, ENC_UTF8, ENC_UTF16LE, ENC_UTF16BE, ENC_UCS4LE, ENC_UCS4BE, ENC_UCS4UOO3412, ENC_UCS4UOO2143};

typedef unsigned int UCS4;
typedef unsigned int UTF16;

#if !(defined(ARCH_BIG_ENDIAN) || defined(ARCH_LITTLE_ENDIAN))
#error "Not big-endian or little-endian architecture"
#endif

// Same for UTF-8 encoding. utf8[0]: most significant byte, utf[MAX_UTF8_BYTES-1]:
// less significant byte.
typedef BYTE UTF8[MAX_UTF8_BYTES];

struct ENCSTREAM {
	BYTE* buffer;
	unsigned int index;
	unsigned int size;
	BOOL eob;
};

struct ENCSTREAM* EN_NewStream (unsigned int buffersize);
void EN_FreeStream (struct ENCSTREAM* s);
enum ENCODING EN_SearchEncoding (struct ENCSTREAM* s);
BOOL EN_UCS4ToUTF8 (UCS4 c, struct ENCSTREAM* s);
BOOL EN_UCS4ToUTF16 (UCS4 c, struct ENCSTREAM* s);
void EN_ClearUCS4 (UCS4* c);
BOOL EN_UTF8ToUCS4 (struct ENCSTREAM* s, UCS4* c);
BOOL EN_UTF16LeToUCS4 (struct ENCSTREAM* s, UCS4* c);
BOOL EN_UTF16BeToUCS4 (struct ENCSTREAM* s, UCS4* c);
BOOL EN_ReadUCS4Le (struct ENCSTREAM* s, UCS4* c);
BOOL EN_ReadUCS4Be (struct ENCSTREAM* s, UCS4* c);
BOOL EN_ReadUCS4Uoo3412 (struct ENCSTREAM* s, UCS4* c);
BOOL EN_ReadUCS4Uoo2143 (struct ENCSTREAM* s, UCS4* c);

#endif
