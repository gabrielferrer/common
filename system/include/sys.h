/*
	file: sys.h
	programming: Gabriel Ferrer
	date: 16-08-2012
*/

#ifndef SYS_H
#define SYS_H

#include <stdarg.h>

void SYS_Exit (char* funcname, char* message);
void SYS_ExitF (char* funcname, char* message, ...);
void SYS_ExitFR (char* funcname, char* message, char* buf, ...);
void SYS_ExitE (char* funcname, int errcode);
void SYS_ExitER (char* funcname, int errcode, char* buf, size_t n);
void SYS_Print (char* funcname, char* message);
void SYS_PrintE (char* funcname, int errcode);
void SYS_PrintER (char* funcname, int errcode, char* buf, size_t n);
void SYS_PrintF (char* funcname, char* message, ...);
void SYS_PrintFR (char* funcname, char* message, char* buf, ...);
void SYS_VPrintF (char* funcname, char* message, va_list vl);
void SYS_VPrintFR (char* funcname, char* message, char* buf, va_list vl);

#endif
