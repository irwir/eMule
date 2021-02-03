%{
#include "stdafx.h"
#include "resource.h"
#include <io.h>
#include "OtherFunctions.h"
#include "SearchExpr.h"
#include "scanner.h"
#include "parser.hpp"
#include "ED2KLink.h"
#include "StringConversion.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	ECHO					TRACE
#define	YY_INPUT				ReadLexBuff
#define	YY_FATAL_ERROR			FatalLexError

extern int yyerror(LPCTSTR errstr);
extern int yyerrorf(LPCTSTR errstr, ...);

static void ReadLexBuff(char *pcBuff, int &riResult, size_t uMaxSize);
static void FatalLexError(yyconst char msg[]);
int opt_strnicmp(const char *pszString, const char *pszMatch, size_t nMinMatch);

static CStringA s_strInputBuff;
static LPCSTR s_pszLexBuff;
static bool s_bKeepQuotedStrings;

#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant
#pragma warning(disable:4242) // conversion from <type1> to <type1>, possible loss of data
#pragma warning(disable:4244) // conversion from <type1> to <type2>, possible loss of data
#pragma warning(disable:4505) // unreferenced local function has been removed
%}

/* flex 2.5.35: There is a bug in the 'noyywrap' option which creates a wrong 'yywrap' macro.
 *              Need to use an explicit 'yywrap' function to get the same functionality.
 */
/*%option noyywrap*/

/* we are using the REJECT macro */
%option reject

/* do not create an interactive scanner */
%option nointeractive

/* do not create calls to 'isatty' to determine if the input file is 'interactive' */
%option never-interactive

/* generate an 8-bit scanner */
%option 8bit

/* suppress inclusion of the non-ANSI header file "unistd.h" */
%option nounistd


/* Reject some special characters for 'keywords'
 *
 * SPACE	obvious
 * ( and )	obvious
 * "		quoted string
 * <		comparison operator
 * >		comparison operator
 * =		comparison operator
 *
 * The '@' character does not need to be rejected because:
 *	- it is allowed only at the start of a meta-id (e.g. @size)
 *  - all terminals which are allowed to precede a meta-id and which are allowed
 *    to follow a meta-id are already rejected (e.g. (a)@size>1). This way we can
 *    still allow keywords which include a '@' character (e.g. fr@nk)
 */
keywordchar		[^ \"()<>=]
digit			[0-9]
num1			{digit}+\.?([eE][-+]?{digit}+)?
num2			{digit}*\.{digit}+([eE][-+]?{digit}+)?
number			{num1}|{num2}

%x NUMBER
%x SIZE
%x TYPE
%x STRING
%x LENGTH

%%

[ ]				{ /* Skip blanks. */ }
"OR"			{ return TOK_OR; }
"AND"			{ return TOK_AND; }
"NOT"			{ return TOK_NOT; }

"@"[a-z]+		{
					if (opt_strnicmp(yytext+1, "size", 3) == 0) {
						BEGIN(SIZE);
						return TOK_SIZE;
					}
					else if (opt_strnicmp(yytext+1, "type", 3) == 0) {
						BEGIN(TYPE);
						return TOK_TYPE;
					}
					else if (opt_strnicmp(yytext+1, "ext", 3) == 0) {
						BEGIN(STRING);
						return TOK_EXT;
					}
					else if (opt_strnicmp(yytext+1, "availability", 3) == 0 ||
						     opt_strnicmp(yytext+1, "sources", 3) == 0) {
						BEGIN(NUMBER);
						return TOK_SOURCES;
					}
					else if (opt_strnicmp(yytext+1, "complete", 3) == 0) {
						BEGIN(NUMBER);
						return TOK_COMPLETE;
					}
					else if (opt_strnicmp(yytext+1, "bitrate", 3) == 0) {
						BEGIN(NUMBER);
						return TOK_BITRATE;
					}
					else if (opt_strnicmp(yytext+1, "length", 3) == 0) {
						BEGIN(LENGTH);
						return TOK_LENGTH;
					}
					else if (opt_strnicmp(yytext+1, "codec", 3) == 0) {
						BEGIN(STRING);
						return TOK_CODEC;
					}
					else if (opt_strnicmp(yytext+1, "rating", 3) == 0) {
						BEGIN(NUMBER);
						return TOK_RATING;
					}
					else if (opt_strnicmp(yytext+1, "title", 3) == 0) {
						BEGIN(STRING);
						return TOK_TITLE;
					}
					else if (opt_strnicmp(yytext+1, "album", 3) == 0) {
						BEGIN(STRING);
						return TOK_ALBUM;
					}
					else if (opt_strnicmp(yytext+1, "artist", 3) == 0) {
						BEGIN(STRING);
						return TOK_ARTIST;
					}
					yyerrorf(GetResString(IDS_SEARCH_UNKATTR), (LPCTSTR)OptUtf8ToStr(yytext));
					yyterminate();
					/*NOT REACHED*/
				}

<INITIAL,NUMBER,SIZE,TYPE,STRING,LENGTH>"="	{ return TOK_OPR_EQ; }
<NUMBER,SIZE,LENGTH>">"			{ return TOK_OPR_GT; }
<NUMBER,SIZE,LENGTH>"<"			{ return TOK_OPR_LT; }
<NUMBER,SIZE,LENGTH>">="		{ return TOK_OPR_GE; }
<NUMBER,SIZE,LENGTH>"<="		{ return TOK_OPR_LE; }
<NUMBER,SIZE,LENGTH>"<>"		{ return TOK_OPR_NE; }

"ed2k://|file|"[^|]+"|"[0-9]+"|"[a-fA-F0-9]{32}"|/"	{
	//
	// NOTE: There is no need to explicitly handle the
	//
	//	"ed2k::"[a-fA-F0-9]{32}
	//
	// pattern with a separate token. It would be even wrong! "ed2k::<hash>" is to be handled
	// as any other string term. It even can get combined with other search terms and operators
	// within one search tree.
					CString strError;
					try
					{
						CED2KLink *pLink = CED2KLink::CreateLinkFromUrl(CString(yytext));
						if (pLink && pLink->GetKind() == CED2KLink::kFile)
						{
							CED2KFileLink *pFileLink = pLink->GetFileLink();
							if (pFileLink)
							{
								yylval.pstr = new CStringA;
								yylval.pstr->Format("ed2k::%s", md4strA(pFileLink->GetHashKey()));
								delete pLink;
								return TOK_ED2K_LINK;
							}
						}
						delete pLink;
					}
					catch(const CString &strEd2kLinkError)
					{
						strError = strEd2kLinkError;
					}
					if (strError.IsEmpty())
						strError = GetResString(IDS_ERR_BADED2KLINK);
					yyerror(strError);
					yyterminate();
					/*NOT REACHED*/
				}

<NUMBER>{number}[kmg]? {
					BEGIN(INITIAL);
					char *endptr = NULL;
					double dbl = strtod(yytext, &endptr);
					if (endptr) {
						if (*endptr == 'k')
							dbl *= 1000;
						else if (*endptr == 'm')
							dbl *= 1000*1000;
						else if (*endptr == 'g')
							dbl *= 1000*1000*1000;
					}
					yylval.num = (uint64)(dbl + 0.5);
					return TOK_NUMBER;
				}

<SIZE>{number}[bkmgBKMG]? {
					BEGIN(INITIAL);
					char *endptr = NULL;
					double dbl = strtod(yytext, &endptr);
					if (endptr) {
						if (*endptr == 'B' || *endptr == 'b')
							;
						else if (*endptr == 'K' || *endptr == 'k')
							dbl *= 1024;
						else if (*endptr == 'M' || *endptr == 'm')
							dbl *= 1024*1024;
						else if (*endptr == 'G' || *endptr == 'g')
							dbl *= 1024*1024*1024;
						else
							dbl *= 1024*1024;
					}
					else {
						dbl *= 1024*1024;
					}
					yylval.num = (uint64)(dbl + 0.5);
					return TOK_NUMBER;
				}

<LENGTH>{number}[smh]? {
					BEGIN(INITIAL);
					char *endptr = NULL;
					double dbl = strtod(yytext, &endptr);
					if (endptr) {
						if (*endptr == 's')
							;
						else if (*endptr == 'm')
							dbl *= 60;
						else if (*endptr == 'h')
							dbl *= 60*60;
					}
					yylval.num = (uint64)(dbl + 0.5);
					return TOK_NUMBER;
				}

<LENGTH>{digit}+":"{digit}+ {
					BEGIN(INITIAL);
					UINT m, s;
					if (sscanf(yytext, "%u:%u", &m, &s) != 2) {
						yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@length"));
						yyterminate();
						/*NOT REACHED*/
					}
					yylval.num = s + m*60;
					return TOK_NUMBER;
				}

<LENGTH>{digit}+":"{digit}+":"{digit}+ {
					BEGIN(INITIAL);
					UINT h, m, s;
					if (sscanf(yytext, "%u:%u:%u", &h, &m, &s) != 3) {
						yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@length"));
						yyterminate();
						/*NOT REACHED*/
					}
					yylval.num = s + m*60 + h*60*60;
					return TOK_NUMBER;
				}

<TYPE>[a-zA-Z]+	{
					BEGIN(INITIAL);
					if (opt_strnicmp(yytext, "audio", 3) == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_AUDIO);
						return TOK_TYPEVAL;
					}
					else if (opt_strnicmp(yytext, "video", 3) == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_VIDEO);
						return TOK_TYPEVAL;
					}
					else if (opt_strnicmp(yytext, "image", 3) == 0 || _stricmp(yytext, "img") == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_IMAGE);
						return TOK_TYPEVAL;
					}
					else if (opt_strnicmp(yytext, "document", 3) == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_DOCUMENT);
						return TOK_TYPEVAL;
					}
					else if (opt_strnicmp(yytext, "program", 3) == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_PROGRAM);
						return TOK_TYPEVAL;
					}
					else if (opt_strnicmp(yytext, "archive", 3) == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_ARCHIVE);
						return TOK_TYPEVAL;
					}
					else if (_stricmp(yytext, "iso") == 0 || _stricmp(yytext, "cd") == 0)
					{
						yylval.pstr = new CStringA(ED2KFTSTR_CDIMAGE);
						return TOK_TYPEVAL;
					}
					yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@type"));
					yyterminate();
					/*NOT REACHED*/
				}

-				{ return TOK_NOT; }

<INITIAL,STRING>{keywordchar}*	{
					if (YYSTATE == INITIAL)
					{
					    /* Strings prefixed with '-' are to be handled with a different rule, reject it and let
					     * the scanner find the next best rule. */
					    if (yytext[0] == '-') {
						    REJECT;
						    /*NOT REACHED*/
					    }

					    /* Strings prefixed with '@' are treated as 'invalid' meta-tag id. Otherwise they would be
					     * silently used as string terms -> a typo would not be noticed by the user */
					    if (yytext[0] == '@') {
							yyerrorf(GetResString(IDS_SEARCH_UNKATTR), OptUtf8ToStr(yytext));
						    yyterminate();
						    /*NOT REACHED*/
					    }
					}
					else if (YYSTATE == STRING)
						BEGIN(INITIAL);
					yylval.pstr = new CStringA(yytext);
					return TOK_STRING;
                }

<INITIAL,STRING>"\""	{
					int l = 128;
					char *psz = (char*)malloc(l);
					if (psz == NULL) {
						yyerror(_T("Less memory for string"));
						yyterminate();
						/*NOT REACHED*/
					}
					int i = 0;
					int c;
					while ((c = yyinput()) != '\"')
					{
						if (c == EOF || c == '\n'){
							unput(c);
							yyerror(GetResString(IDS_SEARCH_UNTERMSTRING));
							yyterminate();
							/*NOT REACHED*/
							break;
						}
						if (c == '\\'){		/*Escape sequence*/
							switch (c = yyinput())
							{
							case '\n':
								continue;
							case 't':		/*Tab*/
								c = '\t';
								break;
							case 'n':		/*Linefeed*/
								c = '\n';
								break;
							case 'f':		/*Formfeed*/
								c = '\f';
								break;
							case 'r':		/*Carriage return*/
								c = '\r';
								break;
							case '\\':		/*Backslash*/
								c = '\\';
								break;
							case '"':		/*Double quotation mark*/
								c = '\"';
								break;
							case '\'':		/*Single quotation mark*/
								c = '\'';
								break;
							case '?':		/*Question mark*/
								c = '\?';
								break;
							case 'v':		/*Vertical Tab*/
								c = '\v';
								break;
							case 'a':		/*Alert*/
								c = '\a';
								break;
							case 'b':		/*Backspace*/
								c = '\b';
								break;
							case 'x':		/*Hexadecimal number*/
								{
									int n, octv;
									for (n = 1, octv = 0; n <= 3; ++n) {
										if ((c = yyinput()) >= '0' && c <= '9')
											c -= '0';
										else if (c >= 'a' && c <= 'f')
											c = (c - 'a') + 10;
										else if (c >= 'A' && c <= 'F')
											c = (c - 'A') + 10;
										else
											break;
										octv = octv * 16 + c;
									}
									unput(c);
									if (n == 1)
										c = 'x';
									else
										c = octv;
								}
								break;
							}
						}
#ifndef _UNICODE
						else{
							if ((unsigned char)c >= 0x80 && IsDBCSLeadByte(yytext[0])){
								psz[i++] = (unsigned char)c;
								if (i >= l){
									char *pszNew = (char*)realloc(psz, l += 128);
									if (pszNew == NULL){
										free(psz);
										yyerror(_T("Less memory for string"));
										yyterminate();
										/*NOT REACHED*/
										break;
									}
									psz = pszNew;
								}
								c = yyinput();
							}
						}
#endif

						psz[i++] = (unsigned char)c;
						if (i >= l){
							char *pszNew = (char*)realloc(psz, l += 128);
							if (pszNew == NULL){
								free(psz);
								yyerror(_T("Less memory for string"));
								yyterminate();
								/*NOT REACHED*/
								break;
							}
							psz = pszNew;
						}
					}
					psz[i] = '\0';

					if (s_bKeepQuotedStrings && YYSTATE != STRING) {
						CStringA quoted;
						quoted = '\"';
						quoted += psz;
						quoted += '\"';
						yylval.pstr = new CStringA(quoted);
					}
					else
						yylval.pstr = new CStringA(psz);
					free(psz);

					BEGIN(INITIAL);

					// Do not return empty strings, and also do not return quoted empty strings
					if (!yylval.pstr->IsEmpty() && *yylval.pstr != "\"\"")
						return TOK_STRING;

					delete yylval.pstr;
				}

.				{ return yytext[0]; }

<<EOF>>			{ return TOK_EOF; }

%%

/* flex 2.5.35: There is a bug in the 'noyywrap' option which creates a wrong 'yywrap' macro.
 *              Need to use an explicit 'yywrap' function to get the same functionality.
 */
int yywrap()
{
	return 1;
}

#pragma warning(default:4127) // conditional expression is constant
#pragma warning(default:4244) // conversion from 'type1' to 'type2', possible loss of data

static void ReadLexBuff(char *pcBuff, int &riResult, size_t uMaxSize)
{
	ASSERT( s_pszLexBuff != NULL );
	if (s_pszLexBuff == NULL)
		YY_FATAL_ERROR("Input in flex scanner failed");

	ASSERT( sizeof(YY_CHAR) == sizeof(char) );
	size_t uCharsInBuff = strlen(s_pszLexBuff);
	size_t uCharsRead = min(uMaxSize, uCharsInBuff);
	riResult = uCharsRead;
	memcpy(pcBuff, s_pszLexBuff, uCharsRead);
	s_pszLexBuff += uCharsRead;
}

static void FatalLexError(yyconst char msg[])
{
#ifdef _CONSOLE
	printf("Fatal error in flex scanner: %s\n", msg);
#else
	AfxMessageBox(CString(_T("Fatal error in flex scanner: ")) + CString(msg), MB_ICONSTOP);
#endif
}

void LexInit(LPCTSTR pszInput, bool bKeepQuotedStrings)
{
	s_strInputBuff = StrToUtf8(pszInput);
	s_pszLexBuff = (LPCSTR)s_strInputBuff;
	s_bKeepQuotedStrings = bKeepQuotedStrings;
}

void LexFree()
{
	yylex_destroy();
	yytext = NULL;
	yyleng = 0;
	yy_hold_char = '\0';
	yy_n_chars = 0;
	yy_did_buffer_switch_on_eof = 0;
	yylineno = 1;
	s_strInputBuff.Empty();
	s_pszLexBuff = NULL;
}

int opt_strnicmp(const char *pszString, const char *pszMatch, size_t nMinMatch)
{
	size_t nStringLen = strlen(pszString);
	if (nStringLen < nMinMatch)
		return -1;
	size_t nMatchLen = strlen(pszMatch);
	ASSERT( nMatchLen >= nMinMatch );
	if (nStringLen > nMatchLen)
		return 1;
	return _strnicmp(pszString, pszMatch, nStringLen);
}
#pragma warning(pop)