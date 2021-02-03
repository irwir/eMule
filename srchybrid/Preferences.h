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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

static LPCTSTR const strDefaultToolbar = _T("0099010203040506070899091011");

enum EViewSharedFilesAccess
{
	vsfaEverybody,
	vsfaFriends,
	vsfaNobody
};

enum ENotifierSoundType
{
	ntfstNoSound,
	ntfstSoundFile,
	ntfstSpeech
};

enum TLSmode: byte
{
	MODE_NONE,
	MODE_SSL_TLS,
	MODE_STARTTLS
};

enum SMTPauth: byte
{
	AUTH_NONE,
	AUTH_PLAIN,
	AUTH_LOGIN
	/* AUTH_GSSAPI,
	AUTH_DIGEST,
	AUTH_MD5,
	AUTH_CRAM,
	AUTH_OAUTH1,
	AUTH_OAUTH2	*/
};


enum EDefaultDirectory
{
	EMULE_CONFIGDIR = 0,
	EMULE_TEMPDIR = 1,
	EMULE_INCOMINGDIR = 2,
	EMULE_LOGDIR = 3,
	EMULE_ADDLANGDIR = 4, // directories with languages installed by the eMule (parent: EMULE_EXPANSIONDIR)
	EMULE_INSTLANGDIR = 5, // directories with languages installed by the user or installer (parent: EMULE_EXECUTABLEDIR)
	EMULE_WEBSERVERDIR = 6,
	EMULE_SKINDIR = 7,
	EMULE_DATABASEDIR = 8, // the parent directory of the incoming/temp folder
	EMULE_CONFIGBASEDIR = 9, // the parent directory of the config folder
	EMULE_EXECUTABLEDIR = 10, // assumed to be non-writable (!)
	EMULE_TOOLBARDIR = 11,
	EMULE_EXPANSIONDIR = 12 // this is a base directory accessible to all users for things eMule installs
};

enum EToolbarLabelType : uint8;
enum ELogFileFormat : uint8;

// DO NOT EDIT VALUES like changing uint16 to uint32, or insert any value. ONLY append new vars
#pragma pack(push, 1)
struct Preferences_Ext_Struct
{
	uint8	version;
	uchar	userhash[16];
	WINDOWPLACEMENT EmuleWindowPlacement;
};
#pragma pack(pop)

//email notifier
struct EmailSettings
{
	CString	sServer;
	CString	sFrom;
	CString	sTo;
	CString	sUser;
	CString	sPass;
	CString	sEncryptCertName;
	uint16	uPort;
	SMTPauth uAuth;
	TLSmode uTLS;
	bool	bSendMail;
};


// deadlake PROXYSUPPORT
struct ProxySettings
{
	CString	host;
	CString	user;
	CString	password;
	uint16	type;
	uint16	port;
	bool	bEnablePassword;
	bool	bUseProxy;
};

struct Category_Struct
{
	CString	strIncomingPath;
	CString	strTitle;
	CString	strComment;
	COLORREF color;
	UINT	prio;
	CString autocat;
	CString	regexp;
	int		filter;
	bool	filterNeg;
	bool	care4all;
	bool	ac_regexpeval;
	bool	downloadInAlphabeticalOrder; // ZZ:DownloadManager
};

class CPreferences
{
	friend class CPreferencesWnd;
	friend class CPPgConnection;
	friend class CPPgDebug;
	friend class CPPgDirectories;
	friend class CPPgDisplay;
	friend class CPPgFiles;
	friend class CPPgGeneral;
	friend class CPPgIRC;
	friend class CPPgNotify;
	friend class CPPgScheduler;
	friend class CPPgSecurity;
	friend class CPPgServer;
	friend class CPPgTweaks;
	friend class Wizard;

public:
	static CString	strNick;
	// ZZ:UploadSpeedSense -->
	static uint32	m_minupload;
	// ZZ:UploadSpeedSense <--
	static uint32	m_maxupload;
	static uint32	m_maxdownload;
	static LPCSTR	m_pszBindAddrA;
	static CStringA m_strBindAddrA;
	static LPCWSTR	m_pszBindAddrW;
	static CStringW m_strBindAddrW;
	static uint16	port;
	static uint16	udpport;
	static uint16	nServerUDPPort;
	static UINT		maxconnections;
	static UINT		maxhalfconnections;
	static bool		m_bConditionalTCPAccept;
	static bool		reconnect;
	static bool		m_bUseServerPriorities;
	static bool		m_bUseUserSortedServerList;
	static CString	m_strIncomingDir;
	static CStringArray	tempdir;
	static bool		ICH;
	static bool		m_bAutoUpdateServerList;
	static bool		updatenotify;
	static bool		mintotray;
	static bool		autoconnect;
	static bool		m_bAutoConnectToStaticServersOnly; // Barry
	static bool		autotakeed2klinks;	   // Barry
	static bool		addnewfilespaused;	   // Barry
	static UINT		depth3D;			   // Barry
	static bool		m_bEnableMiniMule;
	static int		m_iStraightWindowStyles;
	static bool		m_bUseSystemFontForMainControls;
	static bool		m_bRTLWindowsLayout;
	static CString	m_strSkinProfile;
	static CString	m_strSkinProfileDir;
	static bool		m_bAddServersFromServer;
	static bool		m_bAddServersFromClients;
	static UINT		maxsourceperfile;
	static UINT		trafficOMeterInterval;
	static UINT		statsInterval;
	static bool		m_bFillGraphs;
	static uchar	userhash[16];
	static WINDOWPLACEMENT EmuleWindowPlacement;
	static int		maxGraphDownloadRate;
	static int		maxGraphUploadRate;
	static uint32	maxGraphUploadRateEstimated;
	static bool		beepOnError;
	static bool		confirmExit;
	static DWORD	m_adwStatsColors[15];
	static bool		bHasCustomTaskIconColor;
	static bool		m_bIconflashOnNewMessage;
		   
	static bool		splashscreen;
	static bool		filterLANIPs;
	static bool		m_bAllocLocalHostIP;
	static bool		onlineSig;

	// -khaos--+++> Struct Members for Storing Statistics

	// Saved stats for cumulative downline overhead...
	static uint64	cumDownOverheadTotal;
	static uint64	cumDownOverheadFileReq;
	static uint64	cumDownOverheadSrcEx;
	static uint64	cumDownOverheadServer;
	static uint64	cumDownOverheadKad;
	static uint64	cumDownOverheadTotalPackets;
	static uint64	cumDownOverheadFileReqPackets;
	static uint64	cumDownOverheadSrcExPackets;
	static uint64	cumDownOverheadServerPackets;
	static uint64	cumDownOverheadKadPackets;

	// Saved stats for cumulative upline overhead...
	static uint64	cumUpOverheadTotal;
	static uint64	cumUpOverheadFileReq;
	static uint64	cumUpOverheadSrcEx;
	static uint64	cumUpOverheadServer;
	static uint64	cumUpOverheadKad;
	static uint64	cumUpOverheadTotalPackets;
	static uint64	cumUpOverheadFileReqPackets;
	static uint64	cumUpOverheadSrcExPackets;
	static uint64	cumUpOverheadServerPackets;
	static uint64	cumUpOverheadKadPackets;

	// Saved stats for cumulative upline data...
	static uint32	cumUpSuccessfulSessions;
	static uint32	cumUpFailedSessions;
	static uint32	cumUpAvgTime;
	// Cumulative client breakdown stats for sent bytes...
	static uint64	cumUpData_EDONKEY;
	static uint64	cumUpData_EDONKEYHYBRID;
	static uint64	cumUpData_EMULE;
	static uint64	cumUpData_MLDONKEY;
	static uint64	cumUpData_AMULE;
	static uint64	cumUpData_EMULECOMPAT;
	static uint64	cumUpData_SHAREAZA;
	// Session client breakdown stats for sent bytes...
	static uint64	sesUpData_EDONKEY;
	static uint64	sesUpData_EDONKEYHYBRID;
	static uint64	sesUpData_EMULE;
	static uint64	sesUpData_MLDONKEY;
	static uint64	sesUpData_AMULE;
	static uint64	sesUpData_EMULECOMPAT;
	static uint64	sesUpData_SHAREAZA;

	// Cumulative port breakdown stats for sent bytes...
	static uint64	cumUpDataPort_4662;
	static uint64	cumUpDataPort_OTHER;
	static uint64	cumUpDataPort_PeerCache;
	// Session port breakdown stats for sent bytes...
	static uint64	sesUpDataPort_4662;
	static uint64	sesUpDataPort_OTHER;
	static uint64	sesUpDataPort_PeerCache;

	// Cumulative source breakdown stats for sent bytes...
	static uint64	cumUpData_File;
	static uint64	cumUpData_Partfile;
	// Session source breakdown stats for sent bytes...
	static uint64	sesUpData_File;
	static uint64	sesUpData_Partfile;

	// Saved stats for cumulative downline data...
	static uint32	cumDownCompletedFiles;
	static uint32	cumDownSuccessfulSessions;
	static uint32	cumDownFailedSessions;
	static uint32	cumDownAvgTime;

	// Cumulative statistics for saved due to compression/lost due to corruption
	static uint64	cumLostFromCorruption;
	static uint64	cumSavedFromCompression;
	static uint32	cumPartsSavedByICH;

	// Session statistics for download sessions
	static uint32	sesDownSuccessfulSessions;
	static uint32	sesDownFailedSessions;
	static uint32	sesDownAvgTime;
	static uint32	sesDownCompletedFiles;
	static uint64	sesLostFromCorruption;
	static uint64	sesSavedFromCompression;
	static uint32	sesPartsSavedByICH;

	// Cumulative client breakdown stats for received bytes...
	static uint64	cumDownData_EDONKEY;
	static uint64	cumDownData_EDONKEYHYBRID;
	static uint64	cumDownData_EMULE;
	static uint64	cumDownData_MLDONKEY;
	static uint64	cumDownData_AMULE;
	static uint64	cumDownData_EMULECOMPAT;
	static uint64	cumDownData_SHAREAZA;
	static uint64	cumDownData_URL;
	// Session client breakdown stats for received bytes...
	static uint64	sesDownData_EDONKEY;
	static uint64	sesDownData_EDONKEYHYBRID;
	static uint64	sesDownData_EMULE;
	static uint64	sesDownData_MLDONKEY;
	static uint64	sesDownData_AMULE;
	static uint64	sesDownData_EMULECOMPAT;
	static uint64	sesDownData_SHAREAZA;
	static uint64	sesDownData_URL;

	// Cumulative port breakdown stats for received bytes...
	static uint64	cumDownDataPort_4662;
	static uint64	cumDownDataPort_OTHER;
	static uint64	cumDownDataPort_PeerCache;
	// Session port breakdown stats for received bytes...
	static uint64	sesDownDataPort_4662;
	static uint64	sesDownDataPort_OTHER;
	static uint64	sesDownDataPort_PeerCache;

	// Saved stats for cumulative connection data...
	static float	cumConnAvgDownRate;
	static float	cumConnMaxAvgDownRate;
	static float	cumConnMaxDownRate;
	static float	cumConnAvgUpRate;
	static float	cumConnMaxAvgUpRate;
	static float	cumConnMaxUpRate;
	static time_t	cumConnRunTime;
	static uint32	cumConnNumReconnects;
	static uint32	cumConnAvgConnections;
	static uint32	cumConnMaxConnLimitReached;
	static uint32	cumConnPeakConnections;
	static uint32	cumConnTransferTime;
	static uint32	cumConnDownloadTime;
	static uint32	cumConnUploadTime;
	static uint32	cumConnServerDuration;

	// Saved records for servers / network...
	static uint32	cumSrvrsMostWorkingServers;
	static uint32	cumSrvrsMostUsersOnline;
	static uint32	cumSrvrsMostFilesAvail;

	// Saved records for shared files...
	static uint32	cumSharedMostFilesShared;
	static uint64	cumSharedLargestShareSize;
	static uint64	cumSharedLargestAvgFileSize;
	static uint64	cumSharedLargestFileSize;

	// Save the date when the statistics were last reset...
	static time_t	stat_datetimeLastReset;

	// Save new preferences for PPgStats
	static UINT		statsConnectionsGraphRatio; // This will store the divisor, i.e. for 1:3 it will be 3, for 1:20 it will be 20.
	// Save the expanded branches of the stats tree
	static CString	m_strStatsExpandedTreeItems;

	static bool		m_bShowVerticalHourMarkers;
	static UINT		statsSaveInterval;
	// <-----khaos- End Statistics Members


	// Original Stats Stuff
	static uint64	totalDownloadedBytes;
	static uint64	totalUploadedBytes;
	// End Original Stats Stuff
	static WORD		m_wLanguageID;
	static bool		transferDoubleclick;
	static EViewSharedFilesAccess m_iSeeShares;
	static UINT		m_iToolDelayTime;	// tooltip delay time in seconds
	static bool		bringtoforeground;
	static UINT		splitterbarPosition;
	static UINT		splitterbarPositionSvr;

	static UINT		m_uTransferWnd1;
	static UINT		m_uTransferWnd2;
	//MORPH START - Added by SiRoB, Splitting Bar [O²]
	static UINT		splitterbarPositionStat;
	static UINT		splitterbarPositionStat_HL;
	static UINT		splitterbarPositionStat_HR;
	static UINT		splitterbarPositionFriend;
	static UINT		splitterbarPositionIRC;
	static UINT		splitterbarPositionShared;
	//MORPH END - Added by SiRoB, Splitting Bar [O²]
	static UINT		m_uDeadServerRetries;
	static DWORD	m_dwServerKeepAliveTimeout;
	// -khaos--+++> Changed data type to avoid overflows
	static UINT		statsMax;
	// <-----khaos-
	static UINT		statsAverageMinutes;

	static CString	notifierConfiguration;
	static bool		notifierOnDownloadFinished;
	static bool		notifierOnNewDownload;
	static bool		notifierOnChat;
	static bool		notifierOnLog;
	static bool		notifierOnImportantError;
	static bool		notifierOnEveryChatMsg;
	static bool		notifierOnNewVersion;
	static ENotifierSoundType notifierSoundType;
	static CString	notifierSoundFile;

	static CString	m_strIRCServer;
	static CString	m_strIRCNick;
	static CString	m_strIRCChannelFilter;
	static bool		m_bIRCAddTimeStamp;
	static bool		m_bIRCUseChannelFilter;
	static UINT		m_uIRCChannelUserFilter;
	static CString	m_strIRCPerformString;
	static bool		m_bIRCUsePerform;
	static bool		m_bIRCGetChannelsOnConnect;
	static bool		m_bIRCAcceptLinks;
	static bool		m_bIRCAcceptLinksFriendsOnly;
	static bool		m_bIRCPlaySoundEvents;
	static bool		m_bIRCIgnoreMiscMessages;
	static bool		m_bIRCIgnoreJoinMessages;
	static bool		m_bIRCIgnorePartMessages;
	static bool		m_bIRCIgnoreQuitMessages;
	static bool		m_bIRCIgnorePingPongMessages;
	static bool		m_bIRCIgnoreEmuleAddFriendMsgs;
	static bool		m_bIRCAllowEmuleAddFriend;
	static bool		m_bIRCIgnoreEmuleSendLinkMsgs;
	static bool		m_bIRCJoinHelpChannel;
	static bool		m_bIRCEnableSmileys;
	static bool		m_bMessageEnableSmileys;
	static bool		m_bIRCEnableUTF8;

	static bool		m_bRemove2bin;
	static bool		m_bShowCopyEd2kLinkCmd;
	static bool		m_bpreviewprio;
	static bool		m_bSmartServerIdCheck;
	static uint8	smartidstate;
	static bool		m_bSafeServerConnect;
	static bool		startMinimized;
	static bool		m_bAutoStart;
	static bool		m_bRestoreLastMainWndDlg;
	static int		m_iLastMainWndDlgID;
	static bool		m_bRestoreLastLogPane;
	static int		m_iLastLogPaneID;
	static UINT		MaxConperFive;
	static bool		checkDiskspace;
	static UINT		m_uMinFreeDiskSpace;
	static bool		m_bSparsePartFiles;
	static bool		m_bImportParts;
	static CString	m_strYourHostname;
	static bool		m_bEnableVerboseOptions;
	static bool		m_bVerbose;
	static bool		m_bFullVerbose;
	static int		m_byLogLevel;
	static bool		m_bDebugSourceExchange; // Sony April 23. 2003, button to keep source exchange msg out of verbose log
	static bool		m_bLogBannedClients;
	static bool		m_bLogRatingDescReceived;
	static bool		m_bLogSecureIdent;
	static bool		m_bLogFilteredIPs;
	static bool		m_bLogFileSaving;
	static bool		m_bLogA4AF; // ZZ:DownloadManager
	static bool		m_bLogUlDlEvents;
	static bool		m_bUseDebugDevice;
	static int		m_iDebugServerTCPLevel;
	static int		m_iDebugServerUDPLevel;
	static int		m_iDebugServerSourcesLevel;
	static int		m_iDebugServerSearchesLevel;
	static int		m_iDebugClientTCPLevel;
	static int		m_iDebugClientUDPLevel;
	static int		m_iDebugClientKadUDPLevel;
	static int		m_iDebugSearchResultDetailLevel;
	static bool		m_bupdatequeuelist;
	static bool		m_bManualAddedServersHighPriority;
	static bool		m_btransferfullchunks;
	static int		m_istartnextfile;
	static bool		m_bshowoverhead;
	static bool		m_bDAP;
	static bool		m_bUAP;
	static bool		m_bDisableKnownClientList;
	static bool		m_bDisableQueueList;
	static bool		m_bExtControls;
	static bool		m_bTransflstRemain;

	static UINT		versioncheckdays;
	static bool		showRatesInTitle;
		   
	static CString	m_strTxtEditor;
	static CString	m_strVideoPlayer;
	static CString	m_strVideoPlayerArgs;
	static bool		moviePreviewBackup;
	static int		m_iPreviewSmallBlocks;
	static bool		m_bPreviewCopiedArchives;
	static int		m_iInspectAllFileTypes;
	static bool		m_bPreviewOnIconDblClk;
	static bool		m_bCheckFileOpen;
	static bool		indicateratings;
	static bool		watchclipboard;
	static bool		filterserverbyip;
	static bool		m_bFirstStart;
	static bool		m_bBetaNaggingDone;
	static bool		m_bCreditSystem;
		   
	static bool		log2disk;
	static bool		debug2disk;
	static int		iMaxLogBuff;
	static UINT		uMaxLogFileSize;
	static ELogFileFormat m_iLogFileFormat;
	static bool		scheduler;
	static bool		dontcompressavi;
	static bool		msgonlyfriends;
	static bool		msgsecure;
	static bool		m_bUseChatCaptchas;
		   
	static UINT		filterlevel;
	static UINT		m_uFileBufferSize;
	static INT_PTR	m_iQueueSize;
	static int		m_iCommitFiles;
	static DWORD	m_uFileBufferTimeLimit;
		   
	static UINT		maxmsgsessions;
	static time_t	versioncheckLastAutomatic;
	static CString	messageFilter;
	static CString	commentFilter;
	static CString	filenameCleanups;
	static CString	m_strDateTimeFormat;
	static CString	m_strDateTimeFormat4Log;
	static CString	m_strDateTimeFormat4Lists;
	static LOGFONT	m_lfHyperText;
	static LOGFONT	m_lfLogText;
	static COLORREF m_crLogError;
	static COLORREF m_crLogWarning;
	static COLORREF m_crLogSuccess;
	static int		m_iExtractMetaData;
	static bool		m_bAdjustNTFSDaylightFileTime;
	static bool		m_bRearrangeKadSearchKeywords;
	static bool		m_bAllocFull;
	static bool		m_bShowSharedFilesDetails;
	static bool		m_bShowWin7TaskbarGoodies;
	static bool		m_bShowUpDownIconInTaskbar;
	static bool		m_bForceSpeedsToKB;
	static bool		m_bAutoShowLookups;
	static bool		m_bExtraPreviewWithMenu;

	// Web Server [kuchin]
	static CString	m_strWebPassword;
	static CString	m_strWebLowPassword;
	static uint16	m_nWebPort;
	static bool		m_bWebUseUPnP;
	static bool		m_bWebEnabled;
	static bool		m_bWebUseGzip;
	static int		m_nWebPageRefresh;
	static bool		m_bWebLowEnabled;
	static int		m_iWebTimeoutMins;
	static int		m_iWebFileUploadSizeLimitMB;
	static CString	m_strTemplateFile;
	static ProxySettings proxy; // deadlake PROXYSUPPORT
	static bool		m_bAllowAdminHiLevFunc;
	static CUIntArray m_aAllowedRemoteAccessIPs;
	static bool		m_bWebUseHttps;
	static CString	m_sWebHttpsCertificate;
	static CString	m_sWebHttpsKey;

	static bool		showCatTabInfos;
	static bool		resumeSameCat;
	static bool		dontRecreateGraphs;
	static bool		autofilenamecleanup;
	//static int	allcatType;
	//static bool	allcatTypeNeg;
	static bool		m_bUseAutocompl;
	static bool		m_bShowDwlPercentage;
	static bool		m_bRemoveFinishedDownloads;
	static INT_PTR	m_iMaxChatHistory;
	static bool		m_bShowActiveDownloadsBold;
		   
	static int		m_iSearchMethod;
	static bool		m_bAdvancedSpamfilter;
	static bool		m_bUseSecureIdent;
		   
	static bool		networkkademlia;
	static bool		networked2k;

	// toolbar
	static EToolbarLabelType m_nToolbarLabels;
	static CString	m_sToolbarBitmap;
	static CString	m_sToolbarBitmapFolder;
	static CString	m_sToolbarSettings;
	static bool		m_bReBarToolbar;
	static CSize	m_sizToolbarIconSize;
		   
	static bool		m_bWinaTransToolbar;
	static bool		m_bShowDownloadToolbar;

	//preview
	static bool		m_bPreviewEnabled;
	static bool		m_bAutomaticArcPreviewStart;

	// ZZ:UploadSpeedSense -->
	static bool		m_bDynUpEnabled;
	static int		m_iDynUpPingTolerance;
	static int		m_iDynUpGoingUpDivider;
	static int		m_iDynUpGoingDownDivider;
	static int		m_iDynUpNumberOfPings;
	static int		m_iDynUpPingToleranceMilliseconds;
	static bool		m_bDynUpUseMillisecondPingTolerance;
	// ZZ:UploadSpeedSense <--

	static bool		m_bA4AFSaveCpu; // ZZ:DownloadManager

	static bool		m_bHighresTimer;

	static bool		m_bResolveSharedShellLinks;
	static CStringList shareddir_list;
	static CStringList addresses_list;
	static bool		m_bKeepUnavailableFixedSharedDirs;

	static int		m_iDbgHeap;
	static UINT		m_nWebMirrorAlertLevel;
	static bool		m_bRunAsUser;
	static bool		m_bPreferRestrictedOverUser;
		   
	static bool		m_bUseOldTimeRemaining;

	// PeerCache
	static time_t	m_uPeerCacheLastSearch;
	static uint16	m_nPeerCachePort;
	static bool		m_bPeerCacheWasFound;
	static bool		m_bPeerCacheEnabled;
	static bool		m_bPeerCacheShow;

	// Firewall settings
	static bool		m_bOpenPortsOnStartUp;

	//AICH Options
	static bool		m_bTrustEveryHash;

	// files
	static bool		m_bRememberCancelledFiles;
	static bool		m_bRememberDownloadedFiles;
	static bool		m_bPartiallyPurgeOldKnownFiles;

	//email notifier
	static EmailSettings m_email;

	// encryption / obfuscation / verification
	static bool		m_bCryptLayerRequested;
	static bool		m_bCryptLayerSupported;
	static bool		m_bCryptLayerRequired;
	static uint8	m_byCryptTCPPaddingLength;
	static uint32   m_dwKadUDPKey;

	// UPnP
	static bool		m_bSkipWANIPSetup;
	static bool		m_bSkipWANPPPSetup;
	static bool		m_bEnableUPnP;
	static bool		m_bCloseUPnPOnExit;
	static bool		m_bIsWinServImplDisabled;
	static bool		m_bIsMinilibImplDisabled;
	static int		m_nLastWorkingImpl;

	// Spam
	static bool		m_bEnableSearchResultFilter;

	static BOOL		m_bIsRunningAeroGlass;
	static bool		m_bPreventStandby;
	static bool		m_bStoreSearches;


	enum Table
	{
		tableDownload,
		tableUpload,
		tableQueue,
		tableSearch,
		tableShared,
		tableServer,
		tableClientList,
		tableFilenames,
		tableIrcMain,
		tableIrcChannels,
		tableDownloadClients
	};

	CPreferences();
	~CPreferences();

	static void	Init();
	static void	Uninit();

	static LPCTSTR	GetTempDir(INT_PTR id = 0)			{ return (LPCTSTR)tempdir[(id < tempdir.GetCount()) ? id : 0]; }
	static INT_PTR	GetTempDirCount()					{ return tempdir.GetCount(); }
	static bool		CanFSHandleLargeFiles(int nForCat);
	static LPCTSTR	GetConfigFile();
	static const CString& GetFileCommentsFilePath()		{ return m_strFileCommentsFilePath; }
	static CString	GetMuleDirectory(EDefaultDirectory eDirectory, bool bCreate = true);
	static void		SetMuleDirectory(EDefaultDirectory eDirectory, const CString &strNewDir);
	static void		ChangeUserDirMode(int nNewMode);

	static bool		IsTempFile(const CString &rstrDirectory, const CString &rstrName);
	static bool		IsShareableDirectory(const CString &rstrDir);
	static bool		IsInstallationDirectory(const CString &rstrDir);

	static bool		Save();
	static void		SaveCats();

	static bool		GetUseServerPriorities()			{ return m_bUseServerPriorities; }
	static bool		GetUseUserSortedServerList()		{ return m_bUseUserSortedServerList; }
	static bool		Reconnect()							{ return reconnect; }
	static const CString& GetUserNick()					{ return strNick; }
	static void		SetUserNick(LPCTSTR pszNick);
	static int		GetMaxUserNickLength()				{ return 50; }

	static LPCSTR	GetBindAddrA()						{ return m_pszBindAddrA; }
	static LPCWSTR	GetBindAddrW()						{ return m_pszBindAddrW; }
#ifdef UNICODE
#define GetBindAddr  GetBindAddrW
#else
#define GetBindAddr  GetBindAddrA
#endif // !UNICODE

	static uint16	GetPort()							{ return port; }
	static uint16	GetUDPPort()						{ return udpport; }
	static uint16	GetServerUDPPort()					{ return nServerUDPPort; }
	static uchar*	GetUserHash()						{ return userhash; }
	// ZZ:UploadSpeedSense -->
	static uint32	GetMinUpload()						{ return m_minupload; }
	// ZZ:UploadSpeedSense <--
	static uint32	GetMaxUpload()						{ return m_maxupload; }
	static bool		IsICHEnabled()						{ return ICH; }
	static bool		GetAutoUpdateServerList()			{ return m_bAutoUpdateServerList; }
	static bool		UpdateNotify()						{ return updatenotify; }
	static bool		GetMinToTray()						{ return mintotray; }
	static bool		DoAutoConnect()						{ return autoconnect; }
	static void		SetAutoConnect(bool inautoconnect)	{ autoconnect = inautoconnect; }
	static bool		GetAddServersFromServer()			{ return m_bAddServersFromServer; }
	static bool		GetAddServersFromClients()			{ return m_bAddServersFromClients; }
	static bool*	GetMinTrayPTR()						{ return &mintotray; }
	static UINT		GetTrafficOMeterInterval()			{ return trafficOMeterInterval; }
	static void		SetTrafficOMeterInterval(UINT in)	{ trafficOMeterInterval = in; }
	static UINT		GetStatsInterval()					{ return statsInterval; }
	static void		SetStatsInterval(UINT in)			{ statsInterval = in; }
	static bool		GetFillGraphs()						{ return m_bFillGraphs; }
	static void		SetFillGraphs(bool bFill)			{ m_bFillGraphs = bFill; }

	// -khaos--+++> Many, many, many, many methods.
	static void		SaveStats(int bBackUp = 0);
	static void		SetRecordStructMembers();
	static void		SaveCompletedDownloadsStat();
	static bool		LoadStats(int loadBackUp = 0);
	static void		ResetCumulativeStatistics();

	static void		Add2DownCompletedFiles()			{ ++cumDownCompletedFiles; }
	static void		SetConnMaxAvgDownRate(float in)		{ cumConnMaxAvgDownRate = in; }
	static void		SetConnMaxDownRate(float in)		{ cumConnMaxDownRate = in; }
	static void		SetConnAvgUpRate(float in)			{ cumConnAvgUpRate = in; }
	static void		SetConnMaxAvgUpRate(float in)		{ cumConnMaxAvgUpRate = in; }
	static void		SetConnMaxUpRate(float in)			{ cumConnMaxUpRate = in; }
	static void		SetConnPeakConnections(int in)		{ cumConnPeakConnections = in; }
	static void		SetUpAvgTime(int in)				{ cumUpAvgTime = in; }
	static void		Add2DownSAvgTime(int in)			{ sesDownAvgTime += in; }
	static void		SetDownCAvgTime(int in)				{ cumDownAvgTime = in; }
	static void		Add2ConnTransferTime(int in)		{ cumConnTransferTime += in; }
	static void		Add2ConnDownloadTime(int in)		{ cumConnDownloadTime += in; }
	static void		Add2ConnUploadTime(int in)			{ cumConnUploadTime += in; }
	static void		Add2DownSessionCompletedFiles()		{ ++sesDownCompletedFiles; }
	static void		Add2SessionTransferData(UINT uClientID, UINT uClientPort, BOOL bFromPF, BOOL bUpDown, uint32 bytes, bool sentToFriend = false);
	static void		Add2DownSuccessfulSessions()		{ ++sesDownSuccessfulSessions;
														  ++cumDownSuccessfulSessions; }
	static void		Add2DownFailedSessions()			{ ++sesDownFailedSessions;
														  ++cumDownFailedSessions; }
	static void		Add2LostFromCorruption(uint64 in)	{ sesLostFromCorruption += in; }
	static void		Add2SavedFromCompression(uint64 in)	{ sesSavedFromCompression += in; }
	static void		Add2SessionPartsSavedByICH(int in)	{ sesPartsSavedByICH += in; }

	// Saved stats for cumulative downline overhead
	static uint64	GetDownOverheadTotal()				{ return cumDownOverheadTotal; }
	static uint64	GetDownOverheadFileReq()			{ return cumDownOverheadFileReq; }
	static uint64	GetDownOverheadSrcEx()				{ return cumDownOverheadSrcEx; }
	static uint64	GetDownOverheadServer()				{ return cumDownOverheadServer; }
	static uint64	GetDownOverheadKad()				{ return cumDownOverheadKad; }
	static uint64	GetDownOverheadTotalPackets()		{ return cumDownOverheadTotalPackets; }
	static uint64	GetDownOverheadFileReqPackets()		{ return cumDownOverheadFileReqPackets; }
	static uint64	GetDownOverheadSrcExPackets()		{ return cumDownOverheadSrcExPackets; }
	static uint64	GetDownOverheadServerPackets()		{ return cumDownOverheadServerPackets; }
	static uint64	GetDownOverheadKadPackets()			{ return cumDownOverheadKadPackets; }

	// Saved stats for cumulative upline overhead
	static uint64	GetUpOverheadTotal()				{ return cumUpOverheadTotal; }
	static uint64	GetUpOverheadFileReq()				{ return cumUpOverheadFileReq; }
	static uint64	GetUpOverheadSrcEx()				{ return cumUpOverheadSrcEx; }
	static uint64	GetUpOverheadServer()				{ return cumUpOverheadServer; }
	static uint64	GetUpOverheadKad()					{ return cumUpOverheadKad; }
	static uint64	GetUpOverheadTotalPackets()			{ return cumUpOverheadTotalPackets; }
	static uint64	GetUpOverheadFileReqPackets()		{ return cumUpOverheadFileReqPackets; }
	static uint64	GetUpOverheadSrcExPackets()			{ return cumUpOverheadSrcExPackets; }
	static uint64	GetUpOverheadServerPackets()		{ return cumUpOverheadServerPackets; }
	static uint64	GetUpOverheadKadPackets()			{ return cumUpOverheadKadPackets; }

	// Saved stats for cumulative upline data
	static uint32	GetUpSuccessfulSessions()			{ return cumUpSuccessfulSessions; }
	static uint32	GetUpFailedSessions()				{ return cumUpFailedSessions; }
	static uint32	GetUpAvgTime()						{ return cumUpAvgTime; }

	// Saved stats for cumulative downline data
	static uint32	GetDownCompletedFiles()				{ return cumDownCompletedFiles; }
	static uint32	GetDownC_SuccessfulSessions()		{ return cumDownSuccessfulSessions; }
	static uint32	GetDownC_FailedSessions()			{ return cumDownFailedSessions; }
	static uint32	GetDownC_AvgTime()					{ return cumDownAvgTime; }

	// Session download stats
	static uint32	GetDownSessionCompletedFiles()		{ return sesDownCompletedFiles; }
	static uint32	GetDownS_SuccessfulSessions()		{ return sesDownSuccessfulSessions; }
	static uint32	GetDownS_FailedSessions()			{ return sesDownFailedSessions; }
	static uint32	GetDownS_AvgTime()					{ return GetDownS_SuccessfulSessions() ? sesDownAvgTime / GetDownS_SuccessfulSessions() : 0; }

	// Saved stats for corruption/compression
	static uint64	GetCumLostFromCorruption()			{ return cumLostFromCorruption; }
	static uint64	GetCumSavedFromCompression()		{ return cumSavedFromCompression; }
	static uint64	GetSesLostFromCorruption()			{ return sesLostFromCorruption; }
	static uint64	GetSesSavedFromCompression()		{ return sesSavedFromCompression; }
	static uint32	GetCumPartsSavedByICH()				{ return cumPartsSavedByICH; }
	static uint32	GetSesPartsSavedByICH()				{ return sesPartsSavedByICH; }

	// Cumulative client breakdown stats for sent bytes
	static uint64	GetUpTotalClientData()				{ return  GetCumUpData_EDONKEY()
																+ GetCumUpData_EDONKEYHYBRID()
																+ GetCumUpData_EMULE()
																+ GetCumUpData_MLDONKEY()
																+ GetCumUpData_AMULE()
																+ GetCumUpData_EMULECOMPAT()
																+ GetCumUpData_SHAREAZA(); }
	static uint64	GetCumUpData_EDONKEY()				{ return cumUpData_EDONKEY +		sesUpData_EDONKEY; }
	static uint64	GetCumUpData_EDONKEYHYBRID()		{ return cumUpData_EDONKEYHYBRID +	sesUpData_EDONKEYHYBRID; }
	static uint64	GetCumUpData_EMULE()				{ return cumUpData_EMULE +			sesUpData_EMULE; }
	static uint64	GetCumUpData_MLDONKEY()				{ return cumUpData_MLDONKEY +		sesUpData_MLDONKEY; }
	static uint64	GetCumUpData_AMULE()				{ return cumUpData_AMULE +			sesUpData_AMULE; }
	static uint64	GetCumUpData_EMULECOMPAT()			{ return cumUpData_EMULECOMPAT +	sesUpData_EMULECOMPAT; }
	static uint64	GetCumUpData_SHAREAZA()				{ return cumUpData_SHAREAZA +		sesUpData_SHAREAZA; }

	// Session client breakdown stats for sent bytes
	static uint64	GetUpSessionClientData()			{ return  sesUpData_EDONKEY
																+ sesUpData_EDONKEYHYBRID
																+ sesUpData_EMULE
																+ sesUpData_MLDONKEY
																+ sesUpData_AMULE
																+ sesUpData_EMULECOMPAT
																+ sesUpData_SHAREAZA; }
	static uint64	GetUpData_EDONKEY()					{ return sesUpData_EDONKEY; }
	static uint64	GetUpData_EDONKEYHYBRID()			{ return sesUpData_EDONKEYHYBRID; }
	static uint64	GetUpData_EMULE()					{ return sesUpData_EMULE; }
	static uint64	GetUpData_MLDONKEY()				{ return sesUpData_MLDONKEY; }
	static uint64	GetUpData_AMULE()					{ return sesUpData_AMULE; }
	static uint64	GetUpData_EMULECOMPAT()				{ return sesUpData_EMULECOMPAT; }
	static uint64	GetUpData_SHAREAZA()				{ return sesUpData_SHAREAZA; }

	// Cumulative port breakdown stats for sent bytes...
	static uint64	GetUpTotalPortData()				{ return  GetCumUpDataPort_4662()
																+ GetCumUpDataPort_OTHER()
																+ GetCumUpDataPort_PeerCache(); }
	static uint64	GetCumUpDataPort_4662()				{ return cumUpDataPort_4662 +		sesUpDataPort_4662; }
	static uint64	GetCumUpDataPort_OTHER()			{ return cumUpDataPort_OTHER +		sesUpDataPort_OTHER; }
	static uint64	GetCumUpDataPort_PeerCache()		{ return cumUpDataPort_PeerCache +	sesUpDataPort_PeerCache; }

	// Session port breakdown stats for sent bytes...
	static uint64	GetUpSessionPortData()				{ return  sesUpDataPort_4662
																+ sesUpDataPort_OTHER
																+ sesUpDataPort_PeerCache; }
	static uint64	GetUpDataPort_4662()				{ return sesUpDataPort_4662; }
	static uint64	GetUpDataPort_OTHER()				{ return sesUpDataPort_OTHER; }
	static uint64	GetUpDataPort_PeerCache()			{ return sesUpDataPort_PeerCache; }

	// Cumulative DS breakdown stats for sent bytes...
	static uint64	GetUpTotalDataFile()				{ return GetCumUpData_File() +	GetCumUpData_Partfile(); }
	static uint64	GetCumUpData_File()					{ return cumUpData_File +		sesUpData_File; }
	static uint64	GetCumUpData_Partfile()				{ return cumUpData_Partfile +	sesUpData_Partfile; }
	// Session DS breakdown stats for sent bytes...
	static uint64	GetUpSessionDataFile()				{ return sesUpData_File +		sesUpData_Partfile; }
	static uint64	GetUpData_File()					{ return sesUpData_File; }
	static uint64	GetUpData_Partfile()				{ return sesUpData_Partfile; }

	// Cumulative client breakdown stats for received bytes
	static uint64	GetDownTotalClientData()			{ return  GetCumDownData_EDONKEY()
																+ GetCumDownData_EDONKEYHYBRID()
																+ GetCumDownData_EMULE()
																+ GetCumDownData_MLDONKEY()
																+ GetCumDownData_AMULE()
																+ GetCumDownData_EMULECOMPAT()
																+ GetCumDownData_SHAREAZA()
																+ GetCumDownData_URL(); }
	static uint64	GetCumDownData_EDONKEY()			{ return cumDownData_EDONKEY +		sesDownData_EDONKEY; }
	static uint64	GetCumDownData_EDONKEYHYBRID()		{ return cumDownData_EDONKEYHYBRID + sesDownData_EDONKEYHYBRID; }
	static uint64	GetCumDownData_EMULE()				{ return cumDownData_EMULE +		sesDownData_EMULE; }
	static uint64	GetCumDownData_MLDONKEY()			{ return cumDownData_MLDONKEY +		sesDownData_MLDONKEY; }
	static uint64	GetCumDownData_AMULE()				{ return cumDownData_AMULE +		sesDownData_AMULE; }
	static uint64	GetCumDownData_EMULECOMPAT()		{ return cumDownData_EMULECOMPAT +	sesDownData_EMULECOMPAT; }
	static uint64	GetCumDownData_SHAREAZA()			{ return cumDownData_SHAREAZA +		sesDownData_SHAREAZA; }
	static uint64	GetCumDownData_URL()				{ return cumDownData_URL +			sesDownData_URL; }

	// Session client breakdown stats for received bytes
	static uint64	GetDownSessionClientData()			{ return  sesDownData_EDONKEY
																+ sesDownData_EDONKEYHYBRID
																+ sesDownData_EMULE
																+ sesDownData_MLDONKEY
																+ sesDownData_AMULE
																+ sesDownData_EMULECOMPAT
																+ sesDownData_SHAREAZA
																+ sesDownData_URL; }
	static uint64	GetDownData_EDONKEY()				{ return sesDownData_EDONKEY; }
	static uint64	GetDownData_EDONKEYHYBRID()			{ return sesDownData_EDONKEYHYBRID; }
	static uint64	GetDownData_EMULE()					{ return sesDownData_EMULE; }
	static uint64	GetDownData_MLDONKEY()				{ return sesDownData_MLDONKEY; }
	static uint64	GetDownData_AMULE()					{ return sesDownData_AMULE; }
	static uint64	GetDownData_EMULECOMPAT()			{ return sesDownData_EMULECOMPAT; }
	static uint64	GetDownData_SHAREAZA()				{ return sesDownData_SHAREAZA; }
	static uint64	GetDownData_URL()					{ return sesDownData_URL; }

	// Cumulative port breakdown stats for received bytes...
	static uint64	GetDownTotalPortData()				{ return  GetCumDownDataPort_4662()
																+ GetCumDownDataPort_OTHER()
																+ GetCumDownDataPort_PeerCache(); }
	static uint64	GetCumDownDataPort_4662()			{ return cumDownDataPort_4662		+ sesDownDataPort_4662; }
	static uint64	GetCumDownDataPort_OTHER()			{ return cumDownDataPort_OTHER		+ sesDownDataPort_OTHER; }
	static uint64	GetCumDownDataPort_PeerCache()		{ return cumDownDataPort_PeerCache	+ sesDownDataPort_PeerCache; }

	// Session port breakdown stats for received bytes...
	static uint64	GetDownSessionDataPort()			{ return   sesDownDataPort_4662
																+ sesDownDataPort_OTHER
																+ sesDownDataPort_PeerCache; }
	static uint64	GetDownDataPort_4662()				{ return sesDownDataPort_4662; }
	static uint64	GetDownDataPort_OTHER()				{ return sesDownDataPort_OTHER; }
	static uint64	GetDownDataPort_PeerCache()			{ return sesDownDataPort_PeerCache; }

	// Saved stats for cumulative connection data
	static float	GetConnAvgDownRate()				{ return cumConnAvgDownRate; }
	static float	GetConnMaxAvgDownRate()				{ return cumConnMaxAvgDownRate; }
	static float	GetConnMaxDownRate()				{ return cumConnMaxDownRate; }
	static float	GetConnAvgUpRate()					{ return cumConnAvgUpRate; }
	static float	GetConnMaxAvgUpRate()				{ return cumConnMaxAvgUpRate; }
	static float	GetConnMaxUpRate()					{ return cumConnMaxUpRate; }
	static time_t	GetConnRunTime()					{ return cumConnRunTime; }
	static uint32	GetConnNumReconnects()				{ return cumConnNumReconnects; }
	static uint32	GetConnAvgConnections()				{ return cumConnAvgConnections; }
	static uint32	GetConnMaxConnLimitReached()		{ return cumConnMaxConnLimitReached; }
	static uint32	GetConnPeakConnections()			{ return cumConnPeakConnections; }
	static uint32	GetConnTransferTime()				{ return cumConnTransferTime; }
	static uint32	GetConnDownloadTime()				{ return cumConnDownloadTime; }
	static uint32	GetConnUploadTime()					{ return cumConnUploadTime; }
	static uint32	GetConnServerDuration()				{ return cumConnServerDuration; }

	// Saved records for servers / network
	static uint32	GetSrvrsMostWorkingServers()		{ return cumSrvrsMostWorkingServers; }
	static uint32	GetSrvrsMostUsersOnline()			{ return cumSrvrsMostUsersOnline; }
	static uint32	GetSrvrsMostFilesAvail()			{ return cumSrvrsMostFilesAvail; }

	// Saved records for shared files
	static uint32	GetSharedMostFilesShared()			{ return cumSharedMostFilesShared; }
	static uint64	GetSharedLargestShareSize()			{ return cumSharedLargestShareSize; }
	static uint64	GetSharedLargestAvgFileSize()		{ return cumSharedLargestAvgFileSize; }
	static uint64	GetSharedLargestFileSize()			{ return cumSharedLargestFileSize; }

	// Get the long date/time when the stats were last reset
	static time_t	GetStatsLastResetLng()				{ return stat_datetimeLastReset; }
	static CString	GetStatsLastResetStr(bool formatLong = true);
	static UINT		GetStatsSaveInterval()				{ return statsSaveInterval; }

	// Get and Set our new preferences
	static void		SetStatsMax(UINT in)				{ statsMax = in; }
	static void		SetStatsConnectionsGraphRatio(UINT in) { statsConnectionsGraphRatio = in; }
	static UINT		GetStatsConnectionsGraphRatio()		{ return statsConnectionsGraphRatio; }
	static void		SetExpandedTreeItems(const CString &in)	{ m_strStatsExpandedTreeItems = in; }
	static const CString& GetExpandedTreeItems()		{ return m_strStatsExpandedTreeItems; }

	static uint64	GetTotalDownloaded()				{ return totalDownloadedBytes; }
	static uint64	GetTotalUploaded()					{ return totalUploadedBytes; }

	static bool		IsErrorBeepEnabled()				{ return beepOnError; }
	static bool		IsConfirmExitEnabled()				{ return confirmExit; }
	static void		SetConfirmExit(bool bVal)			{ confirmExit = bVal; }
	static bool		UseSplashScreen()					{ return splashscreen; }
	static bool		FilterLANIPs()						{ return filterLANIPs; }
	static bool		GetAllowLocalHostIP()				{ return m_bAllocLocalHostIP; }
	static bool		IsOnlineSignatureEnabled()			{ return onlineSig; }
	static int		GetMaxGraphUploadRate(bool bEstimateIfUnlimited);
	static int		GetMaxGraphDownloadRate()			{ return maxGraphDownloadRate; }
	static void		SetMaxGraphUploadRate(int in);
	static void		SetMaxGraphDownloadRate(int in)		{ maxGraphDownloadRate = (in ? in : 96); }

	static uint32	GetMaxDownload();
	static uint64	GetMaxDownloadInBytesPerSec(bool dynamic = false);
	static UINT		GetMaxConnections()					{ return maxconnections; }
	static UINT		GetMaxHalfConnections()				{ return maxhalfconnections; }
	static UINT		GetMaxSourcePerFileDefault()		{ return maxsourceperfile; }
	static UINT		GetDeadServerRetries()				{ return m_uDeadServerRetries; }
	static DWORD	GetServerKeepAliveTimeout()			{ return m_dwServerKeepAliveTimeout; }
	static bool		GetConditionalTCPAccept()			{ return m_bConditionalTCPAccept; }

	static LANGID	GetLanguageID();
	static void		SetLanguageID(LANGID lid);
	static void		GetLanguages(CWordArray &aLanguageIDs);
	static void		SetLanguage();
	static bool		IsLanguageSupported(LANGID lidSelected);
	static CString	GetLangDLLNameByID(LANGID lidSelected);
	static void		InitThreadLocale();
	static void		SetRtlLocale(LCID lcid);
	static CString GetHtmlCharset();

	static bool		IsDoubleClickEnabled()				{ return transferDoubleclick; }
	static EViewSharedFilesAccess CanSeeShares()		{ return m_iSeeShares; }
	static UINT		GetToolTipDelay()					{ return m_iToolDelayTime; }
	static bool		IsBringToFront()					{ return bringtoforeground; }

	static UINT		GetSplitterbarPosition()			{ return splitterbarPosition; }
	static void		SetSplitterbarPosition(UINT pos)	{ splitterbarPosition = pos; }
	static UINT		GetSplitterbarPositionServer()		{ return splitterbarPositionSvr; }
	static void		SetSplitterbarPositionServer(UINT pos) { splitterbarPositionSvr = pos; }
	static UINT		GetTransferWnd1()					{ return m_uTransferWnd1; }
	static void		SetTransferWnd1(UINT uWnd1)			{ m_uTransferWnd1 = uWnd1; }
	static UINT		GetTransferWnd2()					{ return m_uTransferWnd2; }
	static void		SetTransferWnd2(UINT uWnd2)			{ m_uTransferWnd2 = uWnd2; }
	//MORPH START - Added by SiRoB, Splitting Bar [O²]
	static UINT		GetSplitterbarPositionStat()		{ return splitterbarPositionStat; }
	static void		SetSplitterbarPositionStat(UINT pos) { splitterbarPositionStat = pos; }
	static UINT		GetSplitterbarPositionStat_HL()		{ return splitterbarPositionStat_HL; }
	static void		SetSplitterbarPositionStat_HL(UINT pos)	{ splitterbarPositionStat_HL = pos; }
	static UINT		GetSplitterbarPositionStat_HR()		{ return splitterbarPositionStat_HR; }
	static void		SetSplitterbarPositionStat_HR(UINT pos)	{ splitterbarPositionStat_HR = pos; }
	static UINT		GetSplitterbarPositionFriend()		{ return splitterbarPositionFriend; }
	static void		SetSplitterbarPositionFriend(UINT pos)	{ splitterbarPositionFriend = pos; }
	static UINT		GetSplitterbarPositionIRC()			{ return splitterbarPositionIRC; }
	static void		SetSplitterbarPositionIRC(UINT pos)	{ splitterbarPositionIRC = pos; }
	static UINT		GetSplitterbarPositionShared()		{ return splitterbarPositionShared; }
	static void		SetSplitterbarPositionShared(UINT pos)	{ splitterbarPositionShared = pos; }
	//MORPH END   - Added by SiRoB, Splitting Bar [O²]
	// -khaos--+++> Changed datatype to avoid overflows
	static UINT		GetStatsMax()						{ return statsMax; }
	// <-----khaos-
	static bool		UseFlatBar()						{ return !depth3D; }
	static int		GetStraightWindowStyles()			{ return m_iStraightWindowStyles; }
	static bool		GetUseSystemFontForMainControls()	{ return m_bUseSystemFontForMainControls; }

	static const CString& GetSkinProfile()				{ return m_strSkinProfile; }
	static void		SetSkinProfile(LPCTSTR pszProfile)	{ m_strSkinProfile = pszProfile; }

	static UINT		GetStatsAverageMinutes()			{ return statsAverageMinutes; }
	static void		SetStatsAverageMinutes(UINT in)		{ statsAverageMinutes = in; }

	static const CString& GetNotifierConfiguration()	{ return notifierConfiguration; }
	static void		SetNotifierConfiguration(LPCTSTR pszConfigPath)	{ notifierConfiguration = pszConfigPath; }
	static bool		GetNotifierOnDownloadFinished()		{ return notifierOnDownloadFinished; }
	static bool		GetNotifierOnNewDownload()			{ return notifierOnNewDownload; }
	static bool		GetNotifierOnChat()					{ return notifierOnChat; }
	static bool		GetNotifierOnLog()					{ return notifierOnLog; }
	static bool		GetNotifierOnImportantError()		{ return notifierOnImportantError; }
	static bool		GetNotifierOnEveryChatMsg()			{ return notifierOnEveryChatMsg; }
	static bool		GetNotifierOnNewVersion()			{ return notifierOnNewVersion; }
	static ENotifierSoundType GetNotifierSoundType()	{ return notifierSoundType; }
	static const CString& GetNotifierSoundFile()		{ return notifierSoundFile; }

	static bool		GetEnableMiniMule()					{ return m_bEnableMiniMule; }
	static bool		GetRTLWindowsLayout()				{ return m_bRTLWindowsLayout; }

	static const CString& GetIRCNick()					{ return m_strIRCNick; }
	static void		SetIRCNick(LPCTSTR pszNick)			{ m_strIRCNick = pszNick; }
	static const CString& GetIRCServer()				{ return m_strIRCServer; }
	static bool		GetIRCAddTimeStamp()				{ return m_bIRCAddTimeStamp; }
	static bool		GetIRCUseChannelFilter()			{ return m_bIRCUseChannelFilter; }
	static const CString& GetIRCChannelFilter()			{ return m_strIRCChannelFilter; }
	static UINT		GetIRCChannelUserFilter()			{ return m_uIRCChannelUserFilter; }
	static bool		GetIRCUsePerform()					{ return m_bIRCUsePerform; }
	static const CString& GetIRCPerformString()			{ return m_strIRCPerformString; }
	static bool		GetIRCJoinHelpChannel()				{ return m_bIRCJoinHelpChannel; }
	static bool		GetIRCGetChannelsOnConnect()		{ return m_bIRCGetChannelsOnConnect; }
	static bool		GetIRCPlaySoundEvents()				{ return m_bIRCPlaySoundEvents; }
	static bool		GetIRCIgnoreMiscMessages()			{ return m_bIRCIgnoreMiscMessages; }
	static bool		GetIRCIgnoreJoinMessages()			{ return m_bIRCIgnoreJoinMessages; }
	static bool		GetIRCIgnorePartMessages()			{ return m_bIRCIgnorePartMessages; }
	static bool		GetIRCIgnoreQuitMessages()			{ return m_bIRCIgnoreQuitMessages; }
	static bool		GetIRCIgnorePingPongMessages()		{ return m_bIRCIgnorePingPongMessages; }
	static bool		GetIRCIgnoreEmuleAddFriendMsgs()	{ return m_bIRCIgnoreEmuleAddFriendMsgs; }
	static bool		GetIRCIgnoreEmuleSendLinkMsgs()		{ return m_bIRCIgnoreEmuleSendLinkMsgs; }
	static bool		GetIRCAllowEmuleAddFriend()			{ return m_bIRCAllowEmuleAddFriend; }
	static bool		GetIRCAcceptLinks()					{ return m_bIRCAcceptLinks; }
	static bool		GetIRCAcceptLinksFriendsOnly()		{ return m_bIRCAcceptLinksFriendsOnly; }
	static bool		GetIRCEnableSmileys()				{ return m_bIRCEnableSmileys; }
	static bool		GetMessageEnableSmileys()			{ return m_bMessageEnableSmileys; }
	static bool		GetIRCEnableUTF8()					{ return m_bIRCEnableUTF8; }

	static WORD		GetWindowsVersion();
	static bool		IsRunningAeroGlassTheme();
	static bool		GetStartMinimized()					{ return startMinimized; }
	static void		SetStartMinimized( bool instartMinimized) { startMinimized = instartMinimized; }
	static bool		GetAutoStart()						{ return m_bAutoStart; }
	static void		SetAutoStart( bool val)				{ m_bAutoStart = val; }

	static bool		GetRestoreLastMainWndDlg()			{ return m_bRestoreLastMainWndDlg; }
	static int		GetLastMainWndDlgID()				{ return m_iLastMainWndDlgID; }
	static void		SetLastMainWndDlgID(int iID)		{ m_iLastMainWndDlgID = iID; }

	static bool		GetRestoreLastLogPane()				{ return m_bRestoreLastLogPane; }
	static int		GetLastLogPaneID()					{ return m_iLastLogPaneID; }
	static void		SetLastLogPaneID(int iID)			{ m_iLastLogPaneID = iID; }

	static bool		GetSmartIdCheck()					{ return m_bSmartServerIdCheck; }
	static void		SetSmartIdCheck(bool in_smartidcheck) { m_bSmartServerIdCheck = in_smartidcheck; }
	static uint8	GetSmartIdState()					{ return smartidstate; }
	static void		SetSmartIdState(uint8 in_smartidstate) { smartidstate = in_smartidstate; }
	static bool		GetPreviewPrio()					{ return m_bpreviewprio; }
	static void		SetPreviewPrio(bool in)				{ m_bpreviewprio = in; }
	static bool		GetUpdateQueueList()				{ return m_bupdatequeuelist; }
	static bool		GetManualAddedServersHighPriority()	{ return m_bManualAddedServersHighPriority; }
	static bool		TransferFullChunks()				{ return m_btransferfullchunks; }
	static void		SetTransferFullChunks( bool m_bintransferfullchunks ) { m_btransferfullchunks = m_bintransferfullchunks; }
	static int		StartNextFile()						{ return m_istartnextfile; }
	static bool		ShowOverhead()						{ return m_bshowoverhead; }
	static void		SetNewAutoUp(bool m_bInUAP)			{ m_bUAP = m_bInUAP; }
	static bool		GetNewAutoUp()						{ return m_bUAP; }
	static void		SetNewAutoDown(bool m_bInDAP)		{ m_bDAP = m_bInDAP; }
	static bool		GetNewAutoDown()					{ return m_bDAP; }
	static bool		IsKnownClientListDisabled()			{ return m_bDisableKnownClientList; }
	static bool		IsQueueListDisabled()				{ return m_bDisableQueueList; }
	static bool		IsFirstStart()						{ return m_bFirstStart; }
	static bool		UseCreditSystem()					{ return m_bCreditSystem; }
	static void		SetCreditSystem(bool m_bInCreditSystem) { m_bCreditSystem = m_bInCreditSystem; }

	static const CString& GetTxtEditor()				{ return m_strTxtEditor; }
	static const CString& GetVideoPlayer()				{ return m_strVideoPlayer; }
	static const CString& GetVideoPlayerArgs()			{ return m_strVideoPlayerArgs; }

	static UINT		GetFileBufferSize()					{ return m_uFileBufferSize; }
	static DWORD	GetFileBufferTimeLimit()			{ return m_uFileBufferTimeLimit; }
	static INT_PTR	GetQueueSize()						{ return m_iQueueSize; }
	static int		GetCommitFiles()					{ return m_iCommitFiles; }
	static bool		GetShowCopyEd2kLinkCmd()			{ return m_bShowCopyEd2kLinkCmd; }

	// Barry
	static UINT		Get3DDepth()						{ return depth3D; }
	static bool		AutoTakeED2KLinks()					{ return autotakeed2klinks; }
	static bool		AddNewFilesPaused()					{ return addnewfilespaused; }

	static bool		TransferlistRemainSortStyle()		{ return m_bTransflstRemain; }
	static void		TransferlistRemainSortStyle(bool in){ m_bTransflstRemain = in; }

	static DWORD	GetStatsColor(int index)			{ return m_adwStatsColors[index]; }
	static void		SetStatsColor(int index, DWORD value) { m_adwStatsColors[index] = value; }
	static int		GetNumStatsColors()					{ return _countof(m_adwStatsColors); }
	static void		GetAllStatsColors(int iCount, LPDWORD pdwColors);
	static bool		SetAllStatsColors(int iCount, const LPDWORD pdwColors);
	static void		ResetStatsColor(int index);
	static bool		HasCustomTaskIconColor()			{ return bHasCustomTaskIconColor; }

	static void		SetMaxConsPerFive(UINT in)			{ MaxConperFive = in; }
	static LPLOGFONT GetHyperTextLogFont()				{ return &m_lfHyperText; }
	static void		SetHyperTextFont(LPLOGFONT plf)		{ m_lfHyperText = *plf; }
	static LPLOGFONT GetLogFont()						{ return &m_lfLogText; }
	static void		SetLogFont(LPLOGFONT plf)			{ m_lfLogText = *plf; }
	static COLORREF GetLogErrorColor()					{ return m_crLogError; }
	static COLORREF GetLogWarningColor()				{ return m_crLogWarning; }
	static COLORREF GetLogSuccessColor()				{ return m_crLogSuccess; }

	static UINT		GetMaxConperFive()					{ return MaxConperFive; }
	static UINT		GetDefaultMaxConperFive();

	static bool		IsSafeServerConnectEnabled()		{ return m_bSafeServerConnect; }
	static void		SetSafeServerConnectEnabled(bool in) { m_bSafeServerConnect = in; }
	static bool		IsMoviePreviewBackup()				{ return moviePreviewBackup; }
	static int		GetPreviewSmallBlocks()				{ return m_iPreviewSmallBlocks; }
	static bool		GetPreviewCopiedArchives()			{ return m_bPreviewCopiedArchives; }
	static int		GetInspectAllFileTypes()			{ return m_iInspectAllFileTypes; }
	static int		GetExtractMetaData()				{ return m_iExtractMetaData; }
	static bool		GetAdjustNTFSDaylightFileTime()		{ return m_bAdjustNTFSDaylightFileTime; }
	static bool		GetRearrangeKadSearchKeywords()		{ return m_bRearrangeKadSearchKeywords; }

	static const CString& GetYourHostname()				{ return m_strYourHostname; }
	static void		SetYourHostname(LPCTSTR pszHostname){ m_strYourHostname = pszHostname; }
	static bool		IsCheckDiskspaceEnabled()			{ return checkDiskspace; }
	static UINT		GetMinFreeDiskSpace()				{ return m_uMinFreeDiskSpace; }
	static bool		GetSparsePartFiles();
	static void		SetSparsePartFiles(bool bEnable)	{ m_bSparsePartFiles = bEnable; }
	static bool		GetResolveSharedShellLinks()		{ return m_bResolveSharedShellLinks; }
	static bool		IsShowUpDownIconInTaskbar()			{ return m_bShowUpDownIconInTaskbar; }
	static bool		IsWin7TaskbarGoodiesEnabled()		{ return m_bShowWin7TaskbarGoodies; }
	static void		SetWin7TaskbarGoodiesEnabled(bool flag)	{ m_bShowWin7TaskbarGoodies = flag; }

	static void		SetMaxUpload(uint32 val);
	static void		SetMaxDownload(uint32 val);

	static WINDOWPLACEMENT GetEmuleWindowPlacement()	{ return EmuleWindowPlacement; }
	static void		SetWindowLayout(const WINDOWPLACEMENT &in) { EmuleWindowPlacement = in; }

	static bool		GetAutoConnectToStaticServersOnly()	{ return m_bAutoConnectToStaticServersOnly; }
	static UINT		GetUpdateDays()						{ return versioncheckdays; }
	static time_t	GetLastVC()							{ return versioncheckLastAutomatic; }
	static void		UpdateLastVC();
	static int		GetIPFilterLevel()					{ return filterlevel; }
	static const CString& GetMessageFilter()			{ return messageFilter; }
	static const CString& GetCommentFilter()			{ return commentFilter; }
	static void		SetCommentFilter(const CString &strFilter) { commentFilter = strFilter; }
	static const CString& GetFilenameCleanups()			{ return filenameCleanups; }

	static bool		ShowRatesOnTitle()					{ return showRatesInTitle; }
	static void		LoadCats();
	static const CString& GetDateTimeFormat()			{ return m_strDateTimeFormat; }
	static const CString& GetDateTimeFormat4Log()		{ return m_strDateTimeFormat4Log; }
	static const CString& GetDateTimeFormat4Lists()		{ return m_strDateTimeFormat4Lists; }

	// Download Categories (Ornis)
	static INT_PTR	AddCat(Category_Struct *cat)		{ catMap.Add(cat); return catMap.GetCount() - 1; }
	static bool		MoveCat(INT_PTR from, INT_PTR to);
	static void		RemoveCat(INT_PTR index);
	static INT_PTR	GetCatCount()						{ return catMap.GetCount(); }
	static bool		SetCatFilter(INT_PTR index, int filter);
	static int		GetCatFilter(INT_PTR index);
	static bool		GetCatFilterNeg(INT_PTR index);
	static void		SetCatFilterNeg(INT_PTR index, bool val);
	static Category_Struct* GetCategory(INT_PTR index)	{ return (index >= 0 && index<catMap.GetCount()) ? catMap[index] : NULL; }
	static const CString& GetCatPath(INT_PTR index)		{ return catMap[index]->strIncomingPath; }
	static DWORD	GetCatColor(INT_PTR index, int nDefault = COLOR_BTNTEXT);

	static bool		GetPreviewOnIconDblClk()			{ return m_bPreviewOnIconDblClk; }
	static bool		GetCheckFileOpen()					{ return m_bCheckFileOpen; }
	static bool		ShowRatingIndicator()				{ return indicateratings; }
	static bool		WatchClipboard4ED2KLinks()			{ return watchclipboard; }
	static bool		GetRemoveToBin()					{ return m_bRemove2bin; }
	static bool		GetFilterServerByIP()				{ return filterserverbyip; }

	static bool		GetLog2Disk()						{ return log2disk; }
	static bool		GetDebug2Disk()						{ return m_bVerbose && debug2disk; }
	static int		GetMaxLogBuff()						{ return iMaxLogBuff; }
	static UINT		GetMaxLogFileSize()					{ return uMaxLogFileSize; }
	static ELogFileFormat GetLogFileFormat()			{ return m_iLogFileFormat; }

	// Web Server
	static uint16	GetWSPort()							{ return m_nWebPort; }
	static bool		GetWSUseUPnP()						{ return m_bWebUseUPnP && GetWSIsEnabled(); }
	static void		SetWSPort(uint16 uPort)				{ m_nWebPort = uPort; }
	static const CString& GetWSPass()					{ return m_strWebPassword; }
	static void		SetWSPass(const CString &strNewPass);
	static bool		GetWSIsEnabled()					{ return m_bWebEnabled; }
	static void		SetWSIsEnabled(bool bEnable)		{ m_bWebEnabled = bEnable; }
	static bool		GetWebUseGzip()						{ return m_bWebUseGzip; }
	static void		SetWebUseGzip(bool bUse)			{ m_bWebUseGzip = bUse; }
	static int		GetWebPageRefresh()					{ return m_nWebPageRefresh; }
	static void		SetWebPageRefresh(int nRefresh)		{ m_nWebPageRefresh = nRefresh; }
	static bool		GetWSIsLowUserEnabled()				{ return m_bWebLowEnabled; }
	static void		SetWSIsLowUserEnabled(bool in)		{ m_bWebLowEnabled = in; }
	static const CString& GetWSLowPass()				{ return m_strWebLowPassword; }
	static int		GetWebTimeoutMins()					{ return m_iWebTimeoutMins; }
	static bool		GetWebAdminAllowedHiLevFunc()		{ return m_bAllowAdminHiLevFunc; }
	static void		SetWSLowPass(const CString &strNewPass);
	static const CUIntArray& GetAllowedRemoteAccessIPs() { return m_aAllowedRemoteAccessIPs; }
	static uint32	GetMaxWebUploadFileSizeMB()			{ return m_iWebFileUploadSizeLimitMB; }
	static bool		GetWebUseHttps()					{ return m_bWebUseHttps; }
	static void		SetWebUseHttps(bool bUse)			{ m_bWebUseHttps = bUse; }
	static const CString& GetWebCertPath()				{ return m_sWebHttpsCertificate; }
	static void		SetWebCertPath(const CString &path)		{ m_sWebHttpsCertificate = path; };
	static const CString& GetWebKeyPath()				{ return m_sWebHttpsKey; }
	static void		SetWebKeyPath(const CString &path)	{ m_sWebHttpsKey = path; };

	static void		SetMaxSourcesPerFile(UINT in)		{ maxsourceperfile = in; }
	static void		SetMaxConnections(UINT in)			{ maxconnections = in; }
	static void		SetMaxHalfConnections(UINT in)		{ maxhalfconnections = in; }
	static bool		IsSchedulerEnabled()				{ return scheduler; }
	static void		SetSchedulerEnabled(bool in)		{ scheduler = in; }
	static bool		GetDontCompressAvi()				{ return dontcompressavi; }

	static bool		MsgOnlyFriends()					{ return msgonlyfriends; }
	static bool		MsgOnlySecure()						{ return msgsecure; }
	static UINT		GetMsgSessionsMax()					{ return maxmsgsessions; }
	static bool		IsSecureIdentEnabled()				{ return m_bUseSecureIdent; } // use client credits->CryptoAvailable() to check if encryption is really available and not this function
	static bool		IsAdvSpamfilterEnabled()			{ return m_bAdvancedSpamfilter; }
	static bool		IsChatCaptchaEnabled()				{ return IsAdvSpamfilterEnabled() && m_bUseChatCaptchas; }
	static const CString& GetTemplate()					{ return m_strTemplateFile; }
	static void		SetTemplate(const CString &in)		{ m_strTemplateFile = in; }
	static bool		GetNetworkKademlia()				{ return networkkademlia && udpport > 0; }
	static void		SetNetworkKademlia(bool val);
	static bool		GetNetworkED2K()					{ return networked2k; }
	static void		SetNetworkED2K(bool val)			{ networked2k = val; }

	// deadlake PROXYSUPPORT
	static const ProxySettings& GetProxySettings()		{ return proxy; }
	static void		SetProxySettings(const ProxySettings &proxysettings) { proxy = proxysettings; }

	static bool		ShowCatTabInfos()					{ return showCatTabInfos; }
	static void		ShowCatTabInfos(bool in)			{ showCatTabInfos = in; }

	static bool		AutoFilenameCleanup()				{ return autofilenamecleanup; }
	static void		AutoFilenameCleanup(bool in)		{ autofilenamecleanup = in; }
	static void		SetFilenameCleanups(const CString &in) { filenameCleanups = in; }

	static bool		GetResumeSameCat()					{ return resumeSameCat; }
	static bool		IsGraphRecreateDisabled()			{ return dontRecreateGraphs; }
	static bool		IsExtControlsEnabled()				{ return m_bExtControls; }
	static void		SetExtControls(bool in)				{ m_bExtControls = in; }
	static bool		GetRemoveFinishedDownloads()		{ return m_bRemoveFinishedDownloads; }

	static INT_PTR	GetMaxChatHistoryLines()			{ return m_iMaxChatHistory; }
	static bool		GetUseAutocompletion()				{ return m_bUseAutocompl; }
	static bool		GetUseDwlPercentage()				{ return m_bShowDwlPercentage; }
	static void		SetUseDwlPercentage(bool in)		{ m_bShowDwlPercentage = in; }
	static bool		GetShowActiveDownloadsBold()		{ return m_bShowActiveDownloadsBold; }
	static bool		GetShowSharedFilesDetails()			{ return m_bShowSharedFilesDetails; }
	static void		SetShowSharedFilesDetails(bool bIn)	{ m_bShowSharedFilesDetails = bIn; }
	static bool		GetAutoShowLookups()				{ return m_bAutoShowLookups; }
	static void		SetAutoShowLookups(bool bIn)		{ m_bAutoShowLookups = bIn; }
	static bool		GetForceSpeedsToKB()				{ return m_bForceSpeedsToKB; }
	static bool		GetExtraPreviewWithMenu()			{ return m_bExtraPreviewWithMenu; }

	//Toolbar
	static const CString& GetToolbarSettings()			{ return m_sToolbarSettings; }
	static void		SetToolbarSettings(const CString &in) { m_sToolbarSettings = in; }
	static const CString& GetToolbarBitmapSettings()	{ return m_sToolbarBitmap; }
	static void		SetToolbarBitmapSettings(const CString &path) { m_sToolbarBitmap = path; }
	static EToolbarLabelType GetToolbarLabelSettings()	{ return m_nToolbarLabels; }
	static void		SetToolbarLabelSettings(EToolbarLabelType eLabelType)	{ m_nToolbarLabels = eLabelType; }
	static bool		GetReBarToolbar()					{ return m_bReBarToolbar; }
	static bool		GetUseReBarToolbar();
	static CSize	GetToolbarIconSize()				{ return m_sizToolbarIconSize; }
	static void		SetToolbarIconSize(CSize siz)		{ m_sizToolbarIconSize = siz; }

	static bool		IsTransToolbarEnabled()				{ return m_bWinaTransToolbar; }
	static bool		IsDownloadToolbarEnabled()			{ return m_bShowDownloadToolbar; }
	static void		SetDownloadToolbar(bool bShow)		{ m_bShowDownloadToolbar = bShow; }

	static int		GetSearchMethod()					{ return m_iSearchMethod; }
	static void		SetSearchMethod(int iMethod)		{ m_iSearchMethod = iMethod; }

	// ZZ:UploadSpeedSense -->
	static bool		IsDynUpEnabled();
	static void		SetDynUpEnabled(bool newValue)		{ m_bDynUpEnabled = newValue; }
	static int		GetDynUpPingTolerance()				{ return m_iDynUpPingTolerance; }
	static int		GetDynUpGoingUpDivider()			{ return m_iDynUpGoingUpDivider; }
	static int		GetDynUpGoingDownDivider()			{ return m_iDynUpGoingDownDivider; }
	static int		GetDynUpNumberOfPings()				{ return m_iDynUpNumberOfPings; }
	static bool		IsDynUpUseMillisecondPingTolerance() { return m_bDynUpUseMillisecondPingTolerance; } // EastShare - Added by TAHO, USS limit
	static int		GetDynUpPingToleranceMilliseconds()	{ return m_iDynUpPingToleranceMilliseconds; } // EastShare - Added by TAHO, USS limit
	static void		SetDynUpPingToleranceMilliseconds(int in) { m_iDynUpPingToleranceMilliseconds = in; }
	// ZZ:UploadSpeedSense <--

	static bool		GetA4AFSaveCpu()					{ return m_bA4AFSaveCpu; } // ZZ:DownloadManager

	static bool		GetHighresTimer()					{ return m_bHighresTimer; }

	static CString	GetHomepageBaseURL()				{ return GetHomepageBaseURLForLevel(GetWebMirrorAlertLevel()); }
	static CString	GetVersionCheckBaseURL();
	static CString	GetVersionCheckURL();
	static void		SetWebMirrorAlertLevel(uint8 newValue) { m_nWebMirrorAlertLevel = newValue; }
	static bool		IsDefaultNick(const CString &strCheck);
	static UINT		GetWebMirrorAlertLevel();
	static bool		UseSimpleTimeRemainingComputation()	{ return m_bUseOldTimeRemaining; }

	static bool		IsRunAsUserEnabled();
	static bool		IsPreferingRestrictedOverUser()		{ return m_bPreferRestrictedOverUser; }

	// PeerCache
	static bool		IsPeerCacheDownloadEnabled()		{ return (m_bPeerCacheEnabled && !IsClientCryptLayerRequested()); }
	static time_t	GetPeerCacheLastSearch()			{ return m_uPeerCacheLastSearch; }
	static bool		WasPeerCacheFound()					{ return m_bPeerCacheWasFound; }
	static void		SetPeerCacheLastSearch(time_t dwLastSearch) { m_uPeerCacheLastSearch = dwLastSearch; }
	static void		SetPeerCacheWasFound(bool bFound)	{ m_bPeerCacheWasFound = bFound; }
	static uint16	GetPeerCachePort()					{ return m_nPeerCachePort; }
	static void		SetPeerCachePort(uint16 nPort)		{ m_nPeerCachePort = nPort; }
	static bool		GetPeerCacheShow()					{ return m_bPeerCacheShow; }

	// Verbose log options
	static bool		GetEnableVerboseOptions()			{ return m_bEnableVerboseOptions; }
	static bool		GetVerbose()						{ return m_bVerbose; }
	static bool		GetFullVerbose()					{ return m_bVerbose && m_bFullVerbose; }
	static bool		GetDebugSourceExchange()			{ return m_bVerbose && m_bDebugSourceExchange; }
	static bool		GetLogBannedClients()				{ return m_bVerbose && m_bLogBannedClients; }
	static bool		GetLogRatingDescReceived()			{ return m_bVerbose && m_bLogRatingDescReceived; }
	static bool		GetLogSecureIdent()					{ return m_bVerbose && m_bLogSecureIdent; }
	static bool		GetLogFilteredIPs()					{ return m_bVerbose && m_bLogFilteredIPs; }
	static bool		GetLogFileSaving()					{ return m_bVerbose && m_bLogFileSaving; }
	static bool		GetLogA4AF()						{ return m_bVerbose && m_bLogA4AF; } // ZZ:DownloadManager
	static bool		GetLogUlDlEvents()					{ return m_bVerbose && m_bLogUlDlEvents; }
	static bool		GetLogKadSecurityEvents()			{ return m_bVerbose && true; }
	static bool		GetUseDebugDevice()					{ return m_bUseDebugDevice; }
	static int		GetDebugServerTCPLevel()			{ return m_iDebugServerTCPLevel; }
	static int		GetDebugServerUDPLevel()			{ return m_iDebugServerUDPLevel; }
	static int		GetDebugServerSourcesLevel()		{ return m_iDebugServerSourcesLevel; }
	static int		GetDebugServerSearchesLevel()		{ return m_iDebugServerSearchesLevel; }
	static int		GetDebugClientTCPLevel()			{ return m_iDebugClientTCPLevel; }
	static int		GetDebugClientUDPLevel()			{ return m_iDebugClientUDPLevel; }
	static int		GetDebugClientKadUDPLevel()			{ return m_iDebugClientKadUDPLevel; }
	static int		GetDebugSearchResultDetailLevel()	{ return m_iDebugSearchResultDetailLevel; }
	static int		GetVerboseLogPriority()				{ return	m_byLogLevel; }

	// Firewall settings
	static bool		IsOpenPortsOnStartupEnabled()		{ return m_bOpenPortsOnStartUp; }

	//AICH Hash
	static bool		IsTrustingEveryHash()				{ return m_bTrustEveryHash; } // this is a debug option

	static bool		IsRememberingDownloadedFiles()		{ return m_bRememberDownloadedFiles; }
	static bool		IsRememberingCancelledFiles()		{ return m_bRememberCancelledFiles; }
	static bool		DoPartiallyPurgeOldKnownFiles()		{ return m_bPartiallyPurgeOldKnownFiles; }
	static void		SetRememberDownloadedFiles(bool nv)	{ m_bRememberDownloadedFiles = nv; }
	static void		SetRememberCancelledFiles(bool nv)	{ m_bRememberCancelledFiles = nv; }
	// mail notifier
	static const EmailSettings &GetEmailSettings()		{ return m_email; };
	static void		SetEmailSettings(const EmailSettings &settings) { m_email = settings; }

	static bool		IsNotifierSendMailEnabled()			{ return m_email.bSendMail; }

	static void		SetNotifierSendMail(bool nv)		{ m_email.bSendMail = nv; }
	static bool		DoFlashOnNewMessage()				{ return m_bIconflashOnNewMessage; }
	static void		IniCopy(const CString &si, const CString &di);

	static void		EstimateMaxUploadCap(uint32 nCurrentUpload);
	static bool		GetAllocCompleteMode()				{ return m_bAllocFull; }
	static void		SetAllocCompleteMode(bool in)		{ m_bAllocFull = in; }

	// encryption
	static bool		IsClientCryptLayerSupported()		{ return m_bCryptLayerSupported; }
	static bool		IsClientCryptLayerRequested()		{ return IsClientCryptLayerSupported() && m_bCryptLayerRequested; }
	static bool		IsClientCryptLayerRequired()		{ return IsClientCryptLayerRequested() && m_bCryptLayerRequired; }
	static bool		IsClientCryptLayerRequiredStrict()	{ return false; } // not even incoming test connections will be answered
	static bool		IsServerCryptLayerUDPEnabled()		{ return IsClientCryptLayerSupported(); }
	static bool		IsServerCryptLayerTCPRequested()	{ return IsClientCryptLayerRequested(); }
	static uint32	GetKadUDPKey()						{ return m_dwKadUDPKey; }
	static uint8	GetCryptTCPPaddingLength()			{ return m_byCryptTCPPaddingLength; }

	// UPnP
	static bool		GetSkipWANIPSetup()					{ return m_bSkipWANIPSetup; }
	static bool		GetSkipWANPPPSetup()				{ return m_bSkipWANPPPSetup; }
	static bool		IsUPnPEnabled()						{ return m_bEnableUPnP; }
	static void		SetSkipWANIPSetup(bool nv)			{ m_bSkipWANIPSetup = nv; }
	static void		SetSkipWANPPPSetup(bool nv)			{ m_bSkipWANPPPSetup = nv; }
	static bool		CloseUPnPOnExit()					{ return m_bCloseUPnPOnExit; }
	static bool		IsWinServUPnPImplDisabled()			{ return m_bIsWinServImplDisabled; }
	static bool		IsMinilibUPnPImplDisabled()			{ return m_bIsMinilibImplDisabled; }
	static int		GetLastWorkingUPnPImpl()			{ return m_nLastWorkingImpl; }
	static void		SetLastWorkingUPnPImpl(int val)		{ m_nLastWorkingImpl = val; }

	// Spam filter
	static bool		IsSearchSpamFilterEnabled()			{ return m_bEnableSearchResultFilter; }

	static bool		IsStoringSearchesEnabled()			{ return m_bStoreSearches; }
	static bool		GetPreventStandby()					{ return m_bPreventStandby; }
	static uint16	GetRandomTCPPort();
	static uint16	GetRandomUDPPort();

	// Beta related
#ifdef _BETA
	static bool		ShouldBetaNag()						{ return !m_bBetaNaggingDone; }
	static void		SetDidBetaNagging()					{ m_bBetaNaggingDone = true; }
#endif
protected:
	static CString	m_strFileCommentsFilePath;
	static Preferences_Ext_Struct *prefsExt;
	static WORD		m_wWinVer;
	static CArray<Category_Struct*,Category_Struct*> catMap;
	static CString	m_astrDefaultDirs[13];
	static bool		m_abDefaultDirsCreated[13];
	static int		m_nCurrentUserDirMode; // Only for PPgTweaks

	static void		CreateUserHash();
	static void		SetStandardValues();
	static int		GetRecommendedMaxConnections();
	static void		LoadPreferences();
	static void		SavePreferences();
	static CString	GetHomepageBaseURLForLevel(int nLevel);
	static CString	GetDefaultDirectory(EDefaultDirectory eDirectory, bool bCreate = true);
};

extern CPreferences thePrefs;
extern bool g_bLowColorDesktop;