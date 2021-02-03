#pragma once

#if _MFC_VER<0x0B00
#include <uxtheme.h>
#include <tmschema.h>
#endif

#pragma warning(push)
#pragma warning(disable:4100) // unreferenced formal parameter
class CVisualStylesXP
{
public:
	CVisualStylesXP();
	~CVisualStylesXP();

	static HTHEME OpenThemeData(HWND hwnd, LPCWSTR pszClassList);
	static HRESULT CloseThemeData(HTHEME hTheme);
	static HRESULT DrawThemeBackground(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
	static HRESULT DrawThemeText(HTHEME hTheme, HDC hdc, int iPartId,
		int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags,
		DWORD dwTextFlags2, LPCRECT pRect);
	static HRESULT GetThemeBackgroundContentRect(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, LPCRECT pBoundingRect,
		LPRECT pContentRect);
	static HRESULT GetThemeBackgroundExtent(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, LPCRECT pContentRect,
		LPRECT pExtentRect);
	static HRESULT GetThemePartSize(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, LPRECT  pRect, enum THEMESIZE eSize, SIZE *psz);
	static HRESULT GetThemeTextExtent(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
		DWORD dwTextFlags, LPCRECT pBoundingRect,
		LPRECT pExtentRect);
	static HRESULT GetThemeTextMetrics(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, TEXTMETRIC* ptm);
	static HRESULT GetThemeBackgroundRegion(HTHEME hTheme, HDC hdc,
		int iPartId, int iStateId, LPCRECT pRect, HRGN *pRegion);
	static HRESULT HitTestThemeBackground(HTHEME hTheme, HDC hdc, int iPartId,
		int iStateId, DWORD dwOptions, LPCRECT pRect, HRGN hrgn,
		POINT ptTest, WORD *pwHitTestCode);
	static HRESULT DrawThemeEdge(HTHEME hTheme, HDC hdc, int iPartId, int iStateId,
		LPCRECT pDestRect, UINT uEdge, UINT uFlags, LPRECT pContentRect);
	static HRESULT DrawThemeIcon(HTHEME hTheme, HDC hdc, int iPartId,
		int iStateId, LPCRECT pRect, HIMAGELIST himl, int iImageIndex);
	static BOOL IsThemePartDefined(HTHEME hTheme, int iPartId,
		int iStateId);
	static BOOL IsThemeBackgroundPartiallyTransparent(HTHEME hTheme,
		int iPartId, int iStateId);
	static HRESULT GetThemeColor(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, COLORREF *pColor);
	static HRESULT GetThemeMetric(HTHEME hTheme, HDC hdc, int iPartId,
		int iStateId, int iPropId, int *piVal);
	static HRESULT GetThemeString(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, LPWSTR pszBuff, int cchMaxBuffChars);
	static HRESULT GetThemeBool(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, BOOL *pfVal);
	static HRESULT GetThemeInt(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, int *piVal);
	static HRESULT GetThemeEnumValue(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, int *piVal);
	static HRESULT GetThemePosition(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, POINT *pPoint);
	static HRESULT GetThemeFont(HTHEME hTheme, HDC hdc, int iPartId,
		int iStateId, int iPropId, LOGFONT *pFont);
	static HRESULT GetThemeRect(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, LPRECT pRect);
	static HRESULT GetThemeMargins(HTHEME hTheme, HDC hdc, int iPartId,
		int iStateId, int iPropId, LPRECT prc, MARGINS *pMargins);
	static HRESULT GetThemeIntList(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, INTLIST *pIntList);
	static HRESULT GetThemePropertyOrigin(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, enum PROPERTYORIGIN *pOrigin);
	static HRESULT SetWindowTheme(HWND hwnd, LPCWSTR pszSubAppName,
		LPCWSTR pszSubIdList);
	static HRESULT GetThemeFilename(HTHEME hTheme, int iPartId,
		int iStateId, int iPropId, LPWSTR pszThemeFileName, int cchMaxBuffChars);
	static COLORREF GetThemeSysColor(HTHEME hTheme, int iColorId);
	static HBRUSH GetThemeSysColorBrush(HTHEME hTheme, int iColorId);
	static BOOL GetThemeSysBool(HTHEME hTheme, int iBoolId);
	static int GetThemeSysSize(HTHEME hTheme, int iSizeId);
	static HRESULT GetThemeSysFont(HTHEME hTheme, int iFontId, LOGFONT *plf);
	static HRESULT GetThemeSysString(HTHEME hTheme, int iStringId,
		LPWSTR pszStringBuff, int cchMaxStringChars);
	static HRESULT GetThemeSysInt(HTHEME hTheme, int iIntId, int *piValue);
	static BOOL IsThemeActive();
	static BOOL IsAppThemed();
	static HTHEME GetWindowTheme(HWND hwnd);
	static HRESULT EnableThemeDialogTexture(HWND hwnd, DWORD dwFlags);
	static BOOL IsThemeDialogTextureEnabled(HWND hwnd);
	static DWORD GetThemeAppProperties();
	static void SetThemeAppProperties(DWORD dwFlags);
	static HRESULT GetCurrentThemeName(
		LPWSTR pszThemeFileName, int cchMaxNameChars,
		LPWSTR pszColorBuff, int cchMaxColorChars,
		LPWSTR pszSizeBuff, int cchMaxSizeChars);
	static HRESULT GetThemeDocumentationProperty(LPCWSTR pszThemeName,
		LPCWSTR pszPropertyName, LPWSTR pszValueBuff, int cchMaxValChars);
	static HRESULT DrawThemeParentBackground(HWND hwnd, HDC hdc, LPRECT prc);
	static HRESULT EnableTheming(BOOL fEnable);

private:
	static HMODULE m_hThemeDll;
	static void* GetProc(LPCSTR szProc, void *pfnFail);

	typedef HTHEME (__stdcall *PFNOPENTHEMEDATA)(HWND hwnd, LPCWSTR pszClassList);
	static HTHEME OpenThemeDataFail(HWND, LPCWSTR)	{ return NULL; }

	typedef HRESULT(__stdcall *PFNCLOSETHEMEDATA)(HTHEME hTheme);
	static HRESULT CloseThemeDataFail(HTHEME)	{ return E_FAIL; }

	typedef HRESULT(__stdcall *PFNDRAWTHEMEBACKGROUND)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, LPCRECT pClipRect);
	static HRESULT DrawThemeBackgroundFail(HTHEME, HDC, int, int, LPCRECT, LPCRECT)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNDRAWTHEMETEXT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, DWORD dwTextFlags2, LPCRECT pRect);
	static HRESULT DrawThemeTextFail(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, DWORD, LPCRECT)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEBACKGROUNDCONTENTRECT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pBoundingRect, LPRECT pContentRect);
	static HRESULT GetThemeBackgroundContentRectFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pBoundingRect, LPRECT pContentRect)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEBACKGROUNDEXTENT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pContentRect, LPRECT pExtentRect);
	static HRESULT GetThemeBackgroundExtentFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pContentRect, LPRECT pExtentRect)	{ return E_FAIL; }

	typedef HRESULT(__stdcall *PFNGETTHEMEPARTSIZE)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPRECT pRect, enum THEMESIZE eSize, SIZE *psz);
	static HRESULT GetThemePartSizeFail(HTHEME, HDC, int, int, LPRECT, enum THEMESIZE, SIZE*)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMETEXTEXTENT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, LPCRECT pBoundingRect, LPRECT pExtentRect);
	static HRESULT GetThemeTextExtentFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, LPCRECT pBoundingRect, LPRECT pExtentRect)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMETEXTMETRICS)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, TEXTMETRIC *ptm);
	static HRESULT GetThemeTextMetricsFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, TEXTMETRIC *ptm)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEBACKGROUNDREGION)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, HRGN *pRegion);
	static HRESULT GetThemeBackgroundRegionFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, HRGN *pRegion){ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNHITTESTTHEMEBACKGROUND)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, DWORD dwOptions, LPCRECT pRect, HRGN hrgn, POINT ptTest, WORD *pwHitTestCode);
	static HRESULT HitTestThemeBackgroundFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, DWORD dwOptions, LPCRECT pRect, HRGN hrgn, POINT ptTest, WORD *pwHitTestCode)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNDRAWTHEMEEDGE)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pDestRect, UINT uEdge, UINT uFlags, LPRECT pContentRect);
	static HRESULT DrawThemeEdgeFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pDestRect, UINT uEdge, UINT uFlags, LPRECT pContentRect)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNDRAWTHEMEICON)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, HIMAGELIST himl, int iImageIndex);
	static HRESULT DrawThemeIconFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, HIMAGELIST himl, int iImageIndex)	{ return E_FAIL; }

	typedef BOOL (__stdcall *PFNISTHEMEPARTDEFINED)(HTHEME hTheme, int iPartId, int iStateId);
	static BOOL IsThemePartDefinedFail(HTHEME hTheme, int iPartId, int iStateId)	{ return FALSE; }

	typedef BOOL (__stdcall *PFNISTHEMEBACKGROUNDPARTIALLYTRANSPARENT)(HTHEME hTheme, int iPartId, int iStateId);
	static BOOL IsThemeBackgroundPartiallyTransparentFail(HTHEME hTheme, int iPartId, int iStateId)	{ return FALSE; }

	typedef HRESULT (__stdcall *PFNGETTHEMECOLOR)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor);
	static HRESULT GetThemeColorFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF *pColor)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEMETRIC)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, int *piVal);
	static HRESULT GetThemeMetricFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, int *piVal)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMESTRING)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPWSTR pszBuff, int cchMaxBuffChars);
	static HRESULT GetThemeStringFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPWSTR pszBuff, int cchMaxBuffChars)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEBOOL)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, BOOL *pfVal);
	static HRESULT GetThemeBoolFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, BOOL *pfVal)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEINT)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int *piVal);
	static HRESULT GetThemeIntFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int *piVal)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEENUMVALUE)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int *piVal);
	static HRESULT GetThemeEnumValueFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, int *piVal)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEPOSITION)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, POINT *pPoint);
	static HRESULT GetThemePositionFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, POINT *pPoint)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEFONT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LOGFONT *pFont);
	static HRESULT GetThemeFontFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LOGFONT *pFont)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMERECT)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPRECT pRect);
	static HRESULT GetThemeRectFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPRECT pRect)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEMARGINS)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LPRECT prc, MARGINS *pMargins);
	static HRESULT GetThemeMarginsFail(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, int iPropId, LPRECT prc, MARGINS *pMargins)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEINTLIST)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, INTLIST *pIntList);
	static HRESULT GetThemeIntListFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, INTLIST *pIntList)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEPROPERTYORIGIN)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, enum PROPERTYORIGIN *pOrigin);
	static HRESULT GetThemePropertyOriginFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, enum PROPERTYORIGIN *pOrigin)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNSETWINDOWTHEME)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
	static HRESULT SetWindowThemeFail(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEFILENAME)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPWSTR pszThemeFileName, int cchMaxBuffChars);
	static HRESULT GetThemeFilenameFail(HTHEME hTheme, int iPartId, int iStateId, int iPropId, LPWSTR pszThemeFileName, int cchMaxBuffChars)	{ return E_FAIL; }

	typedef COLORREF (__stdcall *PFNGETTHEMESYSCOLOR)(HTHEME hTheme, int iColorId);
	static COLORREF GetThemeSysColorFail(HTHEME hTheme, int iColorId)	{ return RGB(255,255,255); }

	typedef HBRUSH (__stdcall *PFNGETTHEMESYSCOLORBRUSH)(HTHEME hTheme, int iColorId);
	static HBRUSH GetThemeSysColorBrushFail(HTHEME hTheme, int iColorId)	{ return NULL; }

	typedef BOOL (__stdcall *PFNGETTHEMESYSBOOL)(HTHEME hTheme, int iBoolId);
	static BOOL GetThemeSysBoolFail(HTHEME hTheme, int iBoolId)	{ return FALSE; }

	typedef int (__stdcall *PFNGETTHEMESYSSIZE)(HTHEME hTheme, int iSizeId);
	static int GetThemeSysSizeFail(HTHEME hTheme, int iSizeId)	{ return 0; }

	typedef HRESULT (__stdcall *PFNGETTHEMESYSFONT)(HTHEME hTheme, int iFontId, LOGFONT *plf);
	static HRESULT GetThemeSysFontFail(HTHEME hTheme, int iFontId, LOGFONT *plf)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMESYSSTRING)(HTHEME hTheme, int iStringId, LPWSTR pszStringBuff, int cchMaxStringChars);
	static HRESULT GetThemeSysStringFail(HTHEME hTheme, int iStringId, LPWSTR pszStringBuff, int cchMaxStringChars)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMESYSINT)(HTHEME hTheme, int iIntId, int *piValue);
	static HRESULT GetThemeSysIntFail(HTHEME hTheme, int iIntId, int *piValue)	{ return E_FAIL; }

	typedef BOOL (__stdcall *PFNISTHEMEACTIVE)();
	static BOOL IsThemeActiveFail()	{ return FALSE; }

	typedef BOOL(__stdcall *PFNISAPPTHEMED)();
	static BOOL IsAppThemedFail()	{ return FALSE; }

	typedef HTHEME (__stdcall *PFNGETWINDOWTHEME)(HWND hwnd);
	static HTHEME GetWindowThemeFail(HWND hwnd)	{ return NULL; }

	typedef HRESULT (__stdcall *PFNENABLETHEMEDIALOGTEXTURE)(HWND hwnd, DWORD dwFlags);
	static HRESULT EnableThemeDialogTextureFail(HWND hwnd, DWORD dwFlags)	{ return E_FAIL; }

	typedef BOOL (__stdcall *PFNISTHEMEDIALOGTEXTUREENABLED)(HWND hwnd);
	static BOOL IsThemeDialogTextureEnabledFail(HWND hwnd)	{ return FALSE; }

	typedef DWORD (__stdcall *PFNGETTHEMEAPPPROPERTIES)();
	static DWORD GetThemeAppPropertiesFail()	{ return 0; }

	typedef void (__stdcall *PFNSETTHEMEAPPPROPERTIES)(DWORD dwFlags);
	static void SetThemeAppPropertiesFail(DWORD dwFlags)	{}

	typedef HRESULT (__stdcall *PFNGETCURRENTTHEMENAME)(LPWSTR pszThemeFileName, int cchMaxNameChars, LPWSTR pszColorBuff, int cchMaxColorChars, LPWSTR pszSizeBuff, int cchMaxSizeChars);
	static HRESULT GetCurrentThemeNameFail(LPWSTR pszThemeFileName, int cchMaxNameChars, LPWSTR pszColorBuff, int cchMaxColorChars, LPWSTR pszSizeBuff, int cchMaxSizeChars)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNGETTHEMEDOCUMENTATIONPROPERTY)(LPCWSTR pszThemeName, LPCWSTR pszPropertyName, LPWSTR pszValueBuff, int cchMaxValChars);
	static HRESULT GetThemeDocumentationPropertyFail(LPCWSTR pszThemeName, LPCWSTR pszPropertyName, LPWSTR pszValueBuff, int cchMaxValChars)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNDRAWTHEMEPARENTBACKGROUND)(HWND hwnd, HDC hdc, LPRECT prc);
	static HRESULT DrawThemeParentBackgroundFail(HWND hwnd, HDC hdc, LPRECT prc)	{ return E_FAIL; }

	typedef HRESULT (__stdcall *PFNENABLETHEMING)(BOOL fEnable);
	static HRESULT EnableThemingFail(BOOL fEnable)	{ return E_FAIL; }
};
#pragma warning(pop)

extern CVisualStylesXP g_xpStyle;