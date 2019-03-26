/*
	file: sys.c
	programming: Gabriel Ferrer
	date: 16-08-2012
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include "sys.h"

#define BUFFER_SIZE 2048

char g_buffer[BUFFER_SIZE];

/*
	Causes an abnormal program termination.
	Shows a message before terminating.
*/
void SYS_Exit (char* funcname, char* message) {
	SYS_Print (funcname, message);
	exit (EXIT_FAILURE);
}

void SYS_ExitF (char* funcname, char* message, ...) {
	va_list vl;

	va_start (vl, message);
	SYS_VPrintFR (funcname, message, g_buffer, vl);
	va_end (vl);
	exit (EXIT_FAILURE);;
}

/*
	Causes an abnormal program termination.
	Shows a formated with parameters message before terminating.
*/
void SYS_ExitFR (char* funcname, char* message, char* buf, ...) {
	va_list vl;

	va_start (vl, buf);
	SYS_VPrintFR (funcname, message, buf, vl);
	va_end (vl);
	exit (EXIT_FAILURE);
}

void SYS_ExitE (char* funcname, int errcode) {
	SYS_ExitER (funcname, errcode, g_buffer, BUFFER_SIZE);
}

void SYS_ExitER (char* funcname, int errcode, char* buf, size_t n) {
	SYS_PrintER (funcname, errcode, buf, n);
	exit (EXIT_FAILURE);
}

void SYS_Print (char* funcname, char* message) {
	fprintf (stderr, "%s(): %s\n", funcname, message);
}

void SYS_PrintE (char* funcname, int errcode) {
	SYS_PrintER (funcname, errcode, g_buffer, BUFFER_SIZE);
}

void SYS_PrintER (char* funcname, int errcode, char* buf, size_t n) {
// MinGW don't define strerror_r.
#if defined _WIN32 || defined __CYGWIN__
	strerror (errcode);
#else
	strerror_r (errcode, buf, n);
#endif
	SYS_Print (funcname, buf);
}

void SYS_PrintF (char* funcname, char* message, ...) {
	va_list vl;

	va_start (vl, message);
	SYS_VPrintFR (funcname, message, g_buffer, vl);
	va_end (vl);
}

void SYS_PrintFR (char* funcname, char* message, char* buf, ...) {
	va_list vl;

	va_start (vl, buf);
	SYS_VPrintFR (funcname, message, buf, vl);
	va_end (vl);
}

void SYS_VPrintF (char* funcname, char* message, va_list vl) {
	SYS_VPrintFR (funcname, message, g_buffer, vl);
}

void SYS_VPrintFR (char* funcname, char* message, char* buf, va_list vl) {
	vsprintf (buf, message, vl);
	SYS_Print (funcname, buf);
}

