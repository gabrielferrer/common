/*
	File: log.h
	Creation: 21-11-2007
	Programming: Gabriel Ferrer
	Description:
		
		Log file definitions.
*/

#ifndef LOG_H
#define LOG_H

#include "defs.h"

	enum LOGKIND {LOGERROR, LOGWARNING, LOGINFORMATION};

	void LOG_WriteLog (enum LOGKIND kind, const char* msg, ...);
	BOOL LOG_OpenLogFile (char* filename);
	void LOG_CloseLogFile ();

#endif
