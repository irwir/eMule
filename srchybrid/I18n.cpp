#include "stdafx.h"
#include <locale.h>
#include "emule.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "langids.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static HINSTANCE s_hLangDLL = NULL;

CString GetResString(UINT uStringID, WORD wLanguageID)
{
	CString resString;
	if (s_hLangDLL)
		(void)resString.LoadString(s_hLangDLL, uStringID, wLanguageID);
	if (resString.IsEmpty())
		(void)resString.LoadString(GetModuleHandle(NULL), uStringID, LANGID_EN_US);
	return resString;
}

CString GetResString(UINT uStringID)
{
	CString resString;
	if (s_hLangDLL)
		resString.LoadString(s_hLangDLL, uStringID);
	if (resString.IsEmpty())
		resString.LoadString(GetModuleHandle(NULL), uStringID);
	return resString;
}

int LocMessageBox(UINT uId, UINT nType, UINT nIDHelp)
{
	return AfxMessageBox(GetResString(uId), nType, nIDHelp);
}

struct SLanguage
{
	LANGID	lid;
	LPCTSTR pszLocale;
	BOOL	bSupported;
	LPCTSTR	pszISOLocale;
	UINT	uCodepage;
	LPCTSTR	pszHtmlCharset;
};


// Codepages (Windows)
// ---------------------------------------------------------------------
// 1250		ANSI - Central European
// 1251		ANSI - Cyrillic
// 1252		ANSI - Latin I
// 1253		ANSI - Greek
// 1254		ANSI - Turkish
// 1255		ANSI - Hebrew
// 1256		ANSI - Arabic
// 1257		ANSI - Baltic
// 1258		ANSI/OEM - Vietnamese
//  932		ANSI/OEM - Japanese, Shift-JIS
//  936		ANSI/OEM - Simplified Chinese (PRC, Singapore)
//  949		ANSI/OEM - Korean (Unified Hangeul Code)
//  950		ANSI/OEM - Traditional Chinese (Taiwan; Hong Kong SAR, PRC)

// HTML charsets	CodePg
// -------------------------------------------
// windows-1250		1250	Central European (Windows)
// windows-1251		1251	Cyrillic (Windows)
// windows-1252		1252	Western European (Windows)
// windows-1253		1253	Greek (Windows)
// windows-1254		1254	Turkish (Windows)
// windows-1255		1255	Hebrew (Windows)
// windows-1256		1256	Arabic (Windows)
// windows-1257		1257	Baltic (Windows)
//
// NOTE: the 'iso-...' charsets are more backward compatible than the 'windows-...' charsets.
// NOTE-ALSO: some of the 'iso-...' charsets are by default *not* installed by IE6 (e.g. Arabic (ISO)) or show up
//	with wrong chars - so, better use the 'windows-' charsets.
//
// iso-8859-1		1252	Western European (ISO)
// iso-8859-2		1250	Central European (ISO)
// iso-8859-3		1254	Latin 3 (ISO)
// iso-8859-4		1257	Baltic (ISO)
// iso-8859-5		1251	Cyrillic (ISO)			does not show up correctly in IE6
// iso-8859-6		1256	Arabic (ISO)			not installed (by default) with IE6
// iso-8859-7		1253	Greek (ISO)
// iso-8859-8		1255	Hebrew (ISO-Visual)
// iso-8859-9		1254	Turkish (ISO)
// iso-8859-15		1252	Latin 9 (ISO)
// iso-2022-jp		 932	Japanese (JIS)
// iso-2022-kr		 949	Korean (ISO)

static SLanguage s_aLanguages[] =
{
	{LANGID_AR_AE,	_T(""),				FALSE,	_T("ar_AE"),	1256,	_T("windows-1256")},	// Arabic (UAE)
	{LANGID_BA_BA,	_T(""),				FALSE,	_T("ba_BA"),	1252,	_T("windows-1252")},	// Basque
	{LANGID_BG_BG,	_T("hun"),			FALSE,	_T("bg_BG"),	1251,	_T("windows-1251")},	// Bulgarian
	{LANGID_CA_ES,	_T(""),				FALSE,	_T("ca_ES"),	1252,	_T("windows-1252")},	// Catalan
	{LANGID_CZ_CZ,	_T("czech"),		FALSE,	_T("cz_CZ"),	1250,	_T("windows-1250")},	// Czech
	{LANGID_DA_DK,	_T("danish"),		FALSE,	_T("da_DK"),	1252,	_T("windows-1252")},	// Danish
	{LANGID_DE_DE,	_T("german"),		FALSE,	_T("de_DE"),	1252,	_T("windows-1252")},	// German (Germany)
	{LANGID_EL_GR,	_T("greek"),		FALSE,	_T("el_GR"),	1253,	_T("windows-1253")},	// Greek
	{LANGID_EN_US,	_T("english"),		TRUE,	_T("en_US"),	1252,	_T("windows-1252")},	// English
	{LANGID_ES_ES_T,_T("spanish"),		FALSE,	_T("es_ES_T"),	1252,	_T("windows-1252")},	// Spanish (Castilian)
	{LANGID_ES_AS,  _T("spanish"),		FALSE,	_T("es_AS"),	1252,	_T("windows-1252")},	// Asturian
	{LANGID_VA_ES,  _T(""),				FALSE,	_T("va_ES"),	1252,	_T("windows-1252")},	// Valencian AVL
	{LANGID_VA_ES_RACV,  _T(""),		FALSE,	_T("va_ES_RACV"),	1252,	_T("windows-1252")},// Valencian RACV
	{LANGID_ET_EE,	_T(""),				FALSE,	_T("et_EE"),	1257,	_T("windows-1257")},	// Estonian
	{LANGID_FA_IR,	_T("farsi"),		FALSE,	_T("fa_IR"),	1256,	_T("windows-1256")},	// Farsi
	{LANGID_FI_FI,	_T("finnish"),		FALSE,	_T("fi_FI"),	1252,	_T("windows-1252")},	// Finnish
	{LANGID_FR_FR,	_T("french"),		FALSE,	_T("fr_FR"),	1252,	_T("windows-1252")},	// French (France)
	{LANGID_FR_BR,	_T("french"),		FALSE,	_T("fr_BR"),	1252,	_T("windows-1252")},	// French (Breton)
	{LANGID_GL_ES,	_T(""),				FALSE,	_T("gl_ES"),	1252,	_T("windows-1252")},	// Galician
	{LANGID_HE_IL,	_T(""),				FALSE,	_T("he_IL"),	1255,	_T("windows-1255")},	// Hebrew
	{LANGID_HU_HU,	_T("hungarian"),	FALSE,	_T("hu_HU"),	1250,	_T("windows-1250")},	// Hungarian
	{LANGID_IT_IT,	_T("italian"),		FALSE,	_T("it_IT"),	1252,	_T("windows-1252")},	// Italian (Italy)
	{LANGID_JP_JP,	_T("japanese"),		FALSE,	_T("jp_JP"),	 932,	_T("shift_jis")},		// Japanese
	{LANGID_KO_KR,	_T("korean"),		FALSE,	_T("ko_KR"),	 949,	_T("euc-kr")},			// Korean
	{LANGID_LT_LT,	_T(""),				FALSE,	_T("lt_LT"),	1257,	_T("windows-1257")},	// Lithuanian
	{LANGID_LV_LV,	_T(""),				FALSE,	_T("lv_LV"),	1257,	_T("windows-1257")},	// Latvian
	{LANGID_MT_MT,	_T("mt"),			FALSE,	_T("mt_MT"),	1254,	_T("windows-1254")},	// Maltese
	{LANGID_NB_NO,	_T("nor"),			FALSE,	_T("nb_NO"),	1252,	_T("windows-1252")},	// Norwegian (Bokmal)
	{LANGID_NN_NO,	_T("non"),			FALSE,	_T("nn_NO"),	1252,	_T("windows-1252")},	// Norwegian (Nynorsk)
	{LANGID_NL_NL,	_T("dutch"),		FALSE,	_T("nl_NL"),	1252,	_T("windows-1252")},	// Dutch (Netherlands)
	{LANGID_PL_PL,	_T("polish"),		FALSE,	_T("pl_PL"),	1250,	_T("windows-1250")},	// Polish
	{LANGID_PT_BR,	_T("ptb"),			FALSE,	_T("pt_BR"),	1252,	_T("windows-1252")},	// Portuguese (Brazil)
	{LANGID_PT_PT,	_T("ptg"),			FALSE,	_T("pt_PT"),	1252,	_T("windows-1252")},	// Portuguese (Portugal)
	{LANGID_RO_RO,	_T(""),				FALSE,	_T("ro_RO"),	1250,	_T("windows-1250")},	// Romanian
	{LANGID_RU_RU,	_T("russian"),		FALSE,	_T("ru_RU"),	1251,	_T("windows-1251")},	// Russian
	{LANGID_SL_SI,	_T("slovak"),		FALSE,	_T("sl_SI"),	1250,	_T("windows-1250")},	// Slovenian
	{LANGID_SQ_AL,	_T(""),				FALSE,	_T("sq_AL"),	1252,	_T("windows-1252")},	// Albanian (Albania)
	{LANGID_SV_SE,	_T("swedish"),		FALSE,	_T("sv_SE"),	1252,	_T("windows-1252")},	// Swedish
	{LANGID_TR_TR,	_T("turkish"),		FALSE,	_T("tr_TR"),	1254,	_T("windows-1254")},	// Turkish
	{LANGID_UA_UA,	_T(""),				FALSE,	_T("ua_UA"),	1251,	_T("windows-1251")},	// Ukrainian
	{LANGID_ZH_CN,	_T("chs"),			FALSE,	_T("zh_CN"),	 936,	_T("gb2312")},			// Chinese (P.R.C.)
	{LANGID_ZH_TW,	_T("cht"),			FALSE,	_T("zh_TW"),	 950,	_T("big5")},			// Chinese (Taiwan)
	{LANGID_VI_VN,	_T(""),				FALSE,	_T("vi_VN"),	1258,	_T("windows-1258")},	// Vietnamese
	{LANGID_UG_CN,	_T(""),				FALSE,	_T("ug_CN"),	1256,	_T("windows-1256")},	// Uighur
	{0, NULL, 0, 0}
};

static void InitLanguages(const CString &rstrLangDir1, const CString &rstrLangDir2)
{
	bool bFirstDir = rstrLangDir1.CompareNoCase(rstrLangDir2) != 0;
	CFileFind ff;
	bool bEnd = !ff.FindFile(rstrLangDir1 + _T("*.dll"), 0);
	while (!bEnd) {
		bEnd = !ff.FindNextFile();
		if (ff.IsDirectory())
			continue;
		TCHAR szLandDLLFileName[_MAX_FNAME];
		_tsplitpath(ff.GetFileName(), NULL, NULL, szLandDLLFileName, NULL);

		for (SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang)
			if (_tcsicmp(pLang->pszISOLocale, szLandDLLFileName) == 0) {
				pLang->bSupported = TRUE;
				break;
			}

		if (bEnd && bFirstDir) {
			ff.Close();
			bEnd = !ff.FindFile(rstrLangDir2 + _T("*.dll"), 0);
			bFirstDir = false;
		}
	}
	ff.Close();
}

static void FreeLangDLL()
{
	if (s_hLangDLL != NULL && s_hLangDLL != GetModuleHandle(NULL)) {
		VERIFY(FreeLibrary(s_hLangDLL));
		s_hLangDLL = NULL;
	}
}

void CPreferences::GetLanguages(CWordArray &aLanguageIDs)
{
	for (const SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang) {
		//if (pLang->bSupported)
		//show all languages, offer download if not supported ones later
		aLanguageIDs.Add(pLang->lid);
	}
}

LANGID CPreferences::GetLanguageID()
{
	return m_wLanguageID;
}

void CPreferences::SetLanguageID(LANGID lid)
{
	m_wLanguageID = lid;
}

static bool CheckLangDLLVersion(const CString &rstrLangDLL)
{
	bool bResult = false;
	DWORD dwUnused;
	DWORD dwVerInfSize = GetFileVersionInfoSize(const_cast<LPTSTR>((LPCTSTR)rstrLangDLL), &dwUnused);
	if (dwVerInfSize != 0) {
		LPBYTE pucVerInf = (LPBYTE)malloc(dwVerInfSize);
		if (pucVerInf) {
			if (GetFileVersionInfo(const_cast<LPTSTR>((LPCTSTR)rstrLangDLL), 0, dwVerInfSize, pucVerInf)) {
				VS_FIXEDFILEINFO *pFileInf;
				UINT uLen;
				if (VerQueryValue(pucVerInf, _T("\\"), (LPVOID*)&pFileInf, &uLen))
					bResult = (pFileInf->dwProductVersionMS == theApp.m_dwProductVersionMS
							&& pFileInf->dwProductVersionLS == theApp.m_dwProductVersionLS);
			}
			free(pucVerInf);
		}
	}

	return bResult;
}

static bool LoadLangLib(const CString &rstrLangDir1, const CString &rstrLangDir2, LANGID lid)
{
	for (const SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang) {
		if (pLang->bSupported && pLang->lid == lid) {
			FreeLangDLL();

			if (pLang->lid == LANGID_EN_US) {
				s_hLangDLL = NULL;
				return true;
			}

			CString strLangDLL;
			strLangDLL.Format(_T("%s%s.dll"), (LPCTSTR)rstrLangDir1, pLang->pszISOLocale);
			if (CheckLangDLLVersion(strLangDLL)) {
				s_hLangDLL = LoadLibrary(strLangDLL);
				if (s_hLangDLL)
					return true;
			}
			if (rstrLangDir1.CompareNoCase(rstrLangDir2) != 0) {
				strLangDLL.Format(_T("%s%s.dll"), (LPCTSTR)rstrLangDir2, pLang->pszISOLocale);
				if (CheckLangDLLVersion(strLangDLL)) {
					s_hLangDLL = LoadLibrary(strLangDLL);
					if (s_hLangDLL)
						return true;
				}
			}
			break;
		}
	}
	return false;
}

void CPreferences::SetLanguage()
{
	InitLanguages(GetMuleDirectory(EMULE_INSTLANGDIR), GetMuleDirectory(EMULE_ADDLANGDIR, false));

	bool bFoundLang = false;
	if (m_wLanguageID)
		bFoundLang = LoadLangLib(GetMuleDirectory(EMULE_INSTLANGDIR), GetMuleDirectory(EMULE_ADDLANGDIR, false), m_wLanguageID);

	if (!bFoundLang) {
		LANGID lidLocale = (LANGID)GetThreadLocale();
		//LANGID lidLocalePri = PRIMARYLANGID(GetThreadLocale());
		//LANGID lidLocaleSub = SUBLANGID(GetThreadLocale());

		bFoundLang = LoadLangLib(GetMuleDirectory(EMULE_INSTLANGDIR), GetMuleDirectory(EMULE_ADDLANGDIR, false), lidLocale);
		if (!bFoundLang) {
			LoadLangLib(GetMuleDirectory(EMULE_INSTLANGDIR), GetMuleDirectory(EMULE_ADDLANGDIR, false), LANGID_EN_US);
			m_wLanguageID = LANGID_EN_US;
			LocMessageBox(IDS_MB_LANGUAGEINFO, MB_ICONASTERISK);
		} else
			m_wLanguageID = lidLocale;
	}

	// if loading a string fails, set language to English
	if (GetResString(IDS_MB_LANGUAGEINFO).IsEmpty()) {
		LoadLangLib(GetMuleDirectory(EMULE_INSTLANGDIR), GetMuleDirectory(EMULE_ADDLANGDIR, false), LANGID_EN_US);
		m_wLanguageID = LANGID_EN_US;
	}

	InitThreadLocale();
}

bool CPreferences::IsLanguageSupported(LANGID lidSelected)
{
	if (lidSelected == LANGID_EN_US)
		return true;
	const CString &sInst(GetMuleDirectory(EMULE_INSTLANGDIR));
	const CString &sAdd(GetMuleDirectory(EMULE_ADDLANGDIR, false));
	InitLanguages(sInst, sAdd);
	for (const SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang)
		if (pLang->lid == lidSelected && pLang->bSupported) {
			CString sDLL;
			sDLL.Format(_T("%s.dll"), pLang->pszISOLocale);
			return CheckLangDLLVersion(sInst + sDLL) || CheckLangDLLVersion(sAdd + sDLL);
		}

	return false;
}

CString CPreferences::GetLangDLLNameByID(LANGID lidSelected)
{
	for (const SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang)
		if (pLang->lid == lidSelected)
			return CString(pLang->pszISOLocale) + _T(".dll");

	ASSERT(0);
	return CString();
}

void CPreferences::SetRtlLocale(LCID lcid)
{
	for (const SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang)
		if (pLang->lid == LANGIDFROMLCID(lcid)) {
			if (pLang->uCodepage) {
				CString strCodepage;
				strCodepage.Format(_T(".%u"), pLang->uCodepage);
				_tsetlocale(LC_CTYPE, strCodepage);
			}
			break;
		}
}

void CPreferences::InitThreadLocale()
{
	ASSERT(m_wLanguageID != 0);

	// NOTE: This function is for testing multi language support only.
	// NOTE: This function is *NOT* to be enabled in release builds nor to be offered by any Mod!
	if (theApp.GetProfileInt(_T("eMule"), _T("SetLanguageACP"), 0) != 0) {
		//LCID lcidUser = GetUserDefaultLCID();		// Installation, or altered by user in control panel (WinXP)

		// get the ANSI code page which is to be used for all non-Unicode conversions.
		LANGID lidSystem = m_wLanguageID;

		// get user's sorting preferences
		//UINT uSortIdUser = SORTIDFROMLCID(lcidUser);
		//UINT uSortVerUser = SORTVERSIONFROMLCID(lcidUser);
		// we can't use the same sorting parameters for 2 different Languages.
		UINT uSortIdUser = SORT_DEFAULT;
		UINT uSortVerUser = 0;

		// set thread locale, this is used for:
		//	- MBCS->Unicode conversions (e.g. search results).
		//	- Unicode->MBCS conversions (e.g. publishing local files (names) in network, or savint text files on local disk)...
		LCID lcid = MAKESORTLCID(lidSystem, uSortIdUser, uSortVerUser);
		SetThreadLocale(lcid);

		// if we set the thread locale (see comments above) we also have to specify the proper
		// code page for the C-RTL, otherwise we may not be able to store some strings as MBCS
		// (Unicode->MBCS conversion may fail)
		SetRtlLocale(lcid);
	}
}

void InitThreadLocale()
{
	thePrefs.InitThreadLocale();
}

CString GetCodePageNameForLocale(LCID lcid)
{
	CString strCodePage;
	int iResult = GetLocaleInfo(lcid, LOCALE_IDEFAULTANSICODEPAGE, strCodePage.GetBuffer(6), 6);
	strCodePage.ReleaseBuffer();

	if (iResult > 0 && !strCodePage.IsEmpty()) {
		UINT uCodePage = _tcstoul(strCodePage, NULL, 10);
		if (uCodePage != ULONG_MAX) {
			CPINFOEXW CPInfoEx = {};
			BOOL(WINAPI *pfnGetCPInfoEx)(UINT, DWORD, LPCPINFOEXW);
			(FARPROC&)pfnGetCPInfoEx = GetProcAddress(GetModuleHandle(_T("kernel32")), "GetCPInfoExW");
			if (pfnGetCPInfoEx && (*pfnGetCPInfoEx)(uCodePage, 0, &CPInfoEx))
				strCodePage = CPInfoEx.CodePageName;
		}
	}
	return strCodePage;
}

CString CPreferences::GetHtmlCharset()
{
	ASSERT(m_wLanguageID != 0);

	LPCTSTR pszHtmlCharset = NULL;
	for (const SLanguage *pLang = s_aLanguages; pLang->lid; ++pLang)
		if (pLang->lid == m_wLanguageID) {
			pszHtmlCharset = pLang->pszHtmlCharset;
			break;
		}

	if (pszHtmlCharset == NULL || pszHtmlCharset[0] == _T('\0')) {
		ASSERT(0); // should never come here

		// try to get charset from code page
		LPCTSTR pszLcLocale = _tsetlocale(LC_CTYPE, NULL);
		if (pszLcLocale) {
			TCHAR szLocaleID[128];
			UINT uCodepage = 0;
			if (_stscanf_s(pszLcLocale, _T("%[a-zA-Z_].%u"), szLocaleID, (unsigned)_countof(szLocaleID), &uCodepage) == 2 && uCodepage != 0) {
				CString strHtmlCodepage;
				strHtmlCodepage.Format(_T("windows-%u"), uCodepage);
				return strHtmlCodepage;
			}
		}
	}

	return pszHtmlCharset;
}

static HHOOK s_hRTLWindowsLayoutOldCbtFilterHook = NULL;

LRESULT CALLBACK RTLWindowsLayoutCbtFilterHook(int code, WPARAM wParam, LPARAM lParam) noexcept
{
	if (code == HCBT_CREATEWND) {
		//LPCREATESTRUCT lpcs = ((LPCBT_CREATEWND)lParam)->lpcs;

		//if ((lpcs->style & WS_CHILD) == 0)
		//	lpcs->dwExStyle |= WS_EX_LAYOUTRTL;	// doesn't seem to have any effect, but shouldn't hurt

		if ((::GetWindowLongPtr((HWND)wParam, GWL_STYLE) & WS_CHILD) == 0)
			::SetWindowLongPtr((HWND)wParam, GWL_EXSTYLE, ::GetWindowLongPtr((HWND)wParam, GWL_EXSTYLE) | WS_EX_LAYOUTRTL);
	}
	return CallNextHookEx(s_hRTLWindowsLayoutOldCbtFilterHook, code, wParam, lParam);
}

void CemuleApp::EnableRTLWindowsLayout()
{
	BOOL(WINAPI *pfnSetProcessDefaultLayout)(DWORD dwFlags);
	(FARPROC&)pfnSetProcessDefaultLayout = GetProcAddress(GetModuleHandle(_T("user32")), "SetProcessDefaultLayout");
	if (pfnSetProcessDefaultLayout)
		(*pfnSetProcessDefaultLayout)(LAYOUT_RTL);

	s_hRTLWindowsLayoutOldCbtFilterHook = SetWindowsHookEx(WH_CBT, RTLWindowsLayoutCbtFilterHook, NULL, GetCurrentThreadId());
}

void CemuleApp::DisableRTLWindowsLayout()
{
	if (s_hRTLWindowsLayoutOldCbtFilterHook) {
		VERIFY(UnhookWindowsHookEx(s_hRTLWindowsLayoutOldCbtFilterHook));
		s_hRTLWindowsLayoutOldCbtFilterHook = NULL;

		BOOL(WINAPI *pfnSetProcessDefaultLayout)(DWORD dwFlags);
		(FARPROC&)pfnSetProcessDefaultLayout = GetProcAddress(GetModuleHandle(_T("user32")), "SetProcessDefaultLayout");
		if (pfnSetProcessDefaultLayout)
			(*pfnSetProcessDefaultLayout)(0);
	}
}