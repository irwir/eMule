#pragma once

#include "WebSocket.h"
#include "PartFile.h"
#include "zlib/zlib.h"

#define WEB_GRAPH_HEIGHT		120
#define WEB_GRAPH_WIDTH			500

#define SESSION_TIMEOUT_SECS	300	// 5 minutes session expiration
#define SHORT_LENGTH_MAX		60	// Max size for strings maximum
#define SHORT_LENGTH			40	// Max size for strings
#define SHORT_LENGTH_MIN		30	// Max size for strings minimum

typedef struct
{
	double download;
	double upload;
	long connections;
} UpDown;

typedef struct
{
	CTime	startTime;
	long	lSession;
	int		lastcat;
	bool	admin;
} Session;

struct BadLogin
{
	uint32	datalen;
	DWORD	timestamp;
};

typedef struct
{
	CString	sCategory;
	CString	sED2kLink;
	CString	sFileHash;
	CString	sFileInfo;
	CString	sFileName;
	CString	sFileNameJS;
	CString	sFileState;
	CString	sFileType;
	double	m_dblCompleted;
	uint64	m_qwFileSize;
	uint64	m_qwFileTransferred;
	uint32	lFileSpeed;
	long	lNotCurrentSourceCount;
	long	lSourceCount;
	long	lTransferringSourceCount;
	bool	bFileAutoPrio;
	int		iComment;
	int		iFileState;
	int		nFilePrio;
	int		nFileStatus;
	bool	bIsComplete;
	bool	bIsGetFLC;
	bool	bIsPreview;
} DownloadFiles;

typedef struct
{
	CString sFileCompletes;
	CString sFileHash;
	CString sFilePriority;
	CString sFileType;
	CString	sED2kLink;
	CString	sFileName;
	CString	sFileState;
	double	dblFileCompletes;
	uint64	m_qwFileSize;
	uint64	nFileAllTimeTransferred;
	uint64	nFileTransferred;
	uint32	nFileAllTimeRequests;
	uint32	nFileAllTimeAccepts;
	UINT	nFileRequests;
	UINT	nFileAccepts;
	byte	nFilePriority;
	bool	bIsPartFile;
	bool	bFileAutoPriority;
} SharedFiles;

typedef struct
{
	CString	sClientState;
	CString	sUserHash;
	CString	sActive;
	CString sFileInfo;
	CString sClientSoft;
	CString	sClientExtra;
	CString	sUserName;
	CString	sFileName;
	CString	sClientNameVersion;
	uint32	nTransferredDown;
	uint32	nTransferredUp;
	int		nDataRate;
} UploadUsers;

typedef struct
{
	CString	sClientExtra;
	CString	sClientNameVersion;
	CString	sClientSoft;
	CString	sClientSoftSpecial;
	CString	sClientState;
	CString	sClientStateSpecial;
	CString	sFileName;
	CString	sIndex;	//SyruS CQArray-Sorting element
	CString	sUserHash;
	CString	sUserName;
	uint32	nScore;
} QueueUsers;

struct SortParams
{
	int eSort;
	bool bReverse;
};

typedef enum
{
	  DOWN_SORT_STATE
	, DOWN_SORT_TYPE
	, DOWN_SORT_NAME
	, DOWN_SORT_SIZE
	, DOWN_SORT_TRANSFERRED
	, DOWN_SORT_SPEED
	, DOWN_SORT_PROGRESS
	, DOWN_SORT_SOURCES
	, DOWN_SORT_PRIORITY
	, DOWN_SORT_CATEGORY
//	, DOWN_SORT_FAKECHECK unused
} DownloadSort;

typedef enum
{
	UP_SORT_CLIENT,
	UP_SORT_USER,
	UP_SORT_VERSION,
	UP_SORT_FILENAME,
	UP_SORT_TRANSFERRED,
	UP_SORT_SPEED
} UploadSort;

typedef enum
{
	QU_SORT_CLIENT,
	QU_SORT_USER,
	QU_SORT_VERSION,
	QU_SORT_FILENAME,
	QU_SORT_SCORE
} QueueSort;

typedef enum
{
	SHARED_SORT_STATE,
	SHARED_SORT_TYPE,
	SHARED_SORT_NAME,
	SHARED_SORT_SIZE,
	SHARED_SORT_TRANSFERRED,
	SHARED_SORT_ALL_TIME_TRANSFERRED,
	SHARED_SORT_REQUESTS,
	SHARED_SORT_ALL_TIME_REQUESTS,
	SHARED_SORT_ACCEPTS,
	SHARED_SORT_ALL_TIME_ACCEPTS,
	SHARED_SORT_COMPLETES,
	SHARED_SORT_PRIORITY
} SharedSort;

typedef struct
{
	CString	sServerDescription;
	CString	sServerFullIP; //for sorting
	CString	sServerIP;
	CString	sServerName;
	CString	sServerPriority;
	CString	sServerState;
	CString	sServerVersion;
	uint32	nServerFiles;
	uint32	nServerHardLimit;
	uint32	nServerMaxUsers;
	uint32	nServerPing;
	uint32	nServerSoftLimit;
	uint32	nServerUsers;
	int		nServerFailed;
	int		nServerPort;
	byte	nServerPriority;
	bool	bServerStatic;
} ServerEntry;

typedef enum
{
	SERVER_SORT_STATE,
	SERVER_SORT_NAME,
	SERVER_SORT_IP,
	SERVER_SORT_DESCRIPTION,
	SERVER_SORT_PING,
	SERVER_SORT_USERS,
	SERVER_SORT_FILES,
	SERVER_SORT_PRIORITY,
	SERVER_SORT_FAILED,
	SERVER_SORT_LIMIT,
	SERVER_SORT_VERSION
} ServerSort;

typedef struct
{
	CArray<UpDown>	PointsForWeb;
	CArray<Session>	Sessions;
	CArray<BadLogin> badlogins;	//TransferredData= IP : time

	CString			sETag;
	CString			sLastModified;
	CString			sShowServerIP;		//Purity: Action Buttons
	CString			sShowSharedFile;	//Purity: Action Buttons
	CString			sShowTransferFile;	//Purity: Action Buttons

	uint32			nUsers;
	DownloadSort	DownloadSort;
	QueueSort		QueueSort;
	ServerSort		ServerSort;
	SharedSort		SharedSort;
	UploadSort		UploadSort;
	bool			bDownloadSortReverse;
	bool			bQueueSortReverse;
	bool			bServerSortReverse;
	bool			bSharedSortReverse;
	bool			bShowServerLine;	//Purity: Action Buttons
	bool			bShowSharedLine;	//Purity: Action Buttons
	bool			bShowTransferLine;	//Purity: Action Buttons
	bool			bShowUploadQueue;
	bool			bShowUploadQueueBanned;
	bool			bShowUploadQueueFriend;
	bool			bUploadSortReverse;
} GlobalParams;

typedef struct
{
	CString			sURL;
	void			*pThis;
	CWebSocket		*pSocket;
	in_addr			inadr;
} ThreadData;

typedef struct
{
	CString	sAddServerBox;
	CString	sBootstrapLine;
	CString	sCatArrow;
	CString	sCommentList;
	CString	sCommentListLine;
	CString	sDebugLog;
	CString	sDownArrow;
	CString	sDownDoubleArrow;
	CString	sFooter;
	CString	sGraphs;
	CString	sHeader;
	CString	sHeaderStylesheet;
	CString	sKad;
	CString	sLog;
	CString	sLogin;
	CString	sMyInfoLog;
	CString	sPreferences;
	CString	sProgressbarImgs;
	CString	sProgressbarImgsPercent;
	CString	sSearch;
	CString	sSearchHeader;
	CString	sSearchResultLine;
	CString	sServerInfo;
	CString	sServerLine;
	CString	sServerList;
	CString	sSharedLine;
	CString	sSharedList;
	CString	sStats;
	CString	sTransferDownFooter;
	CString	sTransferDownHeader;
	CString	sTransferDownLine;
	CString	sTransferImages;
	CString	sTransferList;
	CString	sTransferUpFooter;
	CString	sTransferUpHeader;
	CString	sTransferUpLine;
	CString	sTransferUpQueueBannedHide;
	CString	sTransferUpQueueBannedLine;
	CString	sTransferUpQueueBannedShow;
	CString	sTransferUpQueueFriendHide;
	CString	sTransferUpQueueFriendLine;
	CString	sTransferUpQueueFriendShow;
	CString	sTransferUpQueueHide;
	CString	sTransferUpQueueLine;
	CString	sTransferUpQueueShow;
	CString	sUpArrow;
	CString	sUpDoubleArrow;
	uint16	iProgressbarWidth;
} WebTemplates;

class CWebServer
{
	friend class CWebSocket;

public:
	CWebServer();
	~CWebServer();

	inline void SetIP(u_long ip)				{ m_uCurIP = ip; } //practically not used
	INT_PTR UpdateSessionCount();
	void StartServer();
	void StopServer();
	void RestartSockets();
	void AddStatsLine(UpDown line);
	bool ReloadTemplates();
	INT_PTR GetSessionCount()					{ return m_Params.Sessions.GetCount(); }
	bool IsRunning() const						{ return m_bServerWorking; }
protected:
	//all static method names have an underscore prefix
	static void		_ProcessURL(const ThreadData &Data);
	static void		_ProcessFileReq(const ThreadData &Data);

private:
	static CString	_GetHeader(const ThreadData &Data, long lSession);
	static const CString _GetFooter(const ThreadData &Data);
	static CString	_GetServerList(const ThreadData &Data);
	static CString	_GetTransferList(const ThreadData &Data);
	static CString	_GetSharedFilesList(const ThreadData &Data);
	static CString	_GetGraphs(const ThreadData &Data);
	static CString	_GetLog(const ThreadData &Data);
	static CString	_GetServerInfo(const ThreadData &Data);
	static CString	_GetDebugLog(const ThreadData &Data);
	static CString	_GetStats(const ThreadData &Data);
	static CString  _GetKadDlg(const ThreadData &Data);
	static CString	_GetPreferences(const ThreadData &Data);
	static CString	_GetLoginScreen(const ThreadData &Data);
	//static CString	_GetConnectedServer(const ThreadData &Data);
	static CString	_GetAddServerBox(const ThreadData &Data);
	static CString	_GetCommentlist(const ThreadData &Data);
	static void		_RemoveServer(const CString &sIP, int nPort);
	static void		_AddToStatic(const CString &sIP, int nPort);
	static void		_RemoveFromStatic(const CString &sIP, int nPort);

	//static CString	_GetWebSearch(const ThreadData &Data);
	static CString	_GetSearch(const ThreadData &Data);

	static CString	_ParseURL(const CString &URL, const CString &fieldname);
	static CString	_ParseURLArray(CString URL, CString fieldname);
	static void		_ConnectToServer(const CString &sIP, int nPort);
	static bool		_IsLoggedIn(const ThreadData &Data, long lSession);
	static void		_RemoveTimeOuts(const ThreadData &Data);
	static bool		_RemoveSession(const ThreadData &Data, long lSession);
	static CString	_SpecialChars(const CString &cstr, bool noquote = true);
	static CString	_GetPlainResString(UINT nID, bool noquote = true);
	static void		_GetPlainResString(CString &rstrOut, UINT nID, bool noquote = true);
	static int		_GzipCompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
	static CString	_LoadTemplate(const CString &sAll, const CString &sTemplateName);
	static Session	_GetSessionByID(const ThreadData &Data, long sessionID);
	static bool		_IsSessionAdmin(const ThreadData &Data, long sessionID);
	static bool		_IsSessionAdmin(const ThreadData &Data, const CString &strSsessionID);
	static CString	_GetPermissionDenied();
	static CString	_GetDownloadGraph(const ThreadData &Data, const CString &filehash);
	static void		_InsertCatBox(CString &Out, int preselect, const CString &boxlabel, bool jump, bool extraCats, const CString &sSession, const CString &sFileHash, bool ed2kbox = false);
	static CString	_GetSubCatLabel(int cat);
	static CString  _GetRemoteLinkAddedOk(const ThreadData &Data);
	static CString  _GetRemoteLinkAddedFailed(const ThreadData &Data);
	static void		_SetLastUserCat(const ThreadData &Data, long lSession, int cat);
	static int		_GetLastUserCat(const ThreadData &Data, long lSession);
	static void		_MakeTransferList(CString &Out, CWebServer *pThis, const ThreadData &Data, void *FilesArray, void *UploadArray, bool bAdmin);

	static void		_SaveWIConfigArray(BOOL *array, int size, LPCTSTR key);
	static CString	_GetWebImageNameForFileType(const CString &filename);
	static CString  _GetClientSummary(const CUpDownClient &client);
	static CString	_GetMyInfo(const ThreadData &Data);
	static CString	_GetClientversionImage(const CUpDownClient &client);

	bool			_GetIsTempDisabled() const	{ return m_bIsTempDisabled; } //never used

	//comparators for quick sort
	static int AFX_CDECL _DownloadCmp(void *prm, void const *pv1, void const *pv2);
	static int AFX_CDECL _ServerCmp(void *prm, void const *pv1, void const *pv2);
	static int AFX_CDECL _SharedCmp(void *prm, void const *pv1, void const *pv2);
	static int AFX_CDECL _UploadCmp(void *prm, void const *pv1, void const *pv2);

	// Common data
	GlobalParams	m_Params;
	WebTemplates	m_Templates;
	u_long			m_uCurIP;
	uint32			m_nStartTempDisabledTime;
	int				m_iSearchSortby;
	uint16			m_nIntruderDetect;
	bool			m_bServerWorking;
	bool			m_bSearchAsc;
	bool			m_bIsTempDisabled;
};