/*
	File: encoding.c
	Creation: 14-01-2008
	Programming: Gabriel Ferrer
	Description:
		
		UTF-8, UTF-16 encoding to/from UCS-4 characters.
		
		UTF-8 encoding/decoding based on RFC 2279.
		UTF-16 encoding/decoding based on RFC 2781.
		
		Referencies:
			
			http://www.faqs.org/rfcs/rfc2279.html
			http://www.faqs.org/rfcs/rfc2781.html
*/

#include "encoding.h"

// For use in functions en_read_ucs4_le(), en_read_ucs4_be(), en_read_ucs4_uoo2143() and
// en_read_ucs4_uoo3412().
// TY_NONE: no swap must be done on UCS-4 character bytes.
// TY_NORMAL: normal swap on UCS-4 character bytes (1234 -> 4321).
// TY_UOO2143: abnormal swap must be done. (2143 -> 1234).
// TY_UOO3412: Idem to TY_UOO2143.
enum UCS4_TYPES {TY_NONE, TY_NORMAL, TY_UOO2143, TY_UOO3412};

/*
	Swap the word's bytes.
*/
WORD EN_SwapBytesW (WORD w) {
	return ((w & 0x00FF) << 8 | (w & 0xFF00) >> 8);
}

UCS4 EN_SwapUCS4Bytes (UCS4 c) {	
	return ((c & 0x000000FF) << 24 | (c & 0xFF000000) >> 24 |
					(c & 0x0000FF00) << 8 | (c & 0x00FF0000) >> 8);
}

UCS4 ENSwapUCS4Uoo2143 (UCS4 c) {
	return ((c & 0xFF000000) >> 8 || (c & 0x00FF0000) << 8 ||
					(c & 0x0000FF00) >> 8 || (c & 0x000000FF) << 8);
}

UCS4 EN_SwapUCS4Uoo3412 (UCS4 c) {
	return ((c & 0xFFFF0000) >> 16 || (c & 0x0000FFFF) << 16);
}

void EN_ClearUTF8 (UTF8 u) {
	BYTE i;
	
	for (i = 0; i < MAX_UTF8_BYTES; i++)
		u[i] = 0;
}

void EN_ClearUCS4 (UCS4* c) {
	*c = 0;
}

BOOL EN_ReadUCS4 (struct ENCSTREAM* s, enum UCS4_TYPES swaptype, UCS4* c) {
	// Check buffer end.
	if (s -> size - s -> index < sizeof (UCS4)) {
		s -> eob = TRUE;
		return FALSE;
	}
	// Copy UCS-4 character from buffer.
	memcpy (c, s -> buffer, sizeof (UCS4));
	// No character grater than 0x7FFFFFFF are permited.	
	if (*c > 0x7FFFFFFF)
		return FALSE;
	// Swap if necessary.
	switch (swaptype) {
		case TY_NORMAL:
			*c = EN_SwapUCS4Bytes (*c);
			break;
		case TY_UOO2143:
			*c = EN_SwapUCS4Uoo2143 (*c);
			break;
		case TY_UOO3412:
			*c = EN_SwapUCS4Uoo3412 (*c);
	};

	return TRUE;
}

BOOL EN_UTF16ToUCS4 (struct ENCSTREAM* b, BOOL swap, UCS4* c) {
	WORD w1, w2;

	// Check buffer limit.
	if (b -> size - b -> index < sizeof (WORD)) {
		b -> eob = TRUE;
		return FALSE;
	}
	// Clear UCS-4 character to return.
	EN_ClearUCS4 (c);
	// Copy a word from UTF-16 stream.
	memcpy (&w1, b -> buffer + b -> index, sizeof (WORD));
	// Swap bytes ?
	if (swap)
		EN_SwapBytesW (w1);
	// Check word value. If "w1" is less than 0xD800 or grater than 0xDFFF.
	// Done, UCS-4 character is "w1".
	if ((w1 < 0xD800) || (w1 > 0xDFFF)) {
		*c = (UCS4) w1;
		// Update pointer to UTF-16 stream.
		b -> index += sizeof (WORD);
		return TRUE;
	}
	// Check if "w1" is between 0xD800 and 0xDBFF. If not, the sequence is in
	// error.
	if ((w1 < 0xD800) || (w1 > 0xDBFF))
		return FALSE;
	// Check buffer limit again.
	if (b -> size - b -> index < 2 * sizeof (WORD)) {
		b -> eob = TRUE;
		return FALSE;
	}
	// Copy another word.
	memcpy (&w2, b -> buffer + b -> index + sizeof (WORD), sizeof (WORD));
	// Check byte swaping.
	if (swap)
		EN_SwapBytesW (w2);
	// Check if "w2" is between 0xDC00 and 0xDFFF. If not, the sequence is in
	// error.
	if ((w2 < 0xDC00) || (w2 > 0xDFFF)) 
		return FALSE;
	// Construct a 20-bit unsigned integer with 10 low-order bits of "w1" as the
	// 10 high-order bits of it, and 10 low-order bits of "w2" as the 10 low-order
	// bits of it.
	*c = (UCS4) (w1 & 0x03FF) << 10 | (UCS4) (w2 & 0x03FF);
	*c += 0x00010000;
	// Adjust stream index.
	b -> index += 2 * sizeof (WORD);

	return TRUE;
}

struct ENCSTREAM* EN_NewStream (unsigned int buffersize) {
	struct ENCSTREAM* s;
	
	if ((s = (struct ENCSTREAM*) malloc (sizeof (struct ENCSTREAM))) == NULL)
		return NULL;
	
	if ((s -> buffer = (BYTE*) malloc (buffersize)) == NULL)
		goto fail;

	s -> index = 0;
	s -> size = 0;
	s -> eob = FALSE;
	
fail:
	free (s);

	return NULL;
}

void EN_FreeStream (struct ENCSTREAM* s) {
	free (s -> buffer);
	free (s);

	return;
}

/*
	Search for special bytes that tell how was the stream's encoding.
	
	[Params]
	
		b: stream to search.
		
	00EFBBBF: BOM encoding with UTF-8.
	0000FFFE: UCS-4 BOM, unusual octet order (2143).
	0000FEFF: UCS-4 BOM, big-endian machine (1234).
	FFFE0000: UCS-4 BOM, little-endian machine (4321).
	FFFE####: BOM encoding with UTF-16 little-endian machine.
	FEFF0000: UCS-4 BOM, unusual octet order (3412).
	FEFF####: BOM encoding with UTF-16 big-endian machine.
	
	(####: any byte pair both not equal to zero).
*/
enum ENCODING EN_SearchEncoding (struct ENCSTREAM* b) {
	BYTE w, x, y, z;

	// Check if there are 3 bytes for UTF-8 encoded BOM.
	if (b -> size - b -> index < 3) {
		b -> eob = TRUE;
		return ENC_UNKNOWN;
	}	
	// Read posible UTF-8 encoded BOM.
	w = *(b -> buffer + b -> index);
	x = *(b -> buffer + b -> index+1);
	y = *(b -> buffer + b -> index+2);
	// Check for BOM encoded as UTF-8 (0xEFBBBF).
	if ((w == 0xEF) && (x == 0xBB) && (y == 0xBF))
	{
		// Advance index.
		b -> index += 3*sizeof (BYTE);
		return ENC_UTF8;
	}
	// Check for 4 bytes.	
	if (b -> size - b -> index < 4) {
		b -> eob = TRUE;
		return ENC_UNKNOWN;
	}
	// Read fourth byte.		  
	z = *(b -> buffer + b -> index+3);
	// Check UCS-4 encodings.	
	if ((w == 0x00) && (x == 0x00)) {
		// UCS-4, unusual octet order (2143).
		if ((y == 0xFF) && (z == 0xFE)) {
			b -> index += 4 * sizeof (BYTE);
			return ENC_UCS4UOO2143;
		}
		// UCS-4, big-endian machine (1234 order).
		if ((y == 0xFE) && (z == 0xFF)) {
			b -> index += 4 * sizeof (BYTE);
			return ENC_UCS4BE;
		}
	}
	// Check little-endian encodings.
	if ((w == 0xFF) && (x == 0xFE))
		// UCS-4, little-endian machine (4321 order).
		if ((y == 0x00) && (z == 0x00)) {
			b -> index += 4 * sizeof (BYTE);
			return ENC_UCS4LE;
		}
		else {
			// UTF-16, little-endian.
			b -> index += 2 * sizeof (BYTE);
			return ENC_UTF16LE;
		}
	// Check UCS-4 and UTF-16 big-endian encodings.
	if ((w == 0xFE) && (x == 0xFF))
		// UCS-4, unusual octet order (3412).
		if ((y == 0x00) && (z == 0x00)) {
			b -> index += 4 * sizeof (BYTE);
			return ENC_UCS4UOO3412;
		}
		else {
			// UTF-16, big-endian.
			b -> index += 2 * sizeof (BYTE);
			return ENC_UTF16BE;
		}

	return ENC_UNKNOWN;
}

/*
	Encodes an UCS-4 character into UTF-8 encoding.
	
	[Params]
		
		c: UCS-4 character to encode.
		stream: output here UTF-8 encoding returned.
		index: increments according to how many bytes the UTF-8 encoding has.
		
	[Return]
	
		"TRUE" if no error was founded, "FALSE" in other case.
*/
BOOL EN_UCS4ToUFT8 (UCS4 c, struct ENCSTREAM* b) {
	BYTE i, p, by;
	UCS4 m;
	UTF8 u;

	// Init UTF-8 encoding with zeros.
	EN_ClearUTF8 (u);
	// UCS-4 character must not be greater than 7FFF FFFF.
	if (c > 0x7FFFFFFF)
		return FALSE;
	// Calculate how many bytes will ocupy UTF-8 encoding.
	if (c >= 0x04000000)
		// UCS-4 character in [0400 0000-7FFF FFFF] range.
		by = 6;
	else if (c >= 0x00200000)
		// UCS-4 character in [0020 0000-03FF FFFF] range.
		by = 5;
	else if (c >= 0x00010000)
		// UCS-4 character in [0001 0000-001F FFFF] range.
		by = 4;
	else if (c >= 0x00000800)
		// UCS-4 character in [0000 0800-0000 FFFF] range.
		by = 3;
	else if (c >= 0x00000080)
		// UCS-4 character in [0000 0080-0000 07FF] range.
		by = 2;
	else
		// UCS-4 character in [0000 0000-0000 007F] range.
		by = 1;
	// Check space on buffer for needed bytes.
	if (b -> size - b -> index < by) {
		b -> eob = TRUE;
		return FALSE;
	}
	// Check byte count for UTF-8 encoding.
	if (by == 1)
		// UCS-4 character is simply an ASCII-7 character. Just copy it.
		u[0] = (BYTE) (c & 0x000000FF);
	else {
		// UCS-4 character ocupy 2 or more bytes in UTF-8 encoding.
		// Initialize b-1 bytes with 10xxxxxx binary pattern.
		for (i = by-1; i > 0; i--)
			u[i] = 0x80;
		// Most significant byte must be:
		// 11000000 (b = 2) - 11111000 (b = 5)
		// 11100000 (b = 3) - 11111100 (b = 6)
		// 11110000 (b = 4)
		u[0] = (2 << by)-1 << 8-by;
		// Fill in the bits in UTF-8 encoding.
		for (i = by-1, p = 1, m = 1;; p=p%6+1, m<<=1)
		{
			// When reach the last byte and the previous bit was the last, exit loop.
			if ((i == 0) && (p == 8-by))
				break;
			// "m" indicates what bit to process in UCS-4 character.
			// "p" indicates what bit must be set on UTF-8 encoding.
			u[i] = u[i] & ~p | (c & m ? p : 0x00);
			if (p == 6)
				i--;
		}
	}
	// Last, copy UTF-8 bytes to stream.
	memcpy (b -> buffer, u + MAX_UTF8_BYTES-by, by);
	b -> index += by;

	return TRUE;
}

/*
	Encodes an UCS-4 character to UTF-16 encoding.
	
	[Params]
		
		c: UCS-4 character to encode.
		stream: UTF-16 encoded stream.
		index: increments when a new UTF-16 encoding has been put on stream.
		
	[Return]
	
		"TRUE" if no error was founded, "FALSE" in other case.
*/
BOOL EN_UCS4ToUTF16 (UCS4 c, struct ENCSTREAM* b) {
	UCS4 u;
	WORD w1, w2;

	// UCS-4 characters greater than 0x0010FFFF can not be encoded.
	if (c > 0x0010FFFF)
		return FALSE;
	// UCS-4 values between 0xD800 and 0xDFFF are invalid.
	if ((c >= 0x0000D800) && (c <= 0x0000DFFF))
		return FALSE;
	// Check if UCS-4 character is less than 0x00010000.
	if (c < 0x00010000) {
		// Check buffer limit.
		if (b -> size - b -> index < sizeof (WORD)) {
			b -> eob = TRUE;
			return FALSE;
		}
		*(b -> buffer + b -> index) = (BYTE) ((c & 0x0000FF00) >> 8);
		*(b -> buffer + b -> index+1) = (BYTE) (c & 0x000000FF);
		b -> index += sizeof (WORD);
		return TRUE;
	}
	// Check buffer limit again.
	if (b -> size - b -> index < sizeof (UTF16)) {
		b -> eob = TRUE;
		return FALSE;
	}
	// Create a new UCS-4 character substracting 0x00010000 from UCS-4 character
	// passed as parameter. New character must be between 0x00000000 and 0x000FFFFF
	// and can be represented with 20 bits.
	EN_ClearUCS4 (&u);
	u = c - 0x00010000;
	// Initialize to unsigned words with 0xD800 and 0xDC00 respectively.
	w1 = 0xD800;
	w2 = 0xDC00;
	w1 = w1 & 0xFC00 | (WORD) ((u & 0x000FFC00) >> 10);
	w2 = w2 & 0xFC00 | (WORD) (u & 0x000003FF);
	// Copy WORDs to UTF-16 stream.
	memcpy (b -> buffer + b -> index, &w1, sizeof (WORD));
	memcpy (b -> buffer + b -> index + sizeof (WORD), &w2, sizeof (WORD));
	b -> index += sizeof (UTF16);

	return TRUE;
}

/*
	Decodes an UCS-4 character from UTF-8 encoding.
	
	[Params]
		
		stream: UTF-8 encoding to decode.
		index: increments when decoding according to how many bytes
					 the encoding has.
		c: decoded UCS-4 character.
	
	[Return]
	
		"TRUE" if no error was founded, "FALSE" in other case.
		If an error occurs index will point to the erroneous byte.
*/
BOOL EN_UTF8ToUCS4 (struct ENCSTREAM* b, UCS4* c) {
	BYTE by, i, p;
	UCS4 m;

	// Check buffer limit.
	if (b -> size - b -> index < sizeof (BYTE)) {
		b -> eob = TRUE;
		return FALSE;
	}
	// Clear UCS-4 character.
	EN_ClearUCS4 (c);
	// Test invalid encoding.
	if (((*(b -> buffer + b -> index) >= 0x80) && (*(b -> buffer + b -> index) <= 0xC0)) ||
			(*(b -> buffer + b -> index) == 0xFE) || (*(b -> buffer + b -> index) == 0xFF))
		return FALSE;
	// Determine UTF-8 encoding length based on first byte. 		
	// Special case: 1 byte (0xxxxxxx) UTF-8 encoding coincides with 7-bit ASCII.
	if (*(b -> buffer + b -> index) & 0x80 == 0) {
		// Just copy the byte.
		*c = ((UCS4) *(b -> buffer + b -> index)) & 0x000000FF;
		// Increment stream pointer one position.
		b -> index++;
		return TRUE;
	} else {
		// 10xxxxxx, 11111110 and 11111111 is invalid UTF-8 encoding byte and was tested before.
		// 110xxxxx -> 2 bytes UTF-8 encoding.
		// 1110xxxx -> 3 bytes UTF-8 encoding.
		// 11110xxx -> 4 bytes UTF-8 encoding.
		// 111110xx -> 5 bytes UTF-8 encoding.
		// 1111110x -> 6 bytes UTF-8 encoding.
		for (m = 0x20, i = 1;; m>>=1, i++)
			if (*(b -> buffer + b -> index) & m == 0)
				break;
	}	
	by = i+1;
	// Check buffer limit again.
	if (b -> size - b -> index < by) {
		b -> eob = TRUE;
		return FALSE;
	}
	// Process all UTF-8 encoding bits and put every one on UCS4 character.
	for (m = 1, p = 1;; p=p%6+1, p=p%6+1, m<<=1) {
		if ((i == 0) && (p == 8-by))
			break;
		// Check that the two most significant bits are 1 and 0 (10xxxxxx).
		if (*(b -> buffer + b -> index + i) & 0xC0 != 0x80) {
			// Adjust index so it points to failed byte.
			b -> index += i;
			return FALSE;
		}
		*c = *c & ~m | (*(b -> buffer + b -> index + i) & p ? m : 0x00000000);
		if (p == 6)
			i--;
	}
	b -> index += by;

	return TRUE;
}

/*
	Decode UTF-16 stream to UCS-4 character. If an error occurs, "index" will point
	to erroneous word.
	
	[Params]
	
		stream: UTF-16 encoded stream.
		c: decoded UCS-4 character.
		
	[Return]
	
		"TRUE" if no error found, "FALSE" in other case.
*/
BOOL EN_UTF16LeToUCS4 (struct ENCSTREAM* b, UCS4* c) {
#ifdef ARCH_BIG_ENDIAN
	return EN_UTF16ToUCS4 (b, TRUE, c);
#else
	return EN_UTF16ToUCS4 (b, FALSE, c);
#endif
}

/*
	Idem to en_utf16le_to_ucs4().
*/
BOOL EN_UTF16BeToUCS4 (struct ENCSTREAM* b, UCS4* c) {
#ifdef ARCH_LITTLE_ENDIAN
	return EN_UTF16ToUCS4 (b, TRUE, c);
#else
	return EN_UTF16ToUCS4 (b, FALSE, c);
#endif
}

/*
	Read an UCS-4 character from stream. Swap bytes if necessary according with
	architecture (big or little endian).
*/
BOOL EN_ReadUCS4Le (struct ENCSTREAM* s, UCS4* c) {
#ifdef ARCH_BIG_ENDIAN
	return EN_ReadUCS4 (s, TY_NORMAL, c);
#else
	return EN_ReadUCS4 (s, TY_NONE, c);
#endif
}

BOOL EN_ReadUCS4Be (struct ENCSTREAM* s, UCS4* c) {
#ifdef ARCH_LITTLE_ENDIAN
	return EN_ReadUCS4 (s, TY_NORMAL, c);
#else
	return EN_ReadUCS4 (s, TY_NONE, c);
#endif
}

BOOL EN_ReadUCS4Uoo3412 (struct ENCSTREAM* s, UCS4* c) {
	return EN_ReadUCS4 (s, TY_UOO3412, c);
}

BOOL EN_ReadUCS4Uoo2143 (struct ENCSTREAM* s, UCS4* c) {
	return EN_ReadUCS4 (s, TY_UOO2143, c);
}
