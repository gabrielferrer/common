#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include "defs.h"

typedef enum {IDXSECTION, IDXKEY, IDXCOMMENT} idxtype_t;

/*
	Represents an index node. It is useful for writing data back to the
	configuration file.
	The index is ordered by line number.
	The void pointer "data" can be a section, a key-value pair or
	a comment. It is known by the index type member.
*/
typedef struct index_s {
	int line;
	void* data;
	idxtype_t type;
	struct index_s* next;
} index_t;

/*
	Struct representing a comment into the configuration file.
	Name, line number into the file and next comment.
*/
typedef struct comment_s {
	char* comment;
	struct comment_s* next;
} comment_t;

/*
	Struct representing a key-value pair. Key name, value,
	line number into the file and previous and next key-value
	pair.
*/
typedef struct keyvalue_s {
	char* key;
	char* value;
	struct keyvalue_s* prev;
	struct keyvalue_s* next;
} keyvalue_t;

/*
	Struct representing a section. Section name, key-value
	pairs into the section, line number into the file where
	the section is and previous and next sections.
*/
typedef struct section_s {
	char* name;
	keyvalue_t* keyvalues;
	struct section_s* prev;
	struct section_s* next;
} section_t;

/*
	Struct representing a configuration file.
	File name, sections and comments.
*/
typedef struct config_s {
	FILE* file;
	char* filename;
	section_t* sections;
	// Not necesarily comments go inside sections.
	// It can appear before any section.
	comment_t* comments;
	index_t* index;
} config_t;

/*
	Struct representing a log line. Log line and next log.
*/
typedef struct loglist_s {
	char* log;
	struct loglist_s* next;
} loglist_t;

config_t* CF_ReadConfigFile (const char* name);
bool_t CF_Write (config_t* config);
void CF_Free (config_t* config);
loglist_t* CF_GetLog (void);
bool_t CF_GetBool (config_t* config, const char* section, const char* key, bool_t _default);
int CF_GetInt (config_t* config, const char* section, const char* key, int _default);
char* CF_GetString (config_t* config, const char* section, const char* key, char* _default);
double CF_GetDouble (config_t*, const char* section, const char* key, double _default);
bool_t CF_SetBool (config_t* config, const char* section, const char* key, bool_t value);
bool_t CF_SetInt (config_t* config, const char* section, const char* key, int value);
bool_t CF_SetString (config_t* config, const char* section, const char* key, char* value);
bool_t CF_SetDouble (config_t* config, const char* section, const char* key, double value);

#endif
