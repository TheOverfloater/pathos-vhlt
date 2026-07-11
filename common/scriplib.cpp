#include "cmdlib.h"
#include "filelib.h"
#include "messages.h"
#include "log.h"
#include "scriplib.h"

char            g_token[MAXTOKEN];
char            g_TXcommand;

typedef struct
{
    char            filename[_MAX_PATH];
    char*           buffer;
    char*           script_p;
    char*           end_p;
    int             line;
}
script_t;


#define	MAX_INCLUDES	8


static script_t s_scriptstack[MAX_INCLUDES];
script_t*       s_script;
int             s_scriptline;
bool            s_endofscript;
bool            s_tokenready;                                // only true if UnGetToken was just called


//  AddScriptToStack
//  LoadScriptFile
//  ParseFromMemory
//  UnGetToken
//  EndOfScript
//  GetToken
//  TokenAvailable

// =====================================================================================
//  AddScriptToStack
// =====================================================================================
static void     AddScriptToStack(const char* const filename)
{
    int             size;

    s_script++;

    if (s_script == &s_scriptstack[MAX_INCLUDES])
        Error("s_script file exceeded MAX_INCLUDES");

	strcpy_s(s_script->filename, filename);

    size = LoadFile(s_script->filename, (char**)&s_script->buffer);

    Log("Entering %s\n", s_script->filename);

    s_script->line = 1;
    s_script->script_p = s_script->buffer;
    s_script->end_p = s_script->buffer + size;
}

// =====================================================================================
//  LoadScriptFile
// =====================================================================================
void            LoadScriptFile(const char* const filename)
{
    s_script = s_scriptstack;
    AddScriptToStack(filename);

    s_endofscript = false;
    s_tokenready = false;
}

// =====================================================================================
//  ParseFromMemory
// =====================================================================================
void            ParseFromMemory(char* buffer, const int size)
{
    s_script = s_scriptstack;
    s_script++;

    if (s_script == &s_scriptstack[MAX_INCLUDES])
        Error("s_script file exceeded MAX_INCLUDES");

	strcpy_s(s_script->filename, "memory buffer");

    s_script->buffer = buffer;
    s_script->line = 1;
    s_script->script_p = s_script->buffer;
    s_script->end_p = s_script->buffer + size;

    s_endofscript = false;
    s_tokenready = false;
}

// =====================================================================================
//  UnGetToken
/*
 * Signals that the current g_token was not used, and should be reported
 * for the next GetToken.  Note that
 * 
 * GetToken (true);
 * UnGetToken ();
 * GetToken (false);
 * 
 * could cross a line boundary.
 */
// =====================================================================================
void            UnGetToken()
{
    s_tokenready = true;
}

// =====================================================================================
//  EndOfScript
// =====================================================================================
bool            EndOfScript(const bool crossline)
{
    if (!crossline)
        Error("Line %i is incomplete (did you place a \" inside an entity string?) \n", s_scriptline);

    if (!strcmp(s_script->filename, "memory buffer"))
    {
        s_endofscript = true;
        return false;
    }

    free(s_script->buffer);

    if (s_script == s_scriptstack + 1)
    {
        s_endofscript = true;
        return false;
    }

    s_script--;
    s_scriptline = s_script->line;

    Log("returning to %s\n", s_script->filename);

    return GetToken(crossline);
}

// =====================================================================================
//  GetToken
// =====================================================================================
bool            GetToken(const bool crossline)
{
    char           *token_p;

    if (s_tokenready)                                        // is a g_token allready waiting?
    {
        s_tokenready = false;
        return true;
    }

    if (s_script->script_p >= s_script->end_p)
        return EndOfScript(crossline);

    // skip space
skipspace:
	while (*s_script->script_p <= 32 && *s_script->script_p >= 0)
    {
        if (s_script->script_p >= s_script->end_p)
            return EndOfScript(crossline);

        if (*s_script->script_p++ == '\n')
        {
            if (!crossline)
                Error("Line %i is incomplete (did you place a \" inside an entity string?) \n", s_scriptline);
            s_scriptline = s_script->line++;
        }
    }

    if (s_script->script_p >= s_script->end_p)
        return EndOfScript(crossline);

    // comment fields
    if (*s_script->script_p == ';' || *s_script->script_p == '#' || // semicolon and # is comment field
        (*s_script->script_p == '/' && *((s_script->script_p) + 1) == '/')) // also make // a comment field
    {
        if (!crossline)
            Error("Line %i is incomplete (did you place a \" inside an entity string?) \n", s_scriptline);

        //ets+++
        if (*s_script->script_p == '/')
            s_script->script_p++;
        if (s_script->script_p[1] == 'T' && s_script->script_p[2] == 'X')
            g_TXcommand = s_script->script_p[3];             // AR: "//TX#"-style comment

        //ets---
        while (*s_script->script_p++ != '\n')
        {
            if (s_script->script_p >= s_script->end_p)
                return EndOfScript(crossline);
        }
        //ets+++
        s_scriptline = s_script->line++;                       // AR: this line was missing
        //ets---
        goto skipspace;
    }

    // copy g_token
    token_p = g_token;

    if (*s_script->script_p == '"')
    {
        // quoted token
        s_script->script_p++;
        while (*s_script->script_p != '"')
        {
            *token_p++ = *s_script->script_p++;

            if (s_script->script_p == s_script->end_p)
                break;

            if (token_p == &g_token[MAXTOKEN])
                Error("Token too large on line %i\n", s_scriptline);
        }
        s_script->script_p++;
    }
    else
    {
        // regular token
		while ((*s_script->script_p > 32 || *s_script->script_p < 0) && *s_script->script_p != ';')
        {
            *token_p++ = *s_script->script_p++;

            if (s_script->script_p == s_script->end_p)
                break;

            if (token_p == &g_token[MAXTOKEN])
                Error("Token too large on line %i\n", s_scriptline);
        }
    }

    *token_p = 0;

    if (!strcmp(g_token, "$include"))
    {
        GetToken(false);
        AddScriptToStack(g_token);
        return GetToken(crossline);
    }

    return true;
}

#if 0
// AJM: THIS IS REDUNDANT
// =====================================================================================
//  ParseWadToken
//      basically the same as gettoken, except it isnt limited by MAXTOKEN and is
//      specificaly designed to parse out the wadpaths from the wad keyvalue and dump
//      them into the wadpaths list
//      this was implemented as a hack workaround for Token Too Large errors caused by
//      having long wadpaths or lots of wads in the map editor.
extern void        PushWadPath(const char* const path, bool inuse);
// =====================================================================================
void            ParseWadToken(const bool crossline)
{
    // code somewhat copied from GetToken()
    int             i, j;
    char*           token_p;
    char            temp[_MAX_PATH];

    if (s_script->script_p >= s_script->end_p)
        return;

    // skip space
    while (*s_script->script_p <= 32)
    {
        if (s_script->script_p >= s_script->end_p)
            return;

        if (*s_script->script_p++ == '\n')
        {
            if (!crossline)
                Error("Line %i is incomplete (did you place a \" inside an entity string?) \n", s_scriptline);
            s_scriptline = s_script->line++;
        }
    }

    // EXPECT A QUOTE
    if (*s_script->script_p++ != '"')
        Error("Line %i: Expected a wadpaths definition, got '%s'\n", s_scriptline, *--s_script->script_p);

    // load wadpaths manually
    bool    endoftoken = false;
    for (i = 0; !endoftoken; i++)
    {
        // get the path
        for (j = 0; ; j++)
        {
            token_p = ++s_script->script_p;
            
            // assert max path length
            if (j > _MAX_PATH)
                Error("Line %i: Wadpath definition %i is too long (%s)\n", s_scriptline, temp);

            if (*token_p == '\n')
                Error("Line %i: Expected a wadpaths definition, got linebreak\n", s_scriptline);

            if (*token_p == '"')            // end of wadpath definition
            {
                if (i == 0 && j == 0)       // no wadpaths!
                {
                    Warning("No wadpaths specified.\n");
                    return;
                }

                endoftoken = true;
                break;
            }

            if (*token_p == ';')            // end of this wadpath
                break;

            temp[j] = *token_p;
            temp[j + 1] = 0;
        }

        // push it into the list
        PushWadPath(temp, true);
        temp[0] = 0;
    }

    for (; *s_script->script_p != '\n'; s_script->script_p++)
    {
    }
}
#endif

// =====================================================================================
//  TokenAvailable
//      returns true if there is another token on the line
// =====================================================================================
bool            TokenAvailable()
{
    char           *search_p;

    search_p = s_script->script_p;

    if (search_p >= s_script->end_p)
        return false;

    while (*search_p <= 32)
    {
        if (*search_p == '\n')
            return false;

        search_p++;

        if (search_p == s_script->end_p)
            return false;
    }

    if (*search_p == ';')
        return false;

    return true;
}

//=============================================
// @brief Extracts the filename from a path string
//
// @param pstrin Input string's pointer
// @param pstrout Output string's pointer
//=============================================
void COM_Basename( const char *pstrin, char *pstrout )
{
	int lastdot = 0;
	int lastbar = 0;
	int pathlength = 0;

	for(int i = 0; i < strlen(pstrin); i++)
	{
		if(pstrin[i] == '/' || pstrin[i] == '\\')
			lastbar = i+1;

		if( pstrin[i] == '.' )
			lastdot = i;
	}

	if(!lastdot)
		lastdot = (int)strlen(pstrin);

	for(int i = lastbar; i < strlen(pstrin); i++)
	{
		if(i == lastdot)
			break;

		pstrout[pathlength] = pstrin[i];
		pathlength++;
	}

	pstrout[pathlength] = 0;
}

//=============================================
// @brief Tells if a character is a break character
//
// @param character Character to check
// @param pbreakchars Array of break characters
// @return TRUE if break character, FALSE otherwise
//=============================================
bool COM_IsBreakCharacter( char character, const char* pbreakchars )
{
	if(!pbreakchars)
		return false;

	const char* pstr = pbreakchars;
	while((*pstr))
	{
		if(character == (*pstr))
			return true;

		pstr++;
	}

	return false;
}

//=============================================
// @brief Parses a token from an input string into another string
//
// @param pstr String to parse
// @param pdest Destination character array
// @param pstring Output string
// @param pbreakchars Special characters to break on
// @param ignoreComma Tells if commas should be ignored
// @param checkCurlyBrackets If true, curly brackets are treated like quotes
// @return Rest of the string or null if reached end
//=============================================
const char* COM_Parse( const char *pstr, char* pdest, const char* pbreakchars, bool ignoreComma, bool checkCurlyBrackets )
{
	bool includeSpaces = false;
	int strLength = 0;
	const char* ppstr = pstr;

	// skip whitespaces
	while(*ppstr && isspace(*ppstr) && !COM_IsBreakCharacter((*ppstr), pbreakchars))
		ppstr++;

	while(*ppstr)
	{
		if(*ppstr == '/' && *(ppstr+1) == '/')
		{
			while(*ppstr != '\0' && *ppstr != '\n')
				ppstr++;

			if(*ppstr == '\0')
				break;

			ppstr++;
		}

		if(*ppstr == '/' && *(ppstr+1) == '*')
		{
			while(*ppstr != '*' && *(ppstr+1) != '/')
				ppstr++;

			ppstr += 2;
		}

		if(!includeSpaces && isspace(*ppstr) && !COM_IsBreakCharacter((*ppstr), pbreakchars))
			break;

		if(COM_IsBreakCharacter((*ppstr), pbreakchars))
			break;

		if(*ppstr != '\"')
		{
			if(strLength == MAX_PARSE_LENGTH)
			{
				// Fail if we reached the limit
				pdest[0] = '\0';
				return nullptr;
			}

			pdest[strLength] = *ppstr;
			strLength++; 
		}

		if(*ppstr == '\"' || checkCurlyBrackets && (*ppstr == '(' || *ppstr == ')'))
		{
			if(!includeSpaces)
				includeSpaces = true;
			else if(*ppstr == '\"')
			{
				ppstr++;
				break;
			}
			else
				includeSpaces = false;
		}

		if(!includeSpaces && *ppstr == ',' && !ignoreComma)
			break;

		ppstr++;
	}

	pdest[strLength] = '\0';

	// skip whitespaces
	while(*ppstr && isspace(*ppstr) && !COM_IsBreakCharacter((*ppstr), pbreakchars))
		ppstr++;

	if(*ppstr == '\0')
		ppstr =  nullptr;

	return ppstr;
}

//=============================================
// @brief Tells if a string represents an integer number
//
// @param pstr Input string's pointer
//=============================================
bool COM_IsNumber( const char *pstr )
{
	const char* ppstr = pstr;
	while(*ppstr)
	{
		if(!isdigit(*ppstr) && *ppstr != '.')
			return false;

		ppstr++;
	}

	return true;
}

//=============================================
// @brief Parses an entire line from an input string into another string
//
// @param pstr String to parse
// @param pdest Destination string object
//=============================================
const char* COM_ReadLine( const char* pstr, char* pdest )
{
	char* ppdest = pdest;
	const char* ppstr = pstr;

	while(*ppstr && *ppstr != '\n' && *ppstr != '\r')
	{
		if((ppdest - pdest) >= MAX_LINE_LENGTH)
		{
			pdest[0] = '\0';
			return nullptr;
		}

		*ppdest = *ppstr;
		ppdest++; ppstr++;
	}

	*ppdest = '\0';

	// skip whitespaces
	while(*ppstr && isspace(*ppstr) && *ppstr != '\n' && *ppstr != '\r')
		ppstr++;
	
	// Skip newline characters
	if(ppstr[0] == '\r' && ppstr[1] == '\n')
		ppstr += 2;
	else if(ppstr[0] == '\n')
		ppstr++;

	if(*ppstr == '\0')
		ppstr =  nullptr;

	return ppstr;
}