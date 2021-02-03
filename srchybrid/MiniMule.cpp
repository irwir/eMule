//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include <io.h>
#include <wininet.h>
#include <atlutil.h>
#include "emule.h"
#include "emuledlg.h"
#include "TransferDlg.h"
#include "MiniMule.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "MenuCmds.h"
#include "IESecurity.h"
#include "UserMsgs.h"
#include "opcodes.h"

#if (WINVER < 0x0500)
/* AnimateWindow() Commands */
#define AW_HOR_POSITIVE             0x00000001
#define AW_HOR_NEGATIVE             0x00000002
#define AW_VER_POSITIVE             0x00000004
#define AW_VER_NEGATIVE             0x00000008
#define AW_CENTER                   0x00000010
#define AW_HIDE                     0x00010000
#define AW_ACTIVATE                 0x00020000
#define AW_SLIDE                    0x00040000
#define AW_BLEND                    0x00080000
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


extern UINT g_uMainThreadId;

class CCounter
{
public:
	explicit CCounter(int &ri)
		: m_ri(ri)
	{
		ASSERT(ri == 0);
		++m_ri;
	}
	~CCounter()
	{
		--m_ri;
		ASSERT(m_ri == 0);
	}
	int &m_ri;
};


// CMiniMule dialog
#define	IDT_AUTO_CLOSE_TIMER	100

IMPLEMENT_DYNCREATE(CMiniMule, CDHtmlDialog)

BEGIN_MESSAGE_MAP(CMiniMule, CDHtmlDialog)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_NCLBUTTONDBLCLK()
END_MESSAGE_MAP()

BEGIN_EVENTSINK_MAP(CMiniMule, CDHtmlDialog)
	ON_EVENT(CDHtmlDialog, AFX_IDC_BROWSER, 250 /* BeforeNavigate2 */, _OnBeforeNavigate2, VTS_DISPATCH VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT VTS_PBOOL)
END_EVENTSINK_MAP()

BEGIN_DHTML_EVENT_MAP(CMiniMule)
	DHTML_EVENT_ONCLICK(_T("restoreWndLink"), OnRestoreMainWindow)
	DHTML_EVENT_ONKEYPRESS(_T("restoreWndLink"), OnRestoreMainWindow)
	DHTML_EVENT_ONCLICK(_T("restoreWndImg"), OnRestoreMainWindow)
	DHTML_EVENT_ONKEYPRESS(_T("restoreWndImg"), OnRestoreMainWindow)

	DHTML_EVENT_ONCLICK(_T("openIncomingLink"), OnOpenIncomingFolder)
	DHTML_EVENT_ONKEYPRESS(_T("openIncomingLink"), OnOpenIncomingFolder)
	DHTML_EVENT_ONCLICK(_T("openIncomingImg"), OnOpenIncomingFolder)
	DHTML_EVENT_ONKEYPRESS(_T("openIncomingImg"), OnOpenIncomingFolder)

	DHTML_EVENT_ONCLICK(_T("optionsLink"), OnOptions)
	DHTML_EVENT_ONKEYPRESS(_T("optionsLink"), OnOptions)
	DHTML_EVENT_ONCLICK(_T("optionsImg"), OnOptions)
	DHTML_EVENT_ONKEYPRESS(_T("optionsImg"), OnOptions)
END_DHTML_EVENT_MAP()

CMiniMule::CMiniMule(CWnd *pParent /*=NULL*/)
	: CDHtmlDialog(CMiniMule::IDD, CMiniMule::IDH, pParent)
	, m_iInInitDialog()
	, m_iInCallback()
	, m_bDestroyAfterInitDialog()
	, m_bResolveImages(true)
	, m_bRestoreMainWnd()
	, m_uAutoCloseTimer()
{
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	m_bAutoClose = theApp.GetProfileInt(_T("eMule"), _T("MiniMuleAutoClose"), 0) != 0;
	m_uWndTransparency = theApp.GetProfileInt(_T("eMule"), _T("MiniMuleTransparency"), 0);
	SetHostFlags(m_dwHostFlags
		| DOCHOSTUIFLAG_DIALOG				// MSHTML does not enable selection of the text in the form
		| DOCHOSTUIFLAG_DISABLE_HELP_MENU	// MSHTML does not add the Help menu item to the container's menu.
	);
}

CMiniMule::~CMiniMule()
{
	ASSERT(m_iInInitDialog == 0);
}

STDMETHODIMP CMiniMule::GetOptionKeyPath(LPOLESTR* /*pchKey*/, DWORD /*dw*/) noexcept
{
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	TRACE(_T("%hs\n"), __FUNCTION__);

//OpenKey		HKCU\Software\eMule\IE
//QueryValue	HKCU\Software\eMule\IE\Show_FullURL
//QueryValue	HKCU\Software\eMule\IE\SmartDithering
//QueryValue	HKCU\Software\eMule\IE\RtfConverterFlags

//OpenKey		HKCU\Software\eMule\IE\Main
//QueryValue	HKCU\Software\eMule\IE\Main\Page_Transitions
//QueryValue	HKCU\Software\eMule\IE\Main\Use_DlgBox_Colors
//QueryValue	HKCU\Software\eMule\IE\Main\Anchor Underline
//QueryValue	HKCU\Software\eMule\IE\Main\CSS_Compat
//QueryValue	HKCU\Software\eMule\IE\Main\Expand Alt Text
//QueryValue	HKCU\Software\eMule\IE\Main\Display Inline Images
//QueryValue	HKCU\Software\eMule\IE\Main\Display Inline Videos
//QueryValue	HKCU\Software\eMule\IE\Main\Play_Background_Sounds
//QueryValue	HKCU\Software\eMule\IE\Main\Play_Animations
//QueryValue	HKCU\Software\eMule\IE\Main\Print_Background
//QueryValue	HKCU\Software\eMule\IE\Main\Use Stylesheets
//QueryValue	HKCU\Software\eMule\IE\Main\SmoothScroll
//QueryValue	HKCU\Software\eMule\IE\Main\Show image placeholders
//QueryValue	HKCU\Software\eMule\IE\Main\Disable Script Debugger
//QueryValue	HKCU\Software\eMule\IE\Main\DisableScriptDebuggerIE
//QueryValue	HKCU\Software\eMule\IE\Main\Move System Caret
//QueryValue	HKCU\Software\eMule\IE\Main\Force Offscreen Composition
//QueryValue	HKCU\Software\eMule\IE\Main\Enable AutoImageResize
//QueryValue	HKCU\Software\eMule\IE\Main\Q051873
//QueryValue	HKCU\Software\eMule\IE\Main\UseThemes
//QueryValue	HKCU\Software\eMule\IE\Main\UseHR
//QueryValue	HKCU\Software\eMule\IE\Main\Q300829
//QueryValue	HKCU\Software\eMule\IE\Main\Disable_Local_Machine_Navigate
//QueryValue	HKCU\Software\eMule\IE\Main\Cleanup HTCs
//QueryValue	HKCU\Software\eMule\IE\Main\Q331869
//QueryValue	HKCU\Software\eMule\IE\Main\AlwaysAllowExecCommand

//OpenKey		HKCU\Software\eMule\IE\Settings
//QueryValue	HKCU\Software\eMule\IE\Settings\Anchor Color
//QueryValue	HKCU\Software\eMule\IE\Settings\Anchor Color Visited
//QueryValue	HKCU\Software\eMule\IE\Settings\Anchor Color Hover
//QueryValue	HKCU\Software\eMule\IE\Settings\Always Use My Colors
//QueryValue	HKCU\Software\eMule\IE\Settings\Always Use My Font Size
//QueryValue	HKCU\Software\eMule\IE\Settings\Always Use My Font Face
//QueryValue	HKCU\Software\eMule\IE\Settings\Use Anchor Hover Color
//QueryValue	HKCU\Software\eMule\IE\Settings\MiscFlags

//OpenKey		HKCU\Software\eMule\IE\Styles

//OpenKey		HKCU\Software\eMule\IE\International
//OpenKey		HKCU\Software\eMule\IE\International\Scripts
//OpenKey		HKCU\Software\eMule\IE\International\Scripts\3

	return E_NOTIMPL;
}

void CMiniMule::DoDataExchange(CDataExchange *pDX)
{
	CDHtmlDialog::DoDataExchange(pDX);
}

BOOL CMiniMule::CreateControlSite(COleControlContainer *pContainer, COleControlSite **ppSite, UINT /*nID*/, REFCLSID /*clsid*/)
{
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	CMuleBrowserControlSite *pBrowserSite;
	try {
		pBrowserSite = new CMuleBrowserControlSite(pContainer, this);
	} catch (...) {
		return FALSE;
	}
	*ppSite = pBrowserSite;
	return TRUE;
}

CString CreateFilePathUrl(LPCTSTR pszFilePath, int nProtocol)
{
	// Do *not* use 'AtlCanonicalizeUrl' (or similar function) to convert a file path into
	// an encoded URL. Basically this works, but if the file path contains special characters
	// like e.g. Umlaute, the IE control can not open the encoded URL.
	//
	// The file path "D:\dir_ä#,.-_öäü#'+~´`ß}=])[({&%$!^°\Kopie von ### MiniMule3CyanSnow.htm"
	// can get opened successfully by the IE control *without* using any URL encoding.
	//
	// Though, regardless of using 'AtlCanonicalizeUrl' or not, there is still one special
	// case where the IE control can not open the URL. If the file starts with something like
	//	"c:\#dir\emule.exe". For any unknown reason the sequence "c:\#" causes troubles for
	// the IE control. It does not help to escape that sequence. It always fails.
	//
	CString strEncodedFilePath;
	if (nProtocol == INTERNET_SCHEME_RES) {
		// "res://" protocol has to be specified with 2 slashes ("res:///" does not work)
		strEncodedFilePath = _T("res://");
	} else {
		ASSERT(nProtocol == INTERNET_SCHEME_FILE);
		// "file://" protocol has to be specified with 3 slashes
		strEncodedFilePath = _T("file:///");
	}
	return strEncodedFilePath + pszFilePath;
}

BOOL CMiniMule::OnInitDialog()
{
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInCallback == 0);
	const CString &strHtmlFile = theApp.GetSkinFileItem(_T("MiniMule"), _T("HTML"));
	if (!strHtmlFile.IsEmpty()) {
		if (_taccess(strHtmlFile, 0) == 0) {
			m_strCurrentUrl = CreateFilePathUrl(strHtmlFile, INTERNET_SCHEME_FILE);
			m_nHtmlResID = 0;
			m_szHtmlResID = NULL;
			m_bResolveImages = false;
		}
	}

	if (m_strCurrentUrl.IsEmpty()) {
		TCHAR szModulePath[MAX_PATH];
		DWORD dwModPathLen = ::GetModuleFileName(AfxGetResourceHandle(), szModulePath, _countof(szModulePath));
		if (dwModPathLen != 0 && dwModPathLen < _countof(szModulePath)) {
			m_strCurrentUrl = CreateFilePathUrl(szModulePath, INTERNET_SCHEME_RES);
			m_strCurrentUrl.AppendFormat(_T("/%u"), m_nHtmlResID);
			m_nHtmlResID = 0;
			m_szHtmlResID = NULL;
			m_bResolveImages = true;
		}
	}

	// TODO: Only in debug build: Check the size of the dialog resource right before 'OnInitDialog'
	// to ensure the window is small enough!

	// PROBLEM: 'CDHtmlDialog::OnInitDialog' creates the IE control asynchronously and it causes the application
	// to dispatch messages - this creates a lot of problems. Therefore we have the 'm_iInInitDialog' flag which
	// must get evaluated whenever calling a MiniMule function, especially before the MiniMule gets destroyed.
	// Otherwise, we may crash due to deleting the MiniMule and OCX control while it is being created.

	TRACE("%s before CDHtmlDialog::OnInitDialog()\n", __FUNCTION__);
	ASSERT_VALID(this);
	ASSERT(m_iInInitDialog == 0);
	++m_iInInitDialog;

	CDHtmlDialog::OnInitDialog();

	--m_iInInitDialog;
	ASSERT(m_iInInitDialog == 0);
	TRACE("%s after CDHtmlDialog::OnInitDialog()\n", __FUNCTION__);
	ASSERT_VALID(this);

	if (m_uWndTransparency) {
		m_layeredWnd.AddLayeredStyle(m_hWnd);
		m_layeredWnd.SetTransparentPercentage(m_hWnd, m_uWndTransparency);
	}

	SetWindowText(_T("eMule v") + theApp.m_strCurVersionLong);

	return TRUE;  // return TRUE unless you set the focus to the control
}

void CMiniMule::OnClose()
{
	TRACE("%s\n", __FUNCTION__);
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	ASSERT(m_iInCallback == 0);
	KillAutoCloseTimer();

	if (GetAutoClose()) {
		BOOL(WINAPI *pfnAnimateWindow)(HWND hWnd, DWORD dwTime, DWORD dwFlags);
		(FARPROC&)pfnAnimateWindow = GetProcAddress(GetModuleHandle(_T("user32")), "AnimateWindow");
		if (pfnAnimateWindow)
			(*pfnAnimateWindow)(m_hWnd, 200, AW_HIDE | AW_BLEND | AW_CENTER);
	}

	CDHtmlDialog::OnClose();

	///////////////////////////////////////////////////////////////////////////
	// Destroy the MiniMule window

	// Solution #1: Posting a close-message to main window (can not be done with 'SendMessage') may
	// create message queue sync. problems when having high system load.
	//theApp.emuledlg->PostMessage(UM_CLOSE_MINIMULE, (WPARAM)m_bRestoreMainWnd);

	// Solution #2: 'DestroyModeless' -- posts a 'destroy' message to 'this' which will have a very
	// similar effect (and most likely problems) than using PostMessage(<main-window>).
	//DestroyModeless();

	// Solution #3: 'DestroyWindow' -- destroys the window and *deletes* 'this'. On return of
	// 'DestroyWindow' the 'this' is no longer valid! However, this should be safe because MFC
	// is also using the same 'technique' for several window classes.
	theApp.emuledlg->m_pMiniMule = NULL;
	bool bRestoreMainWnd = m_bRestoreMainWnd;
	DestroyWindow();
	//NOTE: 'this' IS NO LONGER VALID!
	if (bRestoreMainWnd)
		theApp.emuledlg->RestoreWindow();
}

void CMiniMule::OnDestroy()
{
	TRACE("%s\n", __FUNCTION__);
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	ASSERT(m_iInCallback == 0);
	KillAutoCloseTimer();
	CDHtmlDialog::OnDestroy();
}

void CMiniMule::PostNcDestroy()
{
	TRACE("%s\n", __FUNCTION__);
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CDHtmlDialog::PostNcDestroy();
	if (theApp.emuledlg)
		theApp.emuledlg->m_pMiniMule = NULL;
	delete this;
}

void CMiniMule::Localize()
{
	SetElementHtml(_T("connectedLabel"), CComBSTR(GetResString(IDS_CONNECTED)));
	SetElementHtml(_T("upRateLabel"), CComBSTR(GetResString(IDS_PW_CON_UPLBL)));
	SetElementHtml(_T("downRateLabel"), CComBSTR(GetResString(IDS_PW_CON_DOWNLBL)));
	SetElementHtml(_T("completedLabel"), CComBSTR(GetResString(IDS_DL_TRANSFCOMPL)));
	SetElementHtml(_T("freeSpaceLabel"), CComBSTR(GetResString(IDS_STATS_FREESPACE)));

	CComPtr<IHTMLElement> a;
	GetElementInterface(_T("openIncomingLink"), &a);
	if (a) {
		a->put_title(CComBSTR(GetResNoAmp(IDS_OPENINC)));
		a.Release();
	}
	GetElementInterface(_T("optionsLink"), &a);
	if (a) {
		a->put_title(CComBSTR(GetResNoAmp(IDS_EM_PREFS)));
		a.Release();
	}
	GetElementInterface(_T("restoreWndLink"), &a);
	if (a) {
		a->put_title(CComBSTR(GetResNoAmp(IDS_MAIN_POPUP_RESTORE)));
		a.Release();
	}

	CComPtr<IHTMLImgElement> img;
	GetElementInterface(_T("openIncomingImg"), &img);
	if (img) {
		img->put_alt(CComBSTR(GetResNoAmp(IDS_OPENINC)));
		img.Release();
	}
	GetElementInterface(_T("optionsImg"), &img);
	if (img) {
		img->put_alt(CComBSTR(GetResNoAmp(IDS_EM_PREFS)));
		img.Release();
	}
	GetElementInterface(_T("restoreWndImg"), &img);
	if (img) {
		img->put_alt(CComBSTR(GetResNoAmp(IDS_MAIN_POPUP_RESTORE)));
		img.Release();
	}
}

void CMiniMule::UpdateContent(UINT uUpDatarate, UINT uDownDatarate)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	if (m_bResolveImages) {
		static LPCTSTR const _apszConnectedImgs[] =
		{
			_T("CONNECTEDNOTNOT.GIF"),
			_T("CONNECTEDNOTLOW.GIF"),
			_T("CONNECTEDNOTHIGH.GIF"),
			_T("CONNECTEDLOWNOT.GIF"),
			_T("CONNECTEDLOWLOW.GIF"),
			_T("CONNECTEDLOWHIGH.GIF"),
			_T("CONNECTEDHIGHNOT.GIF"),
			_T("CONNECTEDHIGHLOW.GIF"),
			_T("CONNECTEDHIGHHIGH.GIF")
		};

		UINT uIconIdx = theApp.emuledlg->GetConnectionStateIconIndex();
		ASSERT(uIconIdx < _countof(_apszConnectedImgs));

		TCHAR szModulePath[MAX_PATH];
		DWORD dwModPathLen = ::GetModuleFileName(AfxGetResourceHandle(), szModulePath, _countof(szModulePath));
		if (dwModPathLen != 0 && dwModPathLen < _countof(szModulePath)) {
			CComPtr<IHTMLImgElement> img;
			GetElementInterface(_T("connectedImg"), &img);
			if (img) {
				CString strFilePathUrl(CreateFilePathUrl(szModulePath, INTERNET_SCHEME_RES));
				CString strResourceURL;
				strResourceURL.Format(_T("%s/%s"), (LPCTSTR)strFilePathUrl, _apszConnectedImgs[uIconIdx]);
				img->put_src(CComBSTR(strResourceURL));
			}
		}
	}

	SetElementHtml(_T("connected"), CComBSTR(GetResString(theApp.IsConnected() ? IDS_YES : IDS_NO)));
	SetElementHtml(_T("upRate"), CComBSTR(theApp.emuledlg->GetUpDatarateString(uUpDatarate)));
	SetElementHtml(_T("downRate"), CComBSTR(theApp.emuledlg->GetDownDatarateString(uDownDatarate)));
	UINT uCompleted = 0;
	if (thePrefs.GetRemoveFinishedDownloads())
		uCompleted = thePrefs.GetDownSessionCompletedFiles();
	else if (theApp.emuledlg->transferwnd && theApp.emuledlg->transferwnd->GetDownloadList()->m_hWnd) {
		int iTotal;
		uCompleted = theApp.emuledlg->transferwnd->GetDownloadList()->GetCompleteDownloads(-1, iTotal);	 // [Ded]: -1 to get the count of all completed files in all categories
	}
	SetElementHtml(_T("completed"), CComBSTR(CastItoIShort(uCompleted, false, 0)));
	SetElementHtml(_T("freeSpace"), CComBSTR(CastItoXBytes(GetFreeTempSpace(-1))));
}

STDMETHODIMP CMiniMule::TranslateUrl(DWORD /*dwTranslate*/, OLECHAR *pchURLIn, OLECHAR **ppchURLOut) noexcept
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	UNREFERENCED_PARAMETER(pchURLIn);
	TRACE(_T("%hs: %ls\n"), __FUNCTION__, pchURLIn);
	*ppchURLOut = NULL;
	return S_FALSE;
}

void CMiniMule::_OnBeforeNavigate2(LPDISPATCH pDisp, VARIANT *URL, VARIANT* /*Flags*/, VARIANT* /*TargetFrameName*/, VARIANT* /*PostData*/, VARIANT* /*Headers*/, BOOL *Cancel)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	CString strURL(V_BSTR(URL));
	TRACE(_T("%hs: %s\n"), __FUNCTION__, (LPCTSTR)strURL);

	// No external links allowed!
	TCHAR szScheme[INTERNET_MAX_SCHEME_LENGTH];
	URL_COMPONENTS Url = {};
	Url.dwStructSize = sizeof Url;
	Url.lpszScheme = szScheme;
	Url.dwSchemeLength = _countof(szScheme);
	if (::InternetCrackUrl(strURL, 0, 0, &Url) && Url.dwSchemeLength) {
		if (Url.nScheme != INTERNET_SCHEME_UNKNOWN  // <absolute local file path>
			&& Url.nScheme != INTERNET_SCHEME_RES	// res://...
			&& Url.nScheme != INTERNET_SCHEME_FILE)	// file://...
		{
			*Cancel = TRUE;
			return;
		}
	}

	OnBeforeNavigate(pDisp, strURL);
}

void CMiniMule::OnBeforeNavigate(LPDISPATCH pDisp, LPCTSTR pszUrl)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	TRACE(_T("%hs: %s\n"), __FUNCTION__, pszUrl);
	CDHtmlDialog::OnBeforeNavigate(pDisp, pszUrl);
}

void CMiniMule::OnNavigateComplete(LPDISPATCH pDisp, LPCTSTR pszUrl)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	TRACE(_T("%hs: %s\n"), __FUNCTION__, pszUrl);
	// If the HTML file contains 'OnLoad' scripts, the HTML DOM is fully accessible
	// only after 'DocumentComplete', but not after 'OnNavigateComplete'
	CDHtmlDialog::OnNavigateComplete(pDisp, pszUrl);
}

void CMiniMule::OnDocumentComplete(LPDISPATCH pDisp, LPCTSTR pszUrl)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	if (theApp.emuledlg->m_pMiniMule == NULL) {
		// FIX ME
		// apparently in some rare cases (high cpu load, fast double clicks) this function is called when the object was destroyed already
		ASSERT(0);
		return;
	}

	CCounter cc(m_iInCallback);

	TRACE(_T("%hs: %s\n"), __FUNCTION__, pszUrl);
	// If the HTML file contains 'OnLoad' scripts, the HTML DOM is fully accessible
	// only after 'DocumentComplete', but not after 'OnNavigateComplete'
	CDHtmlDialog::OnDocumentComplete(pDisp, pszUrl);

	if (m_bResolveImages) {
		TCHAR szModulePath[MAX_PATH];
		DWORD dwModPathLen = ::GetModuleFileName(AfxGetResourceHandle(), szModulePath, _countof(szModulePath));
		if (dwModPathLen != 0 && dwModPathLen < _countof(szModulePath)) {
			CString strFilePathUrl(CreateFilePathUrl(szModulePath, INTERNET_SCHEME_RES));

			static const struct
			{
				LPCTSTR pszImgId;
				LPCTSTR pszResourceId;
			} _aImg[] = {
				{ _T("connectedImg"),	_T("CONNECTED.GIF") },
				{ _T("uploadImg"),		_T("UPLOAD.GIF") },
				{ _T("downloadImg"),	_T("DOWNLOAD.GIF") },
				{ _T("completedImg"),	_T("COMPLETED.GIF") },
				{ _T("freeSpaceImg"),	_T("FREESPACE.GIF") },
				{ _T("restoreWndImg"),	_T("RESTOREWINDOW.GIF") },
				{ _T("openIncomingImg"),_T("OPENINCOMING.GIF") },
				{ _T("optionsImg"),		_T("PREFERENCES.GIF") }
			};

			for (unsigned i = 0; i < _countof(_aImg); ++i) {
				CComPtr<IHTMLImgElement> elm;
				GetElementInterface(_aImg[i].pszImgId, &elm);
				if (elm) {
					CString strResourceURL;
					strResourceURL.Format(_T("%s/%s"), (LPCTSTR)strFilePathUrl, _aImg[i].pszResourceId);
					elm->put_src(CComBSTR(strResourceURL));
				}
			}

			CComPtr<IHTMLTable> elm;
			GetElementInterface(_T("table"), &elm);
			if (elm) {
				CString strResourceURL;
				strResourceURL.Format(_T("%s/%s"), (LPCTSTR)strFilePathUrl, _T("TABLEBACKGND.GIF"));
				elm->put_background(CComBSTR(strResourceURL));
				elm.Release();
			}
		}
	}

	Localize();
	UpdateContent();

	if (m_spHtmlDoc) {
		CComQIPtr<IHTMLElement> body;
		if (m_spHtmlDoc->get_body(&body) == S_OK && body) {
			// NOTE: The IE control will always use the size of the associated dialog resource (IDD_MINIMULE)
			// as the minimum window size. 'scrollWidth' and 'scrollHeight' will therefore never return values
			// smaller than the size of that window. To have the auto-size working correctly even for
			// very small window sizes, the size of the dialog resource should therefore be kept very small!
			// TODO: Only in debug build: Check the size of the dialog resource right before 'OnInitDialog'.
			CComQIPtr<IHTMLElement2> body2 = body;
			long lScrollWidth;
			long lScrollHeight;
			if (body2->get_scrollWidth(&lScrollWidth) == S_OK && lScrollWidth > 0 && body2->get_scrollHeight(&lScrollHeight) == S_OK && lScrollHeight > 0)
				AutoSizeAndPosition(CSize(lScrollWidth, lScrollHeight));
		}
	}

	if (m_bAutoClose)
		CreateAutoCloseTimer();
}

#if 0
UINT GetTaskbarPos(HWND hwndTaskbar)
{
	if (hwndTaskbar != NULL) {
		// See also: Q179908
		//
		// However, using 'SHAppBarMessage' is quite *dangerous* because it sends a message to a different thread. This
		// means that the sending thread will process incoming non-queued messages while waiting for its message to be processed.
		// In other words, while processing 'SHAppBarMessage' our thread will receive incoming messages.
		//
		APPBARDATA abd = {};
		abd.cbSize = (DWORD)sizeof abd;
		abd.hWnd = hwndTaskbar;
		SHAppBarMessage(ABM_GETTASKBARPOS, &abd);

		// SHAppBarMessage may fail to get the rectangle...
		CRect rcAppBar(abd.rc);
		if (rcAppBar.IsRectEmpty())
			GetWindowRect(hwndTaskbar, &abd.rc);

		if (abd.rc.top == abd.rc.left && abd.rc.bottom > abd.rc.right)
			return ABE_LEFT;
		else if (abd.rc.top == abd.rc.left && abd.rc.bottom < abd.rc.right)
			return ABE_TOP;
		else if (abd.rc.top > abd.rc.left)
			return ABE_BOTTOM;
		return ABE_RIGHT;
	}
	return ABE_BOTTOM;
}
#endif

void CMiniMule::AutoSizeAndPosition(CSize sizClient)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	TRACE("AutoSizeAndPosition: %dx%d\n", sizClient.cx, sizClient.cy);
	CSize sizDesktop(::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN));
	if (sizClient.cx > sizDesktop.cx / 2)
		sizClient.cx = sizDesktop.cx / 2;
	if (sizClient.cy > sizDesktop.cy / 2)
		sizClient.cy = sizDesktop.cy / 2;

	CRect rcWnd;
	GetWindowRect(&rcWnd);
	if (sizClient.cx > 0 && sizClient.cy > 0) {
		CRect rcClient(0, 0, sizClient.cx, sizClient.cy);
		::AdjustWindowRectEx(&rcClient, GetStyle(), FALSE, GetExStyle());
		rcClient.OffsetRect(-rcClient.left, -rcClient.top);
		rcWnd = rcClient;
	}

	CRect rcTaskbar(0, sizDesktop.cy - 34, sizDesktop.cx, sizDesktop.cy);
	HWND hWndTaskbar = ::FindWindow(_T("Shell_TrayWnd"), NULL);
	if (hWndTaskbar)
		::GetWindowRect(hWndTaskbar, &rcTaskbar);
	CPoint ptWnd;
	UINT uTaskbarPos;
#if 0
	// Do *NOT* use 'GetTaskbarPos' (which will use 'SHAppBarMessage') -- it may cause us to crash due to internal
	// details in 'SHAppBarMessage' !
	uTaskbarPos = GetTaskbarPos(hWndTaskbar);
#else
	if (rcTaskbar.left <= 0) {
		if (rcTaskbar.top <= 0) {
			if (rcTaskbar.Width() > rcTaskbar.Height())
				uTaskbarPos = ABE_TOP;
			else
				uTaskbarPos = ABE_LEFT;
		} else
			uTaskbarPos = ABE_BOTTOM;
	} else
		uTaskbarPos = ABE_RIGHT;
#endif
	switch (uTaskbarPos) {
	case ABE_TOP:
		ptWnd.SetPoint(sizDesktop.cx - 8 - rcWnd.Width(), rcTaskbar.Height() + 8);
		break;
	case ABE_LEFT:
		ptWnd.SetPoint(rcTaskbar.Width() + 8, sizDesktop.cy - 8 - rcWnd.Height());
		break;
	case ABE_RIGHT:
		ptWnd.SetPoint(sizDesktop.cx - rcTaskbar.Width() - 8 - rcWnd.Width(), sizDesktop.cy - 8 - rcWnd.Height());
		break;
	default:
		ASSERT(uTaskbarPos == ABE_BOTTOM);
		ptWnd.SetPoint(sizDesktop.cx - 8 - rcWnd.Width(), sizDesktop.cy - rcTaskbar.Height() - 8 - rcWnd.Height());
	}

	SetWindowPos(NULL, ptWnd.x, ptWnd.y, rcWnd.Width(), rcWnd.Height(), SWP_NOZORDER | SWP_SHOWWINDOW);
}

void CMiniMule::CreateAutoCloseTimer()
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	if (m_uAutoCloseTimer == 0)
		m_uAutoCloseTimer = SetTimer(IDT_AUTO_CLOSE_TIMER, SEC2MS(3), NULL);
}

void CMiniMule::KillAutoCloseTimer()
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	if (m_uAutoCloseTimer != 0) {
		VERIFY(KillTimer(m_uAutoCloseTimer));
		m_uAutoCloseTimer = 0;
	}
}

void CMiniMule::OnTimer(UINT_PTR nIDEvent)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	if (m_bAutoClose && nIDEvent == m_uAutoCloseTimer) {
		ASSERT(m_iInInitDialog == 0);
		KillAutoCloseTimer();

		CPoint pt;
		::GetCursorPos(&pt);
		CRect rcWnd;
		GetWindowRect(&rcWnd);
		if (!rcWnd.PtInRect(pt))
			PostMessage(WM_CLOSE);
		else
			CreateAutoCloseTimer();
	}
	CDHtmlDialog::OnTimer(nIDEvent);
}

void CMiniMule::RestoreMainWindow()
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	if (!theApp.IsClosing() && !theApp.emuledlg->IsWindowVisible()) {
		if (!theApp.emuledlg->IsPreferencesDlgOpen()) {
			KillAutoCloseTimer();
			m_bRestoreMainWnd = true;
			PostMessage(WM_CLOSE);
		} else
			MessageBeep(MB_OK);
	}
}

void CMiniMule::OnNcLButtonDblClk(UINT nHitTest, CPoint point)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CDHtmlDialog::OnNcLButtonDblClk(nHitTest, point);
	if (nHitTest == HTCAPTION)
		RestoreMainWindow();
}

HRESULT CMiniMule::OnRestoreMainWindow(IHTMLElement* /*pElement*/)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CCounter cc(m_iInCallback);
	RestoreMainWindow();
	return S_OK;
}

HRESULT CMiniMule::OnOpenIncomingFolder(IHTMLElement* /*pElement*/)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CCounter cc(m_iInCallback);
	if (!theApp.IsClosing()) {
		theApp.emuledlg->SendMessage(WM_COMMAND, MP_HM_OPENINC);
		if (GetAutoClose())
			PostMessage(WM_CLOSE);
	}
	return S_OK;
}

HRESULT CMiniMule::OnOptions(IHTMLElement* /*pElement*/)
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CCounter cc(m_iInCallback);
	if (!theApp.IsClosing()) {
		// showing the 'Pref' dialog will process the message queue -> timer messages will be dispatched -> kill auto close timer!
		KillAutoCloseTimer();
		if (theApp.emuledlg->ShowPreferences() == -1)
			MessageBeep(MB_OK);
		if (GetAutoClose())
			CreateAutoCloseTimer();
	}
	return S_OK;
}

STDMETHODIMP CMiniMule::ShowContextMenu(DWORD /*dwID*/, POINT* /*ppt*/, IUnknown* /*pcmdtReserved*/, IDispatch* /*pdispReserved*/) noexcept
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CCounter cc(m_iInCallback);
	// Avoid IE context menu
	return S_OK;	// S_OK = Host displayed its own user interface (UI). MSHTML will not attempt to display its UI.
}

STDMETHODIMP CMiniMule::TranslateAccelerator(LPMSG lpMsg, const GUID* /*pguidCmdGroup*/, DWORD /*nCmdID*/) noexcept
{
	ASSERT_VALID(this);
	ASSERT(GetCurrentThreadId() == g_uMainThreadId);
	ASSERT(m_iInInitDialog == 0);
	CCounter cc(m_iInCallback);
	// Allow only some basic keys
	//
	//TODO: Allow the ESC key (for closing the window); does currently not work properly because
	// we don't get a callback that the window was just hidden(!) by MSHTML.
	switch (lpMsg->message) {
	case WM_CHAR:
		switch (lpMsg->wParam) {
		case ' ':			// SPACE - Activate a link
			return S_FALSE;	// S_FALSE = Let the control process the key stroke.
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		switch (lpMsg->wParam) {
		case VK_TAB:		// Cycling through controls which can get the focus
		case VK_SPACE:		// Activate a link
			return S_FALSE; // S_FALSE = Let the control process the key stroke.
		case VK_ESCAPE:
			//TODO: Small problem here. If the options dialog was open and was closed with ESC,
			//we still get an ESC here too and the HTML window would be closed too.
			//PostMessage(WM_CLOSE);
			break;
		}
		break;
	}

	// Avoid any IE shortcuts (especially F5 (Refresh) which messes up the content)
	return S_OK;	// S_OK = Don't let the control process the key stroke.
}