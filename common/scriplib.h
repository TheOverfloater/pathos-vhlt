#ifndef SCRIPLIB_H__
#define SCRIPLIB_H__

#if _MSC_VER >= 1000
#pragma once
#endif

#include "cmdlib.h"

#define	MAXTOKEN 4096

// Maximum line length
static const int MAX_LINE_LENGTH = 4096;
// Maximum parse length
static const int MAX_PARSE_LENGTH = 256;

extern char     g_token[MAXTOKEN];
extern char     g_TXcommand;                               // global for Quark maps texture alignment hack

extern void     LoadScriptFile(const char* const filename);
extern void     ParseFromMemory(char* buffer, int size);

extern bool     GetToken(bool crossline);
extern void     UnGetToken();
extern bool     TokenAvailable();

extern void		COM_Basename( const char *pstrin, char *pstrout );
extern const char* COM_Parse( const char *pstr, char* pdest, const char* pbreakchars = nullptr, bool ignoreComma = false, bool checkCurlyBrackets = false );
extern const char* COM_ReadLine( const char* pstr, char* pdest );
extern bool		COM_IsNumber( const char *pstr );

#define MAX_WAD_PATHS   42
extern char         g_szWadPaths[MAX_WAD_PATHS][_MAX_PATH];
extern int          g_iNumWadPaths;

#endif //**/ SCRIPLIB_H__
