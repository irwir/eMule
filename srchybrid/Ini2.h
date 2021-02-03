// Ini.h: Schnittstelle für die Klasse CIni.

/*
 Autor: Michael Schikora
 Mail:  schiko@schikos.de

 If you found this code useful,
 please let me know

 How to use:

 void CMyClass::UpdateFromIni(bool bFromIni)
{
	CIni ini(m_strFileName,m_strSection);
	ini.SER_GET(bFromIni,m_nValueXY);
	ini.SER_GET(bFromIni,m_strValue);
	ini.SER_ARR(bFromIni,m_arValue,MAX_AR);
	ini.SER_ARR(bFromIni,m_ar3D,3);
	//or with default values
	ini.SER_GETD(bFromIni,m_nValueXY,5);
	ini.SER_GETD(bFromIni,m_strValue,"Hello");
	ini.SER_ARRD(bFromIni,m_arValue,MAX_AR,10);
	ini.SER_ARRD(bFromIni,m_ar3D,3,5);
}
*/
#pragma once

#define SER_GET(bGet,value) SerGet(bGet,value,#value)
#define SER_ARR(bGet,value,n) SerGet(bGet,value,n,#value)
#define SER_GETD(bGet,value,default) SerGet(bGet,value,#value,NULL,default)
#define SER_ARRD(bGet,value,n,default) SerGet(bGet,value,n,#value,default)

class CIni
{
public:
	// In order to avoid storing in the windows directory if the IniFilename contains no path,
	// the module's directory will be added to the FileName,
	// bModulePath=true: ModuleDir, bModulePath=false: CurrentDir
	static void AddModulePath(CString &rstrFileName, bool bModulPath = true);
	static CString GetDefaultSection();
	static CString GetDefaultIniFile(bool bModulPath = true);

	CIni();
	explicit CIni(const CIni &Ini);
	explicit CIni(const CString &rstrFileName);
	CIni(const CString &rstrFileName, const CString &rstrSection);
	CIni& operator=(const CIni &Ini);
	virtual	~CIni() = default;

	void SetFileName(const CString &rstrFileName);
	void SetSection(const CString &rstrSection);
	const CString& GetFileName() const;
	const CString& GetSection() const;

	CString		GetStringUTF8(LPCTSTR lpszEntry,LPCTSTR		lpszDefault = NULL,			 LPCTSTR lpszSection = NULL);
	CString		GetStringLong(LPCTSTR lpszEntry,LPCTSTR		lpszDefault = NULL,			 LPCTSTR lpszSection = NULL);
	double		GetDouble(LPCTSTR lpszEntry,	double		fDefault = 0.0,				 LPCTSTR lpszSection = NULL);
	CString		GetString(LPCTSTR lpszEntry,	LPCTSTR		lpszDefault = NULL,			 LPCTSTR lpszSection = NULL);
	float		GetFloat(LPCTSTR lpszEntry,		float		fDefault = 0.0F,			 LPCTSTR lpszSection = NULL);
	int			GetInt(LPCTSTR lpszEntry,		int			nDefault = 0,				 LPCTSTR lpszSection = NULL);
	ULONGLONG	GetUInt64(LPCTSTR lpszEntry,	ULONGLONG	nDefault = 0,				 LPCTSTR lpszSection = NULL);
	WORD		GetWORD(LPCTSTR lpszEntry,		WORD		nDefault = 0,				 LPCTSTR lpszSection = NULL);
	bool		GetBool(LPCTSTR lpszEntry,		bool		bDefault = false,			 LPCTSTR lpszSection = NULL);
	CPoint		GetPoint(LPCTSTR lpszEntry,		const CPoint &ptDefault = CPoint(),		 LPCTSTR lpszSection = NULL);
	CRect		GetRect(LPCTSTR lpszEntry,		const CRect &rcDefault = CRect(),		 LPCTSTR lpszSection = NULL);
	COLORREF	GetColRef(LPCTSTR lpszEntry,	COLORREF	crDefault = RGB(128,128,128),LPCTSTR lpszSection = NULL);
	bool		GetBinary(LPCTSTR lpszEntry,	BYTE		**ppData,	UINT *pBytes,	 LPCTSTR pszSection = NULL);

	void	WriteString(LPCTSTR lpszEntry,	  LPCTSTR		lpsz,				LPCTSTR lpszSection = NULL);
	void	WriteStringUTF8(LPCTSTR lpszEntry,LPCTSTR		lpsz,				LPCTSTR lpszSection = NULL);
	void	WriteDouble(LPCTSTR lpszEntry,	  double		f,					LPCTSTR lpszSection = NULL);
	void	WriteFloat(LPCTSTR lpszEntry,	  float			f,					LPCTSTR lpszSection = NULL);
	void	WriteInt(LPCTSTR lpszEntry,		  int			n,					LPCTSTR lpszSection = NULL);
	void	WriteUInt64(LPCTSTR lpszEntry,	  ULONGLONG		n,					LPCTSTR lpszSection = NULL);
	void	WriteWORD(LPCTSTR lpszEntry,	  WORD			n,					LPCTSTR lpszSection = NULL);
	void	WriteBool(LPCTSTR lpszEntry,	  bool			b,					LPCTSTR lpszSection = NULL);
	void	WritePoint(LPCTSTR lpszEntry,	  const CPoint	&pt,				LPCTSTR lpszSection = NULL);
	void	WriteRect(LPCTSTR lpszEntry,	  const CRect	&rect,				LPCTSTR lpszSection = NULL);
	void	WriteColRef(LPCTSTR lpszEntry,	  COLORREF		cr,					LPCTSTR lpszSection = NULL);
	bool	WriteBinary(LPCTSTR lpszEntry,	  LPBYTE		pData,	UINT nBytes,LPCTSTR lpszSection = NULL);

	void	SerGetString( bool bGet, CString  &rstr, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, LPCTSTR lpszDefault = NULL);
	void	SerGetDouble( bool bGet, double	  &f,	 LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, double fDefault = 0.0);
	void	SerGetFloat(  bool bGet, float	  &f,	 LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, float fDefault = 0.0);
	void	SerGetInt(	  bool bGet, int	  &n,	 LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, int nDefault = 0);
	void	SerGetDWORD(  bool bGet, DWORD	  &n,	 LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, DWORD nDefault = 0);
	void	SerGetBool(	  bool bGet, bool	  &b,	 LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, bool bDefault = false);
	void	SerGetPoint(  bool bGet, CPoint	  &pt,	 LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, const CPoint &ptDefault = CPoint());
	void	SerGetRect(	  bool bGet, CRect	  &rect, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, const CRect &rectDefault = CRect());
	void	SerGetColRef( bool bGet, COLORREF &cr,   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, COLORREF crDefault = RGB(128,128,128));

	void	SerGet(bool bGet, CString	&rstr, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, LPCTSTR lpszDefault = NULL);
	void	SerGet(bool bGet, double	&f,	   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, double fDefault = 0.0);
	void	SerGet(bool bGet, float		&f,	   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, float fDefault = 0.0F);
	void	SerGet(bool bGet, int		&n,	   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, int nDefault = 0);
	void	SerGet(bool bGet, short		&n,	   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, int nDefault = 0);
	void	SerGet(bool bGet, DWORD		&n,	   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, DWORD nDefault = 0);
	void	SerGet(bool bGet, WORD		&n,	   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, DWORD nDefault = 0);
	void	SerGet(bool bGet, CPoint	&pt,   LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, const CPoint &ptDefault = CPoint());
	void	SerGet(bool bGet, CRect		&rect, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, const CRect &rectDefault = CRect());

	void	SerGet(bool	bGet, CString	*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, LPCTSTR lpszDefault = NULL);
	void	SerGet(bool	bGet, double	*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, double fDefault = 0.0);
	void	SerGet(bool	bGet, float		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, float fDefault = 0.0F);
	void	SerGet(bool	bGet, BYTE		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, BYTE nDefault = 0);
	void	SerGet(bool	bGet, int		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, int iDefault = 0);
	void	SerGet(bool	bGet, short		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, int iDefault = 0);
	void	SerGet(bool	bGet, DWORD		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, DWORD dwDefault = 0);
	void	SerGet(bool	bGet, WORD		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, DWORD dwDefault = 0);
	void	SerGet(bool	bGet, CPoint	*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, const CPoint &ptDefault = CPoint());
	void	SerGet(bool	bGet, CRect		*ar, int nCount, LPCTSTR lpszEntry, LPCTSTR lpszSection = NULL, const CRect &rcDefault = CRect());

	void	DeleteKey(LPCTSTR lpszKey);
	static int Parse(const CString &strIn, int nOffset, CString &strOut);

private:
	static CString	Read(LPCTSTR lpszFileName, LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault);
	static void		Write(LPCTSTR lpszFileName, LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue);

	//true: Filenames without path take the Modulepath
	//false: Filenames without path take the CurrentDirectory
	bool	m_bModulePath; //should be before string members for correct order of initialisation

	CString m_strFileName;
	CString m_strSection;

};