%{
#include "stdafx.h"
#include "resource.h"
#include "OtherFunctions.h"
#include "SearchExpr.h"
#include "scanner.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define YY_	_T

extern CStringArray g_astrParserErrors;

void ParsedSearchExpression(const CSearchExpr *pexpr);
int yyerror(LPCTSTR errstr);
int yyerrorf(LPCTSTR errstr, ...);

#pragma warning(push)
#pragma warning(disable:4065) // switch statement contains 'default' but no 'case' labels
#pragma warning(disable:4102) // 'yyerrlab1' : unreferenced label
#pragma warning(disable:4127) // conditional expression is constant
#pragma warning(disable:4242) // conversion from <type1> to <type1>, possible loss of data
#pragma warning(disable:4244) // conversion from <type1> to <type2>, possible loss of data
#pragma warning(disable:4702) // unreachable code

%}

%union {
	uint64			num;
	int				iopr;
	CStringA		*pstr;
	CSearchExpr		*pexpr;
	CSearchAttr		*pattr;
}

%token TOK_STRING TOK_NUMBER TOK_SIZE TOK_TYPE TOK_EXT TOK_SOURCES TOK_COMPLETE TOK_BITRATE TOK_LENGTH TOK_CODEC TOK_RATING TOK_TITLE TOK_ALBUM TOK_ARTIST TOK_TYPEVAL
%token TOK_AND TOK_OR TOK_NOT
%token TOK_OPR_EQ TOK_OPR_LT TOK_OPR_LE TOK_OPR_GT TOK_OPR_GE TOK_OPR_NE
%token TOK_ED2K_LINK
%token TOK_EOF

%type <pexpr>	and_searchexpr searchexpr
%type <pattr>	attribute
%type <pstr>	TOK_STRING TOK_ED2K_LINK TOK_TYPEVAL
%type <iopr>	int_opr
%type <num>		TOK_NUMBER

/* The precedence of TOK_AND has to match the implicit precedence of 'and_searchexpr' */
%left TOK_AND
%left TOK_OR
%left TOK_NOT

%%
/*-------------------------------------------------------------------*/

action			: and_searchexpr TOK_EOF
				/* Here we have 2 choices:
				 *
				 * 1.) We specify the 'searchexpr' to be terminated by EOF token and
				 *     'return 0' when it was reduced.
				 *
				 * 2.) We do *not* 'return' but take care of *not* calling 'ParsedSearchExpression'
				 *     a 2nd time.
				 *
				 * This is needed to parse any possible input which may be entered after
				 * the first search expression and to create according 'syntax error' messages.
				 *
				 * Example:
				 *	"a b="	... this would silently slip through, if none of the above cases
				 *				would be done and if we just 'return 0'.
				 */
					{
						ParsedSearchExpression($1);
						delete $1;
						return 0;
					}
				| TOK_ED2K_LINK TOK_EOF
					{
						CSearchExpr *pexpr = new CSearchExpr(&CSearchAttr($1));
						ParsedSearchExpression(pexpr);
						delete pexpr;
						delete $1;
						return 0;
					}
				/* --------- Error Handling --------- */
				| and_searchexpr error
					{
						yyerror(GetResString(IDS_SEARCH_GENERALERROR));
						delete $1;
						return 1;
					}
				;

				/* Implicit AND
				 * ------------
				 * For this to work correctly, the operator precedence of TOK_AND has to be
				 * set to *lowest*. Otherwise the expressions:
				 *	"a b OR c d"
				 * and
				 *  "a AND b OR c AND d"
				 * would not be equal!
				 */
and_searchexpr	: searchexpr
					{
						$$ = $1;
					}
				/* Reverse the recursion to get a "better" search tree (can be processed with less recursion in general) */
			    | searchexpr and_searchexpr
					{
						CSearchExpr *pexpr = new CSearchExpr;
						pexpr->Add(SEARCHOP_AND);
						pexpr->Add($1);
						pexpr->Add($2);
						$$ = pexpr;
						delete $1;
						delete $2;
					}
				;

searchexpr		: attribute
					{
						$$ = new CSearchExpr($1);
						delete $1;
					}
				| searchexpr TOK_AND searchexpr
					{
						CSearchExpr *pexpr = new CSearchExpr;
						pexpr->Add(SEARCHOP_AND);
						pexpr->Add($1);
						pexpr->Add($3);
						$$ = pexpr;
						delete $1;
						delete $3;
					}
				| searchexpr TOK_OR searchexpr
					{
						CSearchExpr *pexpr = new CSearchExpr;
						pexpr->Add(SEARCHOP_OR);
						pexpr->Add($1);
						pexpr->Add($3);
						$$ = pexpr;
						delete $1;
						delete $3;
					}
				| searchexpr TOK_NOT searchexpr
					{
						CSearchExpr *pexpr = new CSearchExpr;
						pexpr->Add(SEARCHOP_NOT);
						pexpr->Add($1);
						pexpr->Add($3);
						$$ = pexpr;
						delete $1;
						delete $3;
					}
				| '(' and_searchexpr ')'
					{
						$$ = $2;
					}
				/* --------- Error Handling --------- */
				| searchexpr TOK_AND error
					{
						yyerror(GetResString(IDS_SEARCH_MISSINGANDRIGHT));
						delete $1;
						return 1;
					}
				| searchexpr TOK_OR error
					{
						yyerror(GetResString(IDS_SEARCH_MISSINGORRIGHT));
						delete $1;
						return 1;
					}
				| searchexpr TOK_NOT error
					{
						yyerror(GetResString(IDS_SEARCH_MISSINGNOTRIGHT));
						delete $1;
						return 1;
					}
				| '(' error
					{
						yyerror(GetResString(IDS_SEARCH_MISSINGEXPRPARANT));
						return 1;
					}
				| '(' and_searchexpr error
					{
						yyerror(GetResString(IDS_SEARCH_MISSINGCLOSINGPARANT));
						delete $2;
						return 1;
					}
				;

attribute		: TOK_STRING
					{
						$$ = new CSearchAttr($1);
						delete $1;
					}
				| TOK_SIZE int_opr TOK_NUMBER
					{
						$$ = new CSearchAttr(FT_FILESIZE, $2, $3);
					}
				| TOK_TYPE TOK_OPR_EQ TOK_TYPEVAL
					{
						$$ = new CSearchAttr(FT_FILETYPE, $3);
						delete $3;
					}
				| TOK_EXT TOK_OPR_EQ TOK_STRING
					{
						$$ = new CSearchAttr(FT_FILEFORMAT, $3);
						delete $3;
					}
				| TOK_SOURCES int_opr TOK_NUMBER
					{
						$$ = new CSearchAttr(FT_SOURCES, $2, $3);
					}
				| TOK_COMPLETE int_opr TOK_NUMBER
					{
						$$ = new CSearchAttr(FT_COMPLETE_SOURCES, $2, $3);
					}
				| TOK_RATING int_opr TOK_NUMBER
					{
						$$ = new CSearchAttr(FT_FILERATING, $2, $3);
					}
				| TOK_BITRATE int_opr TOK_NUMBER
					{
						$$ = new CSearchAttr(FT_MEDIA_BITRATE, $2, $3);
					}
				| TOK_LENGTH int_opr TOK_NUMBER
					{
						$$ = new CSearchAttr(FT_MEDIA_LENGTH, $2, $3);
					}
				| TOK_CODEC TOK_OPR_EQ TOK_STRING
					{
						$$ = new CSearchAttr(FT_MEDIA_CODEC, $3);
						delete $3;
					}
				| TOK_TITLE TOK_OPR_EQ TOK_STRING
					{
						$$ = new CSearchAttr(FT_MEDIA_TITLE, $3);
						delete $3;
					}
				| TOK_ALBUM TOK_OPR_EQ TOK_STRING
					{
						$$ = new CSearchAttr(FT_MEDIA_ALBUM, $3);
						delete $3;
					}
				| TOK_ARTIST TOK_OPR_EQ TOK_STRING
					{
						$$ = new CSearchAttr(FT_MEDIA_ARTIST, $3);
						delete $3;
					}
				/* --------- Error Handling --------- */

				| int_opr
					{ yyerrorf(GetResString(IDS_SEARCH_OPRERR), DbgGetSearchOperatorName($1)); return 1; }

				| TOK_SIZE int_opr error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@size")); return 1; }
				| TOK_SIZE error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@size")); return 1; }

				| TOK_TYPE TOK_OPR_EQ error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@type")); return 1; }
				| TOK_TYPE error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@type")); return 1; }

				| TOK_EXT TOK_OPR_EQ error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@ext")); return 1; }
				| TOK_EXT error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@ext")); return 1; }

				| TOK_SOURCES int_opr error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@sources")); return 1; }
				| TOK_SOURCES error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@sources")); return 1; }

				| TOK_COMPLETE int_opr error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@complete")); return 1; }
				| TOK_COMPLETE error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@complete")); return 1; }

				| TOK_RATING int_opr error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@rating")); return 1; }
				| TOK_RATING error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@rating")); return 1; }

				| TOK_BITRATE int_opr error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@bitrate")); return 1; }
				| TOK_BITRATE error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@bitrate")); return 1; }

				| TOK_LENGTH int_opr error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@length")); return 1; }
				| TOK_LENGTH error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@length")); return 1; }

				| TOK_CODEC TOK_OPR_EQ error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@codec")); return 1; }
				| TOK_CODEC error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@codec")); return 1; }

				| TOK_TITLE TOK_OPR_EQ error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@title")); return 1; }
				| TOK_TITLE error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@title")); return 1; }

				| TOK_ALBUM TOK_OPR_EQ error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@album")); return 1; }
				| TOK_ALBUM error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@album")); return 1; }

				| TOK_ARTIST TOK_OPR_EQ error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@artist")); return 1; }
				| TOK_ARTIST error
					{ yyerrorf(GetResString(IDS_SEARCH_ATTRERR), _T("@artist")); return 1; }
				;

int_opr			: TOK_OPR_EQ
					{ $$ = ED2K_SEARCH_OP_EQUAL; }
				| TOK_OPR_GT
					{ $$ = ED2K_SEARCH_OP_GREATER; }
				| TOK_OPR_LT
					{ $$ = ED2K_SEARCH_OP_LESS; }
				| TOK_OPR_GE
					{ $$ = ED2K_SEARCH_OP_GREATER_EQUAL; }
				| TOK_OPR_LE
					{ $$ = ED2K_SEARCH_OP_LESS_EQUAL; }
				| TOK_OPR_NE
					{ $$ = ED2K_SEARCH_OP_NOTEQUAL; }
				;

%%

int yyerror(LPCTSTR errstr)
{
	// Errors created by yacc generated code
	//yyerror ("syntax error: cannot back up");
	//yyerror ("syntax error; also virtual memory exhausted");
	//yyerror ("syntax error");
	//yyerror ("parser stack overflow");

	if (_tcscmp(errstr, _T("syntax error")) == 0) {
		// If there is already an error in the list, don't add the "syntax error" string.
		// This is needed to not 'overwrite' any errors which were placed by 'lex' there,
		// because we will read only the last error eventually.
		if (!g_astrParserErrors.IsEmpty())
			return EXIT_FAILURE;
	}
	else {
		if (!g_astrParserErrors.IsEmpty() && g_astrParserErrors[g_astrParserErrors.GetCount() - 1] != _T("syntax error"))
			return EXIT_FAILURE;
	}
	g_astrParserErrors.Add(errstr);
	return EXIT_FAILURE;
}

int yyerrorf(LPCTSTR errstr, ...)
{
	// If there is already an error in the list, don't add the "syntax error" string.
	// This is needed to not 'overwrite' any errors which were placed by 'lex' there,
	// because we will read only the last error eventually.
	if (_tcscmp(errstr, _T("syntax error")) == 0) {
		if (!g_astrParserErrors.IsEmpty())
			return EXIT_FAILURE;
	}
	else {
		if (!g_astrParserErrors.IsEmpty() && g_astrParserErrors[g_astrParserErrors.GetCount() - 1] != _T("syntax error"))
			return EXIT_FAILURE;
	}

	va_list argp;
	va_start(argp, errstr);
	CString strError;
	strError.FormatV(errstr, argp);
	g_astrParserErrors.Add(strError);
	va_end(argp);
	return EXIT_FAILURE;
}
#pragma warning(pop)