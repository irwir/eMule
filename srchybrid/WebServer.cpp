#include "stdafx.h"
#include <locale.h>
#include <algorithm>
#include "emule.h"
#include "StringConversion.h"
#include "WebServer.h"
#include "ClientCredits.h"
#include "ClientList.h"
#include "DownloadQueue.h"
#include "ED2KLink.h"
#include "emuledlg.h"
#include "FriendList.h"
#include "MD5Sum.h"
#include "ini2.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "KademliaWnd.h"
#include "KadSearchListCtrl.h"
#include "kademlia/kademlia/Entry.h"
#include "KnownFileList.h"
#include "ListenSocket.h"
#include "Log.h"
#include "MenuCmds.h"
#include "Preferences.h"
#include "Server.h"
#include "ServerList.h"
#include "ServerWnd.h"
#include "SearchList.h"
#include "SearchDlg.h"
#include "SearchParams.h"
#include "SharedFileList.h"
#include "ServerConnect.h"
#include "StatisticsDlg.h"
#include "Opcodes.h"
#include "QArray.h"
#include "TransferDlg.h"
#include "UploadQueue.h"
#include "UpDownClient.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define HTTPInit "Server: eMule\r\nConnection: close\r\nContent-Type: text/html\r\n"
#define HTTPInitGZ HTTPInit "Content-Encoding: gzip\r\n"
#define HTTPENCODING _T("utf-8")

#define WEB_SERVER_TEMPLATES_VERSION	7

//SyruS CQArray-Sorting operators
bool operator > (QueueUsers &first, QueueUsers &second)
{
	return (first.sIndex.CompareNoCase(second.sIndex) > 0);
}
bool operator < (QueueUsers &first, QueueUsers &second)
{
	return (first.sIndex.CompareNoCase(second.sIndex) < 0);
}

bool operator > (SearchFileStruct &first, SearchFileStruct &second)
{
	return (first.m_strIndex.CompareNoCase(second.m_strIndex) > 0);
}
bool operator < (SearchFileStruct &first, SearchFileStruct &second)
{
	return (first.m_strIndex.CompareNoCase(second.m_strIndex) < 0);
}

static BOOL	WSdownloadColumnHidden[8];
static BOOL	WSuploadColumnHidden[5];
static BOOL	WSqueueColumnHidden[4];
static BOOL	WSsharedColumnHidden[7];
static BOOL	WSserverColumnHidden[10];
static BOOL	WSsearchColumnHidden[4];

CWebServer::CWebServer()
	: m_Templates()
	, m_uCurIP()
	, m_nStartTempDisabledTime()
	, m_iSearchSortby(3)
	, m_nIntruderDetect()
	, m_bServerWorking()
	, m_bSearchAsc()
	, m_bIsTempDisabled()
{
	CIni ini(thePrefs.GetConfigFile(), _T("WebServer"));

	ini.SerGet(true, WSdownloadColumnHidden, _countof(WSdownloadColumnHidden), _T("downloadColumnHidden"));
	ini.SerGet(true, WSuploadColumnHidden, _countof(WSuploadColumnHidden), _T("uploadColumnHidden"));
	ini.SerGet(true, WSqueueColumnHidden, _countof(WSqueueColumnHidden), _T("queueColumnHidden"));
	ini.SerGet(true, WSsearchColumnHidden, _countof(WSsearchColumnHidden), _T("searchColumnHidden"));
	ini.SerGet(true, WSsharedColumnHidden, _countof(WSsharedColumnHidden), _T("sharedColumnHidden"));
	ini.SerGet(true, WSserverColumnHidden, _countof(WSserverColumnHidden), _T("serverColumnHidden"));

	m_Params.bShowUploadQueue = ini.GetBool(_T("ShowUploadQueue"), false);
	m_Params.bShowUploadQueueBanned = ini.GetBool(_T("ShowUploadQueueBanned"), false);
	m_Params.bShowUploadQueueFriend = ini.GetBool(_T("ShowUploadQueueFriend"), false);

	m_Params.bDownloadSortReverse = ini.GetBool(_T("DownloadSortReverse"), true);
	m_Params.bUploadSortReverse = ini.GetBool(_T("UploadSortReverse"), true);
	m_Params.bQueueSortReverse = ini.GetBool(_T("QueueSortReverse"), true);
	m_Params.bServerSortReverse = ini.GetBool(_T("ServerSortReverse"), true);
	m_Params.bSharedSortReverse = ini.GetBool(_T("SharedSortReverse"), true);

	m_Params.DownloadSort = (DownloadSort)ini.GetInt(_T("DownloadSort"), DOWN_SORT_NAME);
	m_Params.UploadSort = (UploadSort)ini.GetInt(_T("UploadSort"), UP_SORT_FILENAME);
	m_Params.QueueSort = (QueueSort)ini.GetInt(_T("QueueSort"), QU_SORT_FILENAME);
	m_Params.ServerSort = (ServerSort)ini.GetInt(_T("ServerSort"), SERVER_SORT_NAME);
	m_Params.SharedSort = (SharedSort)ini.GetInt(_T("SharedSort"), SHARED_SORT_NAME);
}

CWebServer::~CWebServer()
{
	// save layout settings
	CIni ini(thePrefs.GetConfigFile(), _T("WebServer"));

	ini.WriteBool(_T("ShowUploadQueue"), m_Params.bShowUploadQueue);
	ini.WriteBool(_T("ShowUploadQueueBanned"), m_Params.bShowUploadQueueBanned);
	ini.WriteBool(_T("ShowUploadQueueFriend"), m_Params.bShowUploadQueueFriend);

	ini.WriteBool(_T("DownloadSortReverse"), m_Params.bDownloadSortReverse);
	ini.WriteBool(_T("UploadSortReverse"), m_Params.bUploadSortReverse);
	ini.WriteBool(_T("QueueSortReverse"), m_Params.bQueueSortReverse);
	ini.WriteBool(_T("ServerSortReverse"), m_Params.bServerSortReverse);
	ini.WriteBool(_T("SharedSortReverse"), m_Params.bSharedSortReverse);

	ini.WriteInt(_T("DownloadSort"), m_Params.DownloadSort);
	ini.WriteInt(_T("UploadSort"), m_Params.UploadSort);
	ini.WriteInt(_T("QueueSort"), m_Params.QueueSort);
	ini.WriteInt(_T("ServerSort"), m_Params.ServerSort);
	ini.WriteInt(_T("SharedSort"), m_Params.SharedSort);

	if (m_bServerWorking)
		StopSockets();
}

void CWebServer::_SaveWIConfigArray(BOOL *array, int size, LPCTSTR key)
{
	CIni ini(thePrefs.GetConfigFile(), _T("WebServer"));
	ini.SerGet(false, array, size, key);
}

bool CWebServer::ReloadTemplates()
{
	TCHAR *sPrevLocale = _tsetlocale(LC_TIME, NULL);

	_tsetlocale(LC_TIME, _T("English"));
	CTime t = CTime::GetCurrentTime();
	m_Params.sLastModified = t.FormatGmt("%a, %d %b %Y %H:%M:%S GMT");
	m_Params.sETag = MD5Sum(m_Params.sLastModified).GetHashString();
	_tsetlocale(LC_TIME, sPrevLocale);

	const CString &sFile(thePrefs.GetTemplate());

	CStdioFile file;
	if (file.Open(sFile, CFile::modeRead | CFile::shareDenyWrite | CFile::typeText)) {
		CString sAll, sLine;
		while (file.ReadString(sLine))
			sAll.AppendFormat(_T("%s\n"), (LPCTSTR)sLine);
		file.Close();

		const CString &sVersion(_LoadTemplate(sAll, _T("TMPL_VERSION")));
		if (_tstol(sVersion) >= WEB_SERVER_TEMPLATES_VERSION) {
			m_Templates.sHeader = _LoadTemplate(sAll, _T("TMPL_HEADER"));
			m_Templates.sHeaderStylesheet = _LoadTemplate(sAll, _T("TMPL_HEADER_STYLESHEET"));
			m_Templates.sFooter = _LoadTemplate(sAll, _T("TMPL_FOOTER"));
			m_Templates.sServerList = _LoadTemplate(sAll, _T("TMPL_SERVER_LIST"));
			m_Templates.sServerLine = _LoadTemplate(sAll, _T("TMPL_SERVER_LINE"));
			m_Templates.sTransferImages = _LoadTemplate(sAll, _T("TMPL_TRANSFER_IMAGES"));
			m_Templates.sTransferList = _LoadTemplate(sAll, _T("TMPL_TRANSFER_LIST"));
			m_Templates.sTransferDownHeader = _LoadTemplate(sAll, _T("TMPL_TRANSFER_DOWN_HEADER"));
			m_Templates.sTransferDownFooter = _LoadTemplate(sAll, _T("TMPL_TRANSFER_DOWN_FOOTER"));
			m_Templates.sTransferDownLine = _LoadTemplate(sAll, _T("TMPL_TRANSFER_DOWN_LINE"));
			m_Templates.sTransferUpHeader = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_HEADER"));
			m_Templates.sTransferUpFooter = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_FOOTER"));
			m_Templates.sTransferUpLine = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_LINE"));
			m_Templates.sTransferUpQueueShow = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_SHOW"));
			m_Templates.sTransferUpQueueHide = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_HIDE"));
			m_Templates.sTransferUpQueueLine = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_LINE"));
			m_Templates.sTransferUpQueueBannedShow = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_BANNED_SHOW"));
			m_Templates.sTransferUpQueueBannedHide = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_BANNED_HIDE"));
			m_Templates.sTransferUpQueueBannedLine = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_BANNED_LINE"));
			m_Templates.sTransferUpQueueFriendShow = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_FRIEND_SHOW"));
			m_Templates.sTransferUpQueueFriendHide = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_FRIEND_HIDE"));
			m_Templates.sTransferUpQueueFriendLine = _LoadTemplate(sAll, _T("TMPL_TRANSFER_UP_QUEUE_FRIEND_LINE"));
			m_Templates.sSharedList = _LoadTemplate(sAll, _T("TMPL_SHARED_LIST"));
			m_Templates.sSharedLine = _LoadTemplate(sAll, _T("TMPL_SHARED_LINE"));
			m_Templates.sGraphs = _LoadTemplate(sAll, _T("TMPL_GRAPHS"));
			m_Templates.sLog = _LoadTemplate(sAll, _T("TMPL_LOG"));
			m_Templates.sServerInfo = _LoadTemplate(sAll, _T("TMPL_SERVERINFO"));
			m_Templates.sDebugLog = _LoadTemplate(sAll, _T("TMPL_DEBUGLOG"));
			m_Templates.sStats = _LoadTemplate(sAll, _T("TMPL_STATS"));
			m_Templates.sPreferences = _LoadTemplate(sAll, _T("TMPL_PREFERENCES"));
			m_Templates.sLogin = _LoadTemplate(sAll, _T("TMPL_LOGIN"));
			m_Templates.sAddServerBox = _LoadTemplate(sAll, _T("TMPL_ADDSERVERBOX"));
			m_Templates.sSearch = _LoadTemplate(sAll, _T("TMPL_SEARCH"));
			m_Templates.iProgressbarWidth = (uint16)_tstoi(_LoadTemplate(sAll, _T("PROGRESSBARWIDTH")));
			m_Templates.sSearchHeader = _LoadTemplate(sAll, _T("TMPL_SEARCH_RESULT_HEADER"));
			m_Templates.sSearchResultLine = _LoadTemplate(sAll, _T("TMPL_SEARCH_RESULT_LINE"));
			m_Templates.sProgressbarImgs = _LoadTemplate(sAll, _T("PROGRESSBARIMGS"));
			m_Templates.sProgressbarImgsPercent = _LoadTemplate(sAll, _T("PROGRESSBARPERCENTIMG"));
			m_Templates.sCatArrow = _LoadTemplate(sAll, _T("TMPL_CATARROW"));
			m_Templates.sDownArrow = _LoadTemplate(sAll, _T("TMPL_DOWNARROW"));
			m_Templates.sUpArrow = _LoadTemplate(sAll, _T("TMPL_UPARROW"));
			m_Templates.sDownDoubleArrow = _LoadTemplate(sAll, _T("TMPL_DNDOUBLEARROW"));
			m_Templates.sUpDoubleArrow = _LoadTemplate(sAll, _T("TMPL_UPDOUBLEARROW"));
			m_Templates.sKad = _LoadTemplate(sAll, _T("TMPL_KADDLG"));
			m_Templates.sBootstrapLine = _LoadTemplate(sAll, _T("TMPL_BOOTSTRAPLINE"));
			m_Templates.sMyInfoLog = _LoadTemplate(sAll, _T("TMPL_MYINFO"));
			m_Templates.sCommentList = _LoadTemplate(sAll, _T("TMPL_COMMENTLIST"));
			m_Templates.sCommentListLine = _LoadTemplate(sAll, _T("TMPL_COMMENTLIST_LINE"));

			m_Templates.sProgressbarImgsPercent.Replace(_T("[PROGRESSGIFNAME]"), _T("%s"));
			m_Templates.sProgressbarImgsPercent.Replace(_T("[PROGRESSGIFINTERNAL]"), _T("%i"));
			m_Templates.sProgressbarImgs.Replace(_T("[PROGRESSGIFNAME]"), _T("%s"));
			m_Templates.sProgressbarImgs.Replace(_T("[PROGRESSGIFINTERNAL]"), _T("%i"));
			return true;
		}
		if (thePrefs.GetWSIsEnabled() || m_bServerWorking) {
			CString buffer;
			buffer.Format(GetResString(IDS_WS_ERR_LOADTEMPLATE), (LPCTSTR)sFile);
			AddLogLine(true, buffer);
			AfxMessageBox(buffer, MB_OK);
			StopServer();
		}
	} else if (m_bServerWorking) {
		AddLogLine(true, GetResString(IDS_WEB_ERR_CANTLOAD), (LPCTSTR)sFile);
		StopServer();
	}
	return false;
}

CString CWebServer::_LoadTemplate(const CString &sAll, const CString &sTemplateName)
{
	int len = sTemplateName.GetLength();
	CString sTemplate;
	sTemplate.Format(_T("<--%s-->"), (LPCTSTR)sTemplateName);
	int nStart = sAll.Find(sTemplate);
	sTemplate.Insert(len + 3, _T("_END"));
	int nEnd = sAll.Find(sTemplate);
	if (nStart >= 0 && nStart < nEnd) {
		nStart += len + 7;
		return sAll.Mid(nStart, nEnd - nStart - 1);
	}
	if (sTemplateName == _T("TMPL_VERSION"))
		AddLogLine(true, (LPCTSTR)GetResString(IDS_WS_ERR_LOADTEMPLATE), (LPCTSTR)sTemplateName);
	if (nStart == -1)
		AddLogLine(false, (LPCTSTR)GetResString(IDS_WEB_ERR_CANTLOAD), (LPCTSTR)sTemplateName);
	return CString();
}

//Cax2 - restarts the server with the new port settings
void CWebServer::RestartSockets()
{
	StopSockets();
	if (m_bServerWorking)
		StartSockets(this);
}

void CWebServer::StartServer()
{
	if (m_bServerWorking == thePrefs.GetWSIsEnabled())
		return;
	m_bServerWorking = thePrefs.GetWSIsEnabled();
	if (m_bServerWorking) {
		ReloadTemplates();
		if (m_bServerWorking) {
			StartSockets(this);
			m_nIntruderDetect = 0;
			m_nStartTempDisabledTime = 0;
			m_bIsTempDisabled = false;
		}
	} else
		StopSockets();

	UINT uid = (thePrefs.GetWSIsEnabled() && m_bServerWorking) ? IDS_ENABLED : IDS_DISABLED;
	AddLogLine(false, _T("%s: %s%s")
		, (LPCTSTR)_GetPlainResString(IDS_PW_WS)
		, (LPCTSTR)_GetPlainResString(uid).MakeLower()
		, (uid == IDS_ENABLED && thePrefs.GetWebUseHttps()) ? _T(" (HTTPS)") : _T(""));
}

void CWebServer::StopServer()
{
	if (m_bServerWorking) {
		StopSockets();
		m_bServerWorking = false;
	}
	thePrefs.SetWSIsEnabled(false);
}

void CWebServer::_RemoveServer(const CString &sIP, int nPort)
{
	CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
	if (server != NULL)
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_SERVER_REMOVE, (LPARAM)server);
}

void CWebServer::_AddToStatic(const CString &sIP, int nPort)
{
	CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
	if (server != NULL)
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_ADD_TO_STATIC, (LPARAM)server);
}

void CWebServer::_RemoveFromStatic(const CString &sIP, int nPort)
{
	CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
	if (server != NULL)
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_REMOVE_FROM_STATIC, (LPARAM)server);
}


void CWebServer::AddStatsLine(UpDown line)
{
	m_Params.PointsForWeb.Add(line);
	if (m_Params.PointsForWeb.GetCount() > WEB_GRAPH_WIDTH)
		m_Params.PointsForWeb.RemoveAt(0);
}

CString CWebServer::_SpecialChars(const CString &cstr, bool noquote /*=false*/)
{
	CString str(cstr);
	str.Replace(_T("&"), _T("&amp;"));
	str.Replace(_T("<"), _T("&lt;"));
	str.Replace(_T(">"), _T("&gt;"));
	str.Replace(_T("\""), _T("&quot;"));
	if (noquote) {
		str.Replace(_T("'"), _T("&#8217;"));
		str.Replace(_T("\n"), _T("\\n"));
	}
	return str;
}

void CWebServer::_ConnectToServer(const CString &sIP, int nPort)
{
	CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
	if (server != NULL)
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_CONNECTTOSERVER, (LPARAM)server);
}

void CWebServer::_ProcessURL(const ThreadData &Data)
{
	if (!theApp.IsRunning())
		return;

	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return;

	SetThreadLocale(thePrefs.GetLanguageID());

	//(0.29b)//////////////////////////////////////////////////////////////////
	// Here we are in real trouble! We are accessing the entire emule main thread
	// data without any synchronization!! Either we use the message pump for m_pdlgEmule
	// or use some hundreds of critical sections... For now, an exception handler
	// should avoid the worse things.
	//////////////////////////////////////////////////////////////////////////
	(void)CoInitialize(NULL);

#ifndef _DEBUG
	try {
#endif
		bool isUseGzip = thePrefs.GetWebUseGzip();

		srand((unsigned)time(NULL));

		uint32 myip = inet_addr(ipstrA(Data.inadr));

		// check for being banned
		int myfaults = 0;
		DWORD now = ::GetTickCount();
		for (INT_PTR i = pThis->m_Params.badlogins.GetCount(); --i >= 0;) {
			if (now >= pThis->m_Params.badlogins[i].timestamp + MIN2MS(15))
				pThis->m_Params.badlogins.RemoveAt(i); // remove outdated entries
			else
				if (pThis->m_Params.badlogins[i].datalen == myip)
					++myfaults;
		}
		if (myfaults > 4) {
			Data.pSocket->SendContent(HTTPInit, _GetPlainResString(IDS_ACCESSDENIED));
			CoUninitialize();
			return;
		}

		bool justAddLink = false;
		bool login = false;
		CString sSession = _ParseURL(Data.sURL, _T("ses"));
		long lSession = _tstol(sSession);

		if (_ParseURL(Data.sURL, _T("w")) == _T("password")) {
			const CString &ip(ipstr(Data.inadr));

			if (!_ParseURL(Data.sURL, _T("c")).IsEmpty())
				// just sent password to add link remotely. Don't start a session.
				justAddLink = true;

			if (MD5Sum(_ParseURL(Data.sURL, _T("p"))).GetHashString() == thePrefs.GetWSPass()) {
				if (!justAddLink) {
					// user wants to login
					Session ses;
					ses.admin = true;
					ses.startTime = CTime::GetCurrentTime();
					ses.lSession = lSession = (long)(rand() >> 1);
					ses.lastcat = 0 - thePrefs.GetCatFilter(0);
					pThis->m_Params.Sessions.Add(ses);
				}

				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATEMYINFO, 0);

				AddLogLine(true, GetResString(IDS_WEB_ADMINLOGIN) + _T(" (%s)"), (LPCTSTR)ip);
				login = true;
			} else if (thePrefs.GetWSIsLowUserEnabled() && !thePrefs.GetWSLowPass().IsEmpty() && MD5Sum(_ParseURL(Data.sURL, _T("p"))).GetHashString() == thePrefs.GetWSLowPass()) {
				Session ses;
				ses.admin = false;
				ses.startTime = CTime::GetCurrentTime();
				ses.lSession = lSession = (long)(rand() >> 1);
				pThis->m_Params.Sessions.Add(ses);

				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATEMYINFO, 0);

				AddLogLine(true, GetResString(IDS_WEB_GUESTLOGIN) + _T(" (%s)"), (LPCTSTR)ip);
				login = true;
			} else {
				LogWarning(LOG_STATUSBAR, GetResString(IDS_WEB_BADLOGINATTEMPT) + _T(" (%s)"), (LPCTSTR)ip);

				BadLogin newban = {myip, now};	// save failed attempt (ip,time)
				pThis->m_Params.badlogins.Add(newban);
				if (++myfaults > 4) {
					Data.pSocket->SendContent(HTTPInit, _GetPlainResString(IDS_ACCESSDENIED));
					CoUninitialize();
					return;
				}
			}
			isUseGzip = false; // [Julien]
			if (login)	// on login, forget previous failed attempts
				for (INT_PTR i = pThis->m_Params.badlogins.GetCount(); --i >= 0;)
					if (pThis->m_Params.badlogins[i].datalen == myip)
						pThis->m_Params.badlogins.RemoveAt(i);
		}

		TCHAR *gzipOut = NULL;
		DWORD gzipLen = 0;

		CString Out;
		sSession.Format(_T("%ld"), lSession);

		if (_ParseURL(Data.sURL, _T("w")) == _T("logout"))
			_RemoveSession(Data, lSession);

		if (_IsLoggedIn(Data, lSession)) {
			bool bAdmin = _IsSessionAdmin(Data, sSession);
			if (_ParseURL(Data.sURL, _T("w")) == _T("close") && bAdmin && thePrefs.GetWebAdminAllowedHiLevFunc()) {
				theApp.m_app_state = APP_STATE_SHUTTINGDOWN;
				_RemoveSession(Data, lSession);

				// send answer...
				Out += _GetLoginScreen(Data);
				Data.pSocket->SendContent(HTTPInit, Out);

				SendMessage(theApp.emuledlg->m_hWnd, WM_CLOSE, 0, 0);

				CoUninitialize();
				return;
			}

			if (_ParseURL(Data.sURL, _T("w")) == _T("shutdown") && bAdmin) {
				_RemoveSession(Data, lSession);
				// send answer...
				Out += _GetLoginScreen(Data);
				Data.pSocket->SendContent(HTTPInit, Out);

				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_WINFUNC, 1);

				CoUninitialize();
				return;
			}

			if (_ParseURL(Data.sURL, _T("w")) == _T("reboot") && bAdmin) {
				_RemoveSession(Data, lSession);

				// send answer...
				Out += _GetLoginScreen(Data);
				Data.pSocket->SendContent(HTTPInit, Out);

				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_WINFUNC, 2);

				CoUninitialize();
				return;
			}

			if (_ParseURL(Data.sURL, _T("w")) == _T("commentlist")) {
				const CString &Out1(_GetCommentlist(Data));

				if (!Out1.IsEmpty()) {
					Data.pSocket->SendContent(HTTPInit, Out1);

					CoUninitialize();
					return;
				}
			} else if (_ParseURL(Data.sURL, _T("w")) == _T("getfile") && bAdmin) {
				uchar FileHash[MDX_DIGEST_SIZE];
				bool bHash = strmd4(_ParseURL(Data.sURL, _T("filehash")), FileHash);
				CKnownFile *kf = bHash ? theApp.sharedfiles->GetFileByID(FileHash) : NULL;
				if (kf) {
					if (thePrefs.GetMaxWebUploadFileSizeMB() != 0 && kf->GetFileSize() > (uint64)thePrefs.GetMaxWebUploadFileSizeMB() * 1024 * 1024) {
						Data.pSocket->SendReply("HTTP/1.1 403 Forbidden\r\n");

						CoUninitialize();
						return;
					}

					CFile file;
					if (file.Open(kf->GetFilePath(), CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary)) {
						EMFileSize filesize = kf->GetFileSize();

#define SENDFILEBUFSIZE 2048
						char *buffer = (char*)malloc(SENDFILEBUFSIZE);
						if (!buffer) {
							Data.pSocket->SendReply("HTTP/1.1 500 Internal Server Error\r\n");
							CoUninitialize();
							return;
						}

						CStringA fname(kf->GetFileName());
						CStringA szBuf;
						szBuf.Format("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
							"Content-Description: \"%s\"\r\n"
							"Content-Disposition: attachment; filename=\"%s\";\r\n"
							"Content-Transfer-Encoding: binary\r\n"
							"Content-Length: %I64u\r\n\r\n"
							, (LPCSTR)fname, (LPCSTR)fname, (uint64)filesize);
						Data.pSocket->SendData(szBuf, szBuf.GetLength());

						for (UINT r = 1; (uint64)filesize > 0 && r;) {
							r = file.Read(buffer, SENDFILEBUFSIZE);
							filesize -= r;
							Data.pSocket->SendData(buffer, r);
						}
						file.Close();

						free(buffer);
					} else
						Data.pSocket->SendReply("HTTP/1.1 404 File not found\r\n");
					CoUninitialize();
					return;
				}
			}

			Out += _GetHeader(Data, lSession);
			const CString &sPage(_ParseURL(Data.sURL, _T("w")));
			if (sPage == _T("server"))
				Out += _GetServerList(Data);
			else if (sPage == _T("shared"))
				Out += _GetSharedFilesList(Data);
			else if (sPage == _T("transfer"))
				Out += _GetTransferList(Data);
			else if (sPage == _T("search"))
				Out += _GetSearch(Data);
			else if (sPage == _T("graphs"))
				Out += _GetGraphs(Data);
			else if (sPage == _T("log"))
				Out += _GetLog(Data);
			if (sPage == _T("sinfo"))
				Out += _GetServerInfo(Data);
			if (sPage == _T("debuglog"))
				Out += _GetDebugLog(Data);
			if (sPage == _T("myinfo"))
				Out += _GetMyInfo(Data);
			if (sPage == _T("stats"))
				Out += _GetStats(Data);
			if (sPage == _T("kad"))
				Out += _GetKadDlg(Data);
			if (sPage == _T("options")) {
				isUseGzip = false;
				Out += _GetPreferences(Data);
			}
			Out += _GetFooter(Data);

			if (sPage.IsEmpty())
				isUseGzip = false;

			if (isUseGzip) {
				bool bOk = false;
				try {
					CStringA strA(wc2utf8(Out));
					uLongf destLen = strA.GetLength() + 1024;
					gzipOut = new TCHAR[destLen];
					if (_GzipCompress((Bytef*)gzipOut, &destLen, (Bytef*)(LPCSTR)strA, strA.GetLength(), Z_DEFAULT_COMPRESSION) == Z_OK) {
						bOk = true;
						gzipLen = destLen;
					}
				} catch (...) {
					ASSERT(0);
				}
				if (!bOk) {
					isUseGzip = false;
					delete[] gzipOut;
					gzipOut = NULL;
				}
			}
		} else if (justAddLink && login)
			Out += _GetRemoteLinkAddedOk(Data);
		else {
			isUseGzip = false;
			Out += justAddLink ? _GetRemoteLinkAddedFailed(Data) : _GetLoginScreen(Data);
		}

		// send answer...
		if (!isUseGzip)
			Data.pSocket->SendContent(HTTPInit, Out);
		else
			Data.pSocket->SendContent(HTTPInitGZ, gzipOut, gzipLen);

		delete[] gzipOut;

#ifndef _DEBUG
	} catch (...) {
		AddDebugLogLine(DLP_VERYHIGH, false, _T("*** Unknown exception in CWebServer::ProcessURL"));
		ASSERT(0);
	}
#endif

	CoUninitialize();
}

CString CWebServer::_ParseURLArray(CString URL, CString fieldname)
{
	URL.MakeLower();
	fieldname.MakeLower();
	CString res;
	while (!URL.IsEmpty()) {
		int pos = URL.Find(fieldname + _T('='));
		if (pos < 0)
			break;
		const CString &temp(_ParseURL(URL, fieldname));
		if (temp.IsEmpty())
			break;
		res.AppendFormat(_T("%s|"), (LPCTSTR)temp);
		URL.Delete(pos, 10);
	}
	return res;
}

CString CWebServer::_ParseURL(const CString &URL, const CString &fieldname)
{
	int findPos = URL.Find(_T('?'));
	if (findPos >= 0) {
		CString Parameter(URL.Mid(findPos + 1, URL.GetLength() - findPos - 1));

		int findLength;
		// search the field name beginning / middle and strip the rest...
		findPos = Parameter.Find(fieldname + _T('='));
		findLength = !findPos ? fieldname.GetLength() + 1 : 0;
		int iPos = Parameter.Find(_T('&') + fieldname + _T('='));
		if (iPos >= 0) {
			findPos = iPos;
			findLength = fieldname.GetLength() + 2;
		}
		if (findPos >= 0) {
			Parameter.Delete(0, findPos + findLength);
			iPos = Parameter.Find(_T('&'));
			if (iPos >= 0)
				Parameter.Truncate(iPos);
			Parameter.Replace(_T('+'), _T(' '));
			// decode value...
			return OptUtf8ToStr(URLDecode(Parameter, true));
		}
	}
	return CString();
}

CString CWebServer::_GetHeader(const ThreadData &Data, long lSession)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	CString sSession;
	sSession.Format(_T("%ld"), lSession);

	CString Out = pThis->m_Templates.sHeader;
	Out.Replace(_T("[CharSet]"), HTTPENCODING);

	//	Auto-refresh code
	const CString &sPage(_ParseURL(Data.sURL, _T("w")));
	bool bAdmin = _IsSessionAdmin(Data, lSession);

	CString sRefresh;
	if (sPage == _T("options") || sPage == _T("stats") || sPage == _T("password"))
		sRefresh += _T('0');
	else
		sRefresh.Format(_T("%d"), SEC2MS(thePrefs.GetWebPageRefresh()));

	CString swCommand;
	swCommand.Format(_T("%s&amp;cat=%s&amp;dummy=%d")
		, (LPCTSTR)_ParseURL(Data.sURL, _T("w")), (LPCTSTR)_ParseURL(Data.sURL, _T("cat")), rand());

	Out.Replace(_T("[admin]"), (bAdmin && thePrefs.GetWebAdminAllowedHiLevFunc()) ? _T("admin") : _T(""));
	Out.Replace(_T("[Session]"), sSession);
	Out.Replace(_T("[RefreshVal]"), sRefresh);
	Out.Replace(_T("[wCommand]"), swCommand);
	Out.Replace(_T("[eMuleAppName]"), _T("eMule"));
	Out.Replace(_T("[version]"), theApp.m_strCurVersionLong);
	Out.Replace(_T("[StyleSheet]"), pThis->m_Templates.sHeaderStylesheet);
	Out.Replace(_T("[WebControl]"), _GetPlainResString(IDS_WEB_CONTROL));
	Out.Replace(_T("[Transfer]"), _GetPlainResString(IDS_CD_TRANS));
	Out.Replace(_T("[Server]"), _GetPlainResString(IDS_SV_SERVERLIST));
	Out.Replace(_T("[Shared]"), _GetPlainResString(IDS_SHAREDFILES));
	Out.Replace(_T("[Graphs]"), _GetPlainResString(IDS_GRAPHS));
	Out.Replace(_T("[Log]"), _GetPlainResString(IDS_SV_LOG));
	Out.Replace(_T("[ServerInfo]"), _GetPlainResString(IDS_SV_SERVERINFO));
	Out.Replace(_T("[DebugLog]"), _GetPlainResString(IDS_SV_DEBUGLOG));
	Out.Replace(_T("[MyInfo]"), _GetPlainResString(IDS_MYINFO));
	Out.Replace(_T("[Stats]"), _GetPlainResString(IDS_SF_STATISTICS));
	Out.Replace(_T("[Options]"), _GetPlainResString(IDS_EM_PREFS));
	Out.Replace(_T("[Ed2klink]"), _GetPlainResString(IDS_SW_LINK));
	Out.Replace(_T("[Close]"), _GetPlainResString(IDS_WEB_SHUTDOWN));
	Out.Replace(_T("[Reboot]"), _GetPlainResString(IDS_WEB_REBOOT));
	Out.Replace(_T("[Shutdown]"), _GetPlainResString(IDS_WEB_SHUTDOWNSYSTEM));
	Out.Replace(_T("[WebOptions]"), _GetPlainResString(IDS_WEB_ADMINMENU));
	Out.Replace(_T("[Logout]"), _GetPlainResString(IDS_WEB_LOGOUT));
	Out.Replace(_T("[Search]"), _GetPlainResString(IDS_EM_SEARCH));
	Out.Replace(_T("[Download]"), _GetPlainResString(IDS_SW_DOWNLOAD));
	Out.Replace(_T("[Start]"), _GetPlainResString(IDS_SW_START));
	Out.Replace(_T("[Version]"), _GetPlainResString(IDS_VERSION));
	Out.Replace(_T("[VersionCheck]"), thePrefs.GetVersionCheckURL());
	Out.Replace(_T("[Kad]"), _GetPlainResString(IDS_KADEMLIA));

	Out.Replace(_T("[FileIsHashing]"), _GetPlainResString(IDS_HASHING));
	Out.Replace(_T("[FileIsErroneous]"), _GetPlainResString(IDS_ERRORLIKE));
	Out.Replace(_T("[FileIsCompleting]"), _GetPlainResString(IDS_COMPLETING));
	Out.Replace(_T("[FileDetails]"), _GetPlainResString(IDS_FD_TITLE));
	Out.Replace(_T("[FileComments]"), _GetPlainResString(IDS_CMT_SHOWALL));
	Out.Replace(_T("[ClearCompleted]"), _GetPlainResString(IDS_DL_CLEAR));
	Out.Replace(_T("[RunFile]"), _GetPlainResString(IDS_DOWNLOAD));
	Out.Replace(_T("[Resume]"), _GetPlainResString(IDS_DL_RESUME));
	Out.Replace(_T("[Stop]"), _GetPlainResString(IDS_DL_STOP));
	Out.Replace(_T("[Pause]"), _GetPlainResString(IDS_DL_PAUSE));
	Out.Replace(_T("[ConfirmCancel]"), _GetPlainResString(IDS_Q_CANCELDL2));
	Out.Replace(_T("[Cancel]"), _GetPlainResString(IDS_MAIN_BTN_CANCEL));
	Out.Replace(_T("[GetFLC]"), _GetPlainResString(IDS_DOWNLOADMOVIECHUNKS));
	Out.Replace(_T("[Rename]"), _GetPlainResString(IDS_RENAME));
	Out.Replace(_T("[Connect]"), _GetPlainResString(IDS_IRC_CONNECT));
	Out.Replace(_T("[ConfirmRemove]"), _GetPlainResString(IDS_WEB_CONFIRM_REMOVE_SERVER));
	Out.Replace(_T("[ConfirmClose]"), _GetPlainResString(IDS_WEB_MAIN_CLOSE));
	Out.Replace(_T("[ConfirmReboot]"), _GetPlainResString(IDS_WEB_MAIN_REBOOT));
	Out.Replace(_T("[ConfirmShutdown]"), _GetPlainResString(IDS_WEB_MAIN_SHUTDOWN));
	Out.Replace(_T("[RemoveServer]"), _GetPlainResString(IDS_REMOVETHIS));
	Out.Replace(_T("[StaticServer]"), _GetPlainResString(IDS_STATICSERVER));
	Out.Replace(_T("[Friend]"), _GetPlainResString(IDS_PW_FRIENDS));

	Out.Replace(_T("[PriorityVeryLow]"), _GetPlainResString(IDS_PRIOVERYLOW));
	Out.Replace(_T("[PriorityLow]"), _GetPlainResString(IDS_PRIOLOW));
	Out.Replace(_T("[PriorityNormal]"), _GetPlainResString(IDS_PRIONORMAL));
	Out.Replace(_T("[PriorityHigh]"), _GetPlainResString(IDS_PRIOHIGH));
	Out.Replace(_T("[PriorityRelease]"), _GetPlainResString(IDS_PRIORELEASE));
	Out.Replace(_T("[PriorityAuto]"), _GetPlainResString(IDS_PRIOAUTO));

	CString HTTPConState, HTTPConText, HTTPHelp;
	CString HTTPHelpU(_T('0'));
	CString HTTPHelpM(_T('0'));
	CString HTTPHelpV(_T('0'));
	CString HTTPHelpF(_T('0'));
	const CString &sCmd(_ParseURL(Data.sURL, _T("c")));
	bool disconnectissued = (sCmd == _T("disconnect"));
	bool connectissued = (sCmd == _T("connect"));

	if ((theApp.serverconnect->IsConnecting() && !disconnectissued) || connectissued) {
		HTTPConState = _T("connecting");
		HTTPConText = _GetPlainResString(IDS_CONNECTING);
	} else if (theApp.serverconnect->IsConnected() && !disconnectissued) {
		HTTPConState = theApp.serverconnect->IsLowID() ? _T("low") : _T("high");
		CServer *cur_server = theApp.serverlist->GetServerByAddress(
			theApp.serverconnect->GetCurrentServer()->GetAddress(),
			theApp.serverconnect->GetCurrentServer()->GetPort());

		if (cur_server) {
			HTTPConText = cur_server->GetListName();
			if (HTTPConText.GetLength() > SHORT_LENGTH)
				HTTPConText = HTTPConText.Left(SHORT_LENGTH - 3) + _T("...");

			if (bAdmin)
				HTTPConText.AppendFormat(_T(" (<a href=\"?ses=%s&amp;w=server&amp;c=disconnect\">%s</a>)"), (LPCTSTR)sSession, (LPCTSTR)_GetPlainResString(IDS_IRC_DISCONNECT));

			HTTPHelpU = CastItoIShort(cur_server->GetUsers());
			HTTPHelpM = CastItoIShort(cur_server->GetMaxUsers());
			HTTPHelpF = CastItoIShort(cur_server->GetFiles());
			if (cur_server->GetMaxUsers() > 0)
				HTTPHelpV.Format(_T("%.0f"), (100.0 * cur_server->GetUsers()) / cur_server->GetMaxUsers());
			else
				HTTPHelpV = _T("0");
		}

	} else {
		HTTPConState = _T("disconnected");
		HTTPConText = _GetPlainResString(IDS_DISCONNECTED);
		if (bAdmin)
			HTTPConText.AppendFormat(_T(" (<a href=\"?ses=%s&amp;w=server&amp;c=connect\">%s</a>)"), (LPCTSTR)sSession, (LPCTSTR)_GetPlainResString(IDS_CONNECTTOANYSERVER));
	}
	uint32 allUsers = 0;
	uint32 allFiles = 0;
	for (INT_PTR sc = theApp.serverlist->GetServerCount(); --sc >= 0;) {
		const CServer *cur_server = theApp.serverlist->GetServerAt(sc);
		allUsers += cur_server->GetUsers();
		allFiles += cur_server->GetFiles();
	}
	Out.Replace(_T("[AllUsers]"), CastItoIShort(allUsers));
	Out.Replace(_T("[AllFiles]"), CastItoIShort(allFiles));
	Out.Replace(_T("[ConState]"), HTTPConState);
	Out.Replace(_T("[ConText]"), HTTPConText);

	// kad status
	if (Kademlia::CKademlia::IsConnected()) {
		if (Kademlia::CKademlia::IsFirewalled()) {
			HTTPConText = GetResString(IDS_FIREWALLED);
			HTTPConText.AppendFormat(_T(" (<a href=\"?ses=%s&amp;w=kad&amp;c=rcfirewall\">%s</a>"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_KAD_RECHECKFW));
			HTTPConText.AppendFormat(_T(", <a href=\"?ses=%s&amp;w=kad&amp;c=disconnect\">%s</a>)"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_DISCONNECT));
		} else {
			HTTPConText = GetResString(IDS_CONNECTED);
			HTTPConText.AppendFormat(_T(" (<a href=\"?ses=%s&amp;w=kad&amp;c=disconnect\">%s</a>)"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_DISCONNECT));
		}
	} else {
		if (Kademlia::CKademlia::IsRunning())
			HTTPConText = GetResString(IDS_CONNECTING);
		else {
			HTTPConText = GetResString(IDS_DISCONNECTED);
			HTTPConText.AppendFormat(_T(" (<a href=\"?ses=%s&amp;w=kad&amp;c=connect\">%s</a>)"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_CONNECT));
		}
	}
	Out.Replace(_T("[KadConText]"), HTTPConText);

	TCHAR HTTPHeader[100];
	//100/1024 equals to 1/10.24
	if (thePrefs.GetMaxDownload() == UNLIMITED)
		_stprintf(HTTPHeader, _T("%.0f"), theApp.downloadqueue->GetDatarate() / 10.24 / thePrefs.GetMaxGraphDownloadRate());
	else
		_stprintf(HTTPHeader, _T("%.0f"), theApp.downloadqueue->GetDatarate() / 10.24 / thePrefs.GetMaxDownload());
	Out.Replace(_T("[DownloadValue]"), HTTPHeader);

	if (thePrefs.GetMaxUpload() == UNLIMITED)
		_stprintf(HTTPHeader, _T("%.0f"), theApp.uploadqueue->GetDatarate() / 10.24 / thePrefs.GetMaxGraphUploadRate(true));
	else
		_stprintf(HTTPHeader, _T("%.0f"), theApp.uploadqueue->GetDatarate() / 10.24 / thePrefs.GetMaxUpload());
	Out.Replace(_T("[UploadValue]"), HTTPHeader);

	_stprintf(HTTPHeader, _T("%.0f"), (100.0 * theApp.listensocket->GetOpenSockets()) / thePrefs.GetMaxConnections());
	Out.Replace(_T("[ConnectionValue]"), HTTPHeader);
	_stprintf(HTTPHeader, _T("%.1f"), theApp.uploadqueue->GetDatarate() / 1024.0);
	Out.Replace(_T("[CurUpload]"), HTTPHeader);
	_stprintf(HTTPHeader, _T("%.1f"), theApp.downloadqueue->GetDatarate() / 1024.0);
	Out.Replace(_T("[CurDownload]"), HTTPHeader);
	_stprintf(HTTPHeader, _T("%u.0"), theApp.listensocket->GetOpenSockets());
	Out.Replace(_T("[CurConnection]"), HTTPHeader);

	uint32 dwMax = thePrefs.GetMaxUpload();
	if (dwMax == UNLIMITED)
		HTTPHelp = GetResString(IDS_PW_UNLIMITED);
	else
		HTTPHelp.Format(_T("%u"), dwMax);
	Out.Replace(_T("[MaxUpload]"), HTTPHelp);

	dwMax = thePrefs.GetMaxDownload();
	if (dwMax == UNLIMITED)
		HTTPHelp = GetResString(IDS_PW_UNLIMITED);
	else
		HTTPHelp.Format(_T("%u"), dwMax);
	Out.Replace(_T("[MaxDownload]"), HTTPHelp);

	dwMax = thePrefs.GetMaxConnections();
	if (dwMax == UNLIMITED)
		HTTPHelp = GetResString(IDS_PW_UNLIMITED);
	else
		HTTPHelp.Format(_T("%u"), dwMax);
	Out.Replace(_T("[MaxConnection]"), HTTPHelp);
	Out.Replace(_T("[UserValue]"), HTTPHelpV);
	Out.Replace(_T("[MaxUsers]"), HTTPHelpM);
	Out.Replace(_T("[CurUsers]"), HTTPHelpU);
	Out.Replace(_T("[CurFiles]"), HTTPHelpF);
	Out.Replace(_T("[Connection]"), _GetPlainResString(IDS_CONNECTION));
	Out.Replace(_T("[QuickStats]"), _GetPlainResString(IDS_STATUS));

	Out.Replace(_T("[Users]"), _GetPlainResString(IDS_UUSERS));
	Out.Replace(_T("[Files]"), _GetPlainResString(IDS_FILES));
	Out.Replace(_T("[Con]"), _GetPlainResString(IDS_ST_ACTIVEC));
	Out.Replace(_T("[Up]"), _GetPlainResString(IDS_PW_CON_UPLBL));
	Out.Replace(_T("[Down]"), _GetPlainResString(IDS_PW_CON_DOWNLBL));

	if (thePrefs.GetCatCount() > 1)
		_InsertCatBox(Out, 0, pThis->m_Templates.sCatArrow, false, false, sSession, _T(""), true);
	else
		Out.Replace(_T("[CATBOXED2K]"), _T(""));

	return Out;
}

const CString CWebServer::_GetFooter(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	return (pThis == NULL) ? CString() : pThis->m_Templates.sFooter;
}

CString CWebServer::_GetServerList(const ThreadData &Data)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));
	bool bAdmin = _IsSessionAdmin(Data, sSession);
	const CString &sAddServerBox(_GetAddServerBox(Data));

	const CString &sCmd(_ParseURL(Data.sURL, _T("c")));
	const CString &sIP(_ParseURL(Data.sURL, _T("ip")));
	int nPort = _tstoi(_ParseURL(Data.sURL, _T("port")));
	if (bAdmin) {
		if (sCmd == _T("connect")) {
			if (sIP.IsEmpty())
				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_CONNECTTOSERVER, 0);
			else
				_ConnectToServer(sIP, nPort);
		} else if (sCmd == _T("disconnect")) {
			if (theApp.serverconnect->IsConnecting())
				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_STOPCONNECTING, 0);
			else
				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_DISCONNECT, 1);
		} else if (sCmd == _T("remove")) {
			if (!sIP.IsEmpty())
				_RemoveServer(sIP, nPort);
		} else if (sCmd == _T("addtostatic")) {
			if (!sIP.IsEmpty())
				_AddToStatic(sIP, nPort);
		} else if (sCmd == _T("removefromstatic")) {
			if (!sIP.IsEmpty())
				_RemoveFromStatic(sIP, nPort);
		} else if (sCmd == _T("priolow")) {
			if (!sIP.IsEmpty()) {
				CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
				if (server) {
					server->SetPreference(SRV_PR_LOW);
					SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATESERVER, (LPARAM)server);
				}
			}
		} else if (sCmd == _T("prionormal")) {
			if (!sIP.IsEmpty()) {
				CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
				if (server) {
					server->SetPreference(SRV_PR_NORMAL);
					SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATESERVER, (LPARAM)server);
				}
			}
		} else if (sCmd == _T("priohigh")) {
			if (!sIP.IsEmpty()) {
				CServer *server = theApp.serverlist->GetServerByAddress(sIP, (uint16)nPort);
				if (server) {
					server->SetPreference(SRV_PR_HIGH);
					SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATESERVER, (LPARAM)server);
				}
			}
		}
	} else if (sCmd == _T("menu")) {
		int iMenu = _tstol(_ParseURL(Data.sURL, _T("m")));
		bool bValue = _ParseURL(Data.sURL, _T("v")) == _T("1");
		WSserverColumnHidden[iMenu] = bValue;
		_SaveWIConfigArray(WSserverColumnHidden, _countof(WSserverColumnHidden), _T("serverColumnHidden"));
	}

	CString strTmp = _ParseURL(Data.sURL, _T("sortreverse"));
	const CString &sSort(_ParseURL(Data.sURL, _T("sort")));

	if (!sSort.IsEmpty()) {
		bool bDirection = false;

		if (sSort == _T("state"))
			pThis->m_Params.ServerSort = SERVER_SORT_STATE;
		else if (sSort == _T("name")) {
			pThis->m_Params.ServerSort = SERVER_SORT_NAME;
			bDirection = true;
		} else if (sSort == _T("ip"))
			pThis->m_Params.ServerSort = SERVER_SORT_IP;
		else if (sSort == _T("description")) {
			pThis->m_Params.ServerSort = SERVER_SORT_DESCRIPTION;
			bDirection = true;
		} else if (sSort == _T("ping"))
			pThis->m_Params.ServerSort = SERVER_SORT_PING;
		else if (sSort == _T("users"))
			pThis->m_Params.ServerSort = SERVER_SORT_USERS;
		else if (sSort == _T("files"))
			pThis->m_Params.ServerSort = SERVER_SORT_FILES;
		else if (sSort == _T("priority"))
			pThis->m_Params.ServerSort = SERVER_SORT_PRIORITY;
		else if (sSort == _T("failed"))
			pThis->m_Params.ServerSort = SERVER_SORT_FAILED;
		else if (sSort == _T("limit"))
			pThis->m_Params.ServerSort = SERVER_SORT_LIMIT;
		else if (sSort == _T("version"))
			pThis->m_Params.ServerSort = SERVER_SORT_VERSION;

		if (strTmp.IsEmpty())
			pThis->m_Params.bServerSortReverse = bDirection;
	}
	if (!strTmp.IsEmpty())
		pThis->m_Params.bServerSortReverse = (strTmp == _T("true"));

	CString Out = pThis->m_Templates.sServerList;

	Out.Replace(_T("[AddServerBox]"), sAddServerBox);
	Out.Replace(_T("[Session]"), sSession);

	strTmp = (pThis->m_Params.bServerSortReverse) ? _T("&amp;sortreverse=false") : _T("&amp;sortreverse=true");

	if (pThis->m_Params.ServerSort == SERVER_SORT_STATE)
		Out.Replace(_T("[SortState]"), strTmp);
	else
		Out.Replace(_T("[SortState]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_NAME)
		Out.Replace(_T("[SortName]"), strTmp);
	else
		Out.Replace(_T("[SortName]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_IP)
		Out.Replace(_T("[SortIP]"), strTmp);
	else
		Out.Replace(_T("[SortIP]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_DESCRIPTION)
		Out.Replace(_T("[SortDescription]"), strTmp);
	else
		Out.Replace(_T("[SortDescription]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_PING)
		Out.Replace(_T("[SortPing]"), strTmp);
	else
		Out.Replace(_T("[SortPing]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_USERS)
		Out.Replace(_T("[SortUsers]"), strTmp);
	else
		Out.Replace(_T("[SortUsers]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_FILES)
		Out.Replace(_T("[SortFiles]"), strTmp);
	else
		Out.Replace(_T("[SortFiles]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_PRIORITY)
		Out.Replace(_T("[SortPriority]"), strTmp);
	else
		Out.Replace(_T("[SortPriority]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_FAILED)
		Out.Replace(_T("[SortFailed]"), strTmp);
	else
		Out.Replace(_T("[SortFailed]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_LIMIT)
		Out.Replace(_T("[SortLimit]"), strTmp);
	else
		Out.Replace(_T("[SortLimit]"), _T(""));
	if (pThis->m_Params.ServerSort == SERVER_SORT_VERSION)
		Out.Replace(_T("[SortVersion]"), strTmp);
	else
		Out.Replace(_T("[SortVersion]"), _T(""));
	Out.Replace(_T("[ServerList]"), _GetPlainResString(IDS_SV_SERVERLIST));

	const TCHAR *pcSortIcon = (pThis->m_Params.bServerSortReverse) ? pThis->m_Templates.sUpArrow : pThis->m_Templates.sDownArrow;

	_GetPlainResString(strTmp, IDS_SL_SERVERNAME);
	if (WSserverColumnHidden[0]) {
		Out.Replace(_T("[ServernameI]"), _T(""));
		Out.Replace(_T("[ServernameH]"), _T(""));
	} else {
		Out.Replace(_T("[ServernameI]"), (pThis->m_Params.ServerSort == SERVER_SORT_NAME) ? pcSortIcon : _T(""));
		Out.Replace(_T("[ServernameH]"), strTmp);
	}
	Out.Replace(_T("[ServernameM]"), strTmp);

	_GetPlainResString(strTmp, IDS_IP);
	if (WSserverColumnHidden[1]) {
		Out.Replace(_T("[AddressI]"), _T(""));
		Out.Replace(_T("[AddressH]"), _T(""));
	} else {
		Out.Replace(_T("[AddressI]"), (pThis->m_Params.ServerSort == SERVER_SORT_IP) ? pcSortIcon : _T(""));
		Out.Replace(_T("[AddressH]"), strTmp);
	}
	Out.Replace(_T("[AddressM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DESCRIPTION);
	if (WSserverColumnHidden[2]) {
		Out.Replace(_T("[DescriptionI]"), _T(""));
		Out.Replace(_T("[DescriptionH]"), _T(""));
	} else {
		Out.Replace(_T("[DescriptionI]"), (pThis->m_Params.ServerSort == SERVER_SORT_DESCRIPTION) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DescriptionH]"), strTmp);
	}
	Out.Replace(_T("[DescriptionM]"), strTmp);

	_GetPlainResString(strTmp, IDS_PING);
	if (WSserverColumnHidden[3]) {
		Out.Replace(_T("[PingI]"), _T(""));
		Out.Replace(_T("[PingH]"), _T(""));
	} else {
		Out.Replace(_T("[PingI]"), (pThis->m_Params.ServerSort == SERVER_SORT_PING) ? pcSortIcon : _T(""));
		Out.Replace(_T("[PingH]"), strTmp);
	}
	Out.Replace(_T("[PingM]"), strTmp);

	_GetPlainResString(strTmp, IDS_UUSERS);
	if (WSserverColumnHidden[4]) {
		Out.Replace(_T("[UsersI]"), _T(""));
		Out.Replace(_T("[UsersH]"), _T(""));
	} else {
		Out.Replace(_T("[UsersI]"), (pThis->m_Params.ServerSort == SERVER_SORT_USERS) ? pcSortIcon : _T(""));
		Out.Replace(_T("[UsersH]"), strTmp);
	}
	Out.Replace(_T("[UsersM]"), strTmp);

	_GetPlainResString(strTmp, IDS_FILES);
	if (WSserverColumnHidden[5]) {
		Out.Replace(_T("[FilesI]"), _T(""));
		Out.Replace(_T("[FilesH]"), _T(""));
	} else {
		Out.Replace(_T("[FilesI]"), (pThis->m_Params.ServerSort == SERVER_SORT_FILES) ? pcSortIcon : _T(""));
		Out.Replace(_T("[FilesH]"), strTmp);
	}
	Out.Replace(_T("[FilesM]"), strTmp);

	_GetPlainResString(strTmp, IDS_PRIORITY);
	if (WSserverColumnHidden[6]) {
		Out.Replace(_T("[PriorityI]"), _T(""));
		Out.Replace(_T("[PriorityH]"), _T(""));
	} else {
		Out.Replace(_T("[PriorityI]"), (pThis->m_Params.ServerSort == SERVER_SORT_PRIORITY) ? pcSortIcon : _T(""));
		Out.Replace(_T("[PriorityH]"), strTmp);
	}
	Out.Replace(_T("[PriorityM]"), strTmp);

	_GetPlainResString(strTmp, IDS_UFAILED);
	if (WSserverColumnHidden[7]) {
		Out.Replace(_T("[FailedI]"), _T(""));
		Out.Replace(_T("[FailedH]"), _T(""));
	} else {
		Out.Replace(_T("[FailedI]"), (pThis->m_Params.ServerSort == SERVER_SORT_FAILED) ? pcSortIcon : _T(""));
		Out.Replace(_T("[FailedH]"), strTmp);
	}
	Out.Replace(_T("[FailedM]"), strTmp);

	_GetPlainResString(strTmp, IDS_SERVER_LIMITS);
	if (WSserverColumnHidden[8]) {
		Out.Replace(_T("[LimitI]"), _T(""));
		Out.Replace(_T("[LimitH]"), _T(""));
	} else {
		Out.Replace(_T("[LimitI]"), (pThis->m_Params.ServerSort == SERVER_SORT_LIMIT) ? pcSortIcon : _T(""));
		Out.Replace(_T("[LimitH]"), strTmp);
	}
	Out.Replace(_T("[LimitM]"), strTmp);

	_GetPlainResString(strTmp, IDS_SV_SERVERINFO);
	if (WSserverColumnHidden[9]) {
		Out.Replace(_T("[VersionI]"), _T(""));
		Out.Replace(_T("[VersionH]"), _T(""));
	} else {
		Out.Replace(_T("[VersionI]"), (pThis->m_Params.ServerSort == SERVER_SORT_VERSION) ? pcSortIcon : _T(""));
		Out.Replace(_T("[VersionH]"), strTmp);
	}
	Out.Replace(_T("[VersionM]"), strTmp);

	Out.Replace(_T("[Actions]"), _GetPlainResString(IDS_WEB_ACTIONS));

	CArray<ServerEntry> ServerArray;

	// Populating array
	for (INT_PTR sc = theApp.serverlist->GetServerCount(); --sc >= 0;) {
		const CServer &cur_serv = *theApp.serverlist->GetServerAt(sc);
		ServerEntry Entry;
		Entry.sServerName = _SpecialChars(cur_serv.GetListName());
		Entry.sServerIP = cur_serv.GetAddress();
		Entry.nServerPort = cur_serv.GetPort();
		Entry.sServerDescription = _SpecialChars(cur_serv.GetDescription());
		Entry.nServerPing = cur_serv.GetPing();
		Entry.nServerUsers = cur_serv.GetUsers();
		Entry.nServerMaxUsers = cur_serv.GetMaxUsers();
		Entry.nServerFiles = cur_serv.GetFiles();
		Entry.bServerStatic = cur_serv.IsStaticMember();
		UINT uid;
		switch (cur_serv.GetPreference()) {
		case SRV_PR_HIGH:
			uid = IDS_PRIOHIGH;
			Entry.nServerPriority = 2;
			break;
		case SRV_PR_NORMAL:
			uid = IDS_PRIONORMAL;
			Entry.nServerPriority = 1;
			break;
		case SRV_PR_LOW:
			uid = IDS_PRIOLOW;
			Entry.nServerPriority = 0;
			break;
		default:
			uid = 0;
			Entry.nServerPriority = 0;
		}
		if (uid)
			Entry.sServerPriority = _GetPlainResString(uid);
		Entry.nServerFailed = cur_serv.GetFailedCount();
		Entry.nServerSoftLimit = cur_serv.GetSoftFiles();
		Entry.nServerHardLimit = cur_serv.GetHardFiles();
		Entry.sServerVersion = cur_serv.GetVersion();
		if (inet_addr((CStringA)Entry.sServerIP) != INADDR_NONE) {
			CString &newip(Entry.sServerFullIP);
			for (int j = 0, iPos = 0; j < 4 && iPos >= 0; ++j) {
				const CString &temp(Entry.sServerIP.Tokenize(_T("."), iPos));
				newip.AppendFormat(&_T("000%s")[min(temp.GetLength(), 3)], (LPCTSTR)temp);
			}
		} else
			Entry.sServerFullIP = Entry.sServerIP;
		Entry.sServerState = cur_serv.GetFailedCount() ? _T("failed") : _T("disconnected");

		if (theApp.serverconnect->IsConnecting())
			Entry.sServerState = _T("connecting");
		else if (theApp.serverconnect->IsConnected()) {
			if (theApp.serverconnect->GetCurrentServer()->GetFullIP() == cur_serv.GetFullIP())
				Entry.sServerState = theApp.serverconnect->IsLowID() ? _T("low") : _T("high");
		}
		ServerArray.Add(Entry);
	}

	SortParams prm{ (int)pThis->m_Params.ServerSort, pThis->m_Params.bServerSortReverse };
	qsort_s(ServerArray.GetData(), ServerArray.GetCount(), sizeof(ServerEntry), &_ServerCmp, &prm);

	// Displaying
	CString sList, HTTPProcessData, sServerPort, ed2k;
	CString OutE = pThis->m_Templates.sServerLine; // List Entry Templates

	OutE.Replace(_T("[admin]"), bAdmin ? _T("admin") : _T(""));
	OutE.Replace(_T("[session]"), sSession);

	for (INT_PTR i = 0; i < ServerArray.GetCount(); ++i) {
		const ServerEntry &cur_srv(ServerArray[i]);
		HTTPProcessData = OutE;	// Copy Entry Line to Temp

		sServerPort.Format(_T("%i"), cur_srv.nServerPort);
		ed2k.Format(_T("ed2k://|server|%s|%s|/"), (LPCTSTR)cur_srv.sServerIP, (LPCTSTR)sServerPort);

		bool b = (cur_srv.sServerIP == _ParseURL(Data.sURL, _T("ip")) && sServerPort == _ParseURL(Data.sURL, _T("port")));
		HTTPProcessData.Replace(_T("[LastChangedDataset]"), b ?_T("checked") : _T("checked_no"));
		b = cur_srv.bServerStatic;
		HTTPProcessData.Replace(_T("[isstatic]"), b ? _T("staticsrv") : _T(""));
		HTTPProcessData.Replace(_T("[ServerType]"), b ? _T("static") : _T("none"));

		LPCTSTR pcSrvPriority;
		switch (cur_srv.nServerPriority) {
		case 0:
			pcSrvPriority = _T("Low");
			break;
		case 1:
			pcSrvPriority = _T("Normal");
			break;
		case 2:
			pcSrvPriority = _T("High");
			break;
		default:
			pcSrvPriority = _T("");
		}

		HTTPProcessData.Replace(_T("[ed2k]"), ed2k);
		HTTPProcessData.Replace(_T("[ip]"), cur_srv.sServerIP);
		HTTPProcessData.Replace(_T("[port]"), sServerPort);
		HTTPProcessData.Replace(_T("[server-priority]"), pcSrvPriority);

		// DonGato: reduced large server names or descriptions
		if (WSserverColumnHidden[0])
			HTTPProcessData.Replace(_T("[Servername]"), _T(""));
		else if (cur_srv.sServerName.GetLength() > (SHORT_LENGTH)) {
			CString s;
			s.Format(_T("<acronym title=\"%s\">%s...</acronym>"), (LPCTSTR)cur_srv.sServerName, (LPCTSTR)cur_srv.sServerName.Left(SHORT_LENGTH - 3));
			HTTPProcessData.Replace(_T("[Servername]"), s);
		} else
			HTTPProcessData.Replace(_T("[Servername]"), cur_srv.sServerName);
		
		if (WSserverColumnHidden[1])
			HTTPProcessData.Replace(_T("[Address]"), _T(""));
		else {
			CString sAddr;
			sAddr.Format(_T("%s:%d"), (LPCTSTR)cur_srv.sServerIP, cur_srv.nServerPort);
			HTTPProcessData.Replace(_T("[Address]"), sAddr);
		}
		if (WSserverColumnHidden[2])
			HTTPProcessData.Replace(_T("[Description]"), _T(""));
		else if (cur_srv.sServerDescription.GetLength() > SHORT_LENGTH) {
			CString s;
			s.Format(_T("<acronym title=\"%s\">%s...</acronym>"), (LPCTSTR)cur_srv.sServerDescription, (LPCTSTR)cur_srv.sServerDescription.Left(SHORT_LENGTH - 3));
			HTTPProcessData.Replace(_T("[Description]"), s);
		} else
			HTTPProcessData.Replace(_T("[Description]"), cur_srv.sServerDescription);
		
		if (WSserverColumnHidden[3])
			HTTPProcessData.Replace(_T("[Ping]"), _T(""));
		else {
			CString sPing;
			sPing.Format(_T("%lu"), cur_srv.nServerPing);
			HTTPProcessData.Replace(_T("[Ping]"), sPing);
		}

		if (WSserverColumnHidden[4])
			HTTPProcessData.Replace(_T("[Users]"), _T(""));
		else {
			CString sT;
			if (cur_srv.nServerUsers > 0) {
				sT = CastItoIShort(cur_srv.nServerUsers);
				if (cur_srv.nServerMaxUsers > 0)
					sT.AppendFormat(_T(" (%s)"), (LPCTSTR)CastItoIShort(cur_srv.nServerMaxUsers));
			}
			HTTPProcessData.Replace(_T("[Users]"), sT);
		}
		if (WSserverColumnHidden[5] && (cur_srv.nServerFiles > 0))
			HTTPProcessData.Replace(_T("[Files]"), _T(""));
		else
			HTTPProcessData.Replace(_T("[Files]"), CastItoIShort(cur_srv.nServerFiles));
		if (WSserverColumnHidden[6])
			HTTPProcessData.Replace(_T("[Priority]"), _T(""));
		else
			HTTPProcessData.Replace(_T("[Priority]"), cur_srv.sServerPriority);
		if (WSserverColumnHidden[7])
			HTTPProcessData.Replace(_T("[Failed]"), _T(""));
		else {
			CString sFailed;
			sFailed.Format(_T("%d"), cur_srv.nServerFailed);
			HTTPProcessData.Replace(_T("[Failed]"), sFailed);
		}
		if (WSserverColumnHidden[8])
			HTTPProcessData.Replace(_T("[Limit]"), _T(""));
		else {
			CString strTemp;
			strTemp.Format(_T("%s (%s)"), (LPCTSTR)CastItoIShort(cur_srv.nServerSoftLimit),
				(LPCTSTR)CastItoIShort(cur_srv.nServerHardLimit));
			HTTPProcessData.Replace(_T("[Limit]"), strTemp);
		}
		if (WSserverColumnHidden[9])
			HTTPProcessData.Replace(_T("[Version]"), _T(""));
		else if (cur_srv.sServerVersion.GetLength() > SHORT_LENGTH_MIN) {
			CString s;
			s.Format(_T("<acronym title=\"%s\">%s...</acronym>"), (LPCTSTR)cur_srv.sServerVersion, (LPCTSTR)cur_srv.sServerVersion.Left(SHORT_LENGTH_MIN - 3));
			HTTPProcessData.Replace(_T("[Version]"), s);
		} else
			HTTPProcessData.Replace(_T("[Version]"), cur_srv.sServerVersion);
		
		HTTPProcessData.Replace(_T("[ServerState]"), cur_srv.sServerState);
		sList += HTTPProcessData;
	}
	Out.Replace(_T("[ServersList]"), sList);
	Out.Replace(_T("[Session]"), sSession);

	return Out;
}

CString CWebServer::_GetTransferList(const ThreadData &Data)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));
	long lSession = _tstol(sSession);
	bool bAdmin = _IsSessionAdmin(Data, lSession);

	// cat
	int cat;
	const CString &catp(_ParseURL(Data.sURL, _T("cat")));
	if (catp.IsEmpty())
		cat = _GetLastUserCat(Data, lSession);
	else {
		cat = _tstoi(catp);
		_SetLastUserCat(Data, lSession, cat);
	}
	// commands
	CString sCat;
	if (cat != 0)
		sCat.Format(_T("&amp;cat=%i"), cat);

	CString Out;
	if (thePrefs.GetCatCount() > 1)
		_InsertCatBox(Out, cat, CString(), true, true, sSession, CString());
	else
		Out.Replace(_T("[CATBOX]"), _T(""));


	const CString &sClear(_ParseURL(Data.sURL, _T("clearcompleted")));
	if (bAdmin && !sClear.IsEmpty()) {
		if (sClear.CompareNoCase(_T("all")) == 0)
			theApp.emuledlg->SendMessage(WEB_CLEAR_COMPLETED, (WPARAM)0, (LPARAM)cat);
		else if (!sClear.IsEmpty()) {
			uchar FileHash[MDX_DIGEST_SIZE];
			if (strmd4(sClear, FileHash)) {
				uchar *pFileHash = new uchar[MDX_DIGEST_SIZE];
				md4cpy(pFileHash, FileHash);
				theApp.emuledlg->SendMessage(WEB_CLEAR_COMPLETED, (WPARAM)1, reinterpret_cast<LPARAM>(pFileHash));
			}
		}
	}

	CString HTTPTemp = _ParseURL(Data.sURL, _T("ed2k"));

	if (bAdmin && !HTTPTemp.IsEmpty())
		theApp.emuledlg->SendMessage(WEB_ADDDOWNLOADS, (WPARAM)(LPCTSTR)HTTPTemp, cat);

	HTTPTemp = _ParseURL(Data.sURL, _T("c"));

	if (HTTPTemp == _T("menudown")) {
		int iMenu = _tstol(_ParseURL(Data.sURL, _T("m")));
		WSdownloadColumnHidden[iMenu] = (_tstol(_ParseURL(Data.sURL, _T("v"))) != 0);

		CIni ini(thePrefs.GetConfigFile(), _T("WebServer"));

		_SaveWIConfigArray(WSdownloadColumnHidden, _countof(WSdownloadColumnHidden), _T("downloadColumnHidden"));
	} else if (HTTPTemp == _T("menuup")) {
		int iMenu = _tstol(_ParseURL(Data.sURL, _T("m")));
		WSuploadColumnHidden[iMenu] = (_tstol(_ParseURL(Data.sURL, _T("v"))) != 0);
		_SaveWIConfigArray(WSuploadColumnHidden, _countof(WSuploadColumnHidden), _T("uploadColumnHidden"));
	} else if (HTTPTemp == _T("menuqueue")) {
		int iMenu = _tstol(_ParseURL(Data.sURL, _T("m")));
		WSqueueColumnHidden[iMenu] = (_tstol(_ParseURL(Data.sURL, _T("v"))) != 0);
		_SaveWIConfigArray(WSqueueColumnHidden, _countof(WSqueueColumnHidden), _T("queueColumnHidden"));
	} else if (HTTPTemp == _T("menuprio") && bAdmin) {
		const CString &sPrio(_ParseURL(Data.sURL, _T("p")));
		int prio;
		if (sPrio == _T("low"))
			prio = PR_LOW;
		else if (sPrio == _T("high"))
			prio = PR_HIGH;
		else if (sPrio == _T("normal"))
			prio = PR_NORMAL;
		else //if (sPrio == _T("auto"))
			prio = PR_AUTO; //make auto the default
		SendMessage(theApp.emuledlg->m_hWnd, WEB_CATPRIO, (WPARAM)cat, (LPARAM)prio);
	}

	if (bAdmin) {
		const CString &sOp(_ParseURL(Data.sURL, _T("op")));

		if (!sOp.IsEmpty()) {
			const CString &sFile(_ParseURL(Data.sURL, _T("file")));

			if (sFile.IsEmpty()) {
				const CString &sUser(_ParseURL(Data.sURL, _T("userhash")));
				uchar UserHash[MDX_DIGEST_SIZE];

				if (strmd4(sUser, UserHash)) {
					CUpDownClient *cur_client = theApp.clientlist->FindClientByUserHash(UserHash);
					if (cur_client) {
						if (sOp == _T("addfriend"))
							SendMessage(theApp.emuledlg->m_hWnd, WEB_ADDREMOVEFRIEND, (WPARAM)cur_client, (LPARAM)1);
						else if (sOp == _T("removefriend")) {
							const CFriend *f = theApp.friendlist->SearchFriend(UserHash, 0, 0);
							if (f)
								SendMessage(theApp.emuledlg->m_hWnd, WEB_ADDREMOVEFRIEND, (WPARAM)f, (LPARAM)0);
						}
					}
				}
			} else {
				uchar FileHash[MDX_DIGEST_SIZE];
				bool bHash = strmd4(sFile, FileHash);
				CPartFile *found_file = bHash ? theApp.downloadqueue->GetFileByID(FileHash) : NULL;
				if (found_file) {	// SyruS all actions require a found file (removed double-check inside)
					if (sOp == _T("stop"))
						found_file->StopFile();
					else if (sOp == _T("pause"))
						found_file->PauseFile();
					else if (sOp == _T("resume"))
						found_file->ResumeFile();
					else if (sOp == _T("cancel")) {
						found_file->DeletePartFile();
						SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPD_CATTABS, 0);
					} else if (sOp == _T("getflc"))
						found_file->GetPreviewPrio();
					else if (sOp == _T("rename")) {
						const CString &sNewName(_ParseURL(Data.sURL, _T("name")));
						theApp.emuledlg->SendMessage(WEB_FILE_RENAME, (WPARAM)found_file, (LPARAM)(LPCTSTR)sNewName);
					} else if (sOp == _T("priolow")) {
						found_file->SetAutoDownPriority(false);
						found_file->SetDownPriority(PR_LOW);
					} else if (sOp == _T("prionormal")) {
						found_file->SetAutoDownPriority(false);
						found_file->SetDownPriority(PR_NORMAL);
					} else if (sOp == _T("priohigh")) {
						found_file->SetAutoDownPriority(false);
						found_file->SetDownPriority(PR_HIGH);
					} else if (sOp == _T("prioauto")) {
						found_file->SetAutoDownPriority(true);
						found_file->SetDownPriority(PR_HIGH);
					} else if (sOp == _T("setcat")) {
						const CString &newcat(_ParseURL(Data.sURL, _T("filecat")));
						if (!newcat.IsEmpty())
							found_file->SetCategory(_tstol(newcat));
					}
				}
			}
		}
	}

	CString strTmp = _ParseURL(Data.sURL, _T("sortreverse"));
	const CString &sSort(_ParseURL(Data.sURL, _T("sort")));

	if (!sSort.IsEmpty()) {
		bool bDirection = false;
		if (sSort == _T("dstate"))
			pThis->m_Params.DownloadSort = DOWN_SORT_STATE;
		else if (sSort == _T("dtype"))
			pThis->m_Params.DownloadSort = DOWN_SORT_TYPE;
		else if (sSort == _T("dname")) {
			pThis->m_Params.DownloadSort = DOWN_SORT_NAME;
			bDirection = true;
		} else if (sSort == _T("dsize"))
			pThis->m_Params.DownloadSort = DOWN_SORT_SIZE;
		else if (sSort == _T("dtransferred"))
			pThis->m_Params.DownloadSort = DOWN_SORT_TRANSFERRED;
		else if (sSort == _T("dspeed"))
			pThis->m_Params.DownloadSort = DOWN_SORT_SPEED;
		else if (sSort == _T("dprogress"))
			pThis->m_Params.DownloadSort = DOWN_SORT_PROGRESS;
		else if (sSort == _T("dsources"))
			pThis->m_Params.DownloadSort = DOWN_SORT_SOURCES;
		else if (sSort == _T("dpriority"))
			pThis->m_Params.DownloadSort = DOWN_SORT_PRIORITY;
		else if (sSort == _T("dcategory")) {
			pThis->m_Params.DownloadSort = DOWN_SORT_CATEGORY;
			bDirection = true;
		} else if (sSort == _T("uuser")) {
			pThis->m_Params.UploadSort = UP_SORT_USER;
			bDirection = true;
		} else if (sSort == _T("uclient"))
			pThis->m_Params.UploadSort = UP_SORT_CLIENT;
		else if (sSort == _T("uversion"))
			pThis->m_Params.UploadSort = UP_SORT_VERSION;
		else if (sSort == _T("ufilename")) {
			pThis->m_Params.UploadSort = UP_SORT_FILENAME;
			bDirection = true;
		} else if (sSort == _T("utransferred"))
			pThis->m_Params.UploadSort = UP_SORT_TRANSFERRED;
		else if (sSort == _T("uspeed"))
			pThis->m_Params.UploadSort = UP_SORT_SPEED;
		else if (sSort == _T("qclient"))
			pThis->m_Params.QueueSort = QU_SORT_CLIENT;
		else if (sSort == _T("quser")) {
			pThis->m_Params.QueueSort = QU_SORT_USER;
			bDirection = true;
		} else if (sSort == _T("qversion"))
			pThis->m_Params.QueueSort = QU_SORT_VERSION;
		else if (sSort == _T("qfilename")) {
			pThis->m_Params.QueueSort = QU_SORT_FILENAME;
			bDirection = true;
		} else if (sSort == _T("qscore"))
			pThis->m_Params.QueueSort = QU_SORT_SCORE;

		if (!strTmp.IsEmpty())
			bDirection = (strTmp.CompareNoCase(_T("true")) == 0);

		switch (sSort[0]) {
		case _T('d'):
			pThis->m_Params.bDownloadSortReverse = bDirection;
			break;
		case _T('u'):
			pThis->m_Params.bUploadSortReverse = bDirection;
			break;
		case _T('q'):
			pThis->m_Params.bQueueSortReverse = bDirection;
		}
	}

	HTTPTemp = _ParseURL(Data.sURL, _T("showuploadqueue"));
	if (HTTPTemp == _T("true"))
		pThis->m_Params.bShowUploadQueue = true;
	else if (HTTPTemp == _T("false"))
		pThis->m_Params.bShowUploadQueue = false;

	HTTPTemp = _ParseURL(Data.sURL, _T("showuploadqueuebanned"));
	if (HTTPTemp == _T("true"))
		pThis->m_Params.bShowUploadQueueBanned = true;
	else if (HTTPTemp == _T("false"))
		pThis->m_Params.bShowUploadQueueBanned = false;

	HTTPTemp = _ParseURL(Data.sURL, _T("showuploadqueuefriend"));
	if (HTTPTemp == _T("true"))
		pThis->m_Params.bShowUploadQueueFriend = true;
	else if (HTTPTemp == _T("false"))
		pThis->m_Params.bShowUploadQueueFriend = false;

	Out += pThis->m_Templates.sTransferImages;
	Out += pThis->m_Templates.sTransferList;
	Out.Replace(_T("[DownloadHeader]"), pThis->m_Templates.sTransferDownHeader);
	Out.Replace(_T("[DownloadFooter]"), pThis->m_Templates.sTransferDownFooter);
	Out.Replace(_T("[UploadHeader]"), pThis->m_Templates.sTransferUpHeader);
	Out.Replace(_T("[UploadFooter]"), pThis->m_Templates.sTransferUpFooter);
	_InsertCatBox(Out, cat, pThis->m_Templates.sCatArrow, true, true, sSession, _T(""));

	strTmp = (pThis->m_Params.bDownloadSortReverse) ? _T("&amp;sortreverse=false") : _T("&amp;sortreverse=true");

	if (pThis->m_Params.DownloadSort == DOWN_SORT_STATE)
		Out.Replace(_T("[SortDState]"), strTmp);
	else
		Out.Replace(_T("[SortDState]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_TYPE)
		Out.Replace(_T("[SortDType]"), strTmp);
	else
		Out.Replace(_T("[SortDType]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_NAME)
		Out.Replace(_T("[SortDName]"), strTmp);
	else
		Out.Replace(_T("[SortDName]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_SIZE)
		Out.Replace(_T("[SortDSize]"), strTmp);
	else
		Out.Replace(_T("[SortDSize]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_TRANSFERRED)
		Out.Replace(_T("[SortDTransferred]"), strTmp);
	else
		Out.Replace(_T("[SortDTransferred]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_SPEED)
		Out.Replace(_T("[SortDSpeed]"), strTmp);
	else
		Out.Replace(_T("[SortDSpeed]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_PROGRESS)
		Out.Replace(_T("[SortDProgress]"), strTmp);
	else
		Out.Replace(_T("[SortDProgress]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_SOURCES)
		Out.Replace(_T("[SortDSources]"), strTmp);
	else
		Out.Replace(_T("[SortDSources]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_PRIORITY)
		Out.Replace(_T("[SortDPriority]"), strTmp);
	else
		Out.Replace(_T("[SortDPriority]"), _T(""));
	if (pThis->m_Params.DownloadSort == DOWN_SORT_CATEGORY)
		Out.Replace(_T("[SortDCategory]"), strTmp);
	else
		Out.Replace(_T("[SortDCategory]"), _T(""));

	strTmp = (pThis->m_Params.bUploadSortReverse) ? _T("&amp;sortreverse=false") : _T("&amp;sortreverse=true");

	if (pThis->m_Params.UploadSort == UP_SORT_CLIENT)
		Out.Replace(_T("[SortUClient]"), strTmp);
	else
		Out.Replace(_T("[SortUClient]"), _T(""));
	if (pThis->m_Params.UploadSort == UP_SORT_USER)
		Out.Replace(_T("[SortUUser]"), strTmp);
	else
		Out.Replace(_T("[SortUUser]"), _T(""));
	if (pThis->m_Params.UploadSort == UP_SORT_VERSION)
		Out.Replace(_T("[SortUVersion]"), strTmp);
	else
		Out.Replace(_T("[SortUVersion]"), _T(""));
	if (pThis->m_Params.UploadSort == UP_SORT_FILENAME)
		Out.Replace(_T("[SortUFilename]"), strTmp);
	else
		Out.Replace(_T("[SortUFilename]"), _T(""));
	if (pThis->m_Params.UploadSort == UP_SORT_TRANSFERRED)
		Out.Replace(_T("[SortUTransferred]"), strTmp);
	else
		Out.Replace(_T("[SortUTransferred]"), _T(""));
	if (pThis->m_Params.UploadSort == UP_SORT_SPEED)
		Out.Replace(_T("[SortUSpeed]"), strTmp);
	else
		Out.Replace(_T("[SortUSpeed]"), _T(""));

	const TCHAR *pcSortIcon = (pThis->m_Params.bDownloadSortReverse) ? pThis->m_Templates.sUpArrow : pThis->m_Templates.sDownArrow;

	_GetPlainResString(strTmp, IDS_DL_FILENAME);
	if (WSdownloadColumnHidden[0]) {
		Out.Replace(_T("[DFilenameI]"), _T(""));
		Out.Replace(_T("[DFilename]"), _T(""));
	} else {
		Out.Replace(_T("[DFilenameI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_NAME) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DFilename]"), strTmp);
	}
	Out.Replace(_T("[DFilenameM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SIZE);
	if (WSdownloadColumnHidden[1]) {
		Out.Replace(_T("[DSizeI]"), _T(""));
		Out.Replace(_T("[DSize]"), _T(""));
	} else {
		Out.Replace(_T("[DSizeI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_SIZE) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DSize]"), strTmp);
	}
	Out.Replace(_T("[DSizeM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_TRANSFCOMPL);
	if (WSdownloadColumnHidden[2]) {
		Out.Replace(_T("[DTransferredI]"), _T(""));
		Out.Replace(_T("[DTransferred]"), _T(""));
	} else {
		Out.Replace(_T("[DTransferredI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_TRANSFERRED) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DTransferred]"), strTmp);
	}
	Out.Replace(_T("[DTransferredM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_PROGRESS);
	if (WSdownloadColumnHidden[3]) {
		Out.Replace(_T("[DProgressI]"), _T(""));
		Out.Replace(_T("[DProgress]"), _T(""));
	} else {
		Out.Replace(_T("[DProgressI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_PROGRESS) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DProgress]"), strTmp);
	}
	Out.Replace(_T("[DProgressM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SPEED);
	if (WSdownloadColumnHidden[4]) {
		Out.Replace(_T("[DSpeedI]"), _T(""));
		Out.Replace(_T("[DSpeed]"), _T(""));
	} else {
		Out.Replace(_T("[DSpeedI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_SPEED) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DSpeed]"), strTmp);
	}
	Out.Replace(_T("[DSpeedM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SOURCES);
	if (WSdownloadColumnHidden[5]) {
		Out.Replace(_T("[DSourcesI]"), _T(""));
		Out.Replace(_T("[DSources]"), _T(""));
	} else {
		Out.Replace(_T("[DSourcesI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_SOURCES) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DSources]"), strTmp);
	}
	Out.Replace(_T("[DSourcesM]"), strTmp);

	_GetPlainResString(strTmp, IDS_PRIORITY);
	if (WSdownloadColumnHidden[6]) {
		Out.Replace(_T("[DPriorityI]"), _T(""));
		Out.Replace(_T("[DPriority]"), _T(""));
	} else {
		Out.Replace(_T("[DPriorityI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_PRIORITY) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DPriority]"), strTmp);
	}
	Out.Replace(_T("[DPriorityM]"), strTmp);

	_GetPlainResString(strTmp, IDS_CAT);
	if (WSdownloadColumnHidden[7]) {
		Out.Replace(_T("[DCategoryI]"), _T(""));
		Out.Replace(_T("[DCategory]"), _T(""));
	} else {
		Out.Replace(_T("[DCategoryI]"), (pThis->m_Params.DownloadSort == DOWN_SORT_CATEGORY) ? pcSortIcon : _T(""));
		Out.Replace(_T("[DCategory]"), strTmp);
	}
	Out.Replace(_T("[DCategoryM]"), strTmp);

	// add 8th columns here

	pcSortIcon = (pThis->m_Params.bUploadSortReverse) ? pThis->m_Templates.sUpArrow : pThis->m_Templates.sDownArrow;

	_GetPlainResString(strTmp, IDS_QL_USERNAME);
	if (WSuploadColumnHidden[0]) {
		Out.Replace(_T("[UUserI]"), _T(""));
		Out.Replace(_T("[UUser]"), _T(""));
	} else {
		Out.Replace(_T("[UUserI]"), (pThis->m_Params.UploadSort == UP_SORT_USER) ? pcSortIcon : _T(""));
		Out.Replace(_T("[UUser]"), strTmp);
	}
	Out.Replace(_T("[UUserM]"), strTmp);

	_GetPlainResString(strTmp, IDS_CD_VERSION);
	if (WSuploadColumnHidden[1]) {
		Out.Replace(_T("[UVersionI]"), _T(""));
		Out.Replace(_T("[UVersion]"), _T(""));
	} else {
		Out.Replace(_T("[UVersionI]"), (pThis->m_Params.UploadSort == UP_SORT_VERSION) ? pcSortIcon : _T(""));
		Out.Replace(_T("[UVersion]"), strTmp);
	}
	Out.Replace(_T("[UVersionM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_FILENAME);
	if (WSuploadColumnHidden[2]) {
		Out.Replace(_T("[UFilenameI]"), _T(""));
		Out.Replace(_T("[UFilename]"), _T(""));
	} else {
		Out.Replace(_T("[UFilenameI]"), (pThis->m_Params.UploadSort == UP_SORT_FILENAME) ? pcSortIcon : _T(""));
		Out.Replace(_T("[UFilename]"), strTmp);
	}
	Out.Replace(_T("[UFilenameM]"), strTmp);

	_GetPlainResString(strTmp, IDS_STATS_SRATIO);
	if (WSuploadColumnHidden[3]) {
		Out.Replace(_T("[UTransferredI]"), _T(""));
		Out.Replace(_T("[UTransferred]"), _T(""));
	} else {
		Out.Replace(_T("[UTransferredI]"), (pThis->m_Params.UploadSort == UP_SORT_TRANSFERRED) ? pcSortIcon : _T(""));
		Out.Replace(_T("[UTransferred]"), strTmp);
	}
	Out.Replace(_T("[UTransferredM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SPEED);
	if (WSuploadColumnHidden[4]) {
		Out.Replace(_T("[USpeedI]"), _T(""));
		Out.Replace(_T("[USpeed]"), _T(""));
	} else {
		Out.Replace(_T("[USpeedI]"), (pThis->m_Params.UploadSort == UP_SORT_SPEED) ? pcSortIcon : _T(""));
		Out.Replace(_T("[USpeed]"), strTmp);
	}
	Out.Replace(_T("[USpeedM]"), strTmp);

	Out.Replace(_T("[DownloadList]"), _GetPlainResString(IDS_TW_DOWNLOADS));
	Out.Replace(_T("[UploadList]"), _GetPlainResString(IDS_TW_UPLOADS));
	Out.Replace(_T("[Actions]"), _GetPlainResString(IDS_WEB_ACTIONS));
	Out.Replace(_T("[TotalDown]"), _GetPlainResString(IDS_INFLST_USER_TOTALDOWNLOAD));
	Out.Replace(_T("[TotalUp]"), _GetPlainResString(IDS_INFLST_USER_TOTALUPLOAD));
	Out.Replace(_T("[admin]"), bAdmin ? _T("admin") : _T(""));
	_InsertCatBox(Out, cat, _T(""), true, true, sSession, _T(""));

	CArray<DownloadFiles> FilesArray;
	CArray<CPartFile*, CPartFile*> partlist;

	theApp.emuledlg->transferwnd->GetDownloadList()->GetDisplayedFiles(&partlist);

	// Populating array
	for (INT_PTR i = 0; i < partlist.GetCount(); ++i) {

		CPartFile *pPartFile = partlist[i];

		if (pPartFile) {
			if (cat < 0) {
				switch (cat) {
				case -1:
					if (pPartFile->GetCategory() != 0)
						continue;
					break;
				case -2:
					if (!pPartFile->IsPartFile())
						continue;
					break;
				case -3:
					if (pPartFile->IsPartFile())
						continue;
					break;
				case -4:
					if (!((pPartFile->GetStatus() == PS_READY || pPartFile->GetStatus() == PS_EMPTY) && pPartFile->GetTransferringSrcCount() == 0))
						continue;
					break;
				case -5:
					if (!((pPartFile->GetStatus() == PS_READY || pPartFile->GetStatus() == PS_EMPTY) && pPartFile->GetTransferringSrcCount() > 0))
						continue;
					break;
				case -6:
					if (pPartFile->GetStatus() != PS_ERROR)
						continue;
					break;
				case -7:
					if (pPartFile->GetStatus() != PS_PAUSED && !pPartFile->IsStopped())
						continue;
					break;
				case -8:
					if (pPartFile->lastseencomplete == 0)
						continue;
					break;
				case -9:
					if (!pPartFile->IsMovie())
						continue;
					break;
				case -10:
					if (ED2KFT_AUDIO != GetED2KFileTypeID(pPartFile->GetFileName()))
						continue;
					break;
				case -11:
					if (!pPartFile->IsArchive())
						continue;
					break;
				case -12:
					if (ED2KFT_CDIMAGE != GetED2KFileTypeID(pPartFile->GetFileName()))
						continue;
					break;
				case -13:
					if (ED2KFT_DOCUMENT != GetED2KFileTypeID(pPartFile->GetFileName()))
						continue;
					break;
				case -14:
					if (ED2KFT_IMAGE != GetED2KFileTypeID(pPartFile->GetFileName()))
						continue;
					break;
				case -15:
					if (ED2KFT_PROGRAM != GetED2KFileTypeID(pPartFile->GetFileName()))
						continue;
					break;
				case -16:
					if (ED2KFT_EMULECOLLECTION != GetED2KFileTypeID(pPartFile->GetFileName()))
						continue;
				//JOHNTODO: Not too sure here. I was going to add Collections but noticed something strange.
				//Are these supposed to match the list in PartFile around line 5132? Because they do not.
				}
			} else if (cat > 0 && pPartFile->GetCategory() != (UINT)cat)
				continue;

			DownloadFiles dFile;
			dFile.sFileName = _SpecialChars(pPartFile->GetFileName());
			dFile.sFileType = _GetWebImageNameForFileType(dFile.sFileName);
			dFile.sFileNameJS = _SpecialChars(pPartFile->GetFileName());	//for javascript
			dFile.m_qwFileSize = (uint64)pPartFile->GetFileSize();
			dFile.m_qwFileTransferred = (uint64)pPartFile->GetCompletedSize();
			dFile.m_dblCompleted = pPartFile->GetPercentCompleted();
			dFile.lFileSpeed = pPartFile->GetDatarate();

			if (pPartFile->HasComment() || pPartFile->HasRating())
				dFile.iComment = pPartFile->HasBadRating() ? 2 : 1;
			else
				dFile.iComment = 0;

			dFile.iFileState = pPartFile->getPartfileStatusRank();

			LPCTSTR pFileState;
			switch (pPartFile->GetStatus()) {
			case PS_HASHING:
				pFileState = _T("hashing");
				break;
			case PS_WAITINGFORHASH:
				pFileState = _T("waitinghash");
				break;
			case PS_ERROR:
				pFileState = _T("error");
				break;
			case PS_COMPLETING:
				pFileState = _T("completing");
				break;
			case PS_COMPLETE:
				pFileState = _T("complete");
				break;
			case PS_PAUSED:
				pFileState = pPartFile->IsStopped() ? _T("stopped") : _T("paused");
				break;
			default:
				pFileState = (pPartFile->GetDatarate() > 0) ? _T("downloading") : _T("waiting");
			}
			dFile.sFileState = CString(pFileState);

			dFile.bFileAutoPrio = pPartFile->IsAutoDownPriority();
			dFile.nFilePrio = pPartFile->GetDownPriority();
			int pCat = pPartFile->GetCategory();

			CString strCategory = thePrefs.GetCategory(pCat)->strTitle;
			strCategory.Replace(_T("'"), _T("\\'"));

			dFile.sCategory = strCategory;

			dFile.sFileHash = md4str(pPartFile->GetFileHash());
			dFile.lSourceCount = pPartFile->GetSourceCount();
			dFile.lNotCurrentSourceCount = pPartFile->GetNotCurrentSourcesCount();
			dFile.lTransferringSourceCount = pPartFile->GetTransferringSrcCount();
			dFile.bIsComplete = !pPartFile->IsPartFile();
			dFile.bIsPreview = pPartFile->IsReadyForPreview();
			dFile.bIsGetFLC = pPartFile->GetPreviewPrio();

			if (theApp.GetPublicIP() != 0 && !theApp.IsFirewalled())
				dFile.sED2kLink = pPartFile->GetED2kLink(false, false, false, true, theApp.GetPublicIP());
			else
				dFile.sED2kLink = pPartFile->GetED2kLink();

			dFile.sFileInfo = _SpecialChars(pPartFile->GetInfoSummary(true), false);

			FilesArray.Add(dFile);
		}
	}

	SortParams dprm{ (int)pThis->m_Params.DownloadSort, pThis->m_Params.bDownloadSortReverse };
	qsort_s(FilesArray.GetData(), FilesArray.GetCount(), sizeof(DownloadFiles), &_DownloadCmp, &dprm);

	CArray<UploadUsers> UploadArray;

	for (POSITION pos = theApp.uploadqueue->GetFirstFromUploadList(); pos != NULL;) {
		UploadUsers dUser;
		const CUpDownClient &cur_client(*theApp.uploadqueue->GetNextFromUploadList(pos));
		dUser.sUserHash = md4str(cur_client.GetUserHash());
		if (cur_client.GetDatarate() > 0) {
			dUser.sActive = _T("downloading");
			dUser.sClientState = _T("uploading");
		} else {
			dUser.sActive = _T("waiting");
			dUser.sClientState = _T("connecting");
		}

		dUser.sFileInfo = _SpecialChars(_GetClientSummary(cur_client), false);
		dUser.sFileInfo.Replace(_T("\\"), _T("\\\\"));
		dUser.sFileInfo.Replace(_T("\n"), _T("<br>"));
		dUser.sFileInfo.Replace(_T("'"), _T("&#8217;"));

		dUser.sClientSoft = _GetClientversionImage(cur_client);

		if (cur_client.IsBanned())
			dUser.sClientExtra = _T("banned");
		else if (cur_client.IsFriend())
			dUser.sClientExtra = _T("friend");
		else if (cur_client.Credits()->GetScoreRatio(cur_client.GetIP()) > 1)
			dUser.sClientExtra = _T("credit");
		else
			dUser.sClientExtra = _T("none");

		CString cname(cur_client.GetUserName());
		if (cname.GetLength() > SHORT_LENGTH_MIN)
			dUser.sUserName = _SpecialChars(cname.Left(SHORT_LENGTH_MIN - 3)) + _T("...");
		else
			dUser.sUserName = _SpecialChars(cname);

		CKnownFile *file = theApp.sharedfiles->GetFileByID(cur_client.GetUploadFileID());
		if (file)
			dUser.sFileName = _SpecialChars(file->GetFileName());
		else
			dUser.sFileName = _GetPlainResString(IDS_REQ_UNKNOWNFILE);
		dUser.nTransferredDown = cur_client.GetTransferredDown();
		dUser.nTransferredUp = cur_client.GetTransferredUp();
		int iDataRate = cur_client.GetDatarate();
		dUser.nDataRate = ((iDataRate == -1) ? 0 : iDataRate);
		dUser.sClientNameVersion = cur_client.GetClientSoftVer();
		UploadArray.Add(dUser);
	}

	SortParams uprm{ (int)pThis->m_Params.UploadSort, pThis->m_Params.bUploadSortReverse };
	qsort_s(UploadArray.GetData(), UploadArray.GetCount(), sizeof(UploadUsers), &_UploadCmp, &uprm);

	_MakeTransferList(Out, pThis, Data, &FilesArray, &UploadArray, bAdmin);

	Out.Replace(_T("[Session]"), sSession);
	Out.Replace(_T("[CatSel]"), sCat);

	return Out;
}

void CWebServer::_MakeTransferList(CString &Out, CWebServer *pThis, const ThreadData &Data, void *_FilesArray, void *_UploadArray, bool bAdmin)
{
	CArray<DownloadFiles> *FilesArray = (CArray<DownloadFiles>*)_FilesArray;
	CArray<UploadUsers> *UploadArray = (CArray<UploadUsers>*)_UploadArray;

	// Displaying
	int nCountQueue = 0;
	int nCountQueueBanned = 0;
	int nCountQueueFriend = 0;
	int nCountQueueSecure = 0;
	int nCountQueueBannedSecure = 0;
	int nCountQueueFriendSecure = 0;

	CQArray<QueueUsers, QueueUsers> QueueArray;
	for (POSITION pos = theApp.uploadqueue->waitinglist.GetHeadPosition(); pos != NULL;) {
		QueueUsers dUser;
		const CUpDownClient &cur_client(*theApp.uploadqueue->waitinglist.GetNext(pos));
		int iSecure = static_cast<int>(cur_client.Credits()->GetCurrentIdentState(cur_client.GetIP()) == IS_IDENTIFIED);
		if (cur_client.IsBanned()) {
			dUser.sClientExtra = _T("banned");
			++nCountQueueBanned;
			nCountQueueBannedSecure += iSecure;
		} else if (cur_client.IsFriend()) {
			dUser.sClientExtra = _T("friend");
			++nCountQueueFriend;
			nCountQueueFriendSecure += iSecure;
		} else {
			dUser.sClientExtra = _T("none");
			++nCountQueue;
			nCountQueueSecure += iSecure;
		}

		CString usn(cur_client.GetUserName());
		if (usn.GetLength() > SHORT_LENGTH_MIN)
			usn = usn.Left(SHORT_LENGTH_MIN - 3) + _T("...");
		dUser.sUserName = _SpecialChars(usn);

		dUser.sClientNameVersion = cur_client.GetClientSoftVer();
		CKnownFile *file = theApp.sharedfiles->GetFileByID(cur_client.GetUploadFileID());
		dUser.sFileName = file ? _SpecialChars(file->GetFileName()) : _GetPlainResString(IDS_REQ_UNKNOWNFILE);
		dUser.sClientState = dUser.sClientExtra;
		dUser.sClientStateSpecial = _T("connecting");
		dUser.nScore = cur_client.GetScore(false);

		dUser.sClientSoft = _GetClientversionImage(cur_client);
		dUser.sUserHash = md4str(cur_client.GetUserHash());
		//SyruS CQArray-Sorting setting sIndex according to param
		switch (pThis->m_Params.QueueSort) {
		case QU_SORT_CLIENT:
			dUser.sIndex = dUser.sClientSoft;
			break;
		case QU_SORT_USER:
			dUser.sIndex = dUser.sUserName;
			break;
		case QU_SORT_VERSION:
			dUser.sIndex = dUser.sClientNameVersion;
			break;
		case QU_SORT_FILENAME:
			dUser.sIndex = dUser.sFileName;
			break;
		case QU_SORT_SCORE:
			dUser.sIndex.Format(_T("%09u"), dUser.nScore);
		}
		QueueArray.Add(dUser);
	}

	INT_PTR nNextPos = 0;	// position in queue of the user with the highest score -> next upload user
	uint32 nNextScore = 0;	// highest score -> next upload user
	for (INT_PTR i = QueueArray.GetCount(); --i >= 0;)
		if (QueueArray[i].nScore > nNextScore) {
			nNextPos = i;
			nNextScore = QueueArray[i].nScore;
		}

	if (theApp.uploadqueue->waitinglist.GetHeadPosition() != NULL) {
		QueueArray[nNextPos].sClientState = _T("next");
		QueueArray[nNextPos].sClientStateSpecial = QueueArray[nNextPos].sClientState;
	}

	if ((nCountQueue > 0 && pThis->m_Params.bShowUploadQueue)
		|| (nCountQueueBanned > 0 && pThis->m_Params.bShowUploadQueueBanned)
		|| (nCountQueueFriend > 0 && pThis->m_Params.bShowUploadQueueFriend))
	{
#ifdef _DEBUG
		DWORD dwStart = ::GetTickCount();
#endif
		QueueArray.QuickSort(pThis->m_Params.bQueueSortReverse);
#ifdef _DEBUG
		AddDebugLogLine(false, _T("WebServer: Waitingqueue with %u elements sorted in %u ms"), QueueArray.GetCount(), ::GetTickCount() - dwStart);
#endif
	}

	CString sDownList, HTTPProcessData;
	CString HTTPTemp;
	LPCTSTR pcTmp;
	double fTotalSize = 0, fTotalTransferred = 0, fTotalSpeed = 0;

	CString OutE = pThis->m_Templates.sTransferDownLine;
	for (INT_PTR i = 0; i < FilesArray->GetCount(); ++i) {
		const DownloadFiles &downf((*FilesArray)[i]);
		HTTPProcessData = OutE;

		pcTmp = (downf.sFileHash == _ParseURL(Data.sURL, _T("file"))) ? _T("checked") : _T("checked_no");
		HTTPProcessData.Replace(_T("[LastChangedDataset]"), pcTmp);

		//CPartFile *found_file = theApp.downloadqueue->GetFileByID(_GetFileHash(dwnlf.sFileHash, FileHashA4AF));

		CString strFinfo(downf.sFileInfo);
		strFinfo.Replace(_T("\\"), _T("\\\\"));
		CString strFileInfo(strFinfo);

		strFinfo.Replace(_T("'"), _T("&#8217;"));
		strFinfo.Replace(_T("\n"), _T("\\n"));

		strFileInfo.Replace(_T("\n"), _T("<br>"));

		if (!downf.iComment) {
			HTTPProcessData.Replace(_T("[HASCOMMENT]"), _T("<!--"));
			HTTPProcessData.Replace(_T("[HASCOMMENT_END]"), _T("-->"));
		} else {
			HTTPProcessData.Replace(_T("[HASCOMMENT]"), _T(""));
			HTTPProcessData.Replace(_T("[HASCOMMENT_END]"), _T(""));
		}

		if (downf.sFileState.CompareNoCase(_T("downloading")) == 0 || downf.sFileState.CompareNoCase(_T("waiting")) == 0) {
			HTTPProcessData.Replace(_T("[ISACTIVE]"), _T("<!--"));
			HTTPProcessData.Replace(_T("[ISACTIVE_END]"), _T("-->"));
			HTTPProcessData.Replace(_T("[!ISACTIVE]"), _T(""));
			HTTPProcessData.Replace(_T("[!ISACTIVE_END]"), _T(""));
		} else {
			HTTPProcessData.Replace(_T("[ISACTIVE]"), _T(""));
			HTTPProcessData.Replace(_T("[ISACTIVE_END]"), _T(""));
			HTTPProcessData.Replace(_T("[!ISACTIVE]"), _T("<!--"));
			HTTPProcessData.Replace(_T("[!ISACTIVE_END]"), _T("-->"));
		}

		CString ed2k = downf.sED2kLink; //ed2klink
		ed2k.Replace(_T("'"), _T("&#8217;"));
		CString fname = downf.sFileNameJS; //filename
		CString state = downf.sFileState;
		CString fsize; //file size
		fsize.Format(_T("%I64u"), downf.m_qwFileSize);
		const CString &session(_ParseURL(Data.sURL, _T("ses")));

		CString isgetflc; //getflc
		if (!downf.bIsPreview)
			isgetflc = downf.bIsGetFLC ? _T("enabled") : _T("disabled");

		//priority
		if (downf.bFileAutoPrio)
			pcTmp = _T("Auto");
		else {
			switch (downf.nFilePrio) {
			case 0:
				pcTmp = _T("Low");
				break;
			case 1:
				pcTmp = _T("Normal");
				break;
			case 2:
				pcTmp = _T("High");
				break;
			default:
				pcTmp = _T("");
			}
		}

		HTTPProcessData.Replace(_T("[admin]"), bAdmin ? _T("admin") : _T(""));
		HTTPProcessData.Replace(_T("[finfo]"), strFinfo);
		HTTPProcessData.Replace(_T("[fcomments]"), downf.iComment ? _T("yes") : _T(""));
		HTTPProcessData.Replace(_T("[ed2k]"), _SpecialChars(ed2k));
		HTTPProcessData.Replace(_T("[DownState]"), state);
		HTTPProcessData.Replace(_T("[isgetflc]"), isgetflc);
		HTTPProcessData.Replace(_T("[fname]"), _SpecialChars(fname));
		HTTPProcessData.Replace(_T("[fsize]"), fsize);
		HTTPProcessData.Replace(_T("[session]"), session);
		HTTPProcessData.Replace(_T("[filehash]"), downf.sFileHash);
		HTTPProcessData.Replace(_T("[down-priority]"), pcTmp);
		HTTPProcessData.Replace(_T("[FileType]"), downf.sFileType);
		HTTPProcessData.Replace(_T("[downloadable]"), (bAdmin && (thePrefs.GetMaxWebUploadFileSizeMB() == 0 || downf.m_qwFileSize < ((uint64)thePrefs.GetMaxWebUploadFileSizeMB()) * 1024 * 1024)) ? _T("yes") : _T("no"));

		// comment icon
		switch (downf.iComment) {
		case 1:
			pcTmp = _T("cmtgood");
			break;
		case 2:
			pcTmp = _T("cmtbad");
			break;
		//case 0:
		default:
			pcTmp = _T("none");
		}
		HTTPProcessData.Replace(_T("[FileCommentIcon]"), pcTmp);

		pcTmp = (!downf.bIsPreview && downf.bIsGetFLC) ? _T("getflc") : _T("halfnone");
		HTTPProcessData.Replace(_T("[FileIsGetFLC]"), pcTmp);

		if (WSdownloadColumnHidden[0])
			HTTPProcessData.Replace(_T("[ShortFileName]"), _T(""));
		else if (downf.sFileName.GetLength() > (SHORT_LENGTH_MAX))
			HTTPProcessData.Replace(_T("[ShortFileName]"), downf.sFileName.Left(SHORT_LENGTH_MAX - 3) + _T("..."));
		else
			HTTPProcessData.Replace(_T("[ShortFileName]"), downf.sFileName);
		
		HTTPProcessData.Replace(_T("[FileInfo]"), strFileInfo);
		fTotalSize += downf.m_qwFileSize;

		HTTPProcessData.Replace(_T("[2]"), WSdownloadColumnHidden[1] ? _T("") : (LPCTSTR)CastItoXBytes(downf.m_qwFileSize));
		
		if (WSdownloadColumnHidden[2])
			HTTPProcessData.Replace(_T("[3]"), _T(""));
		else if (downf.m_qwFileTransferred > 0) {
			fTotalTransferred += downf.m_qwFileTransferred;
			HTTPProcessData.Replace(_T("[3]"), CastItoXBytes(downf.m_qwFileTransferred));
		} else
			HTTPProcessData.Replace(_T("[3]"), _T("-"));

		HTTPProcessData.Replace(_T("[DownloadBar]"), WSdownloadColumnHidden[3] ? _T("") : (LPCTSTR)_GetDownloadGraph(Data, downf.sFileHash));

		if (WSdownloadColumnHidden[4])
			pcTmp = _T("");
		else if (downf.lFileSpeed > 0) {
			fTotalSpeed += downf.lFileSpeed;
			HTTPTemp.Format(_T("%8.2f"), downf.lFileSpeed / 1024.0);
			pcTmp = HTTPTemp;
		} else
			pcTmp = _T("-");
		HTTPProcessData.Replace(_T("[4]"), pcTmp);

		if (WSdownloadColumnHidden[5])
			pcTmp = _T("");
		else if (downf.lSourceCount > 0) {
			HTTPTemp.Format(_T("%li&nbsp;/&nbsp;%8li&nbsp;(%li)"),
				downf.lSourceCount - downf.lNotCurrentSourceCount,
				downf.lSourceCount,
				downf.lTransferringSourceCount);
			pcTmp = HTTPTemp;
		} else
			pcTmp = _T("-");
		HTTPProcessData.Replace(_T("[5]"), pcTmp);

		if (WSdownloadColumnHidden[6] || downf.nFilePrio < 0 || downf.nFilePrio > 2)
			HTTPProcessData.Replace(_T("[PrioVal]"), _T(""));
		else {
			static const UINT uprio[2][3] =
			{
				{IDS_PRIOLOW, IDS_PRIONORMAL, IDS_PRIOHIGH},
				{IDS_PRIOAUTOLOW, IDS_PRIOAUTONORMAL, IDS_PRIOAUTOHIGH}
			};
			HTTPProcessData.Replace(_T("[PrioVal]"), GetResString(uprio[static_cast<unsigned>(downf.bFileAutoPrio)][downf.nFilePrio]));
		}

		pcTmp = WSdownloadColumnHidden[7] ? _T("") : (LPCTSTR)downf.sCategory;
		HTTPProcessData.Replace(_T("[Category]"), pcTmp);

		_InsertCatBox(HTTPProcessData, 0, _T(""), false, false, session, downf.sFileHash);

		sDownList += HTTPProcessData;
	}

	Out.Replace(_T("[DownloadFilesList]"), sDownList);
	Out.Replace(_T("[TotalDownSize]"), CastItoXBytes(fTotalSize));

	Out.Replace(_T("[TotalDownTransferred]"), CastItoXBytes(fTotalTransferred));

	HTTPTemp.Format(_T("%8.2f"), fTotalSpeed / 1024.0);
	Out.Replace(_T("[TotalDownSpeed]"), HTTPTemp);

	HTTPTemp.Format(_T("%s: %i"), (LPCTSTR)GetResString(IDS_SF_FILE), (int)FilesArray->GetCount());
	Out.Replace(_T("[TotalFiles]"), HTTPTemp);

	HTTPTemp.Format(_T("%i"), pThis->m_Templates.iProgressbarWidth);
	Out.Replace(_T("[PROGRESSBARWIDTHVAL]"), HTTPTemp);

	fTotalSize = fTotalTransferred = fTotalSpeed = 0;
	CString sUpList;

	OutE = pThis->m_Templates.sTransferUpLine;
	OutE.Replace(_T("[admin]"), bAdmin ? _T("admin") : _T(""));

	for (INT_PTR i = 0; i < UploadArray->GetCount(); ++i) {
		const UploadUsers &ulu((*UploadArray)[i]);
		HTTPProcessData = OutE;

		HTTPProcessData.Replace(_T("[UserHash]"), ulu.sUserHash);
		HTTPProcessData.Replace(_T("[UpState]"), ulu.sActive);
		HTTPProcessData.Replace(_T("[FileInfo]"), ulu.sFileInfo);
		HTTPProcessData.Replace(_T("[ClientState]"), ulu.sClientState);
		HTTPProcessData.Replace(_T("[ClientSoft]"), ulu.sClientSoft);
		HTTPProcessData.Replace(_T("[ClientExtra]"), ulu.sClientExtra);

		pcTmp = WSuploadColumnHidden[0] ? _T("") : (LPCTSTR)ulu.sUserName;
		HTTPProcessData.Replace(_T("[1]"), pcTmp);

		pcTmp = WSuploadColumnHidden[1] ? _T("") : (LPCTSTR)ulu.sClientNameVersion;
		HTTPProcessData.Replace(_T("[ClientSoftV]"), pcTmp);

		pcTmp = WSuploadColumnHidden[2] ? _T("") : (LPCTSTR)ulu.sFileName;
		HTTPProcessData.Replace(_T("[2]"), pcTmp);

		if (WSuploadColumnHidden[3])
			pcTmp = _T("");
		else {
			fTotalSize += ulu.nTransferredDown;
			fTotalTransferred += ulu.nTransferredUp;
			HTTPTemp.Format(_T("%s / %s"), (LPCTSTR)CastItoXBytes(ulu.nTransferredDown), (LPCTSTR)CastItoXBytes(ulu.nTransferredUp));
			pcTmp = HTTPTemp;
		}
		HTTPProcessData.Replace(_T("[3]"), pcTmp);

		if (WSuploadColumnHidden[4])
			pcTmp = _T("");
		else {
			fTotalSpeed += ulu.nDataRate;
			HTTPTemp.Format(_T("%8.2f "), max(ulu.nDataRate / 1024.0, 0.0));
			pcTmp = HTTPTemp;
		}
		HTTPProcessData.Replace(_T("[4]"), pcTmp);

		sUpList += HTTPProcessData;
	}
	Out.Replace(_T("[UploadFilesList]"), sUpList);
	HTTPTemp.Format(_T("%s / %s"), (LPCTSTR)CastItoXBytes(fTotalSize), (LPCTSTR)CastItoXBytes(fTotalTransferred));
	Out.Replace(_T("[TotalUpTransferred]"), HTTPTemp);
	HTTPTemp.Format(_T("%8.2f "), max(fTotalSpeed / 1024, 0.0));
	Out.Replace(_T("[TotalUpSpeed]"), HTTPTemp);

	if (pThis->m_Params.bShowUploadQueue) {
		Out.Replace(_T("[UploadQueue]"), pThis->m_Templates.sTransferUpQueueShow);
		Out.Replace(_T("[UploadQueueList]"), _GetPlainResString(IDS_ONQUEUE));

		CString sQueue;

		OutE = pThis->m_Templates.sTransferUpQueueLine;
		OutE.Replace(_T("[admin]"), bAdmin ? _T("admin") : _T(""));

		for (INT_PTR i = 0; i < QueueArray.GetCount(); ++i) {
			if (QueueArray[i].sClientExtra == _T("none")) {
				HTTPProcessData = OutE;
				pcTmp = WSqueueColumnHidden[0] ? _T("") : (LPCTSTR)QueueArray[i].sUserName;
				HTTPProcessData.Replace(_T("[UserName]"), pcTmp);

				pcTmp = WSqueueColumnHidden[1] ? _T("") : (LPCTSTR)QueueArray[i].sClientNameVersion;
				HTTPProcessData.Replace(_T("[ClientSoftV]"), pcTmp);

				pcTmp = WSqueueColumnHidden[2] ? _T("") : (LPCTSTR)QueueArray[i].sFileName;
				HTTPProcessData.Replace(_T("[FileName]"), pcTmp);

				TCHAR HTTPTempC[20];
				if (WSqueueColumnHidden[3])
					*HTTPTempC = _T('\0');
				else
					_stprintf(HTTPTempC, _T("%i"), QueueArray[i].nScore);
				HTTPProcessData.Replace(_T("[Score]"), HTTPTempC);
				HTTPProcessData.Replace(_T("[ClientState]"), QueueArray[i].sClientState);
				HTTPProcessData.Replace(_T("[ClientStateSpecial]"), QueueArray[i].sClientStateSpecial);
				HTTPProcessData.Replace(_T("[ClientSoft]"), QueueArray[i].sClientSoft);
				HTTPProcessData.Replace(_T("[ClientExtra]"), QueueArray[i].sClientExtra);
				HTTPProcessData.Replace(_T("[UserHash]"), QueueArray[i].sUserHash);

				sQueue += HTTPProcessData;
			}
		}
		Out.Replace(_T("[QueueList]"), sQueue);
	} else
		Out.Replace(_T("[UploadQueue]"), pThis->m_Templates.sTransferUpQueueHide);

	if (pThis->m_Params.bShowUploadQueueBanned) {
		Out.Replace(_T("[UploadQueueBanned]"), pThis->m_Templates.sTransferUpQueueBannedShow);
		Out.Replace(_T("[UploadQueueBannedList]"), _GetPlainResString(IDS_BANNED));

		CString sQueueBanned;

		OutE = pThis->m_Templates.sTransferUpQueueBannedLine;

		for (INT_PTR i = 0; i < QueueArray.GetCount(); ++i) {
			if (QueueArray[i].sClientExtra == _T("banned")) {
				HTTPProcessData = OutE;
				pcTmp = WSqueueColumnHidden[0] ? _T("") : (LPCTSTR)QueueArray[i].sUserName;
				HTTPProcessData.Replace(_T("[UserName]"), pcTmp);

				pcTmp = WSqueueColumnHidden[1] ? _T("") : (LPCTSTR)QueueArray[i].sClientNameVersion;
				HTTPProcessData.Replace(_T("[ClientSoftV]"), pcTmp);

				pcTmp = WSqueueColumnHidden[2] ? _T("") : (LPCTSTR)QueueArray[i].sFileName;
				HTTPProcessData.Replace(_T("[FileName]"), pcTmp);

				TCHAR HTTPTempC[20];
				if (WSqueueColumnHidden[3])
					*HTTPTempC = _T('\0');
				else
					_stprintf(HTTPTempC, _T("%i"), QueueArray[i].nScore);
				HTTPProcessData.Replace(_T("[Score]"), HTTPTempC);

				HTTPProcessData.Replace(_T("[ClientState]"), QueueArray[i].sClientState);
				HTTPProcessData.Replace(_T("[ClientStateSpecial]"), QueueArray[i].sClientStateSpecial);
				HTTPProcessData.Replace(_T("[ClientSoft]"), QueueArray[i].sClientSoft);
				HTTPProcessData.Replace(_T("[ClientExtra]"), QueueArray[i].sClientExtra);
				HTTPProcessData.Replace(_T("[UserHash]"), QueueArray[i].sUserHash);

				sQueueBanned += HTTPProcessData;
			}
		}
		Out.Replace(_T("[QueueListBanned]"), sQueueBanned);
	} else
		Out.Replace(_T("[UploadQueueBanned]"), pThis->m_Templates.sTransferUpQueueBannedHide);

	if (pThis->m_Params.bShowUploadQueueFriend) {
		Out.Replace(_T("[UploadQueueFriend]"), pThis->m_Templates.sTransferUpQueueFriendShow);
		Out.Replace(_T("[UploadQueueFriendList]"), _GetPlainResString(IDS_IRC_ADDTOFRIENDLIST));

		CString sQueueFriend;

		OutE = pThis->m_Templates.sTransferUpQueueFriendLine;

		for (INT_PTR i = 0; i < QueueArray.GetCount(); ++i) {
			if (QueueArray[i].sClientExtra == _T("friend")) {
				HTTPProcessData = OutE;
				pcTmp = WSqueueColumnHidden[0] ? _T("") : (LPCTSTR)QueueArray[i].sUserName;
				HTTPProcessData.Replace(_T("[UserName]"), pcTmp);

				pcTmp = WSqueueColumnHidden[1] ? _T("") : (LPCTSTR)QueueArray[i].sClientNameVersion;
				HTTPProcessData.Replace(_T("[ClientSoftV]"), pcTmp);

				pcTmp = WSqueueColumnHidden[2] ? _T("") : (LPCTSTR)QueueArray[i].sFileName;
				HTTPProcessData.Replace(_T("[FileName]"), pcTmp);

				TCHAR HTTPTempC[20];
				if (WSqueueColumnHidden[3])
					*HTTPTempC = _T('\0');
				else
					_stprintf(HTTPTempC, _T("%i"), QueueArray[i].nScore);
				HTTPProcessData.Replace(_T("[Score]"), HTTPTempC);

				HTTPProcessData.Replace(_T("[ClientState]"), QueueArray[i].sClientState);
				HTTPProcessData.Replace(_T("[ClientStateSpecial]"), QueueArray[i].sClientStateSpecial);
				HTTPProcessData.Replace(_T("[ClientSoft]"), QueueArray[i].sClientSoft);
				HTTPProcessData.Replace(_T("[ClientExtra]"), QueueArray[i].sClientExtra);
				HTTPProcessData.Replace(_T("[UserHash]"), QueueArray[i].sUserHash);

				sQueueFriend += HTTPProcessData;
			}
		}
		Out.Replace(_T("[QueueListFriend]"), sQueueFriend);
	} else
		Out.Replace(_T("[UploadQueueFriend]"), pThis->m_Templates.sTransferUpQueueFriendHide);


	CString mCounter;
	mCounter.Format(_T("%i"), nCountQueue);
	Out.Replace(_T("[CounterQueue]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueueBanned);
	Out.Replace(_T("[CounterQueueBanned]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueueFriend);
	Out.Replace(_T("[CounterQueueFriend]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueueSecure);
	Out.Replace(_T("[CounterQueueSecure]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueueBannedSecure);
	Out.Replace(_T("[CounterQueueBannedSecure]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueueFriendSecure);
	Out.Replace(_T("[CounterQueueFriendSecure]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueue + nCountQueueBanned + nCountQueueFriend);
	Out.Replace(_T("[CounterAll]"), mCounter);
	mCounter.Format(_T("%i"), nCountQueueSecure + nCountQueueBannedSecure + nCountQueueFriendSecure);
	Out.Replace(_T("[CounterAllSecure]"), mCounter);
	Out.Replace(_T("[ShowUploadQueue]"), _GetPlainResString(IDS_VIEWQUEUE));
	Out.Replace(_T("[ShowUploadQueueList]"), _GetPlainResString(IDS_WEB_SHOW_UPLOAD_QUEUE));

	Out.Replace(_T("[ShowUploadQueueListBanned]"), _GetPlainResString(IDS_WEB_SHOW_UPLOAD_QUEUE_BANNED));
	Out.Replace(_T("[ShowUploadQueueListFriend]"), _GetPlainResString(IDS_WEB_SHOW_UPLOAD_QUEUE_FRIEND));

	CString strTmp = (pThis->m_Params.bQueueSortReverse) ? _T("&amp;sortreverse=false") : _T("&amp;sortreverse=true");

	if (pThis->m_Params.QueueSort == QU_SORT_CLIENT)
		Out.Replace(_T("[SortQClient]"), strTmp);
	else
		Out.Replace(_T("[SortQClient]"), _T(""));
	if (pThis->m_Params.QueueSort == QU_SORT_USER)
		Out.Replace(_T("[SortQUser]"), strTmp);
	else
		Out.Replace(_T("[SortQUser]"), _T(""));
	if (pThis->m_Params.QueueSort == QU_SORT_VERSION)
		Out.Replace(_T("[SortQVersion]"), strTmp);
	else
		Out.Replace(_T("[SortQVersion]"), _T(""));
	if (pThis->m_Params.QueueSort == QU_SORT_FILENAME)
		Out.Replace(_T("[SortQFilename]"), strTmp);
	else
		Out.Replace(_T("[SortQFilename]"), _T(""));
	if (pThis->m_Params.QueueSort == QU_SORT_SCORE)
		Out.Replace(_T("[SortQScore]"), strTmp);
	else
		Out.Replace(_T("[SortQScore]"), _T(""));

	CString pcSortIcon = (pThis->m_Params.bQueueSortReverse) ? pThis->m_Templates.sUpArrow : pThis->m_Templates.sDownArrow;

	_GetPlainResString(strTmp, IDS_QL_USERNAME);
	if (WSqueueColumnHidden[0]) {
		Out.Replace(_T("[UserNameTitleI]"), _T(""));
		Out.Replace(_T("[UserNameTitle]"), _T(""));
	} else {
		if (pThis->m_Params.QueueSort == QU_SORT_USER)
			Out.Replace(_T("[UserNameTitleI]"), pcSortIcon);
		else
			Out.Replace(_T("[UserNameTitleI]"), _T(""));
		//Out.Replace (_T("[UserNameTitleI]"), (pThis->m_Params.QueueSort == QU_SORT_USER) ? (LPCTSTR)pcSortIcon : _T(""));
		Out.Replace(_T("[UserNameTitle]"), strTmp);
	}
	Out.Replace(_T("[UserNameTitleM]"), strTmp);

	_GetPlainResString(strTmp, IDS_CD_CSOFT);
	if (WSqueueColumnHidden[1]) {
		Out.Replace(_T("[VersionI]"), _T(""));
		Out.Replace(_T("[Version]"), _T(""));
	} else {
		if (pThis->m_Params.QueueSort == QU_SORT_VERSION)
			Out.Replace(_T("[VersionI]"), pcSortIcon);
		else
			Out.Replace(_T("[VersionI]"), _T(""));
		//Out.Replace (_T("[VersionI]"), (pThis->m_Params.QueueSort == QU_SORT_VERSION) ? (LPCTSTR)pcSortIcon : _T(""));
		Out.Replace(_T("[Version]"), strTmp);
	}
	Out.Replace(_T("[VersionM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_FILENAME);
	if (WSqueueColumnHidden[2]) {
		Out.Replace(_T("[FileNameTitleI]"), _T(""));
		Out.Replace(_T("[FileNameTitle]"), _T(""));
	} else {
		if (pThis->m_Params.QueueSort == QU_SORT_FILENAME)
			Out.Replace(_T("[FileNameTitleI]"), pcSortIcon);
		else
			Out.Replace(_T("[FileNameTitleI]"), _T(""));
		//Out.Replace (_T("[FileNameTitleI]"), (pThis->m_Params.QueueSort == QU_SORT_FILENAME) ? (LPCTSTR)pcSortIcon : _T(""));
		Out.Replace(_T("[FileNameTitle]"), strTmp);
	}
	Out.Replace(_T("[FileNameTitleM]"), strTmp);

	_GetPlainResString(strTmp, IDS_SCORE);
	if (WSqueueColumnHidden[3]) {
		Out.Replace(_T("[ScoreTitleI]"), _T(""));
		Out.Replace(_T("[ScoreTitle]"), _T(""));
	} else {
		if (pThis->m_Params.QueueSort == QU_SORT_SCORE)
			Out.Replace(_T("[ScoreTitleI]"), pcSortIcon);
		else
			Out.Replace(_T("[ScoreTitleI]"), _T(""));
		//Out.Replace (_T("[ScoreTitleI]"), (pThis->m_Params.QueueSort == QU_SORT_SCORE) ? (LPCTSTR)pcSortIcon : _T(""));
		Out.Replace(_T("[ScoreTitle]"), strTmp);
	}
	Out.Replace(_T("[ScoreTitleM]"), strTmp);
}

CString CWebServer::_GetSharedFilesList(const ThreadData &Data)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));
	bool bAdmin = _IsSessionAdmin(Data, sSession);
	const CString &strSort(_ParseURL(Data.sURL, _T("sort")));

	CString strTmp = _ParseURL(Data.sURL, _T("sortreverse"));

	if (!strSort.IsEmpty()) {
		bool bDirection = false;

		if (strSort == _T("state"))
			pThis->m_Params.SharedSort = SHARED_SORT_STATE;
		else if (strSort == _T("type"))
			pThis->m_Params.SharedSort = SHARED_SORT_TYPE;
		else if (strSort == _T("name")) {
			pThis->m_Params.SharedSort = SHARED_SORT_NAME;
			bDirection = true;
		} else if (strSort == _T("size"))
			pThis->m_Params.SharedSort = SHARED_SORT_SIZE;
		else if (strSort == _T("transferred"))
			pThis->m_Params.SharedSort = SHARED_SORT_TRANSFERRED;
		else if (strSort == _T("alltimetransferred"))
			pThis->m_Params.SharedSort = SHARED_SORT_ALL_TIME_TRANSFERRED;
		else if (strSort == _T("requests"))
			pThis->m_Params.SharedSort = SHARED_SORT_REQUESTS;
		else if (strSort == _T("alltimerequests"))
			pThis->m_Params.SharedSort = SHARED_SORT_ALL_TIME_REQUESTS;
		else if (strSort == _T("accepts"))
			pThis->m_Params.SharedSort = SHARED_SORT_ACCEPTS;
		else if (strSort == _T("alltimeaccepts"))
			pThis->m_Params.SharedSort = SHARED_SORT_ALL_TIME_ACCEPTS;
		else if (strSort == _T("completes"))
			pThis->m_Params.SharedSort = SHARED_SORT_COMPLETES;
		else if (strSort == _T("priority"))
			pThis->m_Params.SharedSort = SHARED_SORT_PRIORITY;

		if (strTmp.IsEmpty())
			pThis->m_Params.bSharedSortReverse = bDirection;
	}
	if (!strTmp.IsEmpty())
		pThis->m_Params.bSharedSortReverse = (strTmp == _T("true"));

	if (!_ParseURL(Data.sURL, _T("hash")).IsEmpty() && !_ParseURL(Data.sURL, _T("prio")).IsEmpty() && bAdmin) {
		CString hash = _ParseURL(Data.sURL, _T("hash"));
		uchar fileid[MDX_DIGEST_SIZE];
		if (hash.GetLength() == 32 && DecodeBase16(hash, hash.GetLength(), fileid, _countof(fileid))) {
			CKnownFile *cur_file = theApp.sharedfiles->GetFileByID(fileid);

			if (cur_file != 0) {
				const CString &sPrio(_ParseURL(Data.sURL, _T("prio")));
				uint8 uPrio;
				if (sPrio == _T("verylow"))
					uPrio = PR_VERYLOW;
				else if (sPrio == _T("low"))
					uPrio = PR_LOW;
				else if (sPrio == _T("normal"))
					uPrio = PR_NORMAL;
				else if (sPrio == _T("high"))
					uPrio = PR_HIGH;
				else if (sPrio == _T("release"))
					uPrio = PR_VERYHIGH;
				else //if (sPrio == _T("auto"))
					uPrio = PR_AUTO;
				if (uPrio == PR_AUTO) {
					cur_file->SetAutoUpPriority(true);
					cur_file->UpdateAutoUpPriority();
				} else {
					cur_file->SetAutoUpPriority(false);
					cur_file->SetUpPriority(uPrio);
				}
				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPD_SFUPDATE, (LPARAM)cur_file);
			}
		}
	}

	if (_ParseURL(Data.sURL, _T("c")) == _T("menu")) {
		int iMenu = _tstoi(_ParseURL(Data.sURL, _T("m")));
		bool bValue = _tstoi(_ParseURL(Data.sURL, _T("v"))) != 0;
		WSsharedColumnHidden[iMenu] = bValue;
		_SaveWIConfigArray(WSsharedColumnHidden, _countof(WSsharedColumnHidden), _T("sharedColumnHidden"));
	}
	if (_ParseURL(Data.sURL, _T("reload")) == _T("true"))
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_SHARED_FILES_RELOAD, 0);

	CString Out = pThis->m_Templates.sSharedList;

	strTmp = (pThis->m_Params.bSharedSortReverse) ? _T("false") : _T("true");

	//State sorting link
	if (pThis->m_Params.SharedSort == SHARED_SORT_STATE)
		Out.Replace(_T("[SortState]"), _T("sort=state&amp;sortreverse=") + strTmp);
	else
		Out.Replace(_T("[SortState]"), _T("sort=state"));
	//Type sorting link
	if (pThis->m_Params.SharedSort == SHARED_SORT_TYPE)
		Out.Replace(_T("[SortType]"), _T("sort=type&amp;sortreverse=") + strTmp);
	else
		Out.Replace(_T("[SortType]"), _T("sort=type"));
	//Name sorting link
	if (pThis->m_Params.SharedSort == SHARED_SORT_NAME)
		Out.Replace(_T("[SortName]"), _T("sort=name&amp;sortreverse=") + strTmp);
	else
		Out.Replace(_T("[SortName]"), _T("sort=name"));
	//Size sorting Link
	if (pThis->m_Params.SharedSort == SHARED_SORT_SIZE)
		Out.Replace(_T("[SortSize]"), _T("sort=size&amp;sortreverse=") + strTmp);
	else
		Out.Replace(_T("[SortSize]"), _T("sort=size"));
	//Complete Sources sorting Link
	if (pThis->m_Params.SharedSort == SHARED_SORT_COMPLETES)
		Out.Replace(_T("[SortCompletes]"), _T("sort=completes&amp;sortreverse=") + strTmp);
	else
		Out.Replace(_T("[SortCompletes]"), _T("sort=completes"));
	//Priority sorting Link
	if (pThis->m_Params.SharedSort == SHARED_SORT_PRIORITY)
		Out.Replace(_T("[SortPriority]"), _T("sort=priority&amp;sortreverse=") + strTmp);
	else
		Out.Replace(_T("[SortPriority]"), _T("sort=priority"));
	//Transferred sorting link
	if (pThis->m_Params.SharedSort == SHARED_SORT_TRANSFERRED) {
		if (pThis->m_Params.bSharedSortReverse)
			Out.Replace(_T("[SortTransferred]"), _T("sort=alltimetransferred&amp;sortreverse=") + strTmp);
		else
			Out.Replace(_T("[SortTransferred]"), _T("sort=transferred&amp;sortreverse=") + strTmp);
	} else if (pThis->m_Params.SharedSort == SHARED_SORT_ALL_TIME_TRANSFERRED) {
		if (pThis->m_Params.bSharedSortReverse)
			Out.Replace(_T("[SortTransferred]"), _T("sort=transferred&amp;sortreverse=") + strTmp);
		else
			Out.Replace(_T("[SortTransferred]"), _T("sort=alltimetransferred&amp;sortreverse=") + strTmp);
	} else
		Out.Replace(_T("[SortTransferred]"), _T("&amp;sort=transferred&amp;sortreverse=false"));
	//Request sorting link
	if (pThis->m_Params.SharedSort == SHARED_SORT_REQUESTS) {
		if (pThis->m_Params.bSharedSortReverse)
			Out.Replace(_T("[SortRequests]"), _T("sort=alltimerequests&amp;sortreverse=") + strTmp);
		else
			Out.Replace(_T("[SortRequests]"), _T("sort=requests&amp;sortreverse=") + strTmp);
	} else if (pThis->m_Params.SharedSort == SHARED_SORT_ALL_TIME_REQUESTS) {
		if (pThis->m_Params.bSharedSortReverse)
			Out.Replace(_T("[SortRequests]"), _T("sort=requests&amp;sortreverse=") + strTmp);
		else
			Out.Replace(_T("[SortRequests]"), _T("sort=alltimerequests&amp;sortreverse=") + strTmp);
	} else
		Out.Replace(_T("[SortRequests]"), _T("&amp;sort=requests&amp;sortreverse=false"));
	//Accepts sorting link
	if (pThis->m_Params.SharedSort == SHARED_SORT_ACCEPTS) {
		if (pThis->m_Params.bSharedSortReverse)
			Out.Replace(_T("[SortAccepts]"), _T("sort=alltimeaccepts&amp;sortreverse=") + strTmp);
		else
			Out.Replace(_T("[SortAccepts]"), _T("sort=accepts&amp;sortreverse=") + strTmp);
	} else if (pThis->m_Params.SharedSort == SHARED_SORT_ALL_TIME_ACCEPTS) {
		if (pThis->m_Params.bSharedSortReverse)
			Out.Replace(_T("[SortAccepts]"), _T("sort=accepts&amp;sortreverse=") + strTmp);
		else
			Out.Replace(_T("[SortAccepts]"), _T("sort=alltimeaccepts&amp;sortreverse=") + strTmp);
	} else
		Out.Replace(_T("[SortAccepts]"), _T("&amp;sort=accepts&amp;sortreverse=false"));

	if (_ParseURL(Data.sURL, _T("reload")) == _T("true")) {
		CString strResultLog = _SpecialChars(theApp.emuledlg->GetLastLogEntry());	//Pick-up last line of the log
		strResultLog = strResultLog.TrimRight(_T('\n'));
		int iStringIndex = strResultLog.ReverseFind(_T('\n'));
		if (iStringIndex > 0)
			strResultLog.Delete(0, iStringIndex);
		Out.Replace(_T("[Message]"), strResultLog);
	} else
		Out.Replace(_T("[Message]"), _T(""));

	const TCHAR *pcSortIcon = (pThis->m_Params.bSharedSortReverse) ? pThis->m_Templates.sUpArrow : pThis->m_Templates.sDownArrow;

	_GetPlainResString(strTmp, IDS_DL_FILENAME);
	if (WSsharedColumnHidden[0]) {
		Out.Replace(_T("[FilenameI]"), _T(""));
		Out.Replace(_T("[Filename]"), _T(""));
	} else {
		Out.Replace(_T("[FilenameI]"), (pThis->m_Params.SharedSort == SHARED_SORT_NAME) ? pcSortIcon : _T(""));
		Out.Replace(_T("[Filename]"), strTmp);
	}
	Out.Replace(_T("[FilenameM]"), strTmp);

	_GetPlainResString(strTmp, IDS_SF_TRANSFERRED);
	if (WSsharedColumnHidden[1]) {
		Out.Replace(_T("[FileTransferredI]"), _T(""));
		Out.Replace(_T("[FileTransferred]"), _T(""));
	} else {
		LPCTSTR pcIconTmp = (pThis->m_Params.SharedSort == SHARED_SORT_TRANSFERRED) ? pcSortIcon : _T("");
		if (pThis->m_Params.SharedSort == SHARED_SORT_ALL_TIME_TRANSFERRED)
			pcIconTmp = (pThis->m_Params.bSharedSortReverse) ? pThis->m_Templates.sUpDoubleArrow : pThis->m_Templates.sDownDoubleArrow;
		Out.Replace(_T("[FileTransferredI]"), pcIconTmp);
		Out.Replace(_T("[FileTransferred]"), strTmp);
	}
	Out.Replace(_T("[FileTransferredM]"), strTmp);

	_GetPlainResString(strTmp, IDS_SF_REQUESTS);
	if (WSsharedColumnHidden[2]) {
		Out.Replace(_T("[FileRequestsI]"), _T(""));
		Out.Replace(_T("[FileRequests]"), _T(""));
	} else {
		LPCTSTR pcIconTmp = (pThis->m_Params.SharedSort == SHARED_SORT_REQUESTS) ? pcSortIcon : _T("");
		if (pThis->m_Params.SharedSort == SHARED_SORT_ALL_TIME_REQUESTS)
			pcIconTmp = (pThis->m_Params.bSharedSortReverse) ? pThis->m_Templates.sUpDoubleArrow : pThis->m_Templates.sDownDoubleArrow;
		Out.Replace(_T("[FileRequestsI]"), pcIconTmp);
		Out.Replace(_T("[FileRequests]"), strTmp);
	}
	Out.Replace(_T("[FileRequestsM]"), strTmp);

	_GetPlainResString(strTmp, IDS_SF_ACCEPTS);
	if (WSsharedColumnHidden[3]) {
		Out.Replace(_T("[FileAcceptsI]"), _T(""));
		Out.Replace(_T("[FileAccepts]"), _T(""));
	} else {
		LPCTSTR pcIconTmp = (pThis->m_Params.SharedSort == SHARED_SORT_ACCEPTS) ? pcSortIcon : _T("");
		if (pThis->m_Params.SharedSort == SHARED_SORT_ALL_TIME_ACCEPTS)
			pcIconTmp = (pThis->m_Params.bSharedSortReverse) ? pThis->m_Templates.sUpDoubleArrow : pThis->m_Templates.sDownDoubleArrow;
		Out.Replace(_T("[FileAcceptsI]"), pcIconTmp);
		Out.Replace(_T("[FileAccepts]"), strTmp);
	}
	Out.Replace(_T("[FileAcceptsM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SIZE);
	if (WSsharedColumnHidden[4]) {
		Out.Replace(_T("[SizeI]"), _T(""));
		Out.Replace(_T("[Size]"), _T(""));
	} else {
		Out.Replace(_T("[SizeI]"), (pThis->m_Params.SharedSort == SHARED_SORT_SIZE) ? pcSortIcon : _T(""));
		Out.Replace(_T("[Size]"), strTmp);
	}
	Out.Replace(_T("[SizeM]"), strTmp);

	_GetPlainResString(strTmp, IDS_COMPLSOURCES);
	if (WSsharedColumnHidden[5]) {
		Out.Replace(_T("[CompletesI]"), _T(""));
		Out.Replace(_T("[Completes]"), _T(""));
	} else {
		Out.Replace(_T("[CompletesI]"), (pThis->m_Params.SharedSort == SHARED_SORT_COMPLETES) ? pcSortIcon : _T(""));
		Out.Replace(_T("[Completes]"), strTmp);
	}
	Out.Replace(_T("[CompletesM]"), strTmp);

	_GetPlainResString(strTmp, IDS_PRIORITY);
	if (WSsharedColumnHidden[6]) {
		Out.Replace(_T("[PriorityI]"), _T(""));
		Out.Replace(_T("[Priority]"), _T(""));
	} else {
		Out.Replace(_T("[PriorityI]"), (pThis->m_Params.SharedSort == SHARED_SORT_PRIORITY) ? pcSortIcon : _T(""));
		Out.Replace(_T("[Priority]"), strTmp);
	}
	Out.Replace(_T("[PriorityM]"), strTmp);

	Out.Replace(_T("[Actions]"), _GetPlainResString(IDS_WEB_ACTIONS));
	Out.Replace(_T("[Reload]"), _GetPlainResString(IDS_SF_RELOAD));
	Out.Replace(_T("[Session]"), sSession);
	Out.Replace(_T("[SharedList]"), _GetPlainResString(IDS_SHAREDFILES));

	CString OutE = pThis->m_Templates.sSharedLine;

	CArray<SharedFiles> SharedArray;

	// Populating array
	for (POSITION pos = BEFORE_START_POSITION; pos != NULL;) {
		CKnownFile *pFile = theApp.sharedfiles->GetFileNext(pos);
		if (pFile != NULL) {
			bool bPartFile = pFile->IsPartFile();

			SharedFiles dFile;
			//dFile.sFileName = _SpecialChars(cur_file->GetFileName());
			dFile.bIsPartFile = bPartFile;
			dFile.sFileName = pFile->GetFileName();
			dFile.sFileState = bPartFile ? _T("filedown") : _T("file");
			dFile.sFileType = _GetWebImageNameForFileType(dFile.sFileName);
			dFile.m_qwFileSize = pFile->GetFileSize();

			if (theApp.GetPublicIP() && !theApp.IsFirewalled())
				dFile.sED2kLink = pFile->GetED2kLink(false, false, false, true, theApp.GetPublicIP());
			else
				dFile.sED2kLink = pFile->GetED2kLink();

			dFile.nFileTransferred = pFile->statistic.GetTransferred();
			dFile.nFileAllTimeTransferred = pFile->statistic.GetAllTimeTransferred();
			dFile.nFileRequests = pFile->statistic.GetRequests();
			dFile.nFileAllTimeRequests = pFile->statistic.GetAllTimeRequests();
			dFile.nFileAccepts = pFile->statistic.GetAccepts();
			dFile.nFileAllTimeAccepts = pFile->statistic.GetAllTimeAccepts();
			dFile.sFileHash = md4str(pFile->GetFileHash());

			if (pFile->m_nCompleteSourcesCountLo == 0)
				dFile.sFileCompletes.Format(_T("< %hu"), pFile->m_nCompleteSourcesCountHi);
			else if (pFile->m_nCompleteSourcesCountLo == pFile->m_nCompleteSourcesCountHi)
				dFile.sFileCompletes.Format(_T("%hu"), pFile->m_nCompleteSourcesCountLo);
			else
				dFile.sFileCompletes.Format(_T("%hu - %hu"), pFile->m_nCompleteSourcesCountLo, pFile->m_nCompleteSourcesCountHi);

			UINT uid;
			if (pFile->IsAutoUpPriority()) {
				switch (pFile->GetUpPriority()) {
				case PR_LOW:
					uid = IDS_PRIOAUTOLOW;
					break;
				case PR_HIGH:
					uid = IDS_PRIOAUTOHIGH;
					break;
				case PR_VERYHIGH:
					uid = IDS_PRIOAUTORELEASE;
					break;
				//case PR_NORMAL:
				default:
					uid = IDS_PRIOAUTONORMAL;
				}
			} else {
				switch (pFile->GetUpPriority()) {
				case PR_VERYLOW:
					uid = IDS_PRIOVERYLOW;
					break;
				case PR_LOW:
					uid = IDS_PRIOLOW;
					break;
				case PR_HIGH:
					uid = IDS_PRIOHIGH;
					break;
				case PR_VERYHIGH:
					uid = IDS_PRIORELEASE;
					break;
				case PR_NORMAL:
				default:
					uid = IDS_PRIONORMAL;
				}
			}
			dFile.sFilePriority = GetResString(uid);

			dFile.nFilePriority = pFile->GetUpPriority();
			dFile.bFileAutoPriority = pFile->IsAutoUpPriority();
			SharedArray.Add(dFile);
		}
	} //for

	SortParams prm{ (int)pThis->m_Params.SharedSort, pThis->m_Params.bSharedSortReverse };
	qsort_s(SharedArray.GetData(), SharedArray.GetCount(), sizeof(SharedFiles), &_SharedCmp, &prm);

	// Displaying
	CString sSharedList;
	for (INT_PTR i = 0; i < SharedArray.GetCount(); ++i) {
		CString HTTPProcessData(OutE);

		bool b = (SharedArray[i].sFileHash == _ParseURL(Data.sURL, _T("hash")));
		HTTPProcessData.Replace(_T("[LastChangedDataset]"), b ? _T("checked") : _T("checked_no"));

		LPCTSTR sharedpriority;	//priority
		if (SharedArray[i].bFileAutoPriority)
			sharedpriority = _T("Auto");
		else
			switch (SharedArray[i].nFilePriority) {
			case PR_VERYLOW:
				sharedpriority = _T("VeryLow");
				break;
			case PR_LOW:
				sharedpriority = _T("Low");
				break;
			case PR_NORMAL:
				sharedpriority = _T("Normal");
				break;
			case PR_HIGH:
				sharedpriority = _T("High");
				break;
			case PR_VERYHIGH:
				sharedpriority = _T("Release");
				break;
			default:
				sharedpriority = _T("");
			}

		CString ed2k(SharedArray[i].sED2kLink);		//ed2klink
		ed2k.Replace(_T("'"), _T("&#8217;"));
		CString hash(SharedArray[i].sFileHash);		//hash
		CString fname(SharedArray[i].sFileName);	//filename
		fname.Replace(_T("'"), _T("&#8217;"));

		bool downloadable = false;
		uchar fileid[MDX_DIGEST_SIZE];
		if (hash.GetLength() == 32 && DecodeBase16(hash, hash.GetLength(), fileid, _countof(fileid))) {
			HTTPProcessData.Replace(_T("[hash]"), hash);
			const CKnownFile *cur_file = theApp.sharedfiles->GetFileByID(fileid);
			if (cur_file != NULL) {
				HTTPProcessData.Replace(_T("[FileIsPriority]")
					, (cur_file->GetUpPriority() == PR_VERYHIGH) ? _T("release") : _T("none"));
				downloadable = !cur_file->IsPartFile() && (thePrefs.GetMaxWebUploadFileSizeMB() == 0 || SharedArray[i].m_qwFileSize < ((uint64)thePrefs.GetMaxWebUploadFileSizeMB()) * 1024 * 1024);
			}
		}

		HTTPProcessData.Replace(_T("[admin]"), bAdmin ? _T("admin") : _T(""));
		HTTPProcessData.Replace(_T("[ed2k]"), _SpecialChars(ed2k));
		HTTPProcessData.Replace(_T("[fname]"), _SpecialChars(fname));
		HTTPProcessData.Replace(_T("[session]"), sSession);
		HTTPProcessData.Replace(_T("[shared-priority]"), sharedpriority); //DonGato: priority change

		HTTPProcessData.Replace(_T("[FileName]"), _SpecialChars(SharedArray[i].sFileName));
		HTTPProcessData.Replace(_T("[FileType]"), SharedArray[i].sFileType);
		HTTPProcessData.Replace(_T("[FileState]"), SharedArray[i].sFileState);

		HTTPProcessData.Replace(_T("[Downloadable]"), downloadable ? _T("yes") : _T("no"));

		HTTPProcessData.Replace(_T("[IFDOWNLOADABLE]"), downloadable ? _T("") : _T("<!--"));
		HTTPProcessData.Replace(_T("[/IFDOWNLOADABLE]"), downloadable ? _T("") : _T("-->"));

		TCHAR HTTPTempC[100];
		//0
		if (WSsharedColumnHidden[0])
			HTTPProcessData.Replace(_T("[ShortFileName]"), _T(""));
		else if (SharedArray[i].sFileName.GetLength() > (SHORT_LENGTH))
			HTTPProcessData.Replace(_T("[ShortFileName]"), _SpecialChars(SharedArray[i].sFileName.Left(SHORT_LENGTH - 3)) + _T("..."));
		else
			HTTPProcessData.Replace(_T("[ShortFileName]"), _SpecialChars(SharedArray[i].sFileName));
		//1
		HTTPProcessData.Replace(_T("[FileTransferred]"), WSsharedColumnHidden[1] ? _T("") : (LPCTSTR)CastItoXBytes(SharedArray[i].nFileTransferred));
		if (WSsharedColumnHidden[1])
			*HTTPTempC = _T('\0');
		else
			_stprintf(HTTPTempC, _T(" (%s)"), (LPCTSTR)CastItoXBytes(SharedArray[i].nFileAllTimeTransferred));
		HTTPProcessData.Replace(_T("[FileAllTimeTransferred]"), HTTPTempC);
		//2
		if (WSsharedColumnHidden[2])
			*HTTPTempC = _T('\0');
		else
			_stprintf(HTTPTempC, _T("%i"), SharedArray[i].nFileRequests);
		HTTPProcessData.Replace(_T("[FileRequests]"), HTTPTempC);
		if (!WSsharedColumnHidden[2])
			_stprintf(HTTPTempC, _T(" (%i)"), SharedArray[i].nFileAllTimeRequests);
		HTTPProcessData.Replace(_T("[FileAllTimeRequests]"), HTTPTempC);
		//3
		if (WSsharedColumnHidden[3])
			*HTTPTempC = _T('\0');
		else
			_stprintf(HTTPTempC, _T("%i"), SharedArray[i].nFileAccepts);
		HTTPProcessData.Replace(_T("[FileAccepts]"), HTTPTempC);
		if (!WSsharedColumnHidden[3])
			_stprintf(HTTPTempC, _T(" (%i)"), SharedArray[i].nFileAllTimeAccepts);
		HTTPProcessData.Replace(_T("[FileAllTimeAccepts]"), HTTPTempC);
		//4..6
		if (WSsharedColumnHidden[4])
			HTTPProcessData.Replace(_T("[FileSize]"), _T(""));
		else
			HTTPProcessData.Replace(_T("[FileSize]"), CastItoXBytes(SharedArray[i].m_qwFileSize));
		if (WSsharedColumnHidden[5])
			HTTPProcessData.Replace(_T("[Completes]"), _T(""));
		else
			HTTPProcessData.Replace(_T("[Completes]"), SharedArray[i].sFileCompletes);
		if (WSsharedColumnHidden[6])
			HTTPProcessData.Replace(_T("[Priority]"), _T(""));
		else
			HTTPProcessData.Replace(_T("[Priority]"), SharedArray[i].sFilePriority);
		HTTPProcessData.Replace(_T("[FileHash]"), SharedArray[i].sFileHash);

		sSharedList += HTTPProcessData;
	}

	Out.Replace(_T("[SharedFilesList]"), sSharedList);
	Out.Replace(_T("[Session]"), sSession);
	return Out;
}

CString CWebServer::_GetGraphs(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	CString Out = pThis->m_Templates.sGraphs;

	CString strGraphDownload, strGraphUpload, strGraphCons;
	LPCTSTR pszFmt = _T("%u");
	INT_PTR cnt = min(WEB_GRAPH_WIDTH, pThis->m_Params.PointsForWeb.GetCount());
	for (INT_PTR i = 0; i < cnt; ++i) {
		const UpDown &pt = pThis->m_Params.PointsForWeb[i];
		// download
		strGraphDownload.AppendFormat(pszFmt, (uint32)(pt.download * 1024));
		// upload
		strGraphUpload.AppendFormat(pszFmt, (uint32)(pt.upload * 1024));
		// connections
		strGraphCons.AppendFormat(pszFmt, (uint32)(pt.connections));
		pszFmt = _T(",%u");
	}

	Out.Replace(_T("[GraphDownload]"), strGraphDownload);
	Out.Replace(_T("[GraphUpload]"), strGraphUpload);
	Out.Replace(_T("[GraphConnections]"), strGraphCons);

	Out.Replace(_T("[TxtDownload]"), _GetPlainResString(IDS_TW_DOWNLOADS));
	Out.Replace(_T("[TxtUpload]"), _GetPlainResString(IDS_TW_UPLOADS));
	Out.Replace(_T("[TxtTime]"), _GetPlainResString(IDS_TIME));
	Out.Replace(_T("[KByteSec]"), _GetPlainResString(IDS_KBYTESPERSEC));
	Out.Replace(_T("[TxtConnections]"), _GetPlainResString(IDS_SP_ACTCON));

	Out.Replace(_T("[ScaleTime]"), (LPCTSTR)CastSecondsToHM(((time_t)thePrefs.GetTrafficOMeterInterval()) * WEB_GRAPH_WIDTH));

	CString s1;
	s1.Format(_T("%i"), thePrefs.GetMaxGraphDownloadRate() + 4);
	Out.Replace(_T("[MaxDownload]"), s1);
	s1.Format(_T("%i"), thePrefs.GetMaxGraphUploadRate(true) + 4);
	Out.Replace(_T("[MaxUpload]"), s1);
	s1.Format(_T("%u"), thePrefs.GetMaxConnections() + 20);
	Out.Replace(_T("[MaxConnections]"), s1);

	return Out;
}

CString CWebServer::_GetAddServerBox(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));
	if (!_IsSessionAdmin(Data, sSession))
		return CString();

	CString resultlog = _SpecialChars(theApp.emuledlg->GetLastLogEntry()); //Pick-up last line of the log

	CString Out = pThis->m_Templates.sAddServerBox;
	if (_ParseURL(Data.sURL, _T("addserver")) == _T("true")) {
		const CString &strServerAddress(_ParseURL(Data.sURL, _T("serveraddr")).Trim());
		const CString &strServerPort(_ParseURL(Data.sURL, _T("serverport")).Trim());
		if (!strServerAddress.IsEmpty() && !strServerPort.IsEmpty()) {
			CString strServerName = _ParseURL(Data.sURL, _T("servername")).Trim();
			if (strServerName.IsEmpty())
				strServerName = strServerAddress;
			CServer *nsrv = new CServer((uint16)_tstoi(strServerPort), strServerAddress);
			nsrv->SetListName(strServerName);
			if (!theApp.emuledlg->serverwnd->serverlistctrl.AddServer(nsrv, true)) {
				delete nsrv;
				Out.Replace(_T("[Message]"), _GetPlainResString(IDS_ERROR));
			} else {
				const CString &sPrio(_ParseURL(Data.sURL, _T("priority")));
				if (sPrio == _T("low"))
					nsrv->SetPreference(PR_LOW);
				else if (sPrio == _T("normal"))
					nsrv->SetPreference(PR_NORMAL);
				else if (sPrio == _T("high"))
					nsrv->SetPreference(PR_HIGH);

				SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATESERVER, (LPARAM)nsrv);

				if (_ParseURL(Data.sURL, _T("addtostatic")) == _T("true")) {
					_AddToStatic(_ParseURL(Data.sURL, _T("serveraddr")), _tstoi(_ParseURL(Data.sURL, _T("serverport"))));
					resultlog.AppendFormat(_T("<br>%s"), (LPCTSTR)_SpecialChars(theApp.emuledlg->GetLastLogEntry())); //Pick-up last line of the log
				}
				resultlog = resultlog.TrimRight(_T('\n'));
				resultlog = resultlog.Mid(resultlog.ReverseFind(_T('\n')));
				Out.Replace(_T("[Message]"), resultlog);
				if (_ParseURL(Data.sURL, _T("connectnow")) == _T("true"))
					_ConnectToServer(_ParseURL(Data.sURL, _T("serveraddr")), _tstoi(_ParseURL(Data.sURL, _T("serverport"))));
			}
		} else
			Out.Replace(_T("[Message]"), _GetPlainResString(IDS_ERROR));
	} else if (_ParseURL(Data.sURL, _T("updateservermetfromurl")) == _T("true")) {
		const CString &url(_ParseURL(Data.sURL, _T("servermeturl")));
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATESERVERMETFROMURL, (LPARAM)(LPCTSTR)url);

		resultlog = _SpecialChars(theApp.emuledlg->GetLastLogEntry());
		resultlog = resultlog.TrimRight(_T('\n'));
		resultlog = resultlog.Mid(resultlog.ReverseFind(_T('\n')));
		Out.Replace(_T("[Message]"), resultlog);
	} else
		Out.Replace(_T("[Message]"), _T(""));

	Out.Replace(_T("[AddServer]"), _GetPlainResString(IDS_SV_NEWSERVER));
	Out.Replace(_T("[IP]"), _GetPlainResString(IDS_SV_ADDRESS));
	Out.Replace(_T("[Port]"), _GetPlainResString(IDS_PORT));
	Out.Replace(_T("[Name]"), _GetPlainResString(IDS_SW_NAME));
	Out.Replace(_T("[Static]"), _GetPlainResString(IDS_STATICSERVER));
	Out.Replace(_T("[ConnectNow]"), _GetPlainResString(IDS_IRC_CONNECT));
	Out.Replace(_T("[Priority]"), _GetPlainResString(IDS_PRIORITY));
	Out.Replace(_T("[Low]"), _GetPlainResString(IDS_PRIOLOW));
	Out.Replace(_T("[Normal]"), _GetPlainResString(IDS_PRIONORMAL));
	Out.Replace(_T("[High]"), _GetPlainResString(IDS_PRIOHIGH));
	Out.Replace(_T("[Add]"), _GetPlainResString(IDS_SV_ADD));
	Out.Replace(_T("[UpdateServerMetFromURL]"), _GetPlainResString(IDS_SV_MET));
	Out.Replace(_T("[URL]"), _GetPlainResString(IDS_SV_URL));
	Out.Replace(_T("[Apply]"), _GetPlainResString(IDS_PW_APPLY));
	//if (bAdmin) { - was checked on entry
	CString s;
	s.Format(_T("?ses=%s&amp;w=server&amp;c=disconnect"), (LPCTSTR)sSession);
	Out.Replace(_T("[URL_Disconnect]"), s);
	s.Format(_T("?ses=%s&amp;w=server&amp;c=connect"), (LPCTSTR)sSession);
	Out.Replace(_T("[URL_Connect]"), s);
	//} else {
	//	const CString &s(_GetPermissionDenied());
	//	Out.Replace(_T("[URL_Disconnect]"), s);
	//	Out.Replace(_T("[URL_Connect]"), s);
	//}
	Out.Replace(_T("[Disconnect]"), _GetPlainResString(IDS_IRC_DISCONNECT));
	Out.Replace(_T("[Connect]"), _GetPlainResString(IDS_CONNECTTOANYSERVER));
	Out.Replace(_T("[ServerOptions]"), _GetPlainResString(IDS_CONNECTION));
	Out.Replace(_T("[Execute]"), _GetPlainResString(IDS_IRC_PERFORM));

	return Out;
}

CString CWebServer::_GetLog(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));

	CString Out(pThis->m_Templates.sLog);

	if (_ParseURL(Data.sURL, _T("clear")) == _T("yes") && _IsSessionAdmin(Data, sSession))
		theApp.emuledlg->ResetLog();

	Out.Replace(_T("[Clear]"), _GetPlainResString(IDS_PW_RESET));
	Out.Replace(_T("[Log]"), _SpecialChars(theApp.emuledlg->GetAllLogEntries(), false) + _T("<br><a name=\"end\"></a>"));
	Out.Replace(_T("[Session]"), sSession);

	return Out;
}

CString CWebServer::_GetServerInfo(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));

	CString Out = pThis->m_Templates.sServerInfo;

	if (_ParseURL(Data.sURL, _T("clear")) == _T("yes") && _IsSessionAdmin(Data, sSession))
		theApp.emuledlg->ResetServerInfo();

	Out.Replace(_T("[Clear]"), _GetPlainResString(IDS_PW_RESET));
	Out.Replace(_T("[ServerInfo]"), _SpecialChars(theApp.emuledlg->GetServerInfoText(), false) + _T("<br><a name=\"end\"></a>"));
	Out.Replace(_T("[Session]"), sSession);

	return Out;
}

CString CWebServer::_GetDebugLog(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));

	CString Out = pThis->m_Templates.sDebugLog;

	if (_ParseURL(Data.sURL, _T("clear")) == _T("yes") && _IsSessionAdmin(Data, sSession))
		theApp.emuledlg->ResetDebugLog();

	Out.Replace(_T("[Clear]"), _GetPlainResString(IDS_PW_RESET));
	Out.Replace(_T("[DebugLog]"), _SpecialChars(theApp.emuledlg->GetAllDebugLogEntries(), false) + _T("<br><a name=\"end\"></a>"));
	Out.Replace(_T("[Session]"), sSession);

	return Out;
}

CString CWebServer::_GetMyInfo(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	//(void)_ParseURL(Data.sURL, _T("ses"));
	CString Out = pThis->m_Templates.sMyInfoLog;

	Out.Replace(_T("[MYINFOLOG]"), theApp.emuledlg->serverwnd->GetMyInfoString());

	return Out;
}

CString CWebServer::_GetKadDlg(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	/*if (!thePrefs.GetNetworkKademlia()) {
		CString buffer;
		buffer.Format(_T("<br><center>[KADDEACTIVATED]</center>")  );
		return buffer;
	}*/

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));
	CString Out = pThis->m_Templates.sKad;

	if (_IsSessionAdmin(Data, sSession)) {
		if (!_ParseURL(Data.sURL, _T("bootstrap")).IsEmpty()) {
			CString dest;
			dest.Format(_T("%s:%s"), (LPCTSTR)_ParseURL(Data.sURL, _T("ip")), (LPCTSTR)_ParseURL(Data.sURL, _T("port")));
			SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_KAD_BOOTSTRAP, (LPARAM)(LPCTSTR)dest);
		}

		const CString &sAction(_ParseURL(Data.sURL, _T("c")));
		if (sAction == _T("connect"))
			SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_KAD_START, 0);
		else if (sAction == _T("disconnect"))
			SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_KAD_STOP, 0);
		else if (sAction == _T("rcfirewall"))
			SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_KAD_RCFW, 0);
	}
	// check the condition if bootstrap is possible
	Out.Replace(_T("[BOOTSTRAPLINE]"), Kademlia::CKademlia::IsConnected() ? _T("") : (LPCTSTR)pThis->m_Templates.sBootstrapLine);

	// Infos
	CString buffer;
	if (Kademlia::CKademlia::IsConnected())
		if (Kademlia::CKademlia::IsFirewalled()) {
			Out.Replace(_T("[KADSTATUS]"), GetResString(IDS_FIREWALLED));
			buffer.Format(_T("<a href=\"?ses=%s&amp;w=kad&amp;c=rcfirewall\">%s</a>"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_KAD_RECHECKFW));
			buffer.AppendFormat(_T("<br><a href=\"?ses=%s&amp;w=kad&amp;c=disconnect\">%s</a>"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_DISCONNECT));
		} else {
			Out.Replace(_T("[KADSTATUS]"), GetResString(IDS_CONNECTED));
			buffer.Format(_T("<a href=\"?ses=%s&amp;w=kad&amp;c=disconnect\">%s</a>"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_DISCONNECT));
		} else if (Kademlia::CKademlia::IsRunning()) {
			Out.Replace(_T("[KADSTATUS]"), GetResString(IDS_CONNECTING));
			buffer.Format(_T("<a href=\"?ses=%s&amp;w=kad&amp;c=disconnect\">%s</a>"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_DISCONNECT));
		} else {
			Out.Replace(_T("[KADSTATUS]"), GetResString(IDS_DISCONNECTED));
			buffer.Format(_T("<a href=\"?ses=%s&amp;w=kad&amp;c=connect\">%s</a>"), (LPCTSTR)sSession, (LPCTSTR)GetResString(IDS_IRC_CONNECT));
		}

		Out.Replace(_T("[KADACTION]"), buffer);

		// kadstats
		// labels
		buffer.Format(_T("%s<br>%s"), (LPCTSTR)GetResString(IDS_KADCONTACTLAB), (LPCTSTR)GetResString(IDS_KADSEARCHLAB));
		Out.Replace(_T("[KADSTATSLABELS]"), buffer);

		// numbers
		buffer.Format(_T("%u<br>%i"), theApp.emuledlg->kademliawnd->GetContactCount()
			, theApp.emuledlg->kademliawnd->searchList->GetItemCount());
		Out.Replace(_T("[KADSTATSDATA]"), buffer);

		Out.Replace(_T("[BS_IP]"), GetResString(IDS_IP));
		Out.Replace(_T("[BS_PORT]"), GetResString(IDS_PORT));
		Out.Replace(_T("[BOOTSTRAP]"), GetResString(IDS_BOOTSTRAP));
		Out.Replace(_T("[KADSTAT]"), GetResString(IDS_STATSSETUPINFO));
		Out.Replace(_T("[STATUS]"), GetResString(IDS_STATUS));
		Out.Replace(_T("[KAD]"), GetResString(IDS_KADEMLIA));
		Out.Replace(_T("[Session]"), sSession);

		return Out;
}

CString CWebServer::_GetStats(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	//(void)_ParseURL(Data.sURL, _T("ses"));
	// refresh statistics
	SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_SHOWSTATISTICS, 1);

	CString Out = pThis->m_Templates.sStats;
	// eklmn: new stats
	Out.Replace(_T("[Stats]"), theApp.emuledlg->statisticswnd->m_stattree.GetHTMLForExport());

	return Out;
}

CString CWebServer::_GetPreferences(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));

	CString Out = pThis->m_Templates.sPreferences;
	Out.Replace(_T("[Session]"), sSession);

	if ((_ParseURL(Data.sURL, _T("saveprefs")) == _T("true")) && _IsSessionAdmin(Data, sSession)) {
		CString strTmp = _ParseURL(Data.sURL, _T("gzip"));
		if (strTmp == _T("true") || strTmp == _T("on"))
			thePrefs.SetWebUseGzip(true);
		else if (strTmp == _T("false") || strTmp.IsEmpty())
			thePrefs.SetWebUseGzip(false);

		if (!_ParseURL(Data.sURL, _T("refresh")).IsEmpty())
			thePrefs.SetWebPageRefresh(_tstoi(_ParseURL(Data.sURL, _T("refresh"))));

		strTmp = _ParseURL(Data.sURL, _T("maxcapdown"));
		if (!strTmp.IsEmpty())
			thePrefs.SetMaxGraphDownloadRate(_tstoi(strTmp));
		strTmp = _ParseURL(Data.sURL, _T("maxcapup"));
		if (!strTmp.IsEmpty())
			thePrefs.SetMaxGraphUploadRate(_tstoi(strTmp));

		strTmp = _ParseURL(Data.sURL, _T("maxdown"));
		if (!strTmp.IsEmpty()) {
			uint32 dwSpeed = _tstoi(strTmp);
			thePrefs.SetMaxDownload(dwSpeed > 0 ? dwSpeed : UNLIMITED);
		}
		strTmp = _ParseURL(Data.sURL, _T("maxup"));
		if (!strTmp.IsEmpty()) {
			uint32 dwSpeed = _tstoi(strTmp);
			thePrefs.SetMaxUpload(dwSpeed > 0 ? dwSpeed : UNLIMITED);
		}

		if (!_ParseURL(Data.sURL, _T("maxsources")).IsEmpty())
			thePrefs.SetMaxSourcesPerFile(_tstoi(_ParseURL(Data.sURL, _T("maxsources"))));
		if (!_ParseURL(Data.sURL, _T("maxconnections")).IsEmpty())
			thePrefs.SetMaxConnections(_tstoi(_ParseURL(Data.sURL, _T("maxconnections"))));
		if (!_ParseURL(Data.sURL, _T("maxconnectionsperfive")).IsEmpty())
			thePrefs.SetMaxConsPerFive(_tstoi(_ParseURL(Data.sURL, _T("maxconnectionsperfive"))));
	}

	// Fill form
	Out.Replace(_T("[UseGzipVal]"), thePrefs.GetWebUseGzip() ? _T("checked") : _T(""));

	CString sRefresh;

	sRefresh.Format(_T("%d"), thePrefs.GetWebPageRefresh());
	Out.Replace(_T("[RefreshVal]"), sRefresh);

	sRefresh.Format(_T("%u"), thePrefs.GetMaxSourcePerFileDefault());
	Out.Replace(_T("[MaxSourcesVal]"), sRefresh);

	sRefresh.Format(_T("%u"), thePrefs.GetMaxConnections());
	Out.Replace(_T("[MaxConnectionsVal]"), sRefresh);

	sRefresh.Format(_T("%u"), thePrefs.GetMaxConperFive());
	Out.Replace(_T("[MaxConnectionsPer5Val]"), sRefresh);

	Out.Replace(_T("[KBS]"), _GetPlainResString(IDS_KBYTESPERSEC) + _T(':'));
	Out.Replace(_T("[LimitForm]"), _GetPlainResString(IDS_WEB_CONLIMITS) + _T(':'));
	Out.Replace(_T("[MaxSources]"), _GetPlainResString(IDS_PW_MAXSOURCES) + _T(':'));
	Out.Replace(_T("[MaxConnections]"), _GetPlainResString(IDS_PW_MAXC) + _T(':'));
	Out.Replace(_T("[MaxConnectionsPer5]"), _GetPlainResString(IDS_MAXCON5SECLABEL) + _T(':'));
	Out.Replace(_T("[UseGzipForm]"), _GetPlainResString(IDS_WEB_GZIP_COMPRESSION));
	Out.Replace(_T("[UseGzipComment]"), _GetPlainResString(IDS_WEB_GZIP_COMMENT));

	Out.Replace(_T("[RefreshTimeForm]"), _GetPlainResString(IDS_WEB_REFRESH_TIME));
	Out.Replace(_T("[RefreshTimeComment]"), _GetPlainResString(IDS_WEB_REFRESH_COMMENT));
	Out.Replace(_T("[SpeedForm]"), _GetPlainResString(IDS_SPEED_LIMITS));
	Out.Replace(_T("[SpeedCapForm]"), _GetPlainResString(IDS_CAPACITY_LIMITS));

	Out.Replace(_T("[MaxCapDown]"), _GetPlainResString(IDS_PW_CON_DOWNLBL));
	Out.Replace(_T("[MaxCapUp]"), _GetPlainResString(IDS_PW_CON_UPLBL));
	Out.Replace(_T("[MaxDown]"), _GetPlainResString(IDS_PW_CON_DOWNLBL));
	Out.Replace(_T("[MaxUp]"), _GetPlainResString(IDS_PW_CON_UPLBL));
	Out.Replace(_T("[WebControl]"), _GetPlainResString(IDS_WEB_CONTROL));
	Out.Replace(_T("[eMuleAppName]"), _T("eMule"));
	Out.Replace(_T("[Apply]"), _GetPlainResString(IDS_PW_APPLY));

	CString m_sTestURL;
	m_sTestURL.Format(PORTTESTURL, thePrefs.GetPort(), thePrefs.GetUDPPort(), thePrefs.GetLanguageID());

	// the portcheck will need to do an obfuscated callback too if obfuscation is requested, so we have to provide our userhash so it can create the key
	if (thePrefs.IsClientCryptLayerRequested())
		m_sTestURL.AppendFormat(_T("&obfuscated_test=%s"), (LPCTSTR)md4str(thePrefs.GetUserHash()));

	Out.Replace(_T("[CONNECTIONTESTLINK]"), _SpecialChars(m_sTestURL));
	Out.Replace(_T("[CONNECTIONTESTLABEL]"), GetResString(IDS_CONNECTIONTEST));


	CString sT;
	sT.Format(_T("%u"), thePrefs.GetMaxDownload() == UNLIMITED ? 0 : thePrefs.GetMaxDownload());
	Out.Replace(_T("[MaxDownVal]"), sT);

	sT.Format(_T("%u"), thePrefs.GetMaxUpload() == UNLIMITED ? 0 : thePrefs.GetMaxUpload());
	Out.Replace(_T("[MaxUpVal]"), sT);

	sT.Format(_T("%i"), thePrefs.GetMaxGraphDownloadRate());
	Out.Replace(_T("[MaxCapDownVal]"), sT);

	sT.Format(_T("%i"), thePrefs.GetMaxGraphUploadRate(true));
	Out.Replace(_T("[MaxCapUpVal]"), sT);

	return Out;
}

CString CWebServer::_GetLoginScreen(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	//(void)_ParseURL(Data.sURL, _T("ses"));
	CString Out = pThis->m_Templates.sLogin;

	Out.Replace(_T("[CharSet]"), HTTPENCODING);
	Out.Replace(_T("[eMuleAppName]"), _T("eMule"));
	Out.Replace(_T("[version]"), theApp.m_strCurVersionLong);
	Out.Replace(_T("[Login]"), _GetPlainResString(IDS_WEB_LOGIN));
	Out.Replace(_T("[EnterPassword]"), _GetPlainResString(IDS_WEB_ENTER_PASSWORD));
	Out.Replace(_T("[LoginNow]"), _GetPlainResString(IDS_WEB_LOGIN_NOW));
	Out.Replace(_T("[WebControl]"), _GetPlainResString(IDS_WEB_CONTROL));

	CString sFailed;
	if (pThis->m_nIntruderDetect >= 1)
		sFailed.Format(_T("<p class=\"failed\">%s</p>"), (LPCTSTR)_GetPlainResString(IDS_WEB_BADLOGINATTEMPT));
	else
		sFailed = _T("&nbsp;");
	Out.Replace(_T("[FailedLogin]"), sFailed);

	return Out;
}

// We have to add gz-header and some other stuff
// to standard zlib functions
// in order to use gzip in web pages
int CWebServer::_GzipCompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
	static const int gz_magic[2] = {0x1f, 0x8b}; // gzip magic header
	z_stream stream = {};
	stream.zalloc = (alloc_func)NULL;
	stream.zfree = (free_func)NULL;
	stream.opaque = (voidpf)NULL;
	uLong crc = crc32(0, Z_NULL, 0);
	// init Zlib stream
	// NOTE windowBits is passed < 0 to suppress zlib header
	int err = deflateInit2(&stream, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
		return err;

	sprintf((char*)dest, "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1]
		, Z_DEFLATED, 0 /*flags*/, 0, 0, 0, 0 /*time*/, 0 /*xflags*/, 255);
	// wire buffers
	stream.next_in = (Bytef*)source;
	stream.avail_in = (uInt)sourceLen;
	stream.next_out = &dest[10];
	stream.avail_out = *destLen - 18;
	// do it
	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return err;
	}
	err = deflateEnd(&stream);
	crc = crc32(crc, (Bytef*)source, (uInt)sourceLen);
	size_t i = 10 + stream.total_out;
	//CRC
	*(uLong*)&dest[i] = crc;
	// Length
	*(uLong*)&dest[i + sizeof(uLong)] = sourceLen;
	*destLen = 10 + stream.total_out + 8;

	return err;
}

bool CWebServer::_IsLoggedIn(const ThreadData &Data, long lSession)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return false;

	_RemoveTimeOuts(Data);

	// find our session
	// i should have used CMap there, but i like CArray more ;-)
	if (lSession != 0)
		for (INT_PTR i = pThis->m_Params.Sessions.GetCount(); --i >= 0;)
			if (pThis->m_Params.Sessions[i].lSession == lSession) {
				// if found, also reset expiration time
				pThis->m_Params.Sessions[i].startTime = CTime::GetCurrentTime();
				return true;
			}

	return false;
}

void CWebServer::_RemoveTimeOuts(const ThreadData &Data)
{
	// remove expired sessions
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis != NULL)
		pThis->UpdateSessionCount();
}

bool CWebServer::_RemoveSession(const ThreadData &Data, long lSession)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL || lSession == 0)
		return false;

	// find our session
	for (INT_PTR i = pThis->m_Params.Sessions.GetCount(); --i >= 0;)
		if (pThis->m_Params.Sessions[i].lSession == lSession) {
			pThis->m_Params.Sessions.RemoveAt(i);
			AddLogLine(true, (LPCTSTR)GetResString(IDS_WEB_SESSIONEND), (LPCTSTR)ipstr(pThis->m_uCurIP));
			SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATEMYINFO, 0);
			return true;
		}

	return false;
}

Session CWebServer::_GetSessionByID(const ThreadData &Data, long sessionID)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis != NULL && sessionID != 0)
		for (INT_PTR i = pThis->m_Params.Sessions.GetCount(); --i >= 0;)
			if (pThis->m_Params.Sessions[i].lSession == sessionID)
				return pThis->m_Params.Sessions[i];

	return Session{};
}

bool CWebServer::_IsSessionAdmin(const ThreadData &Data, long sessionID)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis != NULL && sessionID != 0)
		for (INT_PTR i = pThis->m_Params.Sessions.GetCount(); --i >= 0;)
			if (pThis->m_Params.Sessions[i].lSession == sessionID)
				return pThis->m_Params.Sessions[i].admin;

	return false;
}

bool CWebServer::_IsSessionAdmin(const ThreadData &Data, const CString &strSsessionID)
{
	return _IsSessionAdmin(Data, _tstol(strSsessionID));
}

CString CWebServer::_GetPermissionDenied()
{
	CString s;
	s.Format(_T("javascript:alert(\'%s\')"), (LPCTSTR)_GetPlainResString(IDS_ACCESSDENIED));
	return s;
}

CString CWebServer::_GetPlainResString(UINT nID, bool noquote)
{
	CString sRet = GetResString(nID);
	sRet.Remove(_T('&'));
	if (noquote) {
		sRet.Replace(_T("'"), _T("&#8217;"));
		sRet.Replace(_T("\n"), _T("\\n"));
	}
	return sRet;
}

void CWebServer::_GetPlainResString(CString &rstrOut, UINT nID, bool noquote)
{
	rstrOut = _GetPlainResString(nID, noquote);
}

// Ornis: creating the progressbar. colored if resources are given/available
CString CWebServer::_GetDownloadGraph(const ThreadData &Data, const CString &filehash)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	uchar fileid[MDX_DIGEST_SIZE];
	if (filehash.GetLength() != MDX_DIGEST_SIZE * 2 || !DecodeBase16(filehash, MDX_DIGEST_SIZE * 2, fileid, _countof(fileid)))
		return CString();

	//	Color style (paused files)
	static LPCTSTR const styles_paused[12] =
	{
		_T("p_green.gif"), _T("p_black.gif"), _T("p_yellow.gif"), _T("p_red.gif"),
		_T("p_blue1.gif"), _T("p_blue2.gif"), _T("p_blue3.gif"), _T("p_blue4.gif"),
		_T("p_blue5.gif"), _T("p_blue6.gif"), _T("p_greenpercent.gif"), _T("transparent.gif")
	};
	//	Color style (active files)
	static LPCTSTR const styles_active[12] =
	{
		_T("green.gif"), _T("black.gif"), _T("yellow.gif"), _T("red.gif"),
		_T("blue1.gif"), _T("blue2.gif"), _T("blue3.gif"), _T("blue4.gif"),
		_T("blue5.gif"), _T("blue6.gif"), _T("greenpercent.gif"), _T("transparent.gif")
	};

	const CPartFile *pPartFile = theApp.downloadqueue->GetFileByID(fileid);
	const LPCTSTR *barcolours = (pPartFile && (pPartFile->GetStatus() == PS_PAUSED)) ? styles_paused : styles_active;

	CString Out;
	if (pPartFile == NULL || !pPartFile->IsPartFile()) {
		Out.Format(pThis->m_Templates.sProgressbarImgsPercent, barcolours[10], pThis->m_Templates.iProgressbarWidth);
		Out += _T("<br>");
		Out.AppendFormat(pThis->m_Templates.sProgressbarImgs, barcolours[0], pThis->m_Templates.iProgressbarWidth);
	} else {
		const CStringA &s_ChunkBar = pPartFile->GetProgressString(pThis->m_Templates.iProgressbarWidth);
		// and now make a graph out of the array - need to be in a progressive way

		int compl = static_cast<int>((pThis->m_Templates.iProgressbarWidth / 100.0) * pPartFile->GetPercentCompleted());
		Out.Format(pThis->m_Templates.sProgressbarImgsPercent, barcolours[compl > 0 ? 10 : 11], (compl > 0 ? compl : 5));
		Out += _T("<br>");

		BYTE lastcolor = 1;
		uint16 lastindex = 0;
		const uint16 uBarWidth = pThis->m_Templates.iProgressbarWidth;
		for (uint16 i = 0; i < uBarWidth; ++i) {
			if (lastcolor != (BYTE)(s_ChunkBar[i] - '0')) {
				if (i > lastindex && lastcolor < _countof(styles_active))
					Out.AppendFormat(pThis->m_Templates.sProgressbarImgs, barcolours[lastcolor], i - lastindex);
				lastcolor = (BYTE)(s_ChunkBar[i] - '0');
				ASSERT(lastcolor >= 0 && lastcolor <= 9);
				lastindex = i;
			}
		}
		Out.AppendFormat(pThis->m_Templates.sProgressbarImgs, barcolours[lastcolor], uBarWidth - lastindex);
	}
	return Out;
}

CString CWebServer::_GetSearch(const ThreadData &Data)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	CString sCat;
	int cat = _tstoi(_ParseURL(Data.sURL, _T("cat")));
	if (cat != 0)
		sCat.Format(_T("%i"), cat);

	const CString &sSession(_ParseURL(Data.sURL, _T("ses")));
	bool bSessionAdmin = _IsSessionAdmin(Data, sSession);
	CString Out = pThis->m_Templates.sSearch;

	if (!_ParseURL(Data.sURL, _T("downloads")).IsEmpty() && bSessionAdmin) {
		const CString &downloads(_ParseURLArray(Data.sURL, _T("downloads")));

		for (int iPos = 0; iPos >= 0;) {
			const CString &resToken(downloads.Tokenize(_T("|"), iPos));
			if (resToken.GetLength() == 32)
				SendMessage(theApp.emuledlg->m_hWnd, WEB_ADDDOWNLOADS, (LPARAM)(LPCTSTR)resToken, cat);
		}
	}

	if (_ParseURL(Data.sURL, _T("c")) == _T("menu")) {
		int iMenu = _tstoi(_ParseURL(Data.sURL, _T("m")));
		bool bValue = _tstoi(_ParseURL(Data.sURL, _T("v"))) != 0;
		WSsearchColumnHidden[iMenu] = bValue;

		_SaveWIConfigArray(WSsearchColumnHidden, _countof(WSsearchColumnHidden), _T("searchColumnHidden"));
	}

	if (!_ParseURL(Data.sURL, _T("tosearch")).IsEmpty() && bSessionAdmin) {

		// perform search
		SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_DELETEALLSEARCHES, 0);

		// get method
		const CString &method((_ParseURL(Data.sURL, _T("method"))));

		SSearchParams *pParams = new SSearchParams;
		pParams->strExpression = _ParseURL(Data.sURL, _T("tosearch"));
		pParams->strFileType = _ParseURL(Data.sURL, _T("type"));
		// for safety: this string is sent to servers and/or kad nodes, validate it!
		if (!pParams->strFileType.IsEmpty()
			&& pParams->strFileType != _T(ED2KFTSTR_ARCHIVE)
			&& pParams->strFileType != _T(ED2KFTSTR_AUDIO)
			&& pParams->strFileType != _T(ED2KFTSTR_CDIMAGE)
			&& pParams->strFileType != _T(ED2KFTSTR_DOCUMENT)
			&& pParams->strFileType != _T(ED2KFTSTR_IMAGE)
			&& pParams->strFileType != _T(ED2KFTSTR_PROGRAM)
			&& pParams->strFileType != _T(ED2KFTSTR_VIDEO)
			&& pParams->strFileType != _T(ED2KFTSTR_EMULECOLLECTION))
		{
			ASSERT(0);
			pParams->strFileType.Empty();
		}
		pParams->ullMinSize = _tstoi64(_ParseURL(Data.sURL, _T("min"))) * 1048576ui64;
		pParams->ullMaxSize = _tstoi64(_ParseURL(Data.sURL, _T("max"))) * 1048576ui64;
		if (pParams->ullMaxSize < pParams->ullMinSize)
			pParams->ullMaxSize = 0;

		const CString &s(_ParseURL(Data.sURL, _T("avail")));
		pParams->uAvailability = s.IsEmpty() ? 0 : _tstoi(s);
		if (pParams->uAvailability > 1000000)
			pParams->uAvailability = 1000000;

		pParams->strExtension = _ParseURL(Data.sURL, _T("ext"));
		if (method == _T("kademlia"))
			pParams->eType = SearchTypeKademlia;
		else if (method == _T("global"))
			pParams->eType = SearchTypeEd2kGlobal;
		else
			pParams->eType = SearchTypeEd2kServer;


		CString strResponse = _GetPlainResString(IDS_SW_SEARCHINGINFO);
		try {
			if (pParams->eType != SearchTypeKademlia) {
				if (!theApp.emuledlg->searchwnd->DoNewEd2kSearch(pParams)) {
					delete pParams;
					pParams = NULL;
					strResponse = _GetPlainResString(IDS_ERR_NOTCONNECTED);
				} else
					::Sleep(SEC2MS(2));	// wait for some results to come in (thanks thread)
			} else if (!theApp.emuledlg->searchwnd->DoNewKadSearch(pParams)) {
				delete pParams;
				pParams = NULL;
				strResponse = _GetPlainResString(IDS_ERR_NOTCONNECTEDKAD);
			}
		} catch (...) {
			strResponse = _GetPlainResString(IDS_ERROR);
			ASSERT(0);
			delete pParams;
		}
		Out.Replace(_T("[Message]"), strResponse);

	} else {
		bool b = (_ParseURL(Data.sURL, _T("tosearch")).IsEmpty() || bSessionAdmin);
		Out.Replace(_T("[Message]"), _GetPlainResString(b ? IDS_SW_REFETCHRES : IDS_ACCESSDENIED));
	}

	CString sSort = _ParseURL(Data.sURL, _T("sort"));
	if (!sSort.IsEmpty())
		pThis->m_iSearchSortby = _tstoi(sSort);
	sSort = _ParseURL(Data.sURL, _T("sortAsc"));
	if (!sSort.IsEmpty())
		pThis->m_bSearchAsc = _tstoi(sSort) != 0;

	CString result = pThis->m_Templates.sSearchHeader;

	CQArray<SearchFileStruct, SearchFileStruct> SearchFileArray;
	theApp.searchlist->GetWebList(&SearchFileArray, pThis->m_iSearchSortby);

	SearchFileArray.QuickSort(pThis->m_bSearchAsc);

	for (INT_PTR i = 0; i < SearchFileArray.GetCount(); ++i) {
		uchar aFileHash[MDX_DIGEST_SIZE];
		SearchFileStruct structFile = SearchFileArray[i];
		DecodeBase16(structFile.m_strFileHash, 32, aFileHash, _countof(aFileHash));

		LPCTSTR strOverlayImage = _T("none");
		uchar nRed, nGreen, nBlue;
		nRed = nGreen = nBlue = 255;
		if (theApp.downloadqueue->GetFileByID(aFileHash) != NULL) {
			nBlue = 128;
			nGreen = 128;
		} else {
			CKnownFile *sameFile = theApp.sharedfiles->GetFileByID(aFileHash);
			if (sameFile == NULL)
				sameFile = theApp.knownfiles->FindKnownFileByID(aFileHash);

			if (sameFile == NULL) {
				//strOverlayImage = _T("none");
			} else {
				//strOverlayImage = _T("release");
				nBlue = 128;
				nRed = 128;
			}
		}

		CString strFmt;
		strFmt.Format(_T("<font color=\"#%02x%02x%02x\">%%s</font>"), nRed, nGreen, nBlue);

		LPCTSTR strSourcesImage;
		if (structFile.m_uSourceCount < 5)
			strSourcesImage = _T("0");
		else if (structFile.m_uSourceCount < 10)
			strSourcesImage = _T("5");
		else if (structFile.m_uSourceCount < 25)
			strSourcesImage = _T("10");
		else if (structFile.m_uSourceCount < 50)
			strSourcesImage = _T("25");
		else
			strSourcesImage = _T("50");

		CString strSources;
		strSources.Format(_T("%u(%u)"), structFile.m_uSourceCount, structFile.m_dwCompleteSourceCount);

		CString strFilename(structFile.m_strFileName);
		strFilename.Replace(_T("'"), _T("\\'"));

		CString strLink;
		strLink.Format(_T("ed2k://|file|%s|%I64u|%s|/"),
			(LPCTSTR)_SpecialChars(strFilename), structFile.m_uFileSize, (LPCTSTR)structFile.m_strFileHash);

		CString s0, s1, s2, s3;
		if (!WSsearchColumnHidden[0])
			s0.Format(strFmt, (LPCTSTR)StringLimit(structFile.m_strFileName, 70));
		if (!WSsearchColumnHidden[1])
			s1.Format(strFmt, (LPCTSTR)CastItoXBytes(structFile.m_uFileSize));
		if (!WSsearchColumnHidden[2])
			s2.Format(strFmt, (LPCTSTR)structFile.m_strFileHash);
		if (!WSsearchColumnHidden[3])
			s3.Format(strFmt, (LPCTSTR)strSources);
		result.AppendFormat(pThis->m_Templates.sSearchResultLine
			, strSourcesImage
			, (LPCTSTR)strLink
			, strOverlayImage
			, (LPCTSTR)_GetWebImageNameForFileType(structFile.m_strFileName)
			, (LPCTSTR)s0 //file name
			, (LPCTSTR)s1 //file size
			, (LPCTSTR)s2 //hash
			, (LPCTSTR)s3 //sources
			, (LPCTSTR)structFile.m_strFileHash
		);
	}

	if (thePrefs.GetCatCount() > 1)
		_InsertCatBox(Out, 0, pThis->m_Templates.sCatArrow, false, false, sSession, _T(""));
	else
		Out.Replace(_T("[CATBOX]"), _T(""));

	Out.Replace(_T("[SEARCHINFOMSG]"), _T(""));
	Out.Replace(_T("[RESULTLIST]"), result);
	Out.Replace(_T("[Result]"), GetResString(IDS_SW_RESULT));
	Out.Replace(_T("[Session]"), sSession);
	Out.Replace(_T("[Name]"), _GetPlainResString(IDS_SW_NAME));
	Out.Replace(_T("[Type]"), _GetPlainResString(IDS_TYPE));
	Out.Replace(_T("[Any]"), _GetPlainResString(IDS_SEARCH_ANY));
	Out.Replace(_T("[Audio]"), _GetPlainResString(IDS_SEARCH_AUDIO));
	Out.Replace(_T("[Image]"), _GetPlainResString(IDS_SEARCH_PICS));
	Out.Replace(_T("[Video]"), _GetPlainResString(IDS_SEARCH_VIDEO));
	Out.Replace(_T("[Document]"), _GetPlainResString(IDS_SEARCH_DOC));
	Out.Replace(_T("[CDImage]"), _GetPlainResString(IDS_SEARCH_CDIMG));
	Out.Replace(_T("[Program]"), _GetPlainResString(IDS_SEARCH_PRG));
	Out.Replace(_T("[Archive]"), _GetPlainResString(IDS_SEARCH_ARC));
	Out.Replace(_T("[eMuleCollection]"), _GetPlainResString(IDS_SEARCH_EMULECOLLECTION));
	Out.Replace(_T("[Search]"), _GetPlainResString(IDS_EM_SEARCH));
//	Out.Replace(_T("[Unicode]"), _GetPlainResString(IDS_SEARCH_UNICODE));
	Out.Replace(_T("[Size]"), _GetPlainResString(IDS_DL_SIZE));
	Out.Replace(_T("[Start]"), _GetPlainResString(IDS_SW_START));

	Out.Replace(_T("[USESSERVER]"), _GetPlainResString(IDS_SERVER));
	Out.Replace(_T("[USEKADEMLIA]"), _GetPlainResString(IDS_KADEMLIA));
	Out.Replace(_T("[METHOD]"), _GetPlainResString(IDS_METHOD));

	Out.Replace(_T("[SizeMin]"), _GetPlainResString(IDS_SEARCHMINSIZE));
	Out.Replace(_T("[SizeMax]"), _GetPlainResString(IDS_SEARCHMAXSIZE));
	Out.Replace(_T("[Availabl]"), _GetPlainResString(IDS_SEARCHAVAIL));
	Out.Replace(_T("[Extention]"), _GetPlainResString(IDS_SEARCHEXTENTION));
	Out.Replace(_T("[Global]"), _GetPlainResString(IDS_GLOBALSEARCH));
	Out.Replace(_T("[MB]"), _GetPlainResString(IDS_MBYTES));
	Out.Replace(_T("[Apply]"), _GetPlainResString(IDS_PW_APPLY));
	Out.Replace(_T("[CatSel]"), sCat);
	Out.Replace(_T("[Ed2klink]"), _GetPlainResString(IDS_SW_LINK));
/*
	CString checked;
	if(thePrefs.GetMethod() == 1)
		checked = _T("checked");
	Out.Replace(_T("[checked]"), checked);
*/
	const TCHAR *pcSortIcon = (pThis->m_bSearchAsc) ? pThis->m_Templates.sUpArrow : pThis->m_Templates.sDownArrow;

	CString strTmp;
	_GetPlainResString(strTmp, IDS_DL_FILENAME);
	Out.Replace(_T("[FilenameI]"), (WSsearchColumnHidden[0] || pThis->m_iSearchSortby != 0) ? _T("") : pcSortIcon);
	Out.Replace(_T("[FilenameH]"), WSsearchColumnHidden[0] ? _T("") : (LPCTSTR)strTmp);
	Out.Replace(_T("[FilenameM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SIZE);
	Out.Replace(_T("[FilesizeI]"), (WSsearchColumnHidden[1] || pThis->m_iSearchSortby != 1) ? _T("") : pcSortIcon);
	Out.Replace(_T("[FilesizeH]"), WSsearchColumnHidden[1] ? _T("") : (LPCTSTR)strTmp);
	Out.Replace(_T("[FilesizeM]"), strTmp);

	_GetPlainResString(strTmp, IDS_FILEHASH);
	Out.Replace(_T("[FilehashI]"), (WSsearchColumnHidden[2] || pThis->m_iSearchSortby != 2) ? _T("") : pcSortIcon);
	Out.Replace(_T("[FilehashH]"), WSsearchColumnHidden[2] ? _T("") : (LPCTSTR)strTmp);
	Out.Replace(_T("[FilehashM]"), strTmp);

	_GetPlainResString(strTmp, IDS_DL_SOURCES);
	Out.Replace(_T("[SourcesI]"), (WSsearchColumnHidden[3] || pThis->m_iSearchSortby != 3) ? _T("") : pcSortIcon);
	Out.Replace(_T("[SourcesH]"), WSsearchColumnHidden[3] ? _T("") : (LPCTSTR)strTmp);
	Out.Replace(_T("[SourcesM]"), strTmp);

	Out.Replace(_T("[Download]"), _GetPlainResString(IDS_DOWNLOAD));

	Out.Replace(_T("[SORTASCVALUE0]"), (pThis->m_iSearchSortby == 0 && pThis->m_bSearchAsc) ? _T("0") : _T("1"));
	Out.Replace(_T("[SORTASCVALUE1]"), (pThis->m_iSearchSortby == 1 && pThis->m_bSearchAsc) ? _T("0") : _T("1"));
	Out.Replace(_T("[SORTASCVALUE2]"), (pThis->m_iSearchSortby == 2 && pThis->m_bSearchAsc) ? _T("0") : _T("1"));
	Out.Replace(_T("[SORTASCVALUE3]"), (pThis->m_iSearchSortby == 3 && pThis->m_bSearchAsc) ? _T("0") : _T("1"));
	Out.Replace(_T("[SORTASCVALUE4]"), (pThis->m_iSearchSortby == 4 && pThis->m_bSearchAsc) ? _T("0") : _T("1"));
	Out.Replace(_T("[SORTASCVALUE5]"), (pThis->m_iSearchSortby == 5 && pThis->m_bSearchAsc) ? _T("0") : _T("1"));

	return Out;
}

INT_PTR CWebServer::UpdateSessionCount()
{
	if (thePrefs.GetWebTimeoutMins() > 0) {
		INT_PTR oldvalue = m_Params.Sessions.GetCount();
		CTime curTime(CTime::GetCurrentTime());
		for (INT_PTR i = oldvalue; --i >= 0;) {
			CTimeSpan ts = curTime - m_Params.Sessions[i].startTime;
			if (ts.GetTotalSeconds() >= MIN2S(thePrefs.GetWebTimeoutMins()))
				m_Params.Sessions.RemoveAt(i);
		}

		if (oldvalue != m_Params.Sessions.GetCount())
			SendMessage(theApp.emuledlg->m_hWnd, WEB_GUI_INTERACTION, WEBGUIIA_UPDATEMYINFO, 0);
	}
	return m_Params.Sessions.GetCount();
}

void CWebServer::_InsertCatBox(CString &Out, int preselect, const CString &boxlabel, bool jump, bool extraCats, const CString &sSession, const CString &sFileHash, bool ed2kbox)
{
	CString tempBuf;
	tempBuf.Format(_T("<form action=\"\">%s<select name=\"cat\" size=\"1\"%s>")
		, (LPCTSTR)boxlabel
		, jump ? _T(" onchange=\"GotoCat(this.form.cat.options[this.form.cat.selectedIndex].value)\"") : _T(""));

	for (int i = 0; i < thePrefs.GetCatCount(); ++i) {
		CString strCategory = thePrefs.GetCategory(i)->strTitle;
		strCategory.Replace(_T("'"), _T("\\'"));
		tempBuf.AppendFormat(_T("<option%s value=\"%i\">%s</option>\n"), (i == preselect) ? _T(" selected") : _T(""), i, (LPCTSTR)strCategory);
	}
	if (extraCats) {
		if (thePrefs.GetCatCount() > 1)
			tempBuf += _T("<option>-------------------</option>\n");

		for (int i = 1; i < 16; ++i)
			tempBuf.AppendFormat(_T("<option%s value=\"%i\">%s</option>\n"), (0 - i == preselect) ? _T(" selected") : _T(""), 0 - i, (LPCTSTR)_GetSubCatLabel(0 - i));
	}
	tempBuf += _T("</select></form>");
	Out.Replace(ed2kbox ? _T("[CATBOXED2K]") : _T("[CATBOX]"), tempBuf);

	LPCTSTR tempBuff3;
	CString tempBuff4;
	CString tempBuff;

	for (int i = 0; i < thePrefs.GetCatCount(); ++i) {
		if (i == preselect) {
			tempBuff3 = _T("checked.gif");
			tempBuff4 = (i ? thePrefs.GetCategory(i)->strTitle : GetResString(IDS_ALL));
		} else
			tempBuff3 = _T("checked_no.gif");

		CString strCategory(i ? thePrefs.GetCategory(i)->strTitle : GetResString(IDS_ALL));
		strCategory.Replace(_T("'"), _T("\\'"));

		tempBuff.AppendFormat(_T("<a href=&quot;/?ses=%s&amp;w=transfer&amp;cat=%d&quot;><div class=menuitems><img class=menuchecked src=%s>%s&nbsp;</div></a>")
			, (LPCTSTR)sSession, i, tempBuff3, (LPCTSTR)strCategory);
	}
	if (extraCats) {
		tempBuff += _T("<div class=menuitems>&nbsp;------------------------------&nbsp;</div>");
		for (int i = 1; i < 16; ++i) {
			if ((0 - i) == preselect) {
				tempBuff3 = _T("checked.gif");
				tempBuff4 = _GetSubCatLabel(0 - i);
			} else
				tempBuff3 = _T("checked_no.gif");

			tempBuff.AppendFormat(_T("<a href=&quot;/?ses=%s&amp;w=transfer&amp;cat=%d&quot;><div class=menuitems><img class=menuchecked src=%s>%s&nbsp;</div></a>")
				, (LPCTSTR)sSession, 0 - i, tempBuff3, (LPCTSTR)_GetSubCatLabel(0 - i));
		}
	}
	Out.Replace(_T("[CatBox]"), tempBuff);
	tempBuff4.Replace(_T("'"), _T("\\'"));
	Out.Replace(_T("[Category]"), tempBuff4);

	if (!sFileHash.IsEmpty()) {
		uchar FileHash[MDX_DIGEST_SIZE];
		if (strmd4(sFileHash, FileHash)) {
			CPartFile *found_file = theApp.downloadqueue->GetFileByID(FileHash);
			//	Get the user category index of 'found_file' into 'preselect'.
			if (found_file)
				preselect = found_file->GetCategory();
		}
	}
	tempBuff.Empty();
	//	For each user category index...
	for (int i = 0; i < thePrefs.GetCatCount(); ++i) {
		CString strCategory(i ? thePrefs.GetCategory(i)->strTitle : GetResString(IDS_CAT_UNASSIGN));
		strCategory.Replace(_T("'"), _T("\\'"));

		tempBuff3 = (i == preselect) ? _T("checked.gif") : _T("checked_no.gif");
		tempBuff.AppendFormat(_T("<a href=&quot;/?ses=%s&amp;w=transfer[CatSel]&amp;op=setcat&amp;file=%s&amp;filecat=%d&quot;><div class=menuitems><img class=menuchecked src=%s>%s&nbsp;</div></a>")
			, (LPCTSTR)sSession, (LPCTSTR)sFileHash, i, (LPCTSTR)tempBuff3, (LPCTSTR)strCategory);
	}

	Out.Replace(_T("[SetCatBox]"), tempBuff);
}

CString CWebServer::_GetSubCatLabel(int cat)
{
	if (cat >= 0 || cat < -16)
		return CString(_T('?'));

	static const UINT ids[16] =
	{
		IDS_ALLOTHERS, IDS_STATUS_NOTCOMPLETED, IDS_DL_TRANSFCOMPL, IDS_WAITING
		, IDS_DOWNLOADING, IDS_ERRORLIKE, IDS_PAUSED, IDS_SEENCOMPL
		, IDS_VIDEO, IDS_AUDIO, IDS_SEARCH_ARC, IDS_SEARCH_CDIMG
		, IDS_SEARCH_DOC, IDS_SEARCH_PICS, IDS_SEARCH_PRG, IDS_SEARCH_EMULECOLLECTION
	};
	return _GetPlainResString(ids[-cat - 1]);
}

CString CWebServer::_GetRemoteLinkAddedOk(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	int cat = _tstoi(_ParseURL(Data.sURL, _T("cat")));
	const CString &HTTPTemp(_ParseURL(Data.sURL, _T("c")));
	theApp.emuledlg->SendMessage(WEB_ADDDOWNLOADS, (WPARAM)(LPCTSTR)HTTPTemp, cat);

	CString Out;
	Out.Format(_T("<status result=\"OK\"><description>%s</description>"), (LPCTSTR)GetResString(IDS_WEB_REMOTE_LINK_ADDED));
	Out.AppendFormat(_T("<filename>%s</filename></status>"), (LPCTSTR)HTTPTemp);
	return Out;
}

CString CWebServer::_GetRemoteLinkAddedFailed(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return CString();

	CString Out(_T("<status result=\"FAILED\" reason=\"WRONG_PASSWORD\">"));
	Out.AppendFormat(_T("<description>%s</description></status>"), (LPCTSTR)GetResString(IDS_WEB_REMOTE_LINK_NOT_ADDED));

	return Out;
}

void CWebServer::_SetLastUserCat(const ThreadData &Data, long lSession, int cat)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return;

	_RemoveTimeOuts(Data);

	// find our session
	if (lSession != 0)
		for (INT_PTR i = pThis->m_Params.Sessions.GetCount(); --i >= 0;) {
			Session &ses = pThis->m_Params.Sessions[i];
			if (ses.lSession == lSession) {
				// if found, also reset expiration time
				ses.startTime = CTime::GetCurrentTime();
				ses.lastcat = cat;
				break;
			}
		}
}

int CWebServer::_GetLastUserCat(const ThreadData &Data, long lSession)
{
	CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return 0;

	_RemoveTimeOuts(Data);

	if (lSession != 0)
		// find our session
		for (INT_PTR i = pThis->m_Params.Sessions.GetCount(); --i >= 0;) {
			Session &ses = pThis->m_Params.Sessions[i];
			if (ses.lSession == lSession) {
				// if found, also reset expiration time
				ses.startTime = CTime::GetCurrentTime();
				return ses.lastcat;
			}
		}

	return 0;
}

void CWebServer::_ProcessFileReq(const ThreadData &Data)
{
	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	if (pThis == NULL)
		return;
	CString filename(Data.sURL);
	CString contenttype;

	CString ext(filename.Right(5).MakeLower());
	int i = ext.ReverseFind(_T('.'));
	ext.Delete(0, i);
	if (i >= 0 && ext.GetLength() > 2) {
		ext.Delete(0, 1);
		if (ext == _T("bmp") || ext == _T("gif") || ext == _T("jpeg") || ext == _T("jpg") || ext == _T("png"))
			contenttype.Format(_T("Content-Type: image/%s\r\n"), (LPCTSTR)ext);
		//DonQ - additional file types
		else if (ext == _T("ico"))
			contenttype = _T("Content-Type: image/x-icon\r\n");
		else if (ext == _T("css"))
			contenttype = _T("Content-Type: text/css\r\n");
		else if (ext == _T("js"))
			contenttype = _T("Content-Type: text/javascript\r\n");
	}

	contenttype.AppendFormat(_T("Last-Modified: %s\r\nETag: %s\r\n"), (LPCTSTR)pThis->m_Params.sLastModified, (LPCTSTR)pThis->m_Params.sETag);

	filename.Replace(_T('/'), _T('\\'));
	if (filename[0] == _T('\\'))
		filename.Delete(0, 1);
	filename.Insert(0, thePrefs.GetMuleDirectory(EMULE_WEBSERVERDIR));

	CFile file;
	if (file.Open(filename, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary)) {
		if (thePrefs.GetMaxWebUploadFileSizeMB() == 0 || file.GetLength() <= thePrefs.GetMaxWebUploadFileSizeMB() * 1024ull * 1024ull) {
			UINT filesize = (UINT)file.GetLength();

			char *buffer = new char[filesize];
			UINT size = file.Read(buffer, filesize);
			file.Close();
			Data.pSocket->SendContent((CStringA)contenttype, buffer, size);
			delete[] buffer;
		} else
			Data.pSocket->SendReply("HTTP/1.1 403 Forbidden\r\n");
	} else
		Data.pSocket->SendReply("HTTP/1.1 404 File not found\r\n");
}

CString CWebServer::_GetWebImageNameForFileType(const CString &filename)
{
	LPCTSTR p;
	switch (GetED2KFileTypeID(filename)) {
	case ED2KFT_AUDIO:
		p = _T("audio");
		break;
	case ED2KFT_VIDEO:
		p = _T("video");
		break;
	case ED2KFT_IMAGE:
		p = _T("picture");
		break;
	case ED2KFT_PROGRAM:
		p = _T("program");
		break;
	case ED2KFT_DOCUMENT:
		p = _T("document");
		break;
	case ED2KFT_ARCHIVE:
		p = _T("archive");
		break;
	case ED2KFT_CDIMAGE:
		p = _T("cdimage");
		break;
	case ED2KFT_EMULECOLLECTION:
		p = _T("emulecollection");
		break;
	default: /*ED2KFT_ANY:*/
		p = _T("other");
	}
	return CString(p);
}

CString CWebServer::_GetClientSummary(const CUpDownClient &client)
{
	CString buffer;
	// name
	buffer.Format(_T("%s %s\n"), (LPCTSTR)GetResString(IDS_CD_UNAME), client.GetUserName());
	// client version
	buffer.AppendFormat(_T("%s: %s\n"), (LPCTSTR)GetResString(IDS_CD_CSOFT), (LPCTSTR)client.GetClientSoftVer());

	// uploading file
	buffer.AppendFormat(_T("%s "), (LPCTSTR)GetResString(IDS_CD_UPLOADREQ));
	const CKnownFile *file = theApp.sharedfiles->GetFileByID(client.GetUploadFileID());
	ASSERT(file);
	if (file)
		buffer += file->GetFileName();

	// transfering time
	buffer.AppendFormat(_T("\n\n%s: %s\n"), (LPCTSTR)GetResString(IDS_UPLOADTIME), (LPCTSTR)CastSecondsToHM(client.GetUpStartTimeDelay() / SEC2MS(1)));

	// transferred data (up, down, global, session)
	buffer.AppendFormat(_T("%s (%s):\n"), (LPCTSTR)GetResString(IDS_FD_TRANS), (LPCTSTR)GetResString(IDS_STATS_SESSION));
	buffer.AppendFormat(_T(".....%s: %s (%s )\n"), (LPCTSTR)GetResString(IDS_PW_CON_UPLBL), (LPCTSTR)CastItoXBytes(client.GetTransferredUp()), (LPCTSTR)CastItoXBytes(client.GetSessionUp()));
	buffer.AppendFormat(_T(".....%s: %s (%s )\n"), (LPCTSTR)GetResString(IDS_DOWNLOAD), (LPCTSTR)CastItoXBytes(client.GetTransferredDown()), (LPCTSTR)CastItoXBytes(client.GetSessionDown()));

	return buffer;
}

CString CWebServer::_GetClientversionImage(const CUpDownClient &client)
{
	TCHAR c;
	switch (client.GetClientSoft()) {
	case SO_EMULE:
	case SO_OLDEMULE:
		c = _T('1');
		break;
	case SO_EDONKEYHYBRID:
		c = _T('h');
		break;
	case SO_AMULE:
		c = _T('a');
		break;
	case SO_SHAREAZA:
		c = _T('s');
		break;
	case SO_MLDONKEY:
		c = _T('m');
		break;
	case SO_LPHANT:
		c = _T('l');
		break;
	case SO_URL:
		c = _T('u');
		break;
	default: //SO_EDONKEY
		c = _T('0');
	}
	return CString(c);
}

CString CWebServer::_GetCommentlist(const ThreadData &Data)
{
	uchar FileHash[MDX_DIGEST_SIZE];
	bool bHash = strmd4(_ParseURL(Data.sURL, _T("filehash")), FileHash);
	const CPartFile *pPartFile = bHash ? theApp.downloadqueue->GetFileByID(FileHash) : NULL;
	if (!pPartFile)
		return CString();

	const CWebServer *pThis = reinterpret_cast<CWebServer*>(Data.pThis);
	CString Out = pThis->m_Templates.sCommentList;

	CString comments;
	comments.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_COMMENT), (LPCTSTR)pPartFile->GetFileName());
	Out.Replace(_T("[COMMENTS]"), comments);

	CString commentlines;
	// prepare commentsinfo-string
	for (POSITION pos = pPartFile->srclist.GetHeadPosition(); pos != NULL;) {
		const CUpDownClient *cur_src = pPartFile->srclist.GetNext(pos);
		if (cur_src->HasFileRating() || !cur_src->GetFileComment().IsEmpty())
			commentlines.AppendFormat(pThis->m_Templates.sCommentListLine
				, (LPCTSTR)_SpecialChars(cur_src->GetUserName())
				, (LPCTSTR)_SpecialChars(cur_src->GetClientFilename())
				, (LPCTSTR)_SpecialChars(cur_src->GetFileComment())
				, (LPCTSTR)_SpecialChars(GetRateString(cur_src->GetFileRating())));
	}

	const CTypedPtrList<CPtrList, Kademlia::CEntry*> &list = pPartFile->getNotes();
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;) {
		const Kademlia::CEntry *entry = list.GetNext(pos);
		commentlines.AppendFormat(pThis->m_Templates.sCommentListLine
			, _T("")
			, (LPCTSTR)_SpecialChars(entry->GetCommonFileName())
			, (LPCTSTR)_SpecialChars(entry->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION)))
			, (LPCTSTR)_SpecialChars(GetRateString((UINT)entry->GetIntTagValue(Kademlia::CKadTagNameString(TAG_FILERATING)))));
	}

	Out.Replace(_T("[COMMENTLINES]"), commentlines);

	Out.Replace(_T("[COMMENTS]"), _T(""));
	Out.Replace(_T("[USERNAME]"), GetResString(IDS_QL_USERNAME));
	Out.Replace(_T("[FILENAME]"), GetResString(IDS_DL_FILENAME));
	Out.Replace(_T("[COMMENT]"), GetResString(IDS_COMMENT));
	Out.Replace(_T("[RATING]"), GetResString(IDS_QL_RATING));
	Out.Replace(_T("[CLOSE]"), GetResString(IDS_CW_CLOSE));
	Out.Replace(_T("[CharSet]"), HTTPENCODING);

	return Out;
}

int AFX_CDECL CWebServer::_DownloadCmp(void *prm, void const *pv1, void const *pv2)
{
	const DownloadFiles &p1 = *reinterpret_cast<const DownloadFiles*>(pv1);
	const DownloadFiles &p2 = *reinterpret_cast<const DownloadFiles*>(pv2);
	int iOrd;
	switch ((DownloadSort)((SortParams*)prm)->eSort) {
	case DOWN_SORT_STATE:
		iOrd = p1.iFileState - p2.iFileState;
		break;
	case DOWN_SORT_TYPE:
		iOrd = p1.sFileType.CompareNoCase(p2.sFileType);
		break;
	case DOWN_SORT_NAME:
		iOrd = p1.sFileName.CompareNoCase(p2.sFileName);
		break;
	case DOWN_SORT_SIZE:
		iOrd = CompareUnsigned64(p1.m_qwFileSize, p2.m_qwFileSize);
		break;
	case DOWN_SORT_TRANSFERRED:
		iOrd = CompareUnsigned64(p1.m_qwFileTransferred, p2.m_qwFileTransferred);
		break;
	case DOWN_SORT_SPEED:
		iOrd = p1.lFileSpeed - p2.lFileSpeed;
		break;
	case DOWN_SORT_PROGRESS:
		iOrd = sgn(p1.m_dblCompleted - p2.m_dblCompleted);
		break;
	case DOWN_SORT_SOURCES:
		iOrd = p1.lSourceCount - p2.lSourceCount;
		break;
	case DOWN_SORT_PRIORITY:
		iOrd = p1.nFilePrio - p2.nFilePrio;
		break;
	case DOWN_SORT_CATEGORY:
		iOrd = p1.sCategory.CompareNoCase(p2.sCategory);
		break;
	default: //unknown
		return 0;
	}
	return ((SortParams*)prm)->bReverse ? iOrd : -iOrd;
}

int AFX_CDECL CWebServer::_ServerCmp(void *prm, void const *pv1, void const *pv2)
{
	const ServerEntry &p1 = *reinterpret_cast<const ServerEntry*>(pv1);
	const ServerEntry &p2 = *reinterpret_cast<const ServerEntry*>(pv2);
	int iOrd;
	switch ((ServerSort)((SortParams*)prm)->eSort) {
	case SERVER_SORT_STATE:
		iOrd = p2.sServerState.CompareNoCase(p1.sServerState); //reversed
		break;
	case SERVER_SORT_NAME:
		iOrd = p1.sServerName.CompareNoCase(p2.sServerName);
		break;
	case SERVER_SORT_IP:
		iOrd = p1.sServerFullIP.Compare(p2.sServerFullIP);
		if (!iOrd)
			iOrd = p1.nServerPort - p2.nServerPort;
		break;
	case SERVER_SORT_DESCRIPTION:
		iOrd = p1.sServerDescription.CompareNoCase(p2.sServerDescription);
		break;
	case SERVER_SORT_PING:
		iOrd = p1.nServerPing - p2.nServerPing;
		break;
	case SERVER_SORT_USERS:
		iOrd = p1.nServerUsers - p2.nServerUsers;
		break;
	case SERVER_SORT_FILES:
		iOrd = p1.nServerFiles - p2.nServerFiles;
		break;
	case SERVER_SORT_PRIORITY:
		iOrd = p1.nServerPriority - p2.nServerPriority;
		break;
	case SERVER_SORT_FAILED:
		iOrd = p1.nServerFailed - p2.nServerFailed;
		break;
	case SERVER_SORT_LIMIT:
		iOrd = p1.nServerSoftLimit - p2.nServerSoftLimit;
		break;
	case SERVER_SORT_VERSION:
		iOrd = p1.sServerVersion.Compare(p2.sServerVersion);
		break;
	default: //unknown
		return 0;
	}
	return ((SortParams*)prm)->bReverse ? iOrd : -iOrd;
}

int AFX_CDECL CWebServer::_SharedCmp(void *prm, void const *pv1, void const *pv2)
{
	const SharedFiles &p1 = *reinterpret_cast<const SharedFiles*>(pv1);
	const SharedFiles &p2 = *reinterpret_cast<const SharedFiles*>(pv2);
	int iOrd;
	switch ((SharedSort)((SortParams*)prm)->eSort) {
	case SHARED_SORT_STATE:
		iOrd = p2.sFileState.CompareNoCase(p1.sFileState); //reversed
		break;
	case SHARED_SORT_TYPE:
		iOrd = p2.sFileType.CompareNoCase(p1.sFileType); //reversed
		break;
	case SHARED_SORT_NAME:
		iOrd = p1.sFileName.CompareNoCase(p2.sFileName);
		break;
	case SHARED_SORT_SIZE:
		iOrd = CompareUnsigned64(p1.m_qwFileSize, p2.m_qwFileSize);
		break;
	case SHARED_SORT_TRANSFERRED:
		iOrd = CompareUnsigned64(p1.nFileTransferred, p2.nFileTransferred);
		break;
	case SHARED_SORT_ALL_TIME_TRANSFERRED:
		iOrd = CompareUnsigned64(p1.nFileAllTimeTransferred, p2.nFileAllTimeTransferred);
		break;
	case SHARED_SORT_REQUESTS:
		iOrd = p1.nFileRequests - p2.nFileRequests;
		break;
	case SHARED_SORT_ALL_TIME_REQUESTS:
		iOrd = p1.nFileAllTimeRequests - p2.nFileAllTimeRequests;
		break;
	case SHARED_SORT_ACCEPTS:
		iOrd = p1.nFileAccepts - p2.nFileAccepts;
		break;
	case SHARED_SORT_ALL_TIME_ACCEPTS:
		iOrd = p1.nFileAllTimeAccepts - p2.nFileAllTimeAccepts;
		break;
	case SHARED_SORT_COMPLETES:
		iOrd = sgn(p1.dblFileCompletes - p2.dblFileCompletes);
		break;
	case SHARED_SORT_PRIORITY:
		iOrd = CSharedFileList::GetRealPrio(p1.nFilePriority) - CSharedFileList::GetRealPrio(p2.nFilePriority);
		break;
	default: //unknown
		return 0;
	}
	return ((SortParams*)prm)->bReverse ? iOrd : -iOrd;
}

int AFX_CDECL CWebServer::_UploadCmp(void *prm, void const *pv1, void const *pv2)
{
	const UploadUsers &p1 = *reinterpret_cast<const UploadUsers*>(pv1);
	const UploadUsers &p2 = *reinterpret_cast<const UploadUsers*>(pv2);
	int iOrd;
	switch ((UploadSort)((SortParams*)prm)->eSort) {
	case UP_SORT_CLIENT:
		iOrd = p1.sClientSoft.CompareNoCase(p2.sClientSoft);
		break;
	case UP_SORT_USER:
		iOrd = p1.sUserName.CompareNoCase(p2.sUserName);
		break;
	case UP_SORT_VERSION:
		iOrd = p1.sClientNameVersion.CompareNoCase(p2.sClientNameVersion);
		break;
	case UP_SORT_FILENAME:
		iOrd = p1.sFileName.CompareNoCase(p2.sFileName);
		break;
	case UP_SORT_TRANSFERRED:
		iOrd = p1.nTransferredUp - p2.nTransferredUp;
		break;
	case UP_SORT_SPEED:
		iOrd = p1.nDataRate - p2.nDataRate;
		break;
	default: //unknown
		return 0;
	}
	return ((SortParams*)prm)->bReverse ? iOrd : -iOrd;
}