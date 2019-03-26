#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "config.h"

#define MAX_SECTION_LENGTH 63
#define MAX_KEY_LENGTH 63
#define MAX_VALUE_LENGTH 1023
#define MAX_COMMENT_LENGTH 1023
#define MAX_FILE_LINE_LENGTH 3072
#define MAX_VALID_COMMON_CHAR 63
#define MAX_VALID_VALUE_CHAR 32
#define BUFFER_SIZE 1024
#define LOG_SIZE 256

typedef enum {LOGERROR, LOGWARNING, LOGINFO} logtype_t;

typedef struct processline_s {
	int bindex;					// Current char (absolute) on buffer.
	int line;					// Current line in process (relative to file).
	int character;				// Current char (into current line) in process.
	int begin;					// Index to section, key or value begining.
	int end;					// Index to section, key or value end.
	int lvc;					// Index of last value char when a comment goes next
								// to it.
	int pcb;					// Posible comment begining. There could be tab or
								// space characters before the comment character.
								// This characters must to be saved for writing back
								// to the config file.
	bool_t psection;			// Indicates partial section copied into "sectionname".
								// It happens when reading a section, the buffer gets
								// empty (the last byte in buffer is reached).
	bool_t pkey;				// Idem with "keyname".
	bool_t pvalue;				// Idem with "valuename".
	bool_t pcomment;			// Idem with "comment".
	bool_t procsection;			// Indicates that a section is in process.
	bool_t prockey;				// Idem for a key.
	bool_t procvalue;			// Idem for a value.
	bool_t equal;				// Indicates that an "=" sign was found.
	bool_t proccomment;			// Indicates that a comment is in process.
	section_t* currsection;		// Pointer to current section in section list.
	keyvalue_t* currkeyvalue;	// Pointer to current key-value pair in section list.
	comment_t* currcomment;		// Pointer to current comment.
	index_t* currindex;			// Pointer to current index entry.
	char* sectionname;			// Buffer for current section name.
	char* keyname;				// Idem for key name.
	char* valuename;			// Idem for value name.
	char* comment;				// Idem for comment.
	unsigned int sectionlength;	// Size in bytes of "sectionname".
	unsigned int keylength;		// Idem for "keyname".
	unsigned int valuelength;	// Idem for "valuename".
	unsigned int commentlength;	// Idem for "comment".
} processline_t;

/*
	Valid chars for sections, keys and values: numbers, letters and the underscore.
	Note that they are ordered.
*/
const char VALIDCOMMONCHARS[MAX_VALID_COMMON_CHAR] = {
	'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','I','J','K',
	'L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','_','a','b','c','d','e',
	'f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
	 
const char VALIDVALUECHARS[MAX_VALID_VALUE_CHAR] = {
	'!','"','#','$','%','&','\'','(',')','*','+','´','-','.','/',':',';','<','=','>','?',
	'@','[','\\',']','^','`','{','|','}','~'};

const char* TRUESTRING = "TRUE";
const char* FALSESTRING = "FALSE";
const char* MSGSECTION = "%s: ']' without corresponding '[' at line %u, character %u";
const char* MSGEMPTYSECTION = "%s: Empty section at line %u, character %u";
const char* MSGINVALIDCHAR = "%s: Invalid character at line %u, character %u";
const char* MSGEMPTYKEY = "%s: '=' without a key before at line %u, character %u";
const char* MSGSTRIPED = "%s: Can't exist a section or key striped into one or more lines. At line %u, character %u";
const char* MSGINVSECCHAR = "%s: Invalid section char at line %u, character %u";
const char* MSGINVVALCHAR = "%s: Invalid value char at line %u, character %u";
const char* MSGINVKEYCHAR = "%s: Invalid key char at line %u, character %u";
const char* MSGNOCOMMENT = "%s: No comment allowed here. In line %u, character %u";

loglist_t* LogList;

/*
	Compare two strings, the first one will be converted to uppercase,
	and the second must to be in uppercase.
	Returns zero if the strings are equal, or not zero if the strings
	doesn't match.

	[Params]

		toupp: this string will be converted to uppercase.
		upper: this string must to be in uppercase.
*/
bool_t CF_CompareString (const char* toupp, const char* upper) {
	unsigned int i;

	i = 0;
	while (toupper (toupp[i]) == upper[i] && toupp[i] != '\0' && upper[i] != '\0')
		i++;

	if (toupp[i] == '\0' && upper[i] == '\0')
		return TRUE;
	else
		return FALSE;
}

char* CF_GetLogType (logtype_t logtype) {
	switch (logtype) {
		case LOGERROR:
			return "Error";

		case LOGWARNING:
			return "Warning";

		case LOGINFO:
			return "Info";

		default:
			return "Unknown";
	}
}

void CF_WriteLog (logtype_t logtype, const char* slog, int line, int character) {
	char log[LOG_SIZE];
	loglist_t* n;

	if ((n = (loglist_t*) malloc (sizeof (loglist_t))) == NULL)
		return;

	memset (log, 0, LOG_SIZE);
	sprintf (log, slog, CF_GetLogType (logtype), line, character);

	if ((n->log = (char*) malloc (strlen (log) + 1)) == NULL)
		goto clean;

	strcpy (n->log, log);
	n->next = NULL;

	if (LogList)
		n->next = LogList;

	LogList = n;

	return;

clean:
	free (n);
}

void CF_CleanLog () {
	loglist_t* l;

	while (LogList) {
		l = LogList;
		LogList = LogList->next;
		free (l);
	}
}

config_t* CF_NewConfig (const char* filename) {
	config_t* c;
	unsigned int len;

	if ((c = (config_t*) malloc (sizeof (config_t))) == NULL)
		return NULL;

	len = strlen (filename);
	if ((c->filename = (char*) malloc (len + 1)) == NULL)
		goto fail;

	strcpy (c->filename, filename);
	c->file = NULL;
	c->sections = NULL;
	c->comments = NULL;
	c->index = NULL;

	return c;

fail:
	free (c);

	return NULL;
}

processline_t* CF_NewProcessLine () {
	processline_t* pl;

	if ((pl = (processline_t*) malloc (sizeof (processline_t))) == NULL)
		return NULL;
		
	if ((pl->sectionname = (char*) malloc (MAX_SECTION_LENGTH + 1)) == NULL)
		goto fail;
		
	if ((pl->keyname = (char*) malloc (MAX_KEY_LENGTH + 1)) == NULL)
		goto fail2;
		
	if ((pl->valuename = (char*) malloc (MAX_VALUE_LENGTH + 1)) == NULL)
		goto fail3;

	if ((pl->comment = (char*) malloc (MAX_COMMENT_LENGTH + 1)) == NULL)
		goto fail4;

	// Set default values.
	pl->bindex = 0;
	pl->line = 1;
	pl->character = 1;
	pl->begin = -1;
	pl->end = -1;
	pl->lvc = -1;
	pl->pcb = -1;
	pl->psection = FALSE;
	pl->pkey = FALSE;
	pl->pvalue = FALSE;
	pl->pcomment = FALSE;
	pl->procsection = FALSE;
	pl->prockey = FALSE;
	pl->procvalue = FALSE;
	pl->proccomment = FALSE;
	pl->equal = FALSE;
	pl->currsection = NULL;
	pl->currkeyvalue = NULL;
	pl->currcomment = NULL;
	pl->currindex = NULL;
	memset (pl->sectionname, 0, MAX_SECTION_LENGTH + 1);
	memset (pl->keyname, 0, MAX_KEY_LENGTH + 1);
	memset (pl->valuename, 0, MAX_VALUE_LENGTH + 1);
	memset (pl->comment, 0, MAX_COMMENT_LENGTH + 1);
	pl->sectionlength = 0;
	pl->keylength = 0;
	pl->valuelength = 0;
	pl->commentlength = 0;
	
	return pl;

fail4:
	free (pl->valuename);
fail3:
	free (pl->keyname);
fail2:
	free (pl->sectionname);
fail:
	free (pl);
		
	return NULL;
}

void CF_FreeIndex (index_t* index) {
	index_t* i;

	while (index) {
		i = index;
		index = index->next;
		free (i);
	}
}

void CF_FreeProcessLine (processline_t* pl) {
	free (pl->sectionname);
	free (pl->keyname);
	free (pl->valuename);
	free (pl->comment);
	free (pl);
}

/*
	Creates a new key-value pair. Return the created kay-value pair structure.
	
	[Params]
		
		key: key's name.
		value: value's name.
		line: line number into the file where the key-value pair is.
		keylen: length of "key".
		valuelen: length of "value".
		
*/
keyvalue_t* CF_NewKeyValue (char* key, char* value, unsigned int keylen, unsigned int valuelen) {
	keyvalue_t* k;
	
	if ((k = (keyvalue_t*) malloc (sizeof (keyvalue_t))) == NULL)
		return NULL;

	if ((k->key = (char*) malloc (keylen + 1)) == NULL)
		goto fail;

	if ((k->value = (char*) malloc (valuelen + 1)) == NULL)
		goto fail1;

	strcpy (k->key, key);
	strcpy (k->value, value);
	k->next = NULL;
	k->prev = NULL;
	
	return k;

fail1:
	free (k->key);
fail:
	free (k);

	return NULL;
}

/*
	Creates a new section. Return the created section structure.

	[Params]
		
		name: section's name.
		line: line number into the file where the section is.
		length: length of "name".
*/
section_t* CF_NewSection (char* name, unsigned int length) {
	section_t* s;
	
	if ((s = (section_t*) malloc (sizeof (section_t))) == NULL)
		return NULL;
		
	if ((s->name = (char*) malloc (length + 1)) == NULL)
		goto fail;

	strcpy (s->name, name);
	s->keyvalues = NULL;
	s->prev = NULL;
	s->next = NULL;
		
	return s;

fail:
	free (s);

	return NULL;
}

/*
	Creates a new comment. Return the created comment structure.

	[Params]

		comment: the comment.
		line: the line number where the comment is.
		length: length of "comment".
*/
comment_t* CF_NewComment (char* comment, unsigned int length) {
	comment_t* c;

	if ((c = (comment_t*) malloc (sizeof (comment_t))) == NULL)
		return NULL;

	if ((c->comment = (char*) malloc (length + 1)) == NULL)
		goto fail;

	strcpy (c->comment, comment);
	c->next = NULL;

	return c;

fail:
	free (c);

	return NULL;
}

void CF_FreeKeyValue (keyvalue_t* k) {
	free (k->key);
	free (k->value);
	free (k);
}

void CF_FreeKeyValues (keyvalue_t* k) {
	keyvalue_t* ak;

	while (k) {
		ak = k;
		k = k->next;
		CF_FreeKeyValue (ak);
	}
}

/*
	Frees a section.
	
	[Params]
		
		s: section to free.
*/
void CF_FreeSection (section_t* s) {
	// CF_FreeSections() can be called before any key-value pair populates the section.
	if (s->keyvalues)
		CF_FreeKeyValues (s->keyvalues);

	free (s->name);
	free (s);
}

/*
	Frees a list of sections.
	
	[Params]
	
		s: list of sections to free.
*/
void CF_FreeSections (section_t* s) {
	section_t* as;
	
	while (s) {
		as = s;
		s = s->next;
		CF_FreeSection (as);
	}
}

void CF_FreeComments (comment_t* c) {
	comment_t* ac;

	while (c) {
		ac = c;
		c = c->next;
		free (ac);
	}
}

void CF_FreeConfig (config_t* c) {
	CF_FreeSections (c->sections);
	CF_FreeComments (c->comments);
	CF_FreeIndex (c->index);
	free (c->filename);

	if (c->file)
		fclose (c->file);

	free (c);
}

/*
	Concats "src" to "dest".
	
	[Params]
	
		dest: destination. The result of concatenation place here.
		src: source. This is concatenated on "dest".
		b: index to data begining.
		e: index to data end.
		len: must be set with the length of "dest". On return will be updated.
*/
char* CF_Cat (char* dest, char* src, int b, int e, unsigned int* len) {
	unsigned int l;
	
	if (b >= 0 && e > 0 && e - b > 0) {
		l = e - b + 1;
		strncpy (dest + *len, src + b, l);
		*len += l;
		*(dest + *len) = '\0';
	}

	return dest;
}

char* CF_Copy (char* dest, char* src, int b, int e, unsigned int* len) {
	*dest = '\0';	// Initialize destination.
	*len = 0;
	/*
		Test if at least one char must be copied. "b" may be zero.
		When exist only one char to copy "e" and "b" ar equals. Then
		e-b=0.
	*/
	if (b >= 0 && e > 0 && e - b >= 0) {
		// Copy e-b chars.
		*len = e - b + 1;
		strncpy (dest, src + b, *len);
		// Don't forget null char.
		*(dest + *len) = '\0';
	}
  
	return dest;
}

comment_t* CF_AddComment (comment_t** l, comment_t* c, char* comment, unsigned int len) {
	comment_t* nc;

	if ((nc = CF_NewComment (comment, len)) == NULL)
		return NULL;

	if (c)
		c->next = nc;

	if (!*l)
		*l = nc;
	
	return nc;
}

/*
	Adds a new section to the end of section list. Return the new section.
	
	[Params]

		l: section list. If this is NULL the first section is chained with it.
		s: current section. The new section will be added next to this.
		name: name of the new section.
		line: line number into the file where the new section is.
		len: "name" length.
*/
section_t* CF_AddSection (section_t** l, section_t* s, char* name, unsigned int len) {
	section_t* ns;
	
	if ((ns = CF_NewSection (name, len)) == NULL)
		return NULL;

	/*
		The first time, the section list is emtpy that means "s" is null and
		the created section will be the first in list.
	*/
	if (s) {
		ns->prev = s;
		s->next = ns;
	};
	// Chain the first section to list.
	if (!*l)
		*l = ns;
	
	return ns;
}

index_t* CF_AddIndexEntry (index_t** index, index_t* i, int line, void* data, idxtype_t t) {
	index_t* ni;

	if ((ni = (index_t*) malloc (sizeof (index_t))) == NULL)
		return NULL;

	ni->line = line;
	ni->data = data;
	ni->type = t;
	ni->next = NULL;

	if (i)
		i->next = ni;

	if (!*index)
		*index = ni;

	return ni;
}

/*
	Adds a new key-value pair to a section. Return the new key-value pair.

	[Params]

		s: section where the new key-value pair will be added.
		k: current key-value pair. The new key-value pair will be added
			next to it.
		key: key's name.
		value: value for the new key.
		line: line number into the file where the key-value pair is.
		keylen: "key" length.
		valuelen: "value" length.
*/
keyvalue_t* CF_AddKeyValue (section_t* s, keyvalue_t* k, char* key, char* value, unsigned int keylen, unsigned int valuelen) {
	keyvalue_t* nk;
	
	if ((nk = CF_NewKeyValue (key, value, keylen, valuelen)) == NULL)
		return NULL;

	if (k) {
		k->next = nk;
		nk->prev = k;
	}
	// Chain the new key-value pair if key-value list is NULL.
	if (!s->keyvalues)
		s->keyvalues = nk;		
	
	return nk;
}

/*
	Tests if "c" is a valid common char. VALIDCOMMONCHARS contains the valid chars for
	sections, keys and values.
	
	[Params]
		
		c: char to test.
*/
bool_t CF_FindCommonChar (char c) {
	unsigned char i, s, m;
	
	i = 0;
	s = MAX_VALID_COMMON_CHAR - 1;
	while (i <= s) {
		m = (i + s) / 2;
		if (VALIDCOMMONCHARS[m] == c)
			return TRUE;
		else if (VALIDCOMMONCHARS[m] < c)
			i = m + 1;
		else
			s = m - 1;
	}
	
	return FALSE;
}

/*
	Tests if "c" is a valid value char. VALIDVALUECHARS contains the valid chars for
	values.
	
	[Params]
		
		c: char to test.
*/
bool_t CF_FindValueChar (char c) {
	unsigned char i, s, m;
	
	i = 0;
	s = MAX_VALID_VALUE_CHAR - 1;
	while (i <= s) {
		m = (i + s) / 2;
		if (VALIDVALUECHARS[m] == c)
			return TRUE;
		else if (VALIDVALUECHARS[m] < c)
			i = m + 1;
		else
			s = m - 1;
	}
	
	return FALSE;
}

/*
	Returns TRUE if "c" is a valid char for a section. Valid chars are letters, numbers and
	the underscore.
*/
bool_t CF_IsSectionChar (char c) {
	return CF_FindCommonChar (c);
}

/*
	Idem to "CF_IsSectionChar()".
*/
bool_t CF_IsKeyChar (char c) {
	return CF_FindCommonChar (c);
}

/*
	Idem to "CF_IsSectionChar()".
*/
bool_t CF_IsValueChar (char c) {
	return CF_FindCommonChar (c) || CF_FindValueChar (c);
}

/*
	Sets the key's value. The old value is freed and a new memory space is
	allocated.

	[Params]

		key: key to change its value.
		value: new value to the key.
*/
bool_t CF_SetValue (keyvalue_t* key, const char* value) {
	unsigned int len;

	free (key->value);

	len = strlen (value);
	if ((key->value = (char*) malloc (len + 1)) == NULL)
		return FALSE;

	strcpy (key->value, value);

	return TRUE;
}

/*
	Process a buffer of text searching for sections and the respective key-value pairs.
	
	[Params]
  
		b: buffer with text.
		len: buffer length (in bytes).
		pl: information pass through function calls. Must be initialized the first time.
*/
bool_t CF_ProcessLine (config_t* c, char* b, int len, processline_t* pl) {
	/*
		Every time this function is called, "index" must be set to zero because
		there is a new buffer to process.
	*/
	pl->bindex = 0;
	while (pl->bindex < len) {
		switch (b[pl->bindex]) {
			case '[':
				if (!pl->proccomment) {
					// Section name begins on the next character to '['.
					pl->begin = pl->bindex + 1;
					pl->procsection = TRUE;
				}
				break;
        
			case ']':
				if (!pl->proccomment)
					if (!pl->procsection) {
						// Section's end without respective section's begin.
						CF_WriteLog (LOGERROR, MSGSECTION, pl->line, pl->character);
						return FALSE;
					} else {
						// Mark section's end.
						pl->end = pl->bindex - 1;
						// Check empty section.
						if (pl->end < pl->begin)
							CF_WriteLog (LOGWARNING, MSGEMPTYSECTION, pl->line, pl->character);
						// If a partial section exists, concat both parts. Else just copy it.
						if (pl->psection)
							CF_Cat (pl->sectionname, b, pl->begin, pl->end, &pl->sectionlength);
						else
							CF_Copy (pl->sectionname, b, pl->begin, pl->end, &pl->sectionlength);
						// Add section to list. Update current section.
						if ((pl->currsection = CF_AddSection (&c->sections, pl->currsection, pl->sectionname, pl->sectionlength)) == NULL)
							return FALSE;
						if ((pl->currindex = CF_AddIndexEntry (&c->index, pl->currindex, pl->line, pl->currsection, IDXSECTION)) == NULL)
							return FALSE;
						// Prepare variables for a new section.
						pl->sectionlength = 0;
						*(pl->sectionname) = '\0';
						// Erase last current keyvalue.
						pl->currkeyvalue = NULL;
						pl->begin = pl->end = -1;
						pl->psection = FALSE;
						pl->procsection = FALSE;
					}
				break;
      
			case '#':
				// No comments allowed when reading a section or key.
				if (pl->procsection || pl->prockey) {
					CF_WriteLog (LOGERROR, MSGNOCOMMENT, pl->line, pl->character);
					return FALSE;
				} else if (pl->procvalue) {
					pl->end = pl->lvc;
					if (pl->pvalue)
						CF_Cat (pl->valuename, b, pl->begin, pl->end, &pl->valuelength);
					else
						CF_Copy (pl->valuename, b, pl->begin, pl->end, &pl->valuelength);
					// Again add a key-value pair.
					if ((pl->currkeyvalue = CF_AddKeyValue (pl->currsection, pl->currkeyvalue, pl->keyname, pl->valuename, pl->keylength,
							pl->valuelength)) == NULL)
						return FALSE;
					if ((pl->currindex = CF_AddIndexEntry (&c->index, pl->currindex, pl->line, pl->currkeyvalue, IDXKEY)) == NULL)
						return FALSE;
					// Again init variables.
					pl->procvalue = FALSE;
					pl->keylength = 0;
					pl->valuelength = 0;
					*(pl->keyname) = '\0';
					*(pl->valuename) = '\0';
					pl->begin = pl->end = pl->lvc = -1;
				}
				// The posible comment is now real.
				pl->begin = pl->pcb;
				// Initialize. Now comment is real.
				pl->pcb = -1;
				// Comments extend to LF.
				pl->proccomment = TRUE;
				break;
      
			case '\t':
			case ' ':
				if (!pl->proccomment) {
					// Backspace, space not allowed on sections.
					if (pl->procsection) {
						CF_WriteLog (LOGERROR, MSGINVALIDCHAR, pl->line, pl->character);
						return FALSE;
					// Tab, space allowed before or after a key.
					} else if (pl->prockey) {
						// More than one space or backspace can exist after a key.
						if (pl->end == -1) {
							// The last key character is the previous to the current char.
							pl->end = pl->bindex - 1;
							// Check if a key is in process.
							if (pl->prockey) {
								if (pl->pkey)
									CF_Cat (pl->keyname, b, pl->begin, pl->end, &pl->keylength);
								else
									CF_Copy (pl->keyname, b, pl->begin, pl->end, &pl->keylength);
								pl->pkey = FALSE;
							}
						}
					// Check if a value is in process.
					} else {
						// Mark last value character position if not marked and there is
						// a value in process.
						if (pl->procvalue && pl->lvc == -1)
							pl->lvc = pl->bindex - 1;
						// Mark posible comment begining.
						if (pl->pcb == -1)
							pl->pcb = pl->bindex;
					}
				}
				break;

			case '=':
				if (!pl->proccomment) {
					// Check if there is a section in process.
					if (pl->procsection) {
						CF_WriteLog(LOGERROR, MSGINVALIDCHAR, pl->line, pl->character);
						return FALSE;
					}          
					// Isn't valid find a "=" sign without a key that precede it.
					if (!pl->prockey) {
						CF_WriteLog (LOGERROR, MSGEMPTYKEY, pl->line, pl->character);
						return FALSE;
					}
					// If there is a "=" previously founded, the current "=" is a value char.
					if (!pl->equal) {
						/*
							If backspace/s or space/s existe after a key, the key was already
							marked (pl->end!=-1)
						*/
						if (pl->end == -1) {
							// The last key character is the previous to the current char.
							pl->end = pl->bindex - 1;
							// Check partial key.
							if (pl->pkey)
								CF_Cat (pl->keyname, b, pl->begin, pl->end, &pl->keylength);
							else
								CF_Copy (pl->keyname, b, pl->begin, pl->end, &pl->keylength);
							pl->pkey = FALSE;
							// Prepare variables for posible value.
							pl->begin = pl->end = -1;            	
						}
						// With the "=" sign, key process terminate.
						pl->prockey = FALSE;
						pl->equal = TRUE;
					}
				}
				break;

			// The line-feed char. This is the end of a line.
			case '\n':
				if (pl->proccomment) {
					pl->end = pl->bindex - 1;
					if (pl->pcomment)
						CF_Cat (pl->comment, b, pl->begin, pl->end, &pl->commentlength);
					else
						CF_Copy (pl->comment, b, pl->begin, pl->end, &pl->commentlength);
					if ((pl->currcomment = CF_AddComment (&c->comments, pl->currcomment, pl->comment, pl->commentlength)) == NULL)
						return FALSE;
					if ((pl->currindex = CF_AddIndexEntry (&c->index, pl->currindex, pl->line, pl->currcomment, IDXCOMMENT)) == NULL)
						return FALSE;
					pl->pcomment = FALSE;
					pl->commentlength = 0;
					*(pl->comment) = '\0';
					pl->begin = pl->end = pl->pcb = -1;
					// A comment finish with line-feed char too.
					pl->proccomment = FALSE;
				} else {
					if (pl->procsection || pl->prockey) {
						CF_WriteLog (LOGERROR, MSGSTRIPED, pl->line, pl->character);
						return FALSE;
					}
					// If "=" sign was found, there is a key ready to add to list.
					if (pl->equal) {
						// Mark end if there is a begin.
						if (pl->begin != -1)
							pl->end = pl->bindex - 1;
						// Check partial value.
						if (pl->pvalue)
							CF_Cat (pl->valuename, b, pl->begin, pl->end, &pl->valuelength);
						else
							CF_Copy (pl->valuename, b, pl->begin, pl->end, &pl->valuelength);
						// Add key-value pair to list. Update current key-value pair.
						if ((pl->currkeyvalue = CF_AddKeyValue (pl->currsection, pl->currkeyvalue, pl->keyname, pl->valuename, pl->keylength,
								pl->valuelength)) == NULL)
							return FALSE;
						if ((pl->currindex = CF_AddIndexEntry (&c->index, pl->currindex, pl->line, pl->currkeyvalue, IDXKEY)) == NULL)
							return FALSE;
						// Here is the end of a value.
						pl->procvalue = FALSE;
						// After the key-value pair was added, prepare varialbles.
						pl->keylength = 0;
						pl->valuelength = 0;
						*(pl->keyname) = '\0';
						*(pl->valuename) = '\0';
						pl->begin = pl->end = -1;
					}
				}
				// Indicate next line.
				pl->line++;
				// Character set to 0 not to 1 because at the end of the while statement
				// is incremented.
				pl->character = 0;
				// Reset equal indicator. A key-value pair that expand for more that one line
				// isn't valid.
				pl->equal = FALSE;
				break;
   
			default:
				if (!pl->proccomment)
					// Check partial section.
					if (pl->psection) {
						// Mark the begining of a partial section.
						if (pl->begin == -1)
							pl->begin = pl->bindex;
					// Check if exist a current section.
					} else if (pl->procsection) {
						// Test if current char is a valid section char.
						if (!CF_IsSectionChar (b[pl->bindex])) {
							CF_WriteLog (LOGERROR, MSGINVSECCHAR, pl->line, pl->character);
							return FALSE;
						}
					// If "=" sign was encountered yet, there are the begining of a value to process.
					// If not, there are the begining of a key to process.
					} else if (pl->equal) {
						// Check if current char is a valid value char.
						if (!CF_IsValueChar (b[pl->bindex])) {
							CF_WriteLog (LOGERROR, MSGINVVALCHAR, pl->line, pl->character);
							return FALSE;
						}
						// Mark the value's begining for a given key allways that didn't was done
						// before.
						if (!pl->procvalue) {
							pl->begin = pl->bindex;
							pl->procvalue = TRUE;
						}
						// A valid value char was found after a space/s or tab/s, so unmark
						// the last valid char position.
						if (pl->lvc != -1)
							pl->lvc = -1;
					// The same for the key.
					} else {
						// Check if current char is a valid key char.
						if (!CF_IsKeyChar (b[pl->bindex])) {
							CF_WriteLog (LOGERROR, MSGINVKEYCHAR, pl->line, pl->character);
							return FALSE;
						}
						// Mark the key's begining.
						if (!pl->prockey) {
							pl->begin = pl->bindex;
							pl->prockey = TRUE;
						}
					}
				break;
		};	// switch.
		pl->bindex++;
		pl->character++;
	}; // while.
  
	/*
		Set partial section, key, value indicators. The indicators are exclusive.
		If one is set the others aren't.
	*/
	pl->psection = pl->procsection && pl->begin != -1;
	pl->pkey = pl->prockey && pl->begin != -1;
	pl->pvalue = pl->procvalue && pl->begin != -1;
	pl->pcomment = pl->proccomment && pl->begin != -1;
  
	return TRUE;
}

/*
	Reads all sections with his respective key-value pairs and
	comments from a file and return a list of sections and a list
	of comments.
	Returns TRUE if the function was successful, FALSE otherwise.
	
	[Params]
	
		c: structure containing info about the configuration file.
*/
bool_t CF_ReadConfigFile (config_t* c) {
	processline_t* pl;
	char* b;
	size_t len;

	if ((b = (char*) malloc (BUFFER_SIZE)) == NULL)
		return FALSE;

	if ((pl = CF_NewProcessLine ()) == NULL)
		goto fail;

	while (!feof (c->file)) {
		len = fread (b, 1, BUFFER_SIZE, c->file);

		if (ferror (c->file))
			goto fail1;

		if (!CF_ProcessLine (c, b, len, pl))
			goto fail1;
	}

	CF_FreeProcessLine (pl);
	free (b);

	return TRUE;

fail1:
	// Free partial sections.
	CF_FreeSections (c->sections);
	CF_FreeProcessLine (pl);
fail:
	free (b);
	
	return FALSE;
}

/*
	Searchs a given key on section list on given section. Return NULL if key was not found.
	
	[Params]
	
		list: section list to search on.
		section: section where key resides.
		key: key to find.
*/
keyvalue_t* CF_SearchKey (section_t* list, const char* section, const char* key) {
	section_t* s;
	keyvalue_t* k;
	
	s = list;
	while (s) {
		// Check if that is the solicited section.
		if (strcmp (section, s->name) == 0) {
			k = s->keyvalues;
			while (k) {
				// Check if that is the key we are searching for.
				if (strcmp (key, k->key) == 0)
					return k;

				k = k->next;
			}
		}

		s = s->next;
	}
	// Return NULL if the key was not found.
	return NULL;
}

char* CF_GetSectionText (section_t* s, char* b) {
	sprintf (b, "[%s]", s->name);

	return b;
}

char* CF_GetKeyText (keyvalue_t* k, char* b) {
	sprintf (b, "%s=%s", k->key, k->value);

	return b;
}

char* CF_GetCommentText (comment_t* c, char* b) {
	sprintf (b, "%s", c->comment);

	return b;
}

char* CF_GetItemText (index_t* i, char* b) {
	switch (i->type) {
		case IDXSECTION:
			CF_GetSectionText ((section_t*) i->data, b);
			break;

		case IDXKEY:
			CF_GetKeyText ((keyvalue_t*) i->data, b);
			break;

		case IDXCOMMENT:
			CF_GetCommentText ((comment_t*) i->data, b);
			break;

		default:
			return NULL;
			break;
	}

	return b;
}

/*
	Writes two items to the configuration file.
	The first item is a section or a key-value pair.
	The second one is a comment.
	Returns TRUE if the function was succesful, FALSE otherwise.

	[Params]

		f: configuration file to write into.
		i1: section or key-value pair.
		i2: a comment.
*/
bool_t CF_WriteItems (FILE* f, index_t* i1, index_t* i2) {
	char b[MAX_FILE_LINE_LENGTH];
	char b1[MAX_FILE_LINE_LENGTH];
	char b2[MAX_FILE_LINE_LENGTH];
	unsigned int len;

	CF_GetItemText (i1, b1);
	CF_GetItemText (i2, b2);
	sprintf (b, "%s%s\n", b1, b2);
	len = strlen (b);
	if (fwrite (b, 1, len, f) != len)
		return FALSE;

	return TRUE;
}

bool_t CF_WriteItem (FILE* f, index_t* i) {
	char b[MAX_FILE_LINE_LENGTH];
	unsigned int len;

	CF_GetItemText (i, b);
	strcat (b, "\n");
	len = strlen (b);
	if (fwrite (b, 1, len, f) != len)
		return FALSE;

	return TRUE;
}

bool_t CF_WriteBlankLines (FILE* f, int count) {
	int i;

	for (i = 0; i < count; i++)
		if (fwrite ("\n", 1, 1, f) != 1)
			return FALSE;

	return TRUE;
}

/*
	Reads a configuration file with sections and key-value pairs per section. The list
	of sections is returned. NULL if no sections are available or if an error occured.
	
	[Params]
	
		name: path and name of the configuration file.
		list: section list.
*/
config_t* CF_ReadConfigFile (const char* name) {
	config_t* c;

	if ((c = CF_NewConfig (name)) == NULL)
		return NULL;
	
	if ((c->file = fopen (name, "r+")) == NULL)
		goto fail;

	// Cleanup any previous log.
	CF_CleanLog ();

	if (!CF_ReadConfigFile (c))
		goto fail1;

	return c;

fail1:
	fclose (c->file);
fail:
	CF_FreeConfig (c);

	return NULL;
}

/*
	Writes the sections, key-value pairs and comments back to the
	configuration file.

	[Params]

		config: structure representing the configuration file.
*/
bool_t CF_Write (config_t* config) {
	index_t* i, * j;

	// Para que el archivo quede truncado hay que cerrarlo y abrirlo
	// para escritura. Cruzar dedos para que no se corte la energia
	// en este momento.
	// Se podria utilizar la funcion ftruncate(), pero no se conoce
	// la longitud a la que debe truncarse el archivo.
	fclose (config->file);

	if ((config->file = fopen(config->filename, "w")) == NULL)
		return FALSE;

	i = config->index;
	while (i) {
		j = i->next;
		if (j && j->line == i->line) {
			// There is a section or key-value pair with a comment next to it.
			if (!CF_WriteItems (config->file, i, j))
				return FALSE;

			if (!CF_WriteBlankLines (config->file, j->line-i->line - 1))
				return FALSE;

			i = j->next;
		} else {
			if (!CF_WriteItem (config->file, i))
				return FALSE;
			if (j)
				if (!CF_WriteBlankLines (config->file, j->line - i->line - 1))
					return FALSE;
			i = i->next;
		}
	}

	fclose (config->file);

	return TRUE;
}

/*
	Frees any allocated data as sections, key-value pairs and log items.
*/
void CF_Free (config_t* config) {
	CF_FreeConfig (config);
	CF_CleanLog();
}

/*
	Returns a list with log items. It can be NULL.
*/
loglist_t* CF_GetLog () {
	return LogList;
}

/*
	Searchs on section list for a given key on a given section. If the key is not founded,
	return a default value.
	
	[Params]
	
		list: section list to search on.
		section: section where key resides.
		key: key to find.
		_default: default value to return if key not founded.
*/

bool_t CF_GetBool (config_t* config, const char* section, const char* key, bool_t _default) {
	keyvalue_t* k;

	// Search for the key.
	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return _default;
	// Compare uppercase string.
	if (CF_CompareString(k->value, TRUESTRING))
		return TRUE;
	else if(CF_CompareString (k->value, FALSESTRING))
		return FALSE;

	return _default;
}

int CF_GetInt (config_t* config, const char* section, const char* key, int _default) {
	keyvalue_t* k;
	int i;

	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return _default;

	if ((i = atoi (k->value)) == 0)
		return _default;

	return i;
}

char* CF_GetString (config_t* config, const char* section, const char* key, char* _default) {
	keyvalue_t* k;

	// Search for the key.
	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return _default;

	return k->value;
}

double CF_GetDouble (config_t* config, const char* section, const char* key, double _default) {
	keyvalue_t* k;
	double f;

	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return _default;

	f = atof (k->value);
	// Check for float error conversion.
	if (errno == ERANGE)
		return _default;

	return f;
}

bool_t CF_SetBool (config_t* config, const char* section, const char* key, bool_t value) {
	keyvalue_t* k;

	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return FALSE;

	if (value)
		return CF_SetValue (k, TRUESTRING);
	else
		return CF_SetValue (k, FALSESTRING);
}

bool_t CF_SetInt (config_t* config, const char* section, const char* key, int value) {
	keyvalue_t* k;
	char v[MAX_VALUE_LENGTH + 1];

	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return FALSE;

	if (sprintf (v, "%i", value) == -1)
		return FALSE;

	return CF_SetValue (k, v);
}

bool_t CF_SetString (config_t* config, const char* section, const char* key, char* value) {
	keyvalue_t* k;

	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return FALSE;

	return CF_SetValue (k, value);
}

bool_t CF_SetDouble (config_t* config, const char* section, const char* key, double value) {
	keyvalue_t* k;
	char v[MAX_VALUE_LENGTH + 1];

	if ((k = CF_SearchKey (config->sections, section, key)) == NULL)
		return FALSE;

	if (sprintf (v, "%g", value) == -1)
		return FALSE;

	return CF_SetValue (k, v);
}
