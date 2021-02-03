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
//#ifdef _DEBUG
//#define _CRTDBG_MAP_ALLOC
//#include <crtdbg.h>
//#endif
#include <locale.h>
#include <io.h>
#include <share.h>
#include <Mmsystem.h>
#include <atlimage.h>
#include "emule.h"
#include "opcodes.h"
#include "mdump.h"
#include "Scheduler.h"
#include "SearchList.h"
#include "kademlia/kademlia/Error.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/utils/UInt128.h"
#include "PerfLog.h"
#include <sockimpl.h> //for *m_pfnSockTerm()
#include "LastCommonRouteFinder.h"
#include "UploadBandwidthThrottler.h"
#include "ClientList.h"
#include "FriendList.h"
#include "ClientUDPSocket.h"
#include "DownloadQueue.h"
#include "IPFilter.h"
#include "Statistics.h"
#include "OtherFunctions.h"
#include "WebServer.h"
#include "UploadQueue.h"
#include "SharedFileList.h"
#include "ServerList.h"
#include "ServerConnect.h"
#include "ListenSocket.h"
#include "ClientCredits.h"
#include "KnownFileList.h"
#include "Server.h"
#include "UpDownClient.h"
#include "ED2KLink.h"
#include "Preferences.h"
#include "secrunasuser.h"
#include "SafeFile.h"
#include "PeerCacheFinder.h"
#include "emuleDlg.h"
#include "SearchDlg.h"
#include "enbitmap.h"
#include "FirewallOpener.h"
#include "StringConversion.h"
#include "Log.h"
#include "Collection.h"
#include "LangIDs.h"
#include "HelpIDs.h"
#include "UPnPImplWrapper.h"
#include "VisualStylesXP.h"
#include "UploadDiskIOThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#if _MSC_VER>=1400 && defined(_UNICODE)
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

CLogFile theLog;
CLogFile theVerboseLog;
bool g_bLowColorDesktop = false;
bool g_bGdiPlusInstalled = false;

//#define USE_16COLOR_ICONS

///////////////////////////////////////////////////////////////////////////////
// C-RTL Memory Debug Support
//
#ifdef _DEBUG
static CMemoryState oldMemState, newMemState, diffMemState;

_CRT_ALLOC_HOOK g_pfnPrevCrtAllocHook = NULL;
CMap<const unsigned char*, const unsigned char*, UINT, UINT> g_allocations;
int eMuleAllocHook(int mode, void *pUserData, size_t nSize, int nBlockUse, long lRequest, const unsigned char *pszFileName, int nLine) noexcept;

//CString _strCrtDebugReportFilePath(_T("eMule CRT Debug Log.log"));
// don't use a CString for that memory - it will not be available on application termination!
#define APP_CRT_DEBUG_LOG_FILE _T("eMule CRT Debug Log.log")
static TCHAR s_szCrtDebugReportFilePath[MAX_PATH] = APP_CRT_DEBUG_LOG_FILE;
#endif //_DEBUG

#ifdef _M_IX86
///////////////////////////////////////////////////////////////////////////////
// SafeSEH - Safe Exception Handlers
//
// This security feature must be enabled at compile time, due to using the
// linker command line option "/SafeSEH". Depending on the used libraries and
// object files which are used to link eMule.exe, the linker may or may not
// throw some errors about 'safeseh'. Those errors have to get resolved until
// the linker is capable of linking eMule.exe *with* "/SafeSEH".
//
// At runtime, we just can check if the linker created an according SafeSEH
// exception table in the '__safe_se_handler_table' object. If SafeSEH was not
// specified at all during link time, the address of '__safe_se_handler_table'
// is NULL -> hence, no SafeSEH is enabled.
///////////////////////////////////////////////////////////////////////////////
extern "C" PVOID __safe_se_handler_table[];
extern "C" BYTE  __safe_se_handler_count;

void InitSafeSEH()
{
	// Need to workaround the optimizer of the C-compiler...
	volatile PVOID safe_se_handler_table = __safe_se_handler_table;
	if (safe_se_handler_table == NULL)
		AfxMessageBox(_T("eMule.exe was not linked with /SafeSEH!"), MB_ICONSTOP);
}
#endif //_M_IX86

///////////////////////////////////////////////////////////////////////////////
// DEP - Data Execution Prevention
//
// For Windows XP SP2 and later. Does *not* have any performance impact!
//
// VS2003:	DEP must be enabled dynamically because the linker does not support
//			the "/NXCOMPAT" command line option.
// VS2005:	DEP can get enabled at link time by using the "/NXCOMPAT" command
//			line option.
// VS2008:	DEP can get enabled at link time by using the "DEP" option within
//			'Visual Studio Linker Advanced Options'.
//
#ifndef PROCESS_DEP_ENABLE
#define	PROCESS_DEP_ENABLE						0x00000001
#define	PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION	0x00000002
#endif//!PROCESS_DEP_ENABLE

void InitDEP()
{
	BOOL(WINAPI *pfnGetProcessDEPPolicy)(HANDLE hProcess, LPDWORD lpFlags, PBOOL lpPermanent);
	BOOL(WINAPI *pfnSetProcessDEPPolicy)(DWORD dwFlags);
	(FARPROC&)pfnGetProcessDEPPolicy = GetProcAddress(GetModuleHandle(_T("kernel32")), "GetProcessDEPPolicy");
	(FARPROC&)pfnSetProcessDEPPolicy = GetProcAddress(GetModuleHandle(_T("kernel32")), "SetProcessDEPPolicy");
	if (pfnGetProcessDEPPolicy && pfnSetProcessDEPPolicy) {
		DWORD dwFlags;
		BOOL bPermanent;
		if ((*pfnGetProcessDEPPolicy)(GetCurrentProcess(), &dwFlags, &bPermanent)) {
			// Vista SP1
			// ===============================================================
			//
			// BOOT.INI nx=OptIn,  VS2003/VS2005
			// ---------------------------------
			// DEP flags: 00000000
			// Permanent: 0
			//
			// BOOT.INI nx=OptOut, VS2003/VS2005
			// ---------------------------------
			// DEP flags: 00000001 (PROCESS_DEP_ENABLE)
			// Permanent: 0
			//
			// BOOT.INI nx=OptIn/OptOut, VS2003 + EditBinX/NXCOMPAT
			// ----------------------------------------------------
			// DEP flags: 00000003 (PROCESS_DEP_ENABLE | *PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION*)
			// Permanent: *1*
			// ---
			// There is no way to remove the PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION flag at runtime,
			// because the DEP policy is already permanent due to the NXCOMPAT flag.
			//
			// BOOT.INI nx=OptIn/OptOut, VS2005 + /NXCOMPAT
			// --------------------------------------------
			// DEP flags: 00000003 (PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION)
			// Permanent: *1*
			//
			// NOTE: It is ultimately important to explicitly enable the DEP policy even if the
			// process' DEP policy is already enabled. If the DEP policy is already enabled due
			// to an OptOut system policy, the DEP policy is though not yet permanent. As long as
			// the DEP policy is not permanent it could get changed during runtime...
			//
			// So, if the DEP policy for the current process is already enabled but not permanent,
			// it has to be explicitly enabled by calling 'SetProcessDEPPolicy' to make it permanent.
			//
			if (((dwFlags & PROCESS_DEP_ENABLE) == 0 || !bPermanent)
#if _ATL_VER>0x0710
				|| (dwFlags & PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION) == 0
#endif
				)
			{
				 // VS2003:	Enable DEP (with ATL-thunk emulation) if not already set by system policy
				 //			or if the policy is not yet permanent.
				 //
				 // VS2005:	Enable DEP (without ATL-thunk emulation) if not already set by system policy
				 //			or linker "/NXCOMPAT" option or if the policy is not yet permanent. We should
				 //			not reach this code path at all because the "/NXCOMPAT" option is specified.
				 //			However, the code path is here for safety reasons.
				dwFlags = PROCESS_DEP_ENABLE;
				// VS2005: Disable ATL-thunks.
				dwFlags |= PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION;
				(*pfnSetProcessDEPPolicy)(dwFlags);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Heap Corruption Detection
//
// For Windows Vista and later. Does *not* have any performance impact!
//
#ifndef HeapEnableTerminationOnCorruption
#define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS)1
#endif//!HeapEnableTerminationOnCorruption

void InitHeapCorruptionDetection()
{
	BOOL(WINAPI *pfnHeapSetInformation)(HANDLE HeapHandle, HEAP_INFORMATION_CLASS HeapInformationClass, PVOID HeapInformation, SIZE_T HeapInformationLength);
	(FARPROC &)pfnHeapSetInformation = GetProcAddress(GetModuleHandle(_T("kernel32")), "HeapSetInformation");
	if (pfnHeapSetInformation)
		(*pfnHeapSetInformation)(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
}


struct SLogItem
{
	UINT uFlags;
	CString line;
};

void CALLBACK myErrHandler(Kademlia::CKademliaError *error)
{
	CString msg;
	msg.Format(_T("\r\nError 0x%08X : %hs\r\n"), error->m_iErrorCode, error->m_szErrorDescription);
	if (!theApp.IsClosing())
		theApp.QueueDebugLogLine(false, _T("%s"), (LPCTSTR)msg);
}

void CALLBACK myDebugAndLogHandler(LPCSTR lpMsg)
{
	if (!theApp.IsClosing())
		theApp.QueueDebugLogLine(false, _T("%hs"), lpMsg);
}

void CALLBACK myLogHandler(LPCSTR lpMsg)
{
	if (!theApp.IsClosing())
		theApp.QueueLogLine(false, _T("%hs"), lpMsg);
}

static const UINT UWM_ARE_YOU_EMULE = RegisterWindowMessage(EMULE_GUID);

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) noexcept;

///////////////////////////////////////////////////////////////////////////////
// CemuleApp

BEGIN_MESSAGE_MAP(CemuleApp, CWinApp)
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP()

CemuleApp::CemuleApp(LPCTSTR lpszAppName)
	: CWinApp(lpszAppName)
	, emuledlg()
	, m_iDfltImageListColorFlags(ILC_COLOR)
	, m_ullComCtrlVer(MAKEDLLVERULL(4, 0, 0, 0))
	, m_app_state(APP_STATE_STARTING)
	, m_hSystemImageList()
	, m_sizSmallSystemIcon(16, 16)
	, m_hBigSystemImageList()
	, m_sizBigSystemIcon(32, 32)
	, m_strDefaultFontFaceName(_T("MS Shell Dlg 2"))
	, m_dwPublicIP()
	, m_bGuardClipboardPrompt()
	, m_bAutoStart()
{
	// Initialize Windows security features.
#if !defined(_DEBUG) && !defined(_WIN64)
	InitSafeSEH();
#endif
	InitDEP();
	InitHeapCorruptionDetection();

	// This does not seem to work well with multithreading, although there is no reason why it should not.
	//_set_sbh_threshold(768);

	srand((unsigned)time(NULL));

	// NOTE: Do *NOT* forget to specify /DELAYLOAD:gdiplus.dll as link parameter.
	HMODULE hLib = LoadLibrary(_T("gdiplus.dll"));
	if (hLib != NULL) {
		g_bGdiPlusInstalled = GetProcAddress(hLib, "GdiplusStartup") != NULL;
		FreeLibrary(hLib);
	}

// MOD Note: Do not change this part - Merkur

	// this is the "base" version number <major>.<minor>.<update>.<build>
	m_dwProductVersionMS = MAKELONG(CemuleApp::m_nVersionMin, CemuleApp::m_nVersionMjr);
	m_dwProductVersionLS = MAKELONG(CemuleApp::m_nVersionBld, CemuleApp::m_nVersionUpd);

	// create a string version (e.g. "0.30a")
	ASSERT(CemuleApp::m_nVersionUpd + 'a' <= 'f');
	m_strCurVersionLongDbg.Format(_T("%u.%u%c.%u"), CemuleApp::m_nVersionMjr, CemuleApp::m_nVersionMin, _T('a') + CemuleApp::m_nVersionUpd, CemuleApp::m_nVersionBld);
#if defined( _DEBUG) || defined(_DEVBUILD)
	m_strCurVersionLong = m_strCurVersionLongDbg;
#else
	m_strCurVersionLong.Format(_T("%u.%u%c"), CemuleApp::m_nVersionMjr, CemuleApp::m_nVersionMin, _T('a') + CemuleApp::m_nVersionUpd);
#endif
	m_strCurVersionLong += CemuleApp::m_sPlatform;

#if defined( _DEBUG) && !defined(_BOOTSTRAPNODESDAT)
	m_strCurVersionLong += _T(" DEBUG");
#endif
#ifdef _BETA
	m_strCurVersionLong += _T(" BETA");
#endif
#ifdef _DEVBUILD
	m_strCurVersionLong += _T(" DEVBUILD");
#endif
#ifdef _BOOTSTRAPNODESDAT
	m_strCurVersionLong += _T(" BOOTSTRAP BUILD");
#endif

	// create the protocol version number
	CString strTmp;
	strTmp.Format(_T("0x%u"), m_dwProductVersionMS);
	VERIFY(_stscanf(strTmp, _T("0x%x"), &m_uCurVersionShort) == 1);
	ASSERT(m_uCurVersionShort < 0x99);

	// create the version check number
	strTmp.Format(_T("0x%u%c"), m_dwProductVersionMS, _T('A') + CemuleApp::m_nVersionUpd);
	VERIFY(_stscanf(strTmp, _T("0x%x"), &m_uCurVersionCheck) == 1);
	ASSERT(m_uCurVersionCheck < 0x999);
// MOD Note: end

	EnableHtmlHelp();
}

// Barry - To find out if app is running or shutting/shut down
bool CemuleApp::IsRunning() const
{
	return m_app_state == APP_STATE_RUNNING || m_app_state == APP_STATE_ASKCLOSE;
}

bool CemuleApp::IsClosing() const
{
	return m_app_state == APP_STATE_SHUTTINGDOWN || m_app_state == APP_STATE_DONE;
}


CemuleApp theApp(_T("eMule"));


// Workaround for bugged 'AfxSocketTerm' (needed at least for MFC 7.0 - 14.14)
void __cdecl __AfxSocketTerm() noexcept
{
	_AFX_SOCK_STATE *pState = _afxSockState.GetData();
	if (pState->m_pfnSockTerm != NULL) {
		VERIFY(WSACleanup() == 0);
		pState->m_pfnSockTerm = NULL;
	}
}

BOOL InitWinsock2(WSADATA *lpwsaData)
{
	_AFX_SOCK_STATE *pState = _afxSockState.GetData();
	if (pState->m_pfnSockTerm == NULL) {
		// initialize Winsock library
		WSADATA wsaData;
		if (lpwsaData == NULL)
			lpwsaData = &wsaData;
		static const WORD wVersionRequested = MAKEWORD(2, 2);
		int nResult = WSAStartup(wVersionRequested, lpwsaData);
		if (nResult != 0)
			return FALSE;
		if (lpwsaData->wVersion != wVersionRequested) {
			WSACleanup();
			return FALSE;
		}
		// setup for termination of sockets
		pState->m_pfnSockTerm = &AfxSocketTerm;
	}
#ifndef _AFXDLL
	//BLOCK: setup maps and lists specific to socket state
	{
		_AFX_SOCK_THREAD_STATE *pThreadState = _afxSockThreadState;
		if (pThreadState->m_pmapSocketHandle == NULL)
			pThreadState->m_pmapSocketHandle = new CMapPtrToPtr;
		if (pThreadState->m_pmapDeadSockets == NULL)
			pThreadState->m_pmapDeadSockets = new CMapPtrToPtr;
		if (pThreadState->m_plistSocketNotifications == NULL)
			pThreadState->m_plistSocketNotifications = new CPtrList;
	}
#endif
	return TRUE;
}

// CemuleApp Initialisierung

BOOL CemuleApp::InitInstance()
{
#ifdef _DEBUG
	// set Floating Point Processor to throw several exceptions, in particular the 'Floating point divide by zero'
	UINT uEmCtrlWord = _control87(0, 0) & _MCW_EM;
	_control87(uEmCtrlWord & ~(/*_EM_INEXACT |*/ _EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID), _MCW_EM);

	// output all ASSERT messages to debug device
	_CrtSetReportMode(_CRT_ASSERT, _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_REPORT_MODE) | _CRTDBG_MODE_DEBUG);
#endif
	free((void*)m_pszProfileName);
	m_pszProfileName = _tcsdup(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("preferences.ini"));

#ifdef _DEBUG
	oldMemState.Checkpoint();
	// Installing that memory debug code works fine in Debug builds when running within VS Debugger,
	// but some other test applications don't like that all....
	//g_pfnPrevCrtAllocHook = _CrtSetAllocHook(&eMuleAllocHook);
#endif
	//afxMemDF = allocMemDF | delayFreeMemDF;


	///////////////////////////////////////////////////////////////////////////
	// Install crash dump creation
	//
	theCrashDumper.uCreateCrashDump = GetProfileInt(_T("eMule"), _T("CreateCrashDump"), 0);
#if !defined(_BETA) && !defined(_DEVBUILD)
	if (theCrashDumper.uCreateCrashDump > 0)
#endif
		theCrashDumper.Enable(_T("eMule ") + m_strCurVersionLongDbg, true, thePrefs.GetMuleDirectory(EMULE_CONFIGDIR));

	///////////////////////////////////////////////////////////////////////////
	// Locale initialization -- BE VERY CAREFUL HERE!!!
	//
	_tsetlocale(LC_ALL, _T(""));		// set all categories of locale to user-default ANSI code page obtained from the OS.
	_tsetlocale(LC_NUMERIC, _T("C"));	// set numeric category to 'C'
	//_tsetlocale(LC_CTYPE, _T("C"));		// set character types category to 'C' (VERY IMPORTANT, we need binary string compares!)

	AfxOleInit();

	if (ProcessCommandline())
		return FALSE;

	///////////////////////////////////////////////////////////////////////////
	// Common Controls initialization
	//
	//						Mjr Min
	// ----------------------------
	// W98 SE, IE5			5	8
	// W2K SP4, IE6 SP1		5	81
	// XP SP2				6   0
	// XP SP3				6   0
	// Vista SP1			6   16
	InitCommonControls();
	switch (thePrefs.GetWindowsVersion()) {
	case _WINVER_2K_:
		m_ullComCtrlVer = MAKEDLLVERULL(5, 81, 0, 0);
		break;
	case _WINVER_XP_:
	case _WINVER_2003_:
		m_ullComCtrlVer = MAKEDLLVERULL(6, 0, 0, 0);
		break;
	default:  //Vista .. Win10
		m_ullComCtrlVer = MAKEDLLVERULL(6, 16, 0, 0);
	};

	m_sizSmallSystemIcon.cx = ::GetSystemMetrics(SM_CXSMICON);
	m_sizSmallSystemIcon.cy = ::GetSystemMetrics(SM_CYSMICON);
	UpdateLargeIconSize();
	UpdateDesktopColorDepth();

	CWinApp::InitInstance();

	if (!InitWinsock2(&m_wsaData) && !AfxSocketInit(&m_wsaData)) {
		LocMessageBox(IDS_SOCKETS_INIT_FAILED, MB_OK, 0);
		return FALSE;
	}

	atexit(__AfxSocketTerm);

	AfxEnableControlContainer();
	if (!AfxInitRichEdit2() && !AfxInitRichEdit())
		AfxMessageBox(_T("Fatal Error: No Rich Edit control library found!")); // should never happen.

	if (!Kademlia::CKademlia::InitUnicode(AfxGetInstanceHandle())) {
		AfxMessageBox(_T("Fatal Error: Failed to load Unicode character tables for Kademlia!")); // should never happen.
		return FALSE; // DO *NOT* START !!!
	}

	extern bool SelfTest();
	if (!SelfTest())
		return FALSE; // DO *NOT* START !!!

	// create & initialize all the important stuff
	thePrefs.Init();
	theStats.Init();

	// check if we have to restart eMule as Secure user
	if (thePrefs.IsRunAsUserEnabled()) {
		CSecRunAsUser rau;
		eResult res = rau.RestartSecure();
		if (res == RES_OK_NEED_RESTART)
			return FALSE; // emule restart as secure user, kill this instance
		if (res == RES_FAILED)
			// something went wrong
			theApp.QueueLogLine(false, GetResString(IDS_RAU_FAILED), (LPCTSTR)rau.GetCurrentUserW());
	}

	if (thePrefs.GetRTLWindowsLayout())
		EnableRTLWindowsLayout();

#ifdef _DEBUG
	_sntprintf(s_szCrtDebugReportFilePath, _countof(s_szCrtDebugReportFilePath) - 1, _T("%s%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_LOGDIR, false), APP_CRT_DEBUG_LOG_FILE);
#endif
	VERIFY(theLog.SetFilePath(thePrefs.GetMuleDirectory(EMULE_LOGDIR, thePrefs.GetLog2Disk()) + _T("eMule.log")));
	VERIFY(theVerboseLog.SetFilePath(thePrefs.GetMuleDirectory(EMULE_LOGDIR, false) + _T("eMule_Verbose.log")));
	theLog.SetMaxFileSize(thePrefs.GetMaxLogFileSize());
	theLog.SetFileFormat(thePrefs.GetLogFileFormat());
	theVerboseLog.SetMaxFileSize(thePrefs.GetMaxLogFileSize());
	theVerboseLog.SetFileFormat(thePrefs.GetLogFileFormat());
	if (thePrefs.GetLog2Disk()) {
		theLog.Open();
		theLog.Log(_T("\r\n"));
	}
	if (thePrefs.GetDebug2Disk()) {
		theVerboseLog.Open();
		theVerboseLog.Log(_T("\r\n"));
	}
	Log(_T("Starting eMule v%s"), (LPCTSTR)m_strCurVersionLong);

	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	emuledlg = new CemuleDlg;
	m_pMainWnd = emuledlg;
	// Barry - Auto-take ed2k links
	if (thePrefs.AutoTakeED2KLinks())
		Ask4RegFix(false, true, false);

	SetAutoStart(thePrefs.GetAutoStart());

	m_pFirewallOpener = new CFirewallOpener();
	m_pFirewallOpener->Init(true); // we need to init it now (even if we may not use it yet) because of CoInitializeSecurity - which kinda ruins the sense of the class interface but ooohh well :P
	// Open WinXP firewall ports if set in preferences and possible
	if (thePrefs.IsOpenPortsOnStartupEnabled()) {
		if (m_pFirewallOpener->DoesFWConnectionExist()) {
			// delete old rules added by eMule
			m_pFirewallOpener->RemoveRule(EMULE_DEFAULTRULENAME_UDP);
			m_pFirewallOpener->RemoveRule(EMULE_DEFAULTRULENAME_TCP);
			// open port for this session
			if (m_pFirewallOpener->OpenPort(thePrefs.GetPort(), NAT_PROTOCOL_TCP, EMULE_DEFAULTRULENAME_TCP, true))
				QueueLogLine(false, GetResString(IDS_FO_TEMPTCP_S), thePrefs.GetPort());
			else
				QueueLogLine(false, GetResString(IDS_FO_TEMPTCP_F), thePrefs.GetPort());

			if (thePrefs.GetUDPPort()) {
				// open port for this session
				if (m_pFirewallOpener->OpenPort(thePrefs.GetUDPPort(), NAT_PROTOCOL_UDP, EMULE_DEFAULTRULENAME_UDP, true))
					QueueLogLine(false, GetResString(IDS_FO_TEMPUDP_S), thePrefs.GetUDPPort());
				else
					QueueLogLine(false, GetResString(IDS_FO_TEMPUDP_F), thePrefs.GetUDPPort());
			}
		}
	}

	// UPnP Port forwarding
	m_pUPnPFinder = new CUPnPImplWrapper();

	// Highres scheduling gives better resolution for Sleep(...) calls, and timeGetTime() calls
	m_wTimerRes = 0;
	if (thePrefs.GetHighresTimer()) {
		TIMECAPS tc;
		if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
			m_wTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
			if (m_wTimerRes > 0) {
				MMRESULT mmResult = timeBeginPeriod(m_wTimerRes);
				if (thePrefs.GetVerbose()) {
					if (mmResult == TIMERR_NOERROR)
						theApp.QueueDebugLogLine(false, _T("Succeeded to set timer/scheduler resolution to %i ms."), m_wTimerRes);
					else {
						theApp.QueueDebugLogLine(false, _T("Failed to set timer/scheduler resolution to %i ms."), m_wTimerRes);
						m_wTimerRes = 0;
					}
				}
			} else
				theApp.QueueDebugLogLine(false, _T("m_wTimerRes == 0. Not setting timer/scheduler resolution."));
		}
	}

	clientlist = new CClientList();
	friendlist = new CFriendList();
	searchlist = new CSearchList();
	knownfiles = new CKnownFileList();
	serverlist = new CServerList();
	serverconnect = new CServerConnect();
	sharedfiles = new CSharedFileList(serverconnect);
	listensocket = new CListenSocket();
	clientudp = new CClientUDPSocket();
	clientcredits = new CClientCreditsList();
	downloadqueue = new CDownloadQueue();	// bugfix - do this before creating the upload queue
	uploadqueue = new CUploadQueue();
	ipfilter = new CIPFilter();
	webserver = new CWebServer(); // Web Server [kuchin]
	scheduler = new CScheduler();
	m_pPeerCache = new CPeerCacheFinder();

	// ZZ:UploadSpeedSense -->
	lastCommonRouteFinder = new LastCommonRouteFinder();
	uploadBandwidthThrottler = new UploadBandwidthThrottler();
	// ZZ:UploadSpeedSense <--

	m_pUploadDiskIOThread = new CUploadDiskIOThread();

	thePerfLog.Startup();
	emuledlg->DoModal();

	DisableRTLWindowsLayout();

	// Barry - Restore old registry if required
	if (thePrefs.AutoTakeED2KLinks())
		RevertReg();

	::CloseHandle(m_hMutexOneInstance);
#ifdef _DEBUG
	if (g_pfnPrevCrtAllocHook)
		_CrtSetAllocHook(g_pfnPrevCrtAllocHook);

	newMemState.Checkpoint();
	if (diffMemState.Difference(oldMemState, newMemState)) {
		TRACE("Memory usage:\n");
		diffMemState.DumpStatistics();
	}
	//_CrtDumpMemoryLeaks();
#endif //_DEBUG

	ClearDebugLogQueue(true);
	ClearLogQueue(true);

	AddDebugLogLine(DLP_VERYLOW, _T("%hs: returning: FALSE"), __FUNCTION__);
	delete emuledlg;
	emuledlg = NULL;
	return FALSE;
}

int CemuleApp::ExitInstance()
{
	AddDebugLogLine(DLP_VERYLOW, _T("%hs"), __FUNCTION__);

	if (m_wTimerRes != 0)
		timeEndPeriod(m_wTimerRes);

	return CWinApp::ExitInstance();
}

#ifdef _DEBUG
int CrtDebugReportCB(int reportType, char *message, int *returnValue) noexcept
{
	FILE *fp = _tfsopen(s_szCrtDebugReportFilePath, _T("a"), _SH_DENYWR);
	if (fp) {
		time_t tNow = time(NULL);
		TCHAR szTime[40];
		_tcsftime(szTime, _countof(szTime), _T("%H:%M:%S"), localtime(&tNow));
		_ftprintf(fp, _T("%ls  %i  %hs"), szTime, reportType, message);
		fclose(fp);
	}
	*returnValue = 0; // avoid invocation of 'AfxDebugBreak' in ASSERT macros
	return TRUE; // avoid further processing of this debug report message by the CRT
}

// allocation hook - for memory statistics gathering
int eMuleAllocHook(int mode, void *pUserData, size_t nSize, int nBlockUse, long lRequest, const unsigned char *pszFileName, int nLine) noexcept
{
	UINT count;
	if (!g_allocations.Lookup(pszFileName, count))
		count = 0;
	if (mode == _HOOK_ALLOC) {
		_CrtSetAllocHook(g_pfnPrevCrtAllocHook);
		g_allocations.SetAt(pszFileName, count + 1);
		_CrtSetAllocHook(&eMuleAllocHook);
	} else if (mode == _HOOK_FREE) {
		_CrtSetAllocHook(g_pfnPrevCrtAllocHook);
		g_allocations.SetAt(pszFileName, count - 1);
		_CrtSetAllocHook(&eMuleAllocHook);
	}
	return g_pfnPrevCrtAllocHook(mode, pUserData, nSize, nBlockUse, lRequest, pszFileName, nLine);
}
#endif

bool CemuleApp::ProcessCommandline()
{
	bool bIgnoreRunningInstances = (GetProfileInt(_T("eMule"), _T("IgnoreInstances"), 0) != 0);
	for (int i = 1; i < __argc; ++i) {
		LPCTSTR pszParam = __targv[i];
		if (pszParam[0] == _T('-') || pszParam[0] == _T('/')) {
			++pszParam;
#ifdef _DEBUG
			if (_tcsicmp(pszParam, _T("assertfile")) == 0)
				_CrtSetReportHook(CrtDebugReportCB);
#endif
			if (_tcsicmp(pszParam, _T("ignoreinstances")) == 0)
				bIgnoreRunningInstances = true;

			if (_tcsicmp(pszParam, _T("AutoStart")) == 0)
				m_bAutoStart = true;
		}
	}

	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// If we create our TCP listen socket with SO_REUSEADDR, we have to ensure that there are
	// not 2 emules are running using the same port.
	// NOTE: This will not prevent from some other application using that port!
	UINT uTcpPort = GetProfileInt(_T("eMule"), _T("Port"), DEFAULT_TCP_PORT_OLD);
	CString strMutextName;
	strMutextName.Format(_T("%s:%u"), EMULE_GUID, uTcpPort);
	m_hMutexOneInstance = CreateMutex(NULL, FALSE, strMutextName);

	HWND maininst = NULL;
	const CString &command(cmdInfo.m_strFileName);

	//this codepart is to determine special cases when we do add a link to our eMule
	//because in this case it would be nonsense to start another instance!
	bool bAlreadyRunning = false;
	if (bIgnoreRunningInstances
		&& cmdInfo.m_nShellCommand == CCommandLineInfo::FileOpen
		&& (command.Find(_T("://")) > 0 || command.Find(_T("magnet:?")) >= 0 || CCollection::HasCollectionExtention(command)))
	{
		bIgnoreRunningInstances = false;
	}
	if (!bIgnoreRunningInstances)
		switch (::GetLastError()) {
		case ERROR_ALREADY_EXISTS:
		case ERROR_ACCESS_DENIED:
			bAlreadyRunning = true;
			EnumWindows(SearchEmuleWindow, (LPARAM)&maininst);
		}

	if (cmdInfo.m_nShellCommand == CCommandLineInfo::FileOpen) {
		if (command.Find(_T("://")) > 0 || command.Find(_T("magnet:?")) >= 0) {
			sendstruct.cbData = (command.GetLength() + 1) * sizeof(TCHAR);
			sendstruct.dwData = OP_ED2KLINK;
			sendstruct.lpData = const_cast<LPTSTR>((LPCTSTR)command);
			if (maininst) {
				SendMessage(maininst, WM_COPYDATA, (WPARAM)0, (LPARAM)(PCOPYDATASTRUCT)&sendstruct);
				return true;
			}

			m_strPendingLink = command;
		} else if (CCollection::HasCollectionExtention(command)) {
			sendstruct.cbData = (command.GetLength() + 1) * sizeof(TCHAR);
			sendstruct.dwData = OP_COLLECTION;
			sendstruct.lpData = const_cast<LPTSTR>((LPCTSTR)command);
			if (maininst) {
				SendMessage(maininst, WM_COPYDATA, (WPARAM)0, (LPARAM)(PCOPYDATASTRUCT)&sendstruct);
				return true;
			}

			m_strPendingLink = command;
		} else {
			sendstruct.cbData = (command.GetLength() + 1) * sizeof(TCHAR);
			sendstruct.dwData = OP_CLCOMMAND;
			sendstruct.lpData = const_cast<LPTSTR>((LPCTSTR)command);
			if (maininst) {
				SendMessage(maininst, WM_COPYDATA, (WPARAM)0, (LPARAM)(PCOPYDATASTRUCT)&sendstruct);
				return true;
			}
			// Don't start if we were invoked with 'exit' command.
			if (command.CompareNoCase(_T("exit")) == 0)
				return true;
		}
	}
	return (maininst || bAlreadyRunning);
}

BOOL CALLBACK CemuleApp::SearchEmuleWindow(HWND hWnd, LPARAM lParam) noexcept
{
	DWORD_PTR dwMsgResult;
	LRESULT res = ::SendMessageTimeout(hWnd, UWM_ARE_YOU_EMULE, 0, 0, SMTO_BLOCK | SMTO_ABORTIFHUNG, SEC2MS(10), &dwMsgResult);
	if (res != 0 && dwMsgResult == UWM_ARE_YOU_EMULE) {
		*reinterpret_cast<HWND*>(lParam) = hWnd;
		return FALSE;
	}
	return TRUE;
}


void CemuleApp::UpdateReceivedBytes(uint32 bytesToAdd)
{
	SetTimeOnTransfer();
	theStats.sessionReceivedBytes += bytesToAdd;
}

void CemuleApp::UpdateSentBytes(uint32 bytesToAdd, bool sentToFriend)
{
	SetTimeOnTransfer();
	theStats.sessionSentBytes += bytesToAdd;

	if (sentToFriend)
		theStats.sessionSentBytesToFriend += bytesToAdd;
}

void CemuleApp::SetTimeOnTransfer()
{
	if (theStats.transferStarttime <= 0)
		theStats.transferStarttime = ::GetTickCount();
}

CString CemuleApp::CreateKadSourceLink(const CAbstractFile *f)
{
	CString strLink;
	if (Kademlia::CKademlia::IsConnected() && theApp.clientlist->GetBuddy() && theApp.IsFirewalled()) {
		CString KadID;
		Kademlia::CKademlia::GetPrefs()->GetKadID().Xor(Kademlia::CUInt128(true)).ToHexString(&KadID);
		strLink.Format(_T("ed2k://|file|%s|%I64u|%s|/|kadsources,%s:%s|/")
			, (LPCTSTR)EncodeUrlUtf8(StripInvalidFilenameChars(f->GetFileName()))
			, (uint64)f->GetFileSize()
			, (LPCTSTR)EncodeBase16(f->GetFileHash(), 16)
			, (LPCTSTR)md4str(thePrefs.GetUserHash()), (LPCTSTR)KadID);
	}
	return strLink;
}

//TODO: Move to emule-window
bool CemuleApp::CopyTextToClipboard(const CString &strText)
{
	if (strText.IsEmpty())
		return false;

	HGLOBAL hGlobalT = GlobalAlloc(GHND | GMEM_SHARE, (strText.GetLength() + 1) * sizeof(TCHAR));
	if (hGlobalT != NULL) {
		LPTSTR pGlobalT = static_cast<LPTSTR>(GlobalLock(hGlobalT));
		if (pGlobalT != NULL) {
			_tcscpy(pGlobalT, strText);
			GlobalUnlock(hGlobalT);
		} else {
			GlobalFree(hGlobalT);
			hGlobalT = NULL;
		}
	}

	CStringA strTextA(strText);
	HGLOBAL hGlobalA = GlobalAlloc(GHND | GMEM_SHARE, (strTextA.GetLength() + 1) * sizeof(char));
	if (hGlobalA != NULL) {
		LPSTR pGlobalA = static_cast<LPSTR>(GlobalLock(hGlobalA));
		if (pGlobalA != NULL) {
			strcpy(pGlobalA, strTextA);
			GlobalUnlock(hGlobalA);
		} else {
			GlobalFree(hGlobalA);
			hGlobalA = NULL;
		}
	}

	if (hGlobalT == NULL && hGlobalA == NULL)
		return false;

	int iCopied = 0;
	if (OpenClipboard(NULL)) {
		if (EmptyClipboard()) {
			if (hGlobalT) {
				if (SetClipboardData(CF_UNICODETEXT, hGlobalT) != NULL)
					++iCopied;
				else {
					GlobalFree(hGlobalT);
					hGlobalT = NULL;
				}
			}
			if (hGlobalA) {
				if (SetClipboardData(CF_TEXT, hGlobalA) != NULL)
					++iCopied;
				else {
					GlobalFree(hGlobalA);
					hGlobalA = NULL;
				}
			}
		}
		CloseClipboard();
	}

	if (iCopied == 0) {
		if (hGlobalT)
			GlobalFree(hGlobalT);
		if (hGlobalA)
			GlobalFree(hGlobalA);
		return false;
	}

	IgnoreClipboardLinks(strText); // this is so eMule won't think the clipboard has ed2k links for adding
	return true;
}

//TODO: Move to emule-window
CString CemuleApp::CopyTextFromClipboard()
{
	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		if (OpenClipboard(NULL)) {
			bool bResult = false;
			CString strClipboard;
			HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
			if (hMem) {
				LPCWSTR pwsz = (LPCWSTR)GlobalLock(hMem);
				if (pwsz) {
					strClipboard = pwsz;
					GlobalUnlock(hMem);
					bResult = true;
				}
			}
			CloseClipboard();
			if (bResult)
				return strClipboard;
		}
	}

	if (!IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(NULL))
		return CString();

	CString retstring;
	HGLOBAL hglb = GetClipboardData(CF_TEXT);
	if (hglb != NULL) {
		LPCSTR lptstr = (LPCSTR)GlobalLock(hglb);
		if (lptstr != NULL) {
			retstring = lptstr;
			GlobalUnlock(hglb);
		}
	}
	CloseClipboard();

	return retstring;
}

void CemuleApp::OnlineSig() // Added By Bouc7
{
	if (!thePrefs.IsOnlineSignatureEnabled())
		return;

	static LPCTSTR const _szFileName = _T("onlinesig.dat");
	CString strFullPath;
	strFullPath.Format(_T("%s%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_CONFIGBASEDIR), _szFileName);

	// The 'onlinesig.dat' is potentially read by other applications at more or less frequent intervals.
	//	 -	Set the file shareing mode to allow other processes to read the file while we are writing
	//		it (see also next point).
	//	 -	Try to write the hole file data at once, so other applications are always reading
	//		a consistent amount of file data. C-RTL uses a 4 KB buffer, this is large enough to write
	//		those 2 lines into the onlinesig.dat file with one IO operation.
	//	 -	Although this file is a text file, we set the file mode to 'binary' because of backward
	//		compatibility with older eMule versions.
	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(strFullPath, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::typeBinary, &fexp)) {
		CString strError;
		strError.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_ERROR_SAVEFILE), _szFileName);
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		GetExceptionMessage(fexp, szError, _countof(szError));
		strError.AppendFormat(_T(" - %s"), szError);
		LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		return;
	}

	try {
		char buffer[20];
		CStringA strBuff;
		if (IsConnected()) {
			file.Write("1|", 2);
			if (serverconnect->IsConnected())
				strBuff = serverconnect->GetCurrentServer()->GetListName();
			else
				strBuff = "Kademlia";
			file.Write(strBuff, strBuff.GetLength());

			file.Write("|", 1);
			if (serverconnect->IsConnected())
				strBuff = serverconnect->GetCurrentServer()->GetAddress();
			else
				strBuff = "0.0.0.0";
			file.Write(strBuff, strBuff.GetLength());

			file.Write("|", 1);
			if (serverconnect->IsConnected()) {
				_itoa(serverconnect->GetCurrentServer()->GetPort(), buffer, 10);
				file.Write(buffer, (UINT)strlen(buffer));
			} else
				file.Write("0", 1);
		} else
			file.Write("0", 1);
		file.Write("\n", 1);

		_snprintf(buffer, _countof(buffer), "%.1f", (float)downloadqueue->GetDatarate() / 1024);
		buffer[_countof(buffer) - 1] = '\0';
		file.Write(buffer, (UINT)strlen(buffer));
		file.Write("|", 1);

		_snprintf(buffer, _countof(buffer), "%.1f", (float)uploadqueue->GetDatarate() / 1024);
		buffer[_countof(buffer) - 1] = '\0';
		file.Write(buffer, (UINT)strlen(buffer));
		file.Write("|", 1);

		_itoa((int)uploadqueue->GetWaitingUserCount(), buffer, 10);
		file.Write(buffer, (UINT)strlen(buffer));

		file.Close();
	} catch (CFileException *ex) {
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		GetExceptionMessage(*ex, szError, _countof(szError));
		CString strError;
		strError.Format(_T("%s %s - %s"), (LPCTSTR)GetResString(IDS_ERROR_SAVEFILE), _szFileName, szError);
		LogError(LOG_STATUSBAR, _T("%s"), (LPCTSTR)strError);
		ex->Delete();
	}
} //End Added By Bouc7

bool CemuleApp::GetLangHelpFilePath(CString &strResult)
{
	// Change extension for help file
	strResult = m_pszHelpFilePath;
	WORD langID = thePrefs.GetLanguageID();
	CString temp;
	if (langID == MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT))
		langID = (WORD)(-1);
	else
		temp.Format(_T(".%u"), langID);
	int pos = strResult.ReverseFind(_T('\\'));   //CML
	if (pos < 0)
		strResult.Replace(_T(".HLP"), _T(".chm"));
	else {
		strResult.Truncate(pos);
		strResult.AppendFormat(_T("\\eMule%s.chm"), (LPCTSTR)temp);
	}
	bool bFound = PathFileExists(strResult);
	if (!bFound && langID > 0) {
		strResult = m_pszHelpFilePath; // if not exists, use original help (English)
		strResult.Replace(_T(".HLP"), _T(".chm"));
	}
	return bFound;
}

void CemuleApp::SetHelpFilePath(LPCTSTR pszHelpFilePath)
{
	free((void*)m_pszHelpFilePath);
	m_pszHelpFilePath = _tcsdup(pszHelpFilePath);
}

void CemuleApp::OnHelp()
{
	if (m_dwPromptContext != 0) {
		// do not call WinHelp when the error is failing to lauch help
		if (m_dwPromptContext != HID_BASE_PROMPT + AFX_IDP_FAILED_TO_LAUNCH_HELP)
			ShowHelp(m_dwPromptContext);
		return;
	}
	ShowHelp(0, HELP_CONTENTS);
}

void CemuleApp::ShowHelp(UINT uTopic, UINT uCmd)
{
	CString strHelpFilePath;
	if (GetLangHelpFilePath(strHelpFilePath) || !ShowWebHelp(uTopic)) {
		SetHelpFilePath(strHelpFilePath);
		WinHelpInternal(uTopic, uCmd);
	}
}

bool CemuleApp::ShowWebHelp(UINT uTopic)
{
	CString strHelpURL;
	strHelpURL.Format(_T("https://onlinehelp.emule-project.net/help.php?language=%u&topic=%u"), thePrefs.GetLanguageID(), uTopic);
	BrowserOpen(strHelpURL, thePrefs.GetMuleDirectory(EMULE_EXECUTABLEDIR));
	return true;
}

int CemuleApp::GetFileTypeSystemImageIdx(LPCTSTR pszFilePath, int iLength /* = -1 */, bool bNormalsSize)
{
	DWORD dwFileAttributes;
	LPCTSTR pszCacheExt = NULL;
	if (iLength == -1)
		iLength = (int)_tcslen(pszFilePath);
	if (iLength > 0 && (pszFilePath[iLength - 1] == _T('\\') || pszFilePath[iLength - 1] == _T('/'))) {
		// it's a directory
		pszCacheExt = _T("\\");
		dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	} else {
		dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		// search last '.' character *after* the last '\\' character
		for (int i = iLength; --i >= 0;) {
			if (pszFilePath[i] == _T('\\') || pszFilePath[i] == _T('/'))
				break;
			if (pszFilePath[i] == _T('.')) {
				// point to 1st character of extension (skip the '.')
				pszCacheExt = &pszFilePath[i + 1];
				break;
			}
		}
		if (pszCacheExt == NULL)
			pszCacheExt = _T("");	// empty extension
	}

	// Search extension in "ext->idx" cache.
	LPVOID vData;
	if (bNormalsSize) {
		if (!m_aBigExtToSysImgIdx.Lookup(pszCacheExt, vData)) {
			// Get index for the system's big icon image list
			SHFILEINFO sfi;
			HIMAGELIST hResult = (HIMAGELIST)SHGetFileInfo(pszFilePath, dwFileAttributes, &sfi, sizeof(sfi),
				SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX);
			if (hResult == 0)
				return 0;
			ASSERT(m_hBigSystemImageList == NULL || m_hBigSystemImageList == hResult);
			m_hBigSystemImageList = hResult;

			// Store icon index in local cache
			m_aBigExtToSysImgIdx.SetAt(pszCacheExt, (LPVOID)sfi.iIcon);
			return sfi.iIcon;
		}
	} else if (!m_aExtToSysImgIdx.Lookup(pszCacheExt, vData)) {
			// Get index for the system's small icon image list
		SHFILEINFO sfi;
		HIMAGELIST hResult = (HIMAGELIST)SHGetFileInfo(pszFilePath, dwFileAttributes, &sfi, sizeof(sfi),
			SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
		if (hResult == 0)
			return 0;
		ASSERT(m_hSystemImageList == NULL || m_hSystemImageList == hResult);
		m_hSystemImageList = hResult;

		// Store icon index in local cache
		m_aExtToSysImgIdx.SetAt(pszCacheExt, (LPVOID)sfi.iIcon);
		return sfi.iIcon;
	}

	// Return already cached value
	return reinterpret_cast<int>(vData);
}

bool CemuleApp::IsConnected(bool bIgnoreEd2k, bool bIgnoreKad)
{
	return (theApp.serverconnect->IsConnected() && !bIgnoreEd2k) || (Kademlia::CKademlia::IsConnected() && !bIgnoreKad);
}

bool CemuleApp::IsPortchangeAllowed()
{
	return theApp.clientlist->GetClientCount() == 0 && !IsConnected();
}

uint32 CemuleApp::GetID()
{
	if (Kademlia::CKademlia::IsConnected() && !Kademlia::CKademlia::IsFirewalled())
		return ntohl(Kademlia::CKademlia::GetIPAddress());
	if (theApp.serverconnect->IsConnected())
		return theApp.serverconnect->GetClientID();
	return static_cast<uint32>(Kademlia::CKademlia::IsConnected() && Kademlia::CKademlia::IsFirewalled());
}

uint32 CemuleApp::GetPublicIP(bool bIgnoreKadIP) const
{
	if (m_dwPublicIP == 0 && Kademlia::CKademlia::IsConnected() && Kademlia::CKademlia::GetIPAddress() && !bIgnoreKadIP)
		return ntohl(Kademlia::CKademlia::GetIPAddress());
	return m_dwPublicIP;
}

void CemuleApp::SetPublicIP(const uint32 dwIP)
{
	if (dwIP != 0) {
		ASSERT(!::IsLowID(dwIP));
		ASSERT(m_pPeerCache);

		if (GetPublicIP() == 0)
			AddDebugLogLine(DLP_VERYLOW, false, _T("My public IP Address is: %s"), (LPCTSTR)ipstr(dwIP));
		else if (Kademlia::CKademlia::IsConnected() && Kademlia::CKademlia::GetPrefs()->GetIPAddress())
			if (htonl(Kademlia::CKademlia::GetIPAddress()) != dwIP)
				AddDebugLogLine(DLP_DEFAULT, false, _T("Public IP Address reported by Kademlia (%s) differs from new-found (%s)"), (LPCTSTR)ipstr(htonl(Kademlia::CKademlia::GetIPAddress())), (LPCTSTR)ipstr(dwIP));
		m_pPeerCache->FoundMyPublicIPAddress(dwIP);
	} else
		AddDebugLogLine(DLP_VERYLOW, false, _T("Deleted public IP"));

	if (dwIP != 0 && dwIP != m_dwPublicIP && serverlist != NULL) {
		m_dwPublicIP = dwIP;
		serverlist->CheckForExpiredUDPKeys();
	} else
		m_dwPublicIP = dwIP;
}

bool CemuleApp::IsFirewalled()
{
	if (theApp.serverconnect->IsConnected() && !theApp.serverconnect->IsLowID())
		return false; // we have an eD2K HighID -> not firewalled

	if (Kademlia::CKademlia::IsConnected() && !Kademlia::CKademlia::IsFirewalled())
		return false; // we have a Kad HighID -> not firewalled

	return true; // firewalled
}

bool CemuleApp::CanDoCallback(CUpDownClient *client)
{
	bool ed2k = theApp.serverconnect->IsConnected();
	bool eLow = theApp.serverconnect->IsLowID();

	if (!Kademlia::CKademlia::IsConnected() || Kademlia::CKademlia::IsFirewalled())
		return ed2k & !eLow; //callback for high ID server connection

	//KAD is connected and Open
	//Special case of a low ID server connection
	//If the client connects to the same server, we prevent callback
	//as it breaks the protocol and will get us banned.
	if (ed2k & eLow) {
		const CServer *srv = theApp.serverconnect->GetCurrentServer();
		return (client->GetServerIP() != srv->GetIP() || client->GetServerPort() != srv->GetPort());
	}
	return true;
}

HICON CemuleApp::LoadIcon(UINT nIDResource) const
{
	// use string resource identifiers!!
	return CWinApp::LoadIcon(nIDResource);
}

HICON CemuleApp::LoadIcon(LPCTSTR lpszResourceName, int cx, int cy, UINT uFlags) const
{
	// Test using of 16 color icons. If 'LR_VGACOLOR' is specified _and_ the icon resource
	// contains a 16 color version, that 16 color version will be loaded. If there is no
	// 16 color version available, Windows will use the next (better) color version found.
#ifdef _DEBUG
	if (g_bLowColorDesktop)
		uFlags |= LR_VGACOLOR;
#endif

	HICON hIcon = NULL;
	LPCTSTR pszSkinProfile = thePrefs.GetSkinProfile();
	if (pszSkinProfile != NULL && pszSkinProfile[0] != _T('\0')) {
		// load icon resource file specification from skin profile
		TCHAR szSkinResource[MAX_PATH];
		GetPrivateProfileString(_T("Icons"), lpszResourceName, _T(""), szSkinResource, _countof(szSkinResource), pszSkinProfile);
		if (szSkinResource[0] != _T('\0')) {
			// expand any optional available environment strings
			TCHAR szExpSkinRes[MAX_PATH];
			if (ExpandEnvironmentStrings(szSkinResource, szExpSkinRes, _countof(szExpSkinRes)) != 0) {
				_tcsncpy(szSkinResource, szExpSkinRes, _countof(szSkinResource));
				szSkinResource[_countof(szSkinResource) - 1] = _T('\0');
			}

			// create absolute path to icon resource file
			TCHAR szFullResPath[MAX_PATH];
			if (PathIsRelative(szSkinResource)) {
				TCHAR szSkinResFolder[MAX_PATH];
				_tcsncpy(szSkinResFolder, pszSkinProfile, _countof(szSkinResFolder));
				szSkinResFolder[_countof(szSkinResFolder) - 1] = _T('\0');
				PathRemoveFileSpec(szSkinResFolder);
				_tmakepathlimit(szFullResPath, NULL, szSkinResFolder, szSkinResource, NULL);
			} else {
				_tcsncpy(szFullResPath, szSkinResource, _countof(szFullResPath));
				szFullResPath[_countof(szFullResPath) - 1] = _T('\0');
			}

			// check for optional icon index or resource identifier within the icon resource file
			bool bExtractIcon = false;
			CString strFullResPath(szFullResPath);
			int iIconIndex = 0;
			int iComma = strFullResPath.ReverseFind(_T(','));
			if (iComma >= 0) {
				if (_stscanf((LPCTSTR)strFullResPath + iComma + 1, _T("%d"), &iIconIndex) == 1)
					bExtractIcon = true;
				strFullResPath.Truncate(iComma);
			}

			if (bExtractIcon) {
				if (uFlags != 0 || !(cx == cy && (cx == 16 || cx == 32))) {
					static UINT(WINAPI *_pfnPrivateExtractIcons)(LPCTSTR, int, int, int, HICON*, UINT*, UINT, UINT)
						= (UINT(WINAPI*)(LPCTSTR, int, int, int, HICON*, UINT*, UINT, UINT))(void*)GetProcAddress(GetModuleHandle(_T("user32")), _TWINAPI("PrivateExtractIcons"));
					if (_pfnPrivateExtractIcons) {
						UINT uIconId;
						(*_pfnPrivateExtractIcons)(strFullResPath, iIconIndex, cx, cy, &hIcon, &uIconId, 1, uFlags);
					}
				}

				if (hIcon == NULL) {
					HICON aIconsLarge[1], aIconsSmall[1];
					int iExtractedIcons = ExtractIconEx(strFullResPath, iIconIndex, aIconsLarge, aIconsSmall, 1);
					if (iExtractedIcons > 0) { // 'iExtractedIcons' is 2(!) if we get a large and a small icon
						// alway try to return the icon size which was requested
						if (cx == 16 && aIconsSmall[0] != NULL) {
							hIcon = aIconsSmall[0];
							aIconsSmall[0] = NULL;
						} else if (cx == 32 && aIconsLarge[0] != NULL) {
							hIcon = aIconsLarge[0];
							aIconsLarge[0] = NULL;
						} else {
							if (aIconsSmall[0] != NULL) {
								hIcon = aIconsSmall[0];
								aIconsSmall[0] = NULL;
							} else if (aIconsLarge[0] != NULL) {
								hIcon = aIconsLarge[0];
								aIconsLarge[0] = NULL;
							}
						}

						for (unsigned i = 0; i < _countof(aIconsLarge); ++i) {
							if (aIconsLarge[i] != NULL)
								VERIFY(::DestroyIcon(aIconsLarge[i]));
							if (aIconsSmall[i] != NULL)
								VERIFY(::DestroyIcon(aIconsSmall[i]));
						}
					}
				}
			} else {
				// WINBUG???: 'ExtractIcon' does not work well on ICO-files when using the color
				// scheme 'Windows-Standard (extragroß)' -> always try to use 'LoadImage'!
				//
				// If the ICO file contains a 16x16 icon, 'LoadImage' will though return a 32x32 icon,
				// if LR_DEFAULTSIZE is specified! -> always specify the requested size!
				hIcon = (HICON)::LoadImage(NULL, szFullResPath, IMAGE_ICON, cx, cy, uFlags | LR_LOADFROMFILE);
				if (hIcon == NULL && ::GetLastError() != ERROR_PATH_NOT_FOUND && g_bGdiPlusInstalled) {
					ULONG_PTR gdiplusToken = 0;
					Gdiplus::GdiplusStartupInput gdiplusStartupInput;
					if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::Ok) {
						Gdiplus::Bitmap bmp(szFullResPath);
						bmp.GetHICON(&hIcon);
					}
					Gdiplus::GdiplusShutdown(gdiplusToken);
				}
			}
		}
	}

	if (hIcon == NULL) {
		if (cx != LR_DEFAULTSIZE || cy != LR_DEFAULTSIZE || uFlags != LR_DEFAULTCOLOR)
			hIcon = (HICON)::LoadImage(AfxGetResourceHandle(), lpszResourceName, IMAGE_ICON, cx, cy, uFlags);
		if (hIcon == NULL) {
			//TODO: Either do not use that function or copy the returned icon. All the calling code is designed
			// in a way that the icons returned by this function are to be freed with 'DestroyIcon'. But an
			// icon which was loaded with 'LoadIcon', is not be freed with 'DestroyIcon'.
			// Right now, we never come here...
			ASSERT(0);
			hIcon = CWinApp::LoadIcon(lpszResourceName);
		}
	}
	return hIcon;
}

HBITMAP CemuleApp::LoadImage(LPCTSTR lpszResourceName, LPCTSTR pszResourceType) const
{
	LPCTSTR pszSkinProfile = thePrefs.GetSkinProfile();
	if (pszSkinProfile != NULL && pszSkinProfile[0] != _T('\0')) {
		// load resource file specification from skin profile
		TCHAR szSkinResource[MAX_PATH];
		GetPrivateProfileString(_T("Bitmaps"), lpszResourceName, _T(""), szSkinResource, _countof(szSkinResource), pszSkinProfile);
		if (szSkinResource[0] != _T('\0')) {
			// expand any optional available environment strings
			TCHAR szExpSkinRes[MAX_PATH];
			if (ExpandEnvironmentStrings(szSkinResource, szExpSkinRes, _countof(szExpSkinRes)) != 0) {
				_tcsncpy(szSkinResource, szExpSkinRes, _countof(szSkinResource));
				szSkinResource[_countof(szSkinResource) - 1] = _T('\0');
			}

			// create absolute path to resource file
			TCHAR szFullResPath[MAX_PATH];
			if (PathIsRelative(szSkinResource)) {
				TCHAR szSkinResFolder[MAX_PATH];
				_tcsncpy(szSkinResFolder, pszSkinProfile, _countof(szSkinResFolder));
				szSkinResFolder[_countof(szSkinResFolder) - 1] = _T('\0');
				PathRemoveFileSpec(szSkinResFolder);
				_tmakepathlimit(szFullResPath, NULL, szSkinResFolder, szSkinResource, NULL);
			} else {
				_tcsncpy(szFullResPath, szSkinResource, _countof(szFullResPath));
				szFullResPath[_countof(szFullResPath) - 1] = _T('\0');
			}

			CEnBitmap bmp;
			if (bmp.LoadImage(szFullResPath))
				return (HBITMAP)bmp.Detach();
		}
	}

	CEnBitmap bmp;
	if (bmp.LoadImage(lpszResourceName, pszResourceType))
		return (HBITMAP)bmp.Detach();
	return NULL;
}

CString CemuleApp::GetSkinFileItem(LPCTSTR lpszResourceName, LPCTSTR pszResourceType) const
{
	LPCTSTR pszSkinProfile = thePrefs.GetSkinProfile();
	if (pszSkinProfile != NULL && pszSkinProfile[0] != _T('\0')) {
		// load resource file specification from skin profile
		TCHAR szSkinResource[MAX_PATH];
		GetPrivateProfileString(pszResourceType, lpszResourceName, _T(""), szSkinResource, _countof(szSkinResource), pszSkinProfile);
		if (szSkinResource[0] != _T('\0')) {
			// expand any optional available environment strings
			TCHAR szExpSkinRes[MAX_PATH];
			if (ExpandEnvironmentStrings(szSkinResource, szExpSkinRes, _countof(szExpSkinRes)) != 0) {
				_tcsncpy(szSkinResource, szExpSkinRes, _countof(szSkinResource));
				szSkinResource[_countof(szSkinResource) - 1] = _T('\0');
			}

			// create absolute path to resource file
			TCHAR szFullResPath[MAX_PATH];
			if (PathIsRelative(szSkinResource)) {
				TCHAR szSkinResFolder[MAX_PATH];
				_tcsncpy(szSkinResFolder, pszSkinProfile, _countof(szSkinResFolder));
				szSkinResFolder[_countof(szSkinResFolder) - 1] = _T('\0');
				PathRemoveFileSpec(szSkinResFolder);
				_tmakepathlimit(szFullResPath, NULL, szSkinResFolder, szSkinResource, NULL);
			} else {
				_tcsncpy(szFullResPath, szSkinResource, _countof(szFullResPath));
				szFullResPath[_countof(szFullResPath) - 1] = _T('\0');
			}

			return szFullResPath;
		}
	}
	return CString();
}

bool CemuleApp::LoadSkinColor(LPCTSTR pszKey, COLORREF &crColor) const
{
	LPCTSTR pszSkinProfile = thePrefs.GetSkinProfile();
	if (pszSkinProfile != NULL && pszSkinProfile[0] != _T('\0')) {
		TCHAR szColor[MAX_PATH];
		GetPrivateProfileString(_T("Colors"), pszKey, _T(""), szColor, _countof(szColor), pszSkinProfile);
		if (szColor[0] != _T('\0')) {
			int red, grn, blu;
			if (_stscanf(szColor, _T("%i , %i , %i"), &red, &grn, &blu) == 3) {
				crColor = RGB(red, grn, blu);
				return true;
			}
		}
	}
	return false;
}

bool CemuleApp::LoadSkinColorAlt(LPCTSTR pszKey, LPCTSTR pszAlternateKey, COLORREF &crColor) const
{
	return LoadSkinColor(pszKey, crColor) || LoadSkinColor(pszAlternateKey, crColor);
}

void CemuleApp::ApplySkin(LPCTSTR pszSkinProfile)
{
	thePrefs.SetSkinProfile(pszSkinProfile);
	AfxGetMainWnd()->SendMessage(WM_SYSCOLORCHANGE);
}

CTempIconLoader::CTempIconLoader(LPCTSTR pszResourceID, int cx, int cy, UINT uFlags)
{
	m_hIcon = theApp.LoadIcon(pszResourceID, cx, cy, uFlags);
}

CTempIconLoader::CTempIconLoader(UINT uResourceID, int /*cx*/, int /*cy*/, UINT uFlags)
{
	UNREFERENCED_PARAMETER(uFlags);
	ASSERT(uFlags == 0);
	m_hIcon = theApp.LoadIcon(uResourceID);
}

CTempIconLoader::~CTempIconLoader()
{
	if (m_hIcon)
		VERIFY(::DestroyIcon(m_hIcon));
}

void CemuleApp::AddEd2kLinksToDownload(const CString &strLinks, int cat)
{
	for (int iPos = 0; iPos >= 0;) {
		const CString &sToken(strLinks.Tokenize(_T(" \t\r\n"), iPos)); //tokenize by whitespace
		if (sToken.IsEmpty())
			break;
		bool bSlash = (sToken.Right(1) == _T("/"));
		CED2KLink *pLink = NULL;
		try {
			pLink = CED2KLink::CreateLinkFromUrl(bSlash ? sToken : sToken + _T('/'));
			if (pLink) {
				if (pLink->GetKind() != CED2KLink::kFile)
					throw CString(_T("bad link"));
				downloadqueue->AddFileLinkToDownload(pLink->GetFileLink(), cat);
				delete pLink;
				pLink = NULL;
			}
		} catch (const CString &error) {
			delete pLink;
			CString sBuffer;
			sBuffer.Format(GetResString(IDS_ERR_INVALIDLINK), (LPCTSTR)error);
			LogError(LOG_STATUSBAR, GetResString(IDS_ERR_LINKERROR), (LPCTSTR)sBuffer);
			return;
		}
	}
}

void CemuleApp::SearchClipboard()
{
	if (m_bGuardClipboardPrompt)
		return;

	const CString strLinks(CopyTextFromClipboard());
	if (strLinks.IsEmpty())
		return;

	if (strLinks == m_strLastClipboardContents)
		return;

	// Do not alter (trim) 'strLinks' and then copy back to 'm_strLastClipboardContents'! The
	// next clipboard content compare would fail because of the modified string.
	LPCTSTR pszTrimmedLinks = strLinks;
	while (_istspace(*pszTrimmedLinks)) // Skip leading whitespaces
		++pszTrimmedLinks;
	m_bGuardClipboardPrompt = !_tcsnicmp(pszTrimmedLinks, _T("ed2k://|file|"), 13);
	if (m_bGuardClipboardPrompt) {
		// Don't feed too long strings into the MessageBox function, it may freak out.
		CString strLinksDisplay(GetResString(IDS_ADDDOWNLOADSFROMCB));
		if (strLinks.GetLength() > 512)
			strLinksDisplay.AppendFormat(_T("\r\n%s..."), (LPCTSTR)strLinks.Left(509));
		else
			strLinksDisplay.AppendFormat(_T("\r\n%s"), (LPCTSTR)strLinks);
		if (AfxMessageBox(strLinksDisplay, MB_YESNO | MB_TOPMOST) == IDYES)
			AddEd2kLinksToDownload(pszTrimmedLinks, 0);
	}
	m_strLastClipboardContents = strLinks; // Save the unmodified(!) clipboard contents
	m_bGuardClipboardPrompt = false;
}

void CemuleApp::PasteClipboard(int cat)
{
	CString strLinks(CopyTextFromClipboard());
	if (!strLinks.Trim().IsEmpty())
		AddEd2kLinksToDownload(strLinks, cat);
}

bool CemuleApp::IsEd2kLinkInClipboard(LPCSTR pszLinkType, int iLinkTypeLen)
{
	bool bFoundLink = false;
	if (IsClipboardFormatAvailable(CF_TEXT)) {
		if (OpenClipboard(NULL)) {
			HGLOBAL	hText = GetClipboardData(CF_TEXT);
			if (hText != NULL) {
				// Use the ANSI string
				LPCSTR pszText = (LPCSTR)GlobalLock(hText);
				if (pszText != NULL) {
					while (isspace(*pszText))
						++pszText;
					bFoundLink = (_strnicmp(pszText, pszLinkType, iLinkTypeLen) == 0);
					GlobalUnlock(hText);
				}
			}
			CloseClipboard();
		}
	}

	return bFoundLink;
}

bool CemuleApp::IsEd2kFileLinkInClipboard()
{
	static const char _szEd2kFileLink[] = "ed2k://|file|"; // Use the ANSI string
	return IsEd2kLinkInClipboard(_szEd2kFileLink, (sizeof _szEd2kFileLink) - 1);
}

bool CemuleApp::IsEd2kServerLinkInClipboard()
{
	static const char _szEd2kServerLink[] = "ed2k://|server|"; // Use the ANSI string
	return IsEd2kLinkInClipboard(_szEd2kServerLink, (sizeof _szEd2kServerLink) - 1);
}

// Elandal:ThreadSafeLogging -->
void CemuleApp::QueueDebugLogLine(bool bAddToStatusbar, LPCTSTR line, ...)
{
	if (!thePrefs.GetVerbose())
		return;

	CString bufferline;
	va_list argptr;
	va_start(argptr, line);
	bufferline.FormatV(line, argptr);
	va_end(argptr);
	if (!bufferline.IsEmpty()) {
		SLogItem *newItem = new SLogItem;
		newItem->uFlags = LOG_DEBUG | (bAddToStatusbar ? LOG_STATUSBAR : 0);
		newItem->line = bufferline;

		m_queueLock.Lock();
		m_QueueDebugLog.AddTail(newItem);
		m_queueLock.Unlock();
	}
}

void CemuleApp::QueueLogLine(bool bAddToStatusbar, LPCTSTR line, ...)
{
	CString bufferline;
	va_list argptr;
	va_start(argptr, line);
	bufferline.FormatV(line, argptr);
	va_end(argptr);
	if (!bufferline.IsEmpty()) {
		SLogItem *newItem = new SLogItem;
		newItem->uFlags = bAddToStatusbar ? LOG_STATUSBAR : 0;
		newItem->line = bufferline;

		m_queueLock.Lock();
		m_QueueLog.AddTail(newItem);
		m_queueLock.Unlock();
	}
}

void CemuleApp::QueueDebugLogLineEx(UINT uFlags, LPCTSTR line, ...)
{
	if (!thePrefs.GetVerbose())
		return;

	CString bufferline;
	va_list argptr;
	va_start(argptr, line);
	bufferline.FormatV(line, argptr);
	va_end(argptr);
	if (!bufferline.IsEmpty()) {
		SLogItem *newItem = new SLogItem;
		newItem->uFlags = uFlags | LOG_DEBUG;
		newItem->line = bufferline;

		m_queueLock.Lock();
		m_QueueDebugLog.AddTail(newItem);
		m_queueLock.Unlock();
	}
}

void CemuleApp::QueueLogLineEx(UINT uFlags, LPCTSTR line, ...)
{
	CString bufferline;
	va_list argptr;
	va_start(argptr, line);
	bufferline.FormatV(line, argptr);
	va_end(argptr);
	if (!bufferline.IsEmpty()) {
		SLogItem *newItem = new SLogItem;
		newItem->uFlags = uFlags;
		newItem->line = bufferline;

		m_queueLock.Lock();
		m_QueueLog.AddTail(newItem);
		m_queueLock.Unlock();
	}
}

void CemuleApp::HandleDebugLogQueue()
{
	m_queueLock.Lock();
	while (!m_QueueDebugLog.IsEmpty()) {
		const SLogItem *newItem = m_QueueDebugLog.RemoveHead();
		if (thePrefs.GetVerbose())
			Log(newItem->uFlags, _T("%s"), (LPCTSTR)newItem->line);
		delete newItem;
	}
	m_queueLock.Unlock();
}

void CemuleApp::HandleLogQueue()
{
	m_queueLock.Lock();
	while (!m_QueueLog.IsEmpty()) {
		const SLogItem *newItem = m_QueueLog.RemoveHead();
		Log(newItem->uFlags, _T("%s"), (LPCTSTR)newItem->line);
		delete newItem;
	}
	m_queueLock.Unlock();
}

void CemuleApp::ClearDebugLogQueue(bool bDebugPendingMsgs)
{
	m_queueLock.Lock();
	while (!m_QueueDebugLog.IsEmpty()) {
		if (bDebugPendingMsgs)
			TRACE(_T("Queued dbg log msg: %s\n"), (LPCTSTR)m_QueueDebugLog.GetHead()->line);
		delete m_QueueDebugLog.RemoveHead();
	}
	m_queueLock.Unlock();
}

void CemuleApp::ClearLogQueue(bool bDebugPendingMsgs)
{
	m_queueLock.Lock();
	while (!m_QueueLog.IsEmpty()) {
		if (bDebugPendingMsgs)
			TRACE(_T("Queued log msg: %s\n"), (LPCTSTR)m_QueueLog.GetHead()->line);
		delete m_QueueLog.RemoveHead();
	}
	m_queueLock.Unlock();
}
// Elandal:ThreadSafeLogging <--

void CemuleApp::CreateAllFonts()
{
	///////////////////////////////////////////////////////////////////////////
	// Symbol font
	//
	//VERIFY( CreatePointFont(m_fontSymbol, 10 * 10, _T("Marlett")) );
	// Creating that font with 'SYMBOL_CHARSET' should be safer (seen in ATL/MFC code). Though
	// it seems that it does not solve the problem with '6' and '9' characters which are
	// shown for some ppl.
	m_fontSymbol.CreateFont(::GetSystemMetrics(SM_CYMENUCHECK), 0, 0, 0,
		FW_NORMAL, 0, 0, 0, SYMBOL_CHARSET, 0, 0, 0, 0, _T("Marlett"));


	///////////////////////////////////////////////////////////////////////////
	// Default GUI Font
	//
	// Fonts which are returned by 'GetStockObject'
	// --------------------------------------------
	// OEM_FIXED_FONT		Terminal
	// ANSI_FIXED_FONT		Courier
	// ANSI_VAR_FONT		MS Sans Serif
	// SYSTEM_FONT			System
	// DEVICE_DEFAULT_FONT	System
	// SYSTEM_FIXED_FONT	Fixedsys
	// DEFAULT_GUI_FONT		MS Shell Dlg (*1)
	//
	// (*1) Do not use 'GetStockObject(DEFAULT_GUI_FONT)' to get the 'Tahoma' font. It does
	// not work...
	//
	// The documentation in MSDN states that DEFAULT_GUI_FONT returns 'Tahoma' on
	// Win2000/XP systems. Though this is wrong, it may be true for US-English locales, but
	// it is wrong for other locales. Furthermore it is even documented that "MS Shell Dlg"
	// gets mapped to "MS Sans Serif" on Windows XP systems. Only "MS Shell Dlg 2" would
	// get mapped to "Tahoma", but "MS Shell Dlg 2" can not be used on prior Windows
	// systems.
	//
	// The reason why "MS Shell Dlg" is though mapped to "Tahoma" when used within dialog
	// resources is unclear.
	//
	// So, to get the same font which is used within dialogs which were created via dialog
	// resources which have the "MS Shell Dlg, 8" specified (again, in that special case
	// "MS Shell Dlg" gets mapped to "Tahoma" and not to "MS Sans Serif"), we just query
	// the main window (which is also a dialog) for the current font.
	//
	LOGFONT lfDefault;
	AfxGetMainWnd()->GetFont()->GetLogFont(&lfDefault);
	// WinXP: lfDefault.lfFaceName = "MS Shell Dlg 2" (!)
	// Vista: lfDefault.lfFaceName = "MS Shell Dlg 2"
	//
	// It would not be an error if that font name does not match our pre-determined
	// font name, I just want to know if that ever happens.
	ASSERT(m_strDefaultFontFaceName == lfDefault.lfFaceName);


	///////////////////////////////////////////////////////////////////////////
	// Bold Default GUI Font
	//
	LOGFONT lfDefaultBold = lfDefault;
	lfDefaultBold.lfWeight = FW_BOLD;
	VERIFY(m_fontDefaultBold.CreateFontIndirect(&lfDefaultBold));


	///////////////////////////////////////////////////////////////////////////
	// Server Log-, Message- and IRC-Window font
	//
	// Since we use "MS Shell Dlg 2" under WinXP (which will give us "Tahoma"),
	// that font is nevertheless set to "MS Sans Serif" because a scaled up "Tahoma"
	// font unfortunately does not look as good as a scaled up "MS Sans Serif" font.
	//
	// No! Do *not* use "MS Sans Serif" (never!). This will give a very old fashioned
	// font on certain Asian Windows systems. So, better use "MS Shell Dlg" or
	// "MS Shell Dlg 2" to let Windows map that font to the proper font on all Windows
	// systems.
	//
	LPLOGFONT plfHyperText = thePrefs.GetHyperTextLogFont();
	if (plfHyperText->lfFaceName[0] == _T('\0') || !m_fontHyperText.CreateFontIndirect(plfHyperText))
		CreatePointFont(m_fontHyperText, 10 * 10, lfDefault.lfFaceName);

	///////////////////////////////////////////////////////////////////////////
	// Verbose Log-font
	//
	// Why can't this font set via the font dialog??
//	HFONT hFontMono = CreateFont(10, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Lucida Console"));
//	m_fontLog.Attach(hFontMono);
	LPLOGFONT plfLog = thePrefs.GetLogFont();
	if (plfLog->lfFaceName[0] != _T('\0'))
		m_fontLog.CreateFontIndirect(plfLog);

	///////////////////////////////////////////////////////////////////////////
	// Font used for Message and IRC edit control, default font, just a little
	// larger.
	//
	// Since we use "MS Shell Dlg 2" under WinXP (which will give us "Tahoma"),
	// that font is nevertheless set to "MS Sans Serif" because a scaled up "Tahoma"
	// font unfortunately does not look as good as a scaled up "MS Sans Serif" font.
	//
	// No! Do *not* use "MS Sans Serif" (never!). This will give a very old fashioned
	// font on certain Asian Windows systems. So, better use "MS Shell Dlg" or
	// "MS Shell Dlg 2" to let Windows map that font to the proper font on all Windows
	// systems.
	//
	CreatePointFont(m_fontChatEdit, 11 * 10, lfDefault.lfFaceName);
}

const CString& CemuleApp::GetDefaultFontFaceName()
{
/* Support for old Windows was dropped
	if (m_strDefaultFontFaceName.IsEmpty()) {
		OSVERSIONINFO osvi;
		osvi.dwOSVersionInfoSize = (DWORD)sizeof osvi;
		if (GetVersionEx(&osvi)
			&& osvi.dwPlatformId == VER_PLATFORM_WIN32_NT
			&& osvi.dwMajorVersion >= 5) // Win2000/XP or higher
			m_strDefaultFontFaceName = _T("MS Shell Dlg 2");
		else
			m_strDefaultFontFaceName = _T("MS Shell Dlg");
	}*/
	return m_strDefaultFontFaceName;
}

void CemuleApp::CreateBackwardDiagonalBrush()
{
	static const WORD awBackwardDiagonalBrushPattern[8] = {0x0f, 0x1e, 0x3c, 0x78, 0xf0, 0xe1, 0xc3, 0x87};
	CBitmap bm;
	if (bm.CreateBitmap(8, 8, 1, 1, awBackwardDiagonalBrushPattern)) {
		LOGBRUSH logBrush = {};
		logBrush.lbStyle = BS_PATTERN;
		logBrush.lbHatch = (ULONG_PTR)bm.GetSafeHandle();
		logBrush.lbColor = RGB(0, 0, 0);
		VERIFY(m_brushBackwardDiagonal.CreateBrushIndirect(&logBrush));
	}
}

void CemuleApp::UpdateDesktopColorDepth()
{
	g_bLowColorDesktop = (GetDesktopColorDepth() <= 8);
#ifdef _DEBUG
	if (!g_bLowColorDesktop)
		g_bLowColorDesktop = (GetProfileInt(_T("eMule"), _T("LowColorRes"), 0) != 0);
#endif

	if (g_bLowColorDesktop) {
		// If we have 4- or 8-bit desktop color depth, Windows will (by design) load only
		// the 16 color versions of icons. Thus we force all image lists also to 4-bit format.
		m_iDfltImageListColorFlags = ILC_COLOR4;
	} else {
		// Get current desktop color depth and derive the image list format from it
		m_iDfltImageListColorFlags = GetAppImageListColorFlag();

		// Don't use 32-bit image lists if not supported by COMCTL32.DLL
		if (m_iDfltImageListColorFlags == ILC_COLOR32 && m_ullComCtrlVer < MAKEDLLVERULL(6, 0, 0, 0)) {
			// We fall back to 16-bit image lists because we do not provide 24-bit
			// versions of icons any longer (due to resource size restrictions for Win98). We
			// could also fall back to 24-bit image lists here but the difference is minimal
			// and considered not to be worth the additional memory consumption.
			//
			// Though, do not fall back to 8-bit image lists because this would let Windows
			// reduce the color resolution to the standard 256 color window system palette.
			// We need a 16-bit or 24-bit image list to hold all our 256 color icons (which
			// are not pre-quantized to standard 256 color windows system palette) without
			// losing any colors.
			m_iDfltImageListColorFlags = ILC_COLOR16;
		}
	}

	// Doesn't help.
	//m_aExtToSysImgIdx.RemoveAll();
}

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) noexcept
{
	// *) This function is invoked by the system from within a *DIFFERENT* thread !!
	//
	// *) This function is invoked only, if eMule was started with "RUNAS"
	//		- when user explicitly/manually logs off from the system (CTRL_LOGOFF_EVENT).
	//		- when user explicitly/manually does a reboot or shutdown (also: CTRL_LOGOFF_EVENT).
	//		- when eMule issues an ExitWindowsEx(EWX_LOGOFF/EWX_REBOOT/EWX_SHUTDOWN)
	//
	// NOTE: Windows will in each case forcefully terminate the process after 20 seconds!
	// Every action which is started after receiving this notification will get forcefully
	// terminated by Windows after 20 seconds.

	if (thePrefs.GetDebug2Disk()) {
		static TCHAR szCtrlType[40];
		LPCTSTR pszCtrlType;
		switch (dwCtrlType) {
		case CTRL_C_EVENT:
			pszCtrlType = _T("CTRL_C_EVENT");
			break;
		case CTRL_BREAK_EVENT:
			pszCtrlType = _T("CTRL_BREAK_EVENT");
			break;
		case CTRL_CLOSE_EVENT:
			pszCtrlType = _T("CTRL_CLOSE_EVENT");
			break;
		case CTRL_LOGOFF_EVENT:
			pszCtrlType = _T("CTRL_LOGOFF_EVENT");
			break;
		case CTRL_SHUTDOWN_EVENT:
			pszCtrlType = _T("CTRL_SHUTDOWN_EVENT");
			break;
		default:
			_sntprintf(szCtrlType, _countof(szCtrlType), _T("0x%08lx"), dwCtrlType);
			szCtrlType[_countof(szCtrlType) - 1] = _T('\0');
			pszCtrlType = szCtrlType;
		}
		theVerboseLog.Logf(_T("%hs: CtrlType=%s"), __FUNCTION__, pszCtrlType);

		// Default ProcessShutdownParameters: Level=0x00000280, Flags=0x00000000
		// Setting 'SHUTDOWN_NORETRY' does not prevent from getting terminated after 20 sec.
		//DWORD dwLevel = 0, dwFlags = 0;
		//GetProcessShutdownParameters(&dwLevel, &dwFlags);
		//theVerboseLog.Logf(_T("%hs: ProcessShutdownParameters #0: Level=0x%08x, Flags=0x%08x"), __FUNCTION__, dwLevel, dwFlags);
		//SetProcessShutdownParameters(dwLevel, SHUTDOWN_NORETRY);
	}

	if (dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT) {
		if (theApp.emuledlg->m_hWnd) {
			if (thePrefs.GetDebug2Disk())
				theVerboseLog.Logf(_T("%hs: Sending TM_CONSOLETHREADEVENT to main window"), __FUNCTION__);

			// Use 'SendMessage' to send the message to the (different) main thread. This is
			// done by intention because it lets this thread wait as long as the main thread
			// has called 'ExitProcess' or returns from processing the message. This is
			// needed to not let Windows terminate the process before the 20 sec. timeout.
			if (!theApp.emuledlg->SendMessage(TM_CONSOLETHREADEVENT, dwCtrlType, (LPARAM)GetCurrentThreadId())) {
				theApp.m_app_state = APP_STATE_SHUTTINGDOWN; // as a last attempt
				if (thePrefs.GetDebug2Disk())
					theVerboseLog.Logf(_T("%hs: Error: Failed to send TM_CONSOLETHREADEVENT to main window - error %u"), __FUNCTION__, ::GetLastError());
			}
		}
	}

	// Returning FALSE does not cause Windows to immediately terminate the process. Though,
	// that only depends on the next registered console control handler. The default seems
	// to wait 20 sec. until the process has terminated. After that timeout Windows
	// nevertheless terminates the process.
	//
	// For whatever unknown reason, this is *not* always true!? It may happen that Windows
	// terminates the process *before* the 20 sec. timeout if (and only if) the console
	// control handler thread has already terminated. So, we have to take care that we do not
	// exit this thread before the main thread has called 'ExitProcess' (in a synchronous
	// way) -- see also the 'SendMessage' above.
	if (thePrefs.GetDebug2Disk())
		theVerboseLog.Logf(_T("%hs: returning"), __FUNCTION__);
	return FALSE; // FALSE: Let the system kill the process with the default handler.
}

void CemuleApp::UpdateLargeIconSize()
{
	// initialize with system values in case we don't find the Shell's registry key
	m_sizBigSystemIcon.cx = ::GetSystemMetrics(SM_CXICON);
	m_sizBigSystemIcon.cy = ::GetSystemMetrics(SM_CYICON);

	// get the Shell's registry key for the large icon size - the large icons which are
	// returned by the Shell are based on that size rather than on the system icon size
	CRegKey key;
	if (key.Open(HKEY_CURRENT_USER, _T("Control Panel\\desktop\\WindowMetrics"), KEY_READ) == ERROR_SUCCESS) {
		TCHAR szShellLargeIconSize[12];
		ULONG ulChars = _countof(szShellLargeIconSize);
		if (key.QueryStringValue(_T("Shell Icon Size"), szShellLargeIconSize, &ulChars) == ERROR_SUCCESS) {
			UINT uIconSize = 0;
			if (_stscanf(szShellLargeIconSize, _T("%u"), &uIconSize) == 1 && uIconSize > 0) {
				m_sizBigSystemIcon.cx = uIconSize;
				m_sizBigSystemIcon.cy = uIconSize;
			}
		}
	}
}

void CemuleApp::ResetStandByIdleTimer()
{
	// check if anything is going on (ongoing upload, download or connected) and reset the idle timer if so
	if (IsConnected() || (uploadqueue != NULL && uploadqueue->GetUploadQueueLength() > 0)
		|| (downloadqueue != NULL && downloadqueue->GetDatarate() > 0))
	{
		EXECUTION_STATE(WINAPI *pfnSetThreadExecutionState)(EXECUTION_STATE);
		(FARPROC&)pfnSetThreadExecutionState = GetProcAddress(GetModuleHandle(_T("kernel32")), "SetThreadExecutionState");
		if (pfnSetThreadExecutionState)
			VERIFY(pfnSetThreadExecutionState(ES_SYSTEM_REQUIRED));
		else
			ASSERT(0);
	}
}

bool CemuleApp::IsXPThemeActive() const
{
	// TRUE: If an XP style (and only an XP style) is active
	return theApp.m_ullComCtrlVer < MAKEDLLVERULL(6, 16, 0, 0) && g_xpStyle.IsThemeActive() && g_xpStyle.IsAppThemed();
}

bool CemuleApp::IsVistaThemeActive() const
{
	// TRUE: If a Vista (or better) style is active
	return theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0) && g_xpStyle.IsThemeActive() && g_xpStyle.IsAppThemed();
}