#pragma once

#include "Opcodes.h"
#include "StringConversion.h"

enum ESearchOperators
{
	SEARCHOP_AND,
	SEARCHOP_OR,
	SEARCHOP_NOT
};

#define	SEARCHOPTOK_AND	"\255AND"
#define	SEARCHOPTOK_OR	"\255OR"
#define	SEARCHOPTOK_NOT	"\255NOT"

class CSearchAttr
{
public:
	CSearchAttr()
		: m_nNum()
		, m_str()
		, m_iTag(FT_FILENAME)
		, m_uIntegerOperator(ED2K_SEARCH_OP_EQUAL)
	{
	}

	explicit CSearchAttr(LPCSTR pszString)
		: m_nNum()
		, m_str(pszString)
		, m_iTag(FT_FILENAME)
		, m_uIntegerOperator(ED2K_SEARCH_OP_EQUAL)
	{
	}

	explicit CSearchAttr(const CStringA *pstrString)
		: m_nNum()
		, m_str(*pstrString)
		, m_iTag(FT_FILENAME)
		, m_uIntegerOperator(ED2K_SEARCH_OP_EQUAL)
	{
	}

	CSearchAttr(int iTag, UINT uIntegerOperator, uint64 nSize)
		: m_nNum(nSize)
		, m_iTag(iTag)
		, m_uIntegerOperator(uIntegerOperator)
	{
	}

	CSearchAttr(int iTag, LPCSTR pszString)
		: m_nNum()
		, m_str(pszString)
		, m_iTag(iTag)
		, m_uIntegerOperator(ED2K_SEARCH_OP_EQUAL)
	{
	}

	CSearchAttr(int iTag, const CStringA *pstrString)
		: m_nNum()
		, m_str(*pstrString)
		, m_iTag(iTag)
		, m_uIntegerOperator(ED2K_SEARCH_OP_EQUAL)
	{
	}

	CString DbgGetAttr() const
	{
		CString strDbg;
		switch (m_iTag)
		{
			case FT_FILESIZE:
			case FT_SOURCES:
			case FT_COMPLETE_SOURCES:
			case FT_FILERATING:
			case FT_MEDIA_BITRATE:
			case FT_MEDIA_LENGTH:
				strDbg.Format(_T("%s%s%I64u"), (LPCTSTR)DbgGetFileMetaTagName(m_iTag), (LPCTSTR)DbgGetSearchOperatorName(m_uIntegerOperator), m_nNum);
				break;
			case FT_FILETYPE:
			case FT_FILEFORMAT:
			case FT_MEDIA_CODEC:
			case FT_MEDIA_TITLE:
			case FT_MEDIA_ALBUM:
			case FT_MEDIA_ARTIST:
				ASSERT( m_uIntegerOperator == ED2K_SEARCH_OP_EQUAL );
				strDbg.Format(_T("%s=%s"), (LPCTSTR)DbgGetFileMetaTagName(m_iTag), (LPCTSTR)OptUtf8ToStr(m_str));
				break;
			default:
				ASSERT( m_iTag == FT_FILENAME );
				strDbg.Format(_T("\"%s\""), (LPCTSTR)OptUtf8ToStr(m_str));
		}
		return strDbg;
	}

	uint64 m_nNum;
	CStringA m_str;
	int m_iTag;
	UINT m_uIntegerOperator;
};


class CSearchExpr
{
public:
	CSearchExpr()
		: m_aExpr()
	{
	}

	explicit CSearchExpr(const CSearchAttr *pAttr)
	{
		m_aExpr.Add(*pAttr);
	}

	void Add(ESearchOperators eOperator)
	{
		if (eOperator == SEARCHOP_OR)
			m_aExpr.Add(CSearchAttr(SEARCHOPTOK_OR));
		else if (eOperator == SEARCHOP_NOT)
			m_aExpr.Add(CSearchAttr(SEARCHOPTOK_NOT));
		else {
			ASSERT( eOperator == SEARCHOP_AND );
			m_aExpr.Add(CSearchAttr(SEARCHOPTOK_AND));
		}
	}

	void Add(const CSearchAttr *pAttr)
	{
		m_aExpr.Add(*pAttr);
	}

	void Add(const CSearchExpr *pExpr)
	{
		m_aExpr.Append(pExpr->m_aExpr);
	}

	CArray<CSearchAttr> m_aExpr;
};