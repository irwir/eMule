#pragma once

int yylex();
void LexInit(LPCTSTR pszInput, bool bKeepQuotedStrings);
void LexFree();