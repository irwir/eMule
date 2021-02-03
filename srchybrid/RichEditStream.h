#pragma once

/////////////////////////////////////////////////////////////////////////////
// CRichEditStream window

class CRichEditStream : public CRichEditCtrl
{
public:
	CRichEditStream();
	virtual	~CRichEditStream() = default;

	CRichEditStream& operator<<(LPCTSTR const psz);
	CRichEditStream& operator<<(const char *const psz);
	CRichEditStream& operator<<(UINT uVal);
	CRichEditStream& operator<<(int iVal);
	CRichEditStream& operator<<(double fVal);

	bool IsEmpty() const
	{
		return GetWindowTextLength() == 0;
	}
	void AppendFormat(LPCTSTR pszFmt, ...)
	{
		va_list argp;
		va_start(argp, pszFmt);
		CString str;
		str.AppendFormatV(pszFmt, argp);
		va_end(argp);
		*this << str;
	}

	void InitColors();
	CHARFORMAT m_cfDef;
	CHARFORMAT m_cfBold;
	CHARFORMAT m_cfRed;

	void GetRTFText(CStringA &rstrText);

protected:
	static DWORD CALLBACK StreamOutCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);
	DECLARE_MESSAGE_MAP()
};