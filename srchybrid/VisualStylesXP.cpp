/////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2001-2002 by David Yuheng Zhao
//
// Distribute and change freely, except: don't remove my name from the source
//
// No guarantee of any kind, express or implied, is included with this
// software; use at your own risk, responsibility for damages (if any) to
// anyone resulting from the use of this software rests entirely with the
// user.
//
// Partly based on the _ThemeHelper struct in MFC7.0 source code (winctrl3.cpp),
// and the difference is that this implementation wraps the full set of
// visual style APIs from the platform SDK August 2001
//
// If you have any questions, I can be reached as follows:
//	yuheng_zhao@yahoo.com
//
//
// How to use:
// Instead of calling the API directly,
//    OpenThemeData(...);
// use the global variable
//    g_xpStyle.OpenThemeData(...);
//
/////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CVisualStylesXP g_xpStyle;

HMODULE CVisualStylesXP::m_hThemeDll = NULL;

CVisualStylesXP::CVisualStylesXP()
{
	m_hThemeDll = LoadLibrary(_T("UxTheme.dll"));
}

CVisualStylesXP::~CVisualStylesXP()
{
	if (m_hThemeDll != NULL) {
		VERIFY(FreeLibrary(m_hThemeDll));
		m_hThemeDll = NULL;
	}
}

void* CVisualStylesXP::GetProc(LPCSTR szProc, void *pfnFail)
{
	if (m_hThemeDll != NULL) {
		void *pRet = GetProcAddress(m_hThemeDll, szProc);
		if (pRet != NULL)
			return pRet;
	}
	return pfnFail;
}

HTHEME CVisualStylesXP::OpenThemeData(HWND hwnd, LPCWSTR pszClassList)
{
	static PFNOPENTHEMEDATA pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNOPENTHEMEDATA)GetProc("OpenThemeData", (void*)OpenThemeDataFail);
	return (*pfn)(hwnd, pszClassList);
}

HRESULT CVisualStylesXP::CloseThemeData(HTHEME hTheme)
{
	static PFNCLOSETHEMEDATA pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNCLOSETHEMEDATA)GetProc("CloseThemeData", (void*)CloseThemeDataFail);
	return (*pfn)(hTheme);
}

HRESULT CVisualStylesXP::DrawThemeBackground(HTHEME hTheme, HDC hdc,
											 int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect)
{
	static PFNDRAWTHEMEBACKGROUND pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNDRAWTHEMEBACKGROUND)GetProc("DrawThemeBackground", (void*)DrawThemeBackgroundFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pRect, pClipRect);
}

HRESULT CVisualStylesXP::DrawThemeText(HTHEME hTheme, HDC hdc, int iPartId,
									   int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags,
									   DWORD dwTextFlags2, LPCRECT pRect)
{
	static PFNDRAWTHEMETEXT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNDRAWTHEMETEXT)GetProc("DrawThemeText", (void*)DrawThemeTextFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pszText, iCharCount, dwTextFlags, dwTextFlags2, pRect);
}
HRESULT CVisualStylesXP::GetThemeBackgroundContentRect(HTHEME hTheme, HDC hdc,
													   int iPartId, int iStateId, LPCRECT pBoundingRect,
													   LPRECT pContentRect)
{
	static PFNGETTHEMEBACKGROUNDCONTENTRECT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEBACKGROUNDCONTENTRECT)GetProc("GetThemeBackgroundContentRect", (void*)GetThemeBackgroundContentRectFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pBoundingRect, pContentRect);
}

HRESULT CVisualStylesXP::GetThemeBackgroundExtent(HTHEME hTheme, HDC hdc,
												  int iPartId, int iStateId, LPCRECT pContentRect,
												  LPRECT pExtentRect)
{
	static PFNGETTHEMEBACKGROUNDEXTENT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEBACKGROUNDEXTENT)GetProc("GetThemeBackgroundExtent", (void*)GetThemeBackgroundExtentFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pContentRect, pExtentRect);
}

HRESULT CVisualStylesXP::GetThemePartSize(HTHEME hTheme, HDC hdc,
										  int iPartId, int iStateId, LPRECT  pRect, enum THEMESIZE eSize, SIZE *psz)
{
	static PFNGETTHEMEPARTSIZE pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEPARTSIZE)GetProc("GetThemePartSize", (void*)GetThemePartSizeFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pRect, eSize, psz);
}

HRESULT CVisualStylesXP::GetThemeTextExtent(HTHEME hTheme, HDC hdc,
											int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
											DWORD dwTextFlags, LPCRECT pBoundingRect, LPRECT pExtentRect)
{
	static PFNGETTHEMETEXTEXTENT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMETEXTEXTENT)GetProc("GetThemeTextExtent", (void*)GetThemeTextExtentFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pszText, iCharCount, dwTextFlags, pBoundingRect, pExtentRect);
}

HRESULT CVisualStylesXP::GetThemeTextMetrics(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, TEXTMETRIC *ptm)
{
	static PFNGETTHEMETEXTMETRICS pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMETEXTMETRICS)GetProc("GetThemeTextMetrics", (void*)GetThemeTextMetricsFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, ptm);
}

HRESULT CVisualStylesXP::GetThemeBackgroundRegion(HTHEME hTheme, HDC hdc,
												  int iPartId, int iStateId, LPCRECT pRect, HRGN *pRegion)
{
	static PFNGETTHEMEBACKGROUNDREGION pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEBACKGROUNDREGION)GetProc("GetThemeBackgroundRegion", (void*)GetThemeBackgroundRegionFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pRect, pRegion);
}

HRESULT CVisualStylesXP::HitTestThemeBackground(HTHEME hTheme, HDC hdc, int iPartId,
												int iStateId, DWORD dwOptions, LPCRECT pRect, HRGN hrgn,
												POINT ptTest, WORD *pwHitTestCode)
{
	static PFNHITTESTTHEMEBACKGROUND pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNHITTESTTHEMEBACKGROUND)GetProc("HitTestThemeBackground", (void*)HitTestThemeBackgroundFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, dwOptions, pRect, hrgn, ptTest, pwHitTestCode);
}

HRESULT CVisualStylesXP::DrawThemeEdge(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
									   LPCRECT pDestRect, UINT uEdge, UINT uFlags, LPRECT pContentRect)
{
	static PFNDRAWTHEMEEDGE pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNDRAWTHEMEEDGE)GetProc("DrawThemeEdge", (void*)DrawThemeEdgeFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pDestRect, uEdge, uFlags, pContentRect);
}

HRESULT CVisualStylesXP::DrawThemeIcon(HTHEME hTheme, HDC hdc, int iPartId,
									   int iStateId, LPCRECT pRect, HIMAGELIST himl, int iImageIndex)
{
	static PFNDRAWTHEMEICON pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNDRAWTHEMEICON)GetProc("DrawThemeIcon", (void*)DrawThemeIconFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, pRect, himl, iImageIndex);
}

BOOL CVisualStylesXP::IsThemePartDefined(HTHEME hTheme, int iPartId, int iStateId)
{
	static PFNISTHEMEPARTDEFINED pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNISTHEMEPARTDEFINED)GetProc("IsThemePartDefined", (void*)IsThemePartDefinedFail);
	return (*pfn)(hTheme, iPartId, iStateId);
}

BOOL CVisualStylesXP::IsThemeBackgroundPartiallyTransparent(HTHEME hTheme, int iPartId, int iStateId)
{
	static PFNISTHEMEBACKGROUNDPARTIALLYTRANSPARENT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNISTHEMEBACKGROUNDPARTIALLYTRANSPARENT)GetProc("IsThemeBackgroundPartiallyTransparent", (void*)IsThemeBackgroundPartiallyTransparentFail);
	return (*pfn)(hTheme, iPartId, iStateId);
}

HRESULT CVisualStylesXP::GetThemeColor(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor)
{
	static PFNGETTHEMECOLOR pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMECOLOR)GetProc("GetThemeColor", (void*)GetThemeColorFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pColor);
}

HRESULT CVisualStylesXP::GetThemeMetric(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, int *piVal)
{
	static PFNGETTHEMEMETRIC pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEMETRIC)GetProc("GetThemeMetric", (void*)GetThemeMetricFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, iPropId, piVal);
}

HRESULT CVisualStylesXP::GetThemeString(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
										LPWSTR pszBuff, int cchMaxBuffChars)
{
	static PFNGETTHEMESTRING pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESTRING)GetProc("GetThemeString", (void*)GetThemeStringFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pszBuff, cchMaxBuffChars);
}

HRESULT CVisualStylesXP::GetThemeBool(HTHEME hTheme, int iPartId, int iStateId, int iPropId, BOOL *pfVal)
{
	static PFNGETTHEMEBOOL pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEBOOL)GetProc("GetThemeBool", (void*)GetThemeBoolFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pfVal);
}

HRESULT CVisualStylesXP::GetThemeInt(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int *piVal)
{
	static PFNGETTHEMEINT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEINT)GetProc("GetThemeInt", (void*)GetThemeIntFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, piVal);
}

HRESULT CVisualStylesXP::GetThemeEnumValue(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int *piVal)
{
	static PFNGETTHEMEENUMVALUE pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEENUMVALUE)GetProc("GetThemeEnumValue", (void*)GetThemeEnumValueFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, piVal);
}

HRESULT CVisualStylesXP::GetThemePosition(HTHEME hTheme, int iPartId, int iStateId, int iPropId, POINT *pPoint)
{
	static PFNGETTHEMEPOSITION pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEPOSITION)GetProc("GetThemePosition", (void*)GetThemePositionFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pPoint);
}

HRESULT CVisualStylesXP::GetThemeFont(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LOGFONT *pFont)
{
	static PFNGETTHEMEFONT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEFONT)GetProc("GetThemeFont", (void*)GetThemeFontFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, iPropId, pFont);
}

HRESULT CVisualStylesXP::GetThemeRect(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPRECT pRect)
{
	static PFNGETTHEMERECT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMERECT)GetProc("GetThemeRect", (void*)GetThemeRectFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pRect);
}

HRESULT CVisualStylesXP::GetThemeMargins(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LPRECT prc,
										 MARGINS *pMargins)
{
	static PFNGETTHEMEMARGINS pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEMARGINS)GetProc("GetThemeMargins", (void*)GetThemeMarginsFail);
	return (*pfn)(hTheme, hdc, iPartId, iStateId, iPropId, prc, pMargins);
}

HRESULT CVisualStylesXP::GetThemeIntList(HTHEME hTheme, int iPartId, int iStateId, int iPropId, INTLIST *pIntList)
{
	static PFNGETTHEMEINTLIST pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEINTLIST)GetProc("GetThemeIntList", (void*)GetThemeIntListFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pIntList);
}

HRESULT CVisualStylesXP::GetThemePropertyOrigin(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
												enum PROPERTYORIGIN *pOrigin)
{
	static PFNGETTHEMEPROPERTYORIGIN pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEPROPERTYORIGIN)GetProc("GetThemePropertyOrigin", (void*)GetThemePropertyOriginFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pOrigin);
}

HRESULT CVisualStylesXP::SetWindowTheme(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)
{
	static PFNSETWINDOWTHEME pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNSETWINDOWTHEME)GetProc("SetWindowTheme", (void*)SetWindowThemeFail);
	return (*pfn)(hwnd, pszSubAppName, pszSubIdList);
}

HRESULT CVisualStylesXP::GetThemeFilename(HTHEME hTheme, int iPartId, int iStateId, int iPropId,
										  LPWSTR pszThemeFileName, int cchMaxBuffChars)
{
	static PFNGETTHEMEFILENAME pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEFILENAME)GetProc("GetThemeFilename", (void*)GetThemeFilenameFail);
	return (*pfn)(hTheme, iPartId, iStateId, iPropId, pszThemeFileName, cchMaxBuffChars);
}

COLORREF CVisualStylesXP::GetThemeSysColor(HTHEME hTheme, int iColorId)
{
	static PFNGETTHEMESYSCOLOR pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSCOLOR)GetProc("GetThemeSysColor", (void*)GetThemeSysColorFail);
	return (*pfn)(hTheme, iColorId);
}

HBRUSH CVisualStylesXP::GetThemeSysColorBrush(HTHEME hTheme, int iColorId)
{
	static PFNGETTHEMESYSCOLORBRUSH pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSCOLORBRUSH)GetProc("GetThemeSysColorBrush", (void*)GetThemeSysColorBrushFail);
	return (*pfn)(hTheme, iColorId);
}

BOOL CVisualStylesXP::GetThemeSysBool(HTHEME hTheme, int iBoolId)
{
	static PFNGETTHEMESYSBOOL pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSBOOL)GetProc("GetThemeSysBool", (void*)GetThemeSysBoolFail);
	return (*pfn)(hTheme, iBoolId);
}

int CVisualStylesXP::GetThemeSysSize(HTHEME hTheme, int iSizeId)
{
	static PFNGETTHEMESYSSIZE pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSSIZE)GetProc("GetThemeSysSize", (void*)GetThemeSysSizeFail);
	return (*pfn)(hTheme, iSizeId);
}

HRESULT CVisualStylesXP::GetThemeSysFont(HTHEME hTheme, int iFontId, LOGFONT *plf)
{
	static PFNGETTHEMESYSFONT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSFONT)GetProc("GetThemeSysFont", (void*)GetThemeSysFontFail);
	return (*pfn)(hTheme, iFontId, plf);
}

HRESULT CVisualStylesXP::GetThemeSysString(HTHEME hTheme, int iStringId, LPWSTR pszStringBuff, int cchMaxStringChars)
{
	static PFNGETTHEMESYSSTRING pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSSTRING)GetProc("GetThemeSysString", (void*)GetThemeSysStringFail);
	return (*pfn)(hTheme, iStringId, pszStringBuff, cchMaxStringChars);
}

HRESULT CVisualStylesXP::GetThemeSysInt(HTHEME hTheme, int iIntId, int *piValue)
{
	static PFNGETTHEMESYSINT pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMESYSINT)GetProc("GetThemeSysInt", (void*)GetThemeSysIntFail);
	return (*pfn)(hTheme, iIntId, piValue);
}

BOOL CVisualStylesXP::IsThemeActive()
{
	static PFNISTHEMEACTIVE pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNISTHEMEACTIVE)GetProc("IsThemeActive", (void*)IsThemeActiveFail);
	return (*pfn)();
}

BOOL CVisualStylesXP::IsAppThemed()
{
	static PFNISAPPTHEMED pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNISAPPTHEMED)GetProc("IsAppThemed", (void*)IsAppThemedFail);
	return (*pfn)();
}

HTHEME CVisualStylesXP::GetWindowTheme(HWND hwnd)
{
	static PFNGETWINDOWTHEME pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETWINDOWTHEME)GetProc("GetWindowTheme", (void*)GetWindowThemeFail);
	return (*pfn)(hwnd);
}

HRESULT CVisualStylesXP::EnableThemeDialogTexture(HWND hwnd, DWORD dwFlags)
{
	static PFNENABLETHEMEDIALOGTEXTURE pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNENABLETHEMEDIALOGTEXTURE)GetProc("EnableThemeDialogTexture", (void*)EnableThemeDialogTextureFail);
	return (*pfn)(hwnd, dwFlags);
}

BOOL CVisualStylesXP::IsThemeDialogTextureEnabled(HWND hwnd)
{
	static PFNISTHEMEDIALOGTEXTUREENABLED pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNISTHEMEDIALOGTEXTUREENABLED)GetProc("IsThemeDialogTextureEnabled", (void*)IsThemeDialogTextureEnabledFail);
	return (*pfn)(hwnd);
}

DWORD CVisualStylesXP::GetThemeAppProperties()
{
	static PFNGETTHEMEAPPPROPERTIES pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEAPPPROPERTIES)GetProc("GetThemeAppProperties", (void*)GetThemeAppPropertiesFail);
	return (*pfn)();
}

void CVisualStylesXP::SetThemeAppProperties(DWORD dwFlags)
{
	static PFNSETTHEMEAPPPROPERTIES pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNSETTHEMEAPPPROPERTIES)GetProc("SetThemeAppProperties", (void*)SetThemeAppPropertiesFail);
	(*pfn)(dwFlags);
}

HRESULT CVisualStylesXP::GetCurrentThemeName(LPWSTR pszThemeFileName, int cchMaxNameChars,
											 LPWSTR pszColorBuff, int cchMaxColorChars,
											 LPWSTR pszSizeBuff, int cchMaxSizeChars)
{
	static PFNGETCURRENTTHEMENAME pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETCURRENTTHEMENAME)GetProc("GetCurrentThemeName", (void*)GetCurrentThemeNameFail);
	return (*pfn)(pszThemeFileName, cchMaxNameChars, pszColorBuff, cchMaxColorChars, pszSizeBuff, cchMaxSizeChars);
}

HRESULT CVisualStylesXP::GetThemeDocumentationProperty(LPCWSTR pszThemeName, LPCWSTR pszPropertyName,
													   LPWSTR pszValueBuff, int cchMaxValChars)
{
	static PFNGETTHEMEDOCUMENTATIONPROPERTY pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNGETTHEMEDOCUMENTATIONPROPERTY)GetProc("GetThemeDocumentationProperty", (void*)GetThemeDocumentationPropertyFail);
	return (*pfn)(pszThemeName, pszPropertyName, pszValueBuff, cchMaxValChars);
}

HRESULT CVisualStylesXP::DrawThemeParentBackground(HWND hwnd, HDC hdc, LPRECT prc)
{
	static PFNDRAWTHEMEPARENTBACKGROUND pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNDRAWTHEMEPARENTBACKGROUND)GetProc("DrawThemeParentBackground", (void*)DrawThemeParentBackgroundFail);
	return (*pfn)(hwnd, hdc, prc);
}

HRESULT CVisualStylesXP::EnableTheming(BOOL fEnable)
{
	static PFNENABLETHEMING pfn = NULL;
	if (pfn == NULL)
		pfn = (PFNENABLETHEMING)GetProc("EnableTheming", (void*)EnableThemingFail);
	return (*pfn)(fEnable);
}