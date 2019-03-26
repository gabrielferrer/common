/*
	File: log.c
	Creation: 21-11-2007
	Programming: Gabriel Ferrer
	Description:
		
		Open a log file and manages the post of message logs to that file.
*/

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include "defs.h"
#include "logfile.h"

#define MAX_NUMBER_TEXT_LENGTH 10
#define MAX_FORMATED_TEXT_LENGTH 512
#define MAX_TEMP_TEXT_LENGTH 640

typedef struct logfile_s {
	FILE* file;
	char* num;
	char* formated;
	char* temp;
} logfile_t;

/* Only one logfile allowed at a time. */
struct LOGFILE* LogFile = NULL;

/*
	Creates and initialize a log-file structure and return it.
*/
logfile_t* LOG_NewLogFile () {
	logfile_t* lf;

	if ((lf = (struct LOGFILE*) malloc (sizeof (logfile_t))) == NULL)
		return NULL;

	if ((lf->num = (char*) malloc (MAX_NUMBER_TEXT_LENGTH + 1)) == NULL)
		goto fail;

	if ((lf->formated = (char*) malloc (MAX_FORMATED_TEXT_LENGTH + 1)) == NULL)
		goto fail1;

	if ((lf->temp = (char*) malloc (MAX_TEMP_TEXT_LENGTH + 1)) == NULL)
		goto fail2;

	lf->file = NULL;
	memset (lf->num, 0, MAX_NUMBER_TEXT_LENGTH + 1);
	memset (lf->formated, 0, MAX_FORMATED_TEXT_LENGTH + 1);
	memset (lf->temp, 0, MAX_TEMP_TEXT_LENGTH + 1);

	return lf;
		
fail2:
	free (lf->formated);
fail1:
	free (lf->num);
fail:
	free (lf);

	return NULL;
}

/*
	Frees a LOGFILE structure.
*/
void LOG_FreeLogFile (logfile_t* lf) {
	free (lf->num);
	free (lf->formated);
	free (lf->temp);
	free (lf);
}

/*
	Puts the date part into "dest". If the day or month occupy only one char they are filled
	with a leading zero.
	
	[Params]
	
		dest: destination. The date is put here.
		tm: system date-time structure.
*/
void LOG_PutDate (char* dest, struct tm* lt) { 
	strcat (dest, "[");

	if (lt->tm_mday < 10)
		strcat(dest, "0");

	strcat (strcat (dest, ultoa (lt->tm_mday, LogFile->num, 10)), "/");

	if (lt->tm_mon < 10)
		strcat(dest, "0");

	strcat (strcat (dest, ultoa (lt->tm_mon + 1, LogFile->num, 10)), "/");
	strcat (dest, ultoa (lt->tm_year + 1900, LogFile->num, 10));
	strcat (dest, "]");
}

/*
	Idem to "LOG_PutDate()" but with the time part.
*/
void LOG_PutTime (char* dest, struct tm* lt) {
	strcat	(dest, "[");

	if (lt->tm_hour < 10)
		strcat (dest, "0");

	strcat (strcat (dest, ultoa(lt->tm_hour, LogFile->num, 10)), ":");

	if (lt->tm_min < 10)
		strcat (dest, "0");

	strcat (strcat (dest, ultoa (lt->tm_min, LogFile->num, 10)), ":");

	if (lt->tm_sec < 10)
		strcat (dest, "0");

	strcat (dest, ultoa(lt->tm_sec, LogFile->num, 10));
	strcat (dest, "]");
}

/*
	Logs the kind of the message. The kind may be ERROR, WARNING or INFORMATION.
*/
void LOG_WriteLogKind (char* dest, enum LOGKIND kind) {
	switch (kind) {
		case LOGERROR:
			strcat (dest, "[ERROR]");
			break;
		
		case LOGWARNING:
			strcat (dest, "[WARNING]");
			break;
			
		case LOGINFORMATION:
			strcat (dest, "[INFORMATION]");
			break;
	}
}

void LOG_WriteLogAuxHead (char* dest, const char* msg) {
	time_t t;
	struct tm* lt;

	*dest = '\0';
	time (&t);
	lt = localtime (&t);
	LOG_PutDate (dest, lt);
	LOG_PutTime (dest, lt);
	strcat (strcat (strcat (dest, "["), msg), "]");
}

void LOG_WriteLogAuxTail (char* dest) {
	int len;

	len = strlen (dest);
	*(dest + len) = 0xA;
	fwrite (dest, len + 1, 1, LogFile->file);
}

/*
  Formats 'msg' with the arguments passed as parameters and writes it to the log's file.
*/
void LOG_WriteLog (enum LOGKIND kind, const char* msg, ...) {
	va_list vl;

	va_start (vl, msg);
	vsprintf (LogFile->formated, msg, vl);
	va_end (vl);
	LOG_WriteLogKind (LogFile->temp, kind);
	LOG_WriteLogAuxHead (LogFile->temp, LogFile->formated);
	LOG_WriteLogAuxTail (LogFile->temp);
}

/*
	Opens a log file "filename" in append mode. Only one log file is allowed at a time.
	Returns "TRUE" if it was successfull else returns "FALSE".
	
	[Params]
	
		filename: file name to log.
*/
BOOL LOG_OpenLogFile (char* filename) {
	/* File must be closed before re-open it. */
	if (LogFile)
		return FALSE;		
	/* Allocate LOGFILE structure. */
	if ((LogFile = LOG_NewLogFile ()) == NULL)
		return FALSE;
	/* Open log file for append. */
	if ((LogFile->file = fopen (filename, "a")) == NULL)
		goto fail;

	return TRUE;
	
fail:
	LOG_FreeLogFile (LogFile);

	return FALSE;
}

/*
	Frees memory used for "LogFile". And ensures "LogFile" is NULL.
*/
void LOG_CloseLogFile () {
	LOG_FreeLog_File (LogFile);
	LogFile = NULL;
}
