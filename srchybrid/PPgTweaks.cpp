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
#include "emule.h"
#include "SearchDlg.h"
#include "PPgTweaks.h"
#include "Scheduler.h"
#include "DownloadQueue.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "TransferDlg.h"
#include "emuledlg.h"
#include "SharedFilesWnd.h"
#include "ServerWnd.h"
#include "HelpIDs.h"
#include "Log.h"
#include "UserMsgs.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	DFLT_MAXCONPERFIVE	20
#define DFLT_MAXHALFOPEN	9

///////////////////////////////////////////////////////////////////////////////
// CPPgTweaks dialog

IMPLEMENT_DYNAMIC(CPPgTweaks, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgTweaks, CPropertyPage)
	ON_WM_HSCROLL()
	ON_WM_DESTROY()
	ON_MESSAGE(UM_TREEOPTSCTRL_NOTIFY, OnTreeOptsCtrlNotify)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_OPENPREFINI, OnBnClickedOpenprefini)
END_MESSAGE_MAP()

CPPgTweaks::CPPgTweaks()
	: CPropertyPage(CPPgTweaks::IDD)
	, m_ctrlTreeOptions(theApp.m_iDfltImageListColorFlags)
	, m_htiA4AFSaveCpu()
	, m_htiAutoArch()
	, m_htiAutoTakeEd2kLinks()
	, m_htiCheckDiskspace()
	, m_htiCloseUPnPPorts()
	, m_htiCommit()
	, m_htiCommitAlways()
	, m_htiCommitNever()
	, m_htiCommitOnShutdown()
	, m_htiConditionalTCPAccept()
	, m_htiCreditSystem()
	, m_htiDebug2Disk()
	, m_htiDebugSourceExchange()
	, m_htiDynUp()
	, m_htiDynUpEnabled()
	, m_htiDynUpGoingDownDivider()
	, m_htiDynUpGoingUpDivider()
	, m_htiDynUpMinUpload()
	, m_htiDynUpNumberOfPings()
	, m_htiDynUpPingTolerance()
	, m_htiDynUpPingToleranceGroup()
	, m_htiDynUpPingToleranceMilliseconds()
	, m_htiDynUpRadioPingTolerance()
	, m_htiDynUpRadioPingToleranceMilliseconds()
	, m_htiExtControls()
	, m_htiExtractMetaData()
	, m_htiExtractMetaDataID3Lib()
	, m_htiExtractMetaDataNever()
	, m_htiFilterLANIPs()
	, m_htiFirewallStartup()
	, m_htiFullAlloc()
	, m_htiImportParts()
	, m_htiLog2Disk()
	, m_htiLogA4AF()
	, m_htiLogBannedClients()
	, m_htiLogFileSaving()
	, m_htiLogFilteredIPs()
	, m_htiLogLevel()
	, m_htiLogRatingDescReceived()
	, m_htiLogSecureIdent()
	, m_htiLogUlDlEvents()
	, m_htiMaxCon5Sec()
	, m_htiMaxHalfOpen()
	, m_htiMinFreeDiskSpace()
	, m_htiResolveShellLinks()
	, m_htiServerKeepAliveTimeout()
	, m_htiShareeMule()
	, m_htiShareeMuleMultiUser()
	, m_htiShareeMuleOldStyle()
	, m_htiShareeMulePublicUser()
	, m_htiSkipWANIPSetup()
	, m_htiSkipWANPPPSetup()
	, m_htiSparsePartFiles()
	, m_htiTCPGroup()
	, m_htiUPnP()
	, m_htiVerbose()
	, m_htiVerboseGroup()
	, m_htiYourHostname()
	, m_fMinFreeDiskSpaceMB()
	, m_iQueueSize()
	, m_uFileBufferSize()
	, m_uServerKeepAliveTimeout()
	, m_iCommitFiles()
	, m_iDynUpGoingDownDivider()
	, m_iDynUpGoingUpDivider()
	, m_iDynUpMinUpload()
	, m_iDynUpNumberOfPings()
	, m_iDynUpPingTolerance()
	, m_iDynUpPingToleranceMilliseconds()
	, m_iDynUpRadioPingTolerance()
	, m_iExtractMetaData()
	, m_iLogLevel()
	, m_iMaxConnPerFive()
	, m_iMaxHalfOpen()
	, m_iShareeMule()
	, m_bA4AFSaveCpu()
	, m_bAutoArchDisable(true)
	, m_bAutoTakeEd2kLinks()
	, m_bCheckDiskspace()
	, m_bCloseUPnPOnExit(true)
	, m_bConditionalTCPAccept()
	, m_bCreditSystem()
	, m_bDebug2Disk()
	, m_bDebugSourceExchange()
	, m_bDynUpEnabled()
	, m_bExtControls()
	, m_bFilterLANIPs()
	, m_bFirewallStartup()
	, m_bFullAlloc()
	, m_bImportParts()
	, m_bInitializedTreeOpts()
	, m_bLog2Disk()
	, m_bLogA4AF()
	, m_bLogBannedClients()
	, m_bLogFileSaving()
	, m_bLogFilteredIPs()
	, m_bLogRatingDescReceived()
	, m_bLogSecureIdent()
	, m_bLogUlDlEvents()
	, m_bResolveShellLinks()
	, m_bShowedWarning()
	, m_bSkipWANIPSetup()
	, m_bSkipWANPPPSetup()
	, m_bSparsePartFiles()
	, m_bVerbose()
{
}

void CPPgTweaks::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILEBUFFERSIZE, m_ctlFileBuffSize);
	DDX_Control(pDX, IDC_QUEUESIZE, m_ctlQueueSize);
	DDX_Control(pDX, IDC_EXT_OPTS, m_ctrlTreeOptions);
	if (!m_bInitializedTreeOpts) {
		int iImgBackup = 8; // default icon
		int iImgLog = 8;
		int iImgDynyp = 8;
		int iImgConnection = 8;
		//	int iImgA4AF = 8;
		int iImgMetaData = 8;
		int iImgUPnP = 8;
		int iImgShareeMule = 8;
		CImageList *piml = m_ctrlTreeOptions.GetImageList(TVSIL_NORMAL);
		if (piml) {
			iImgBackup = piml->Add(CTempIconLoader(_T("Harddisk")));
			iImgLog = piml->Add(CTempIconLoader(_T("Log")));
			iImgDynyp = piml->Add(CTempIconLoader(_T("upload")));
			iImgConnection = piml->Add(CTempIconLoader(_T("connection")));
			//	iImgA4AF = piml->Add(CTempIconLoader(_T("Download")));
			iImgMetaData = piml->Add(CTempIconLoader(_T("MediaInfo")));
			iImgUPnP = piml->Add(CTempIconLoader(_T("connectedhighhigh")));
			iImgShareeMule = piml->Add(CTempIconLoader(_T("viewfiles")));
		}

		/////////////////////////////////////////////////////////////////////////////
		// TCP/IP group
		//
		m_htiTCPGroup = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_TCPIP_CONNS), iImgConnection, TVI_ROOT);
		m_htiMaxCon5Sec = m_ctrlTreeOptions.InsertItem(GetResString(IDS_MAXCON5SECLABEL), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiMaxCon5Sec, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiMaxHalfOpen = m_ctrlTreeOptions.InsertItem(GetResString(IDS_MAXHALFOPENCONS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiMaxHalfOpen, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiConditionalTCPAccept = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_CONDTCPACCEPT), m_htiTCPGroup, m_bConditionalTCPAccept);
		m_htiServerKeepAliveTimeout = m_ctrlTreeOptions.InsertItem(GetResString(IDS_SERVERKEEPALIVETIMEOUT), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiTCPGroup);
		m_ctrlTreeOptions.AddEditBox(m_htiServerKeepAliveTimeout, RUNTIME_CLASS(CNumTreeOptionsEdit));

		/////////////////////////////////////////////////////////////////////////////
		// Miscellaneous group
		//
		m_htiAutoTakeEd2kLinks = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_AUTOTAKEED2KLINKS), TVI_ROOT, m_bAutoTakeEd2kLinks);
		m_htiCreditSystem = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_USECREDITSYSTEM), TVI_ROOT, m_bCreditSystem);
		m_htiFirewallStartup = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_FO_PREF_STARTUP), TVI_ROOT, m_bFirewallStartup);
		m_htiFilterLANIPs = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_PW_FILTER), TVI_ROOT, m_bFilterLANIPs);
		m_htiExtControls = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SHOWEXTSETTINGS), TVI_ROOT, m_bExtControls);
		m_htiA4AFSaveCpu = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_A4AF_SAVE_CPU), TVI_ROOT, m_bA4AFSaveCpu); // ZZ:DownloadManager
		m_htiAutoArch = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_DISABLE_AUTOARCHPREV), TVI_ROOT, m_bAutoArchDisable);
		m_htiYourHostname = m_ctrlTreeOptions.InsertItem(GetResString(IDS_YOURHOSTNAME), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, TVI_ROOT);
		m_ctrlTreeOptions.AddEditBox(m_htiYourHostname, RUNTIME_CLASS(CTreeOptionsEditEx));

		/////////////////////////////////////////////////////////////////////////////
		// File related group
		//
		m_htiSparsePartFiles = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_SPARSEPARTFILES), TVI_ROOT, m_bSparsePartFiles);
		m_htiFullAlloc = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_FULLALLOC), TVI_ROOT, m_bFullAlloc);
		m_htiCheckDiskspace = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_CHECKDISKSPACE), TVI_ROOT, m_bCheckDiskspace);
		m_htiMinFreeDiskSpace = m_ctrlTreeOptions.InsertItem(GetResString(IDS_MINFREEDISKSPACE), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiCheckDiskspace);
		m_ctrlTreeOptions.AddEditBox(m_htiMinFreeDiskSpace, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiCommit = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_COMMITFILES), iImgBackup, TVI_ROOT);
		m_htiCommitNever = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_NEVER), m_htiCommit, m_iCommitFiles == 0);
		m_htiCommitOnShutdown = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_ONSHUTDOWN), m_htiCommit, m_iCommitFiles == 1);
		m_htiCommitAlways = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_ALWAYS), m_htiCommit, m_iCommitFiles == 2);
		m_htiExtractMetaData = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_EXTRACT_META_DATA), iImgMetaData, TVI_ROOT);
		m_htiExtractMetaDataNever = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_NEVER), m_htiExtractMetaData, m_iExtractMetaData == 0);
		m_htiExtractMetaDataID3Lib = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_META_DATA_ID3LIB), m_htiExtractMetaData, m_iExtractMetaData == 1);
		//m_htiExtractMetaDataMediaDet = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_META_DATA_MEDIADET), m_htiExtractMetaData, m_iExtractMetaData == 2);
		m_htiResolveShellLinks = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_RESOLVELINKS), TVI_ROOT, m_bResolveShellLinks);

		/////////////////////////////////////////////////////////////////////////////
		// Logging group
		//
		m_htiLog2Disk = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG2DISK), TVI_ROOT, m_bLog2Disk);
		if (thePrefs.GetEnableVerboseOptions()) {
			m_htiVerboseGroup = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_VERBOSE), iImgLog, TVI_ROOT);
			m_htiVerbose = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_ENABLED), m_htiVerboseGroup, m_bVerbose);
			m_htiLogLevel = m_ctrlTreeOptions.InsertItem(GetResString(IDS_LOG_LEVEL), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiVerboseGroup);
			m_ctrlTreeOptions.AddEditBox(m_htiLogLevel, RUNTIME_CLASS(CNumTreeOptionsEdit));
			m_htiDebug2Disk = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG2DISK), m_htiVerboseGroup, m_bDebug2Disk);
			m_htiDebugSourceExchange = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_DEBUG_SOURCE_EXCHANGE), m_htiVerboseGroup, m_bDebugSourceExchange);
			m_htiLogBannedClients = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_BANNED_CLIENTS), m_htiVerboseGroup, m_bLogBannedClients);
			m_htiLogRatingDescReceived = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_RATING_RECV), m_htiVerboseGroup, m_bLogRatingDescReceived);
			m_htiLogSecureIdent = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_SECURE_IDENT), m_htiVerboseGroup, m_bLogSecureIdent);
			m_htiLogFilteredIPs = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_FILTERED_IPS), m_htiVerboseGroup, m_bLogFilteredIPs);
			m_htiLogFileSaving = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_FILE_SAVING), m_htiVerboseGroup, m_bLogFileSaving);
			m_htiLogA4AF = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_A4AF), m_htiVerboseGroup, m_bLogA4AF); // ZZ:DownloadManager
			m_htiLogUlDlEvents = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_LOG_ULDL_EVENTS), m_htiVerboseGroup, m_bLogUlDlEvents);
		}

		/////////////////////////////////////////////////////////////////////////////
		// USS group
		//
		m_htiDynUp = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_DYNUP), iImgDynyp, TVI_ROOT);
		m_htiDynUpEnabled = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_DYNUPENABLED), m_htiDynUp, m_bDynUpEnabled);
		m_htiDynUpMinUpload = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DYNUP_MINUPLOAD), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiDynUp);
		m_ctrlTreeOptions.AddEditBox(m_htiDynUpMinUpload, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiDynUpPingTolerance = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DYNUP_PINGTOLERANCE), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiDynUp);
		m_ctrlTreeOptions.AddEditBox(m_htiDynUpPingTolerance, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiDynUpPingToleranceMilliseconds = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DYNUP_PINGTOLERANCE_MS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiDynUp);
		m_ctrlTreeOptions.AddEditBox(m_htiDynUpPingToleranceMilliseconds, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiDynUpPingToleranceGroup = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_DYNUP_RADIO_PINGTOLERANCE_HEADER), iImgDynyp, m_htiDynUp);
		m_htiDynUpRadioPingTolerance = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_DYNUP_RADIO_PINGTOLERANCE_PERCENT), m_htiDynUpPingToleranceGroup, m_iDynUpRadioPingTolerance == 0);
		m_htiDynUpRadioPingToleranceMilliseconds = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_DYNUP_RADIO_PINGTOLERANCE_MS), m_htiDynUpPingToleranceGroup, m_iDynUpRadioPingTolerance == 1);
		m_htiDynUpGoingUpDivider = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DYNUP_GOINGUPDIVIDER), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiDynUp);
		m_ctrlTreeOptions.AddEditBox(m_htiDynUpGoingUpDivider, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiDynUpGoingDownDivider = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DYNUP_GOINGDOWNDIVIDER), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiDynUp);
		m_ctrlTreeOptions.AddEditBox(m_htiDynUpGoingDownDivider, RUNTIME_CLASS(CNumTreeOptionsEdit));
		m_htiDynUpNumberOfPings = m_ctrlTreeOptions.InsertItem(GetResString(IDS_DYNUP_NUMBEROFPINGS), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_htiDynUp);
		m_ctrlTreeOptions.AddEditBox(m_htiDynUpNumberOfPings, RUNTIME_CLASS(CNumTreeOptionsEdit));

		/////////////////////////////////////////////////////////////////////////////
		// UPnP group
		//
		m_htiUPnP = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_UPNP), iImgUPnP, TVI_ROOT);
		m_htiCloseUPnPPorts = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_UPNPCLOSEONEXIT), m_htiUPnP, m_bCloseUPnPOnExit);
		m_htiSkipWANIPSetup = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_UPNPSKIPWANIP), m_htiUPnP, m_bSkipWANIPSetup);
		m_htiSkipWANPPPSetup = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_UPNPSKIPWANPPP), m_htiUPnP, m_bSkipWANPPPSetup);

		/////////////////////////////////////////////////////////////////////////////
		// eMule Shared Use
		//
		m_htiShareeMule = m_ctrlTreeOptions.InsertGroup(GetResString(IDS_SHAREEMULELABEL), iImgShareeMule, TVI_ROOT);
		m_htiShareeMuleMultiUser = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_SHAREEMULEMULTI), m_htiShareeMule, m_iShareeMule == 0);
		m_htiShareeMulePublicUser = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_SHAREEMULEPUBLIC), m_htiShareeMule, m_iShareeMule == 1);
		m_htiShareeMuleOldStyle = m_ctrlTreeOptions.InsertRadioButton(GetResString(IDS_SHAREEMULEOLD), m_htiShareeMule, m_iShareeMule == 2);

		m_htiImportParts = m_ctrlTreeOptions.InsertCheckBox(GetResString(IDS_ENABLEIMPORTPARTS), TVI_ROOT, m_bImportParts);

		m_ctrlTreeOptions.Expand(m_htiTCPGroup, TVE_EXPAND);
		if (m_htiVerboseGroup)
			m_ctrlTreeOptions.Expand(m_htiVerboseGroup, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiCommit, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiCheckDiskspace, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiDynUp, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiDynUpPingToleranceGroup, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiExtractMetaData, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiUPnP, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiShareeMule, TVE_EXPAND);
		m_ctrlTreeOptions.SendMessage(WM_VSCROLL, SB_TOP);
		m_bInitializedTreeOpts = true;
	}

	/////////////////////////////////////////////////////////////////////////////
	// TCP/IP group
	//
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiMaxCon5Sec, m_iMaxConnPerFive);
	DDV_MinMaxInt(pDX, m_iMaxConnPerFive, 1, INT_MAX);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiMaxHalfOpen, m_iMaxHalfOpen);
	DDV_MinMaxInt(pDX, m_iMaxHalfOpen, 1, INT_MAX);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiConditionalTCPAccept, m_bConditionalTCPAccept);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiServerKeepAliveTimeout, m_uServerKeepAliveTimeout);

	/////////////////////////////////////////////////////////////////////////////
	// Miscellaneous group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiAutoTakeEd2kLinks, m_bAutoTakeEd2kLinks);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiCreditSystem, m_bCreditSystem);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiFirewallStartup, m_bFirewallStartup);
	m_ctrlTreeOptions.SetCheckBoxEnable(m_htiFirewallStartup, thePrefs.GetWindowsVersion() == _WINVER_XP_ && !IsRunningXPSP2());
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiFilterLANIPs, m_bFilterLANIPs);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiExtControls, m_bExtControls);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiA4AFSaveCpu, m_bA4AFSaveCpu);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiYourHostname, m_sYourHostname);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiAutoArch, m_bAutoArchDisable);

	/////////////////////////////////////////////////////////////////////////////
	// File related group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiSparsePartFiles, m_bSparsePartFiles);
	m_ctrlTreeOptions.SetCheckBoxEnable(m_htiSparsePartFiles, thePrefs.GetWindowsVersion() != _WINVER_VISTA_ /*only disable on Vista, not later versions*/);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiImportParts, m_bImportParts);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiFullAlloc, m_bFullAlloc);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiCheckDiskspace, m_bCheckDiskspace);
	DDX_Text(pDX, IDC_EXT_OPTS, m_htiMinFreeDiskSpace, m_fMinFreeDiskSpaceMB);
	DDV_MinMaxFloat(pDX, m_fMinFreeDiskSpaceMB, 0.0, _UI32_MAX / (1024.0f * 1024.0f));
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiCommit, m_iCommitFiles);
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiExtractMetaData, m_iExtractMetaData);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiResolveShellLinks, m_bResolveShellLinks);

	/////////////////////////////////////////////////////////////////////////////
	// Logging group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLog2Disk, m_bLog2Disk);
	if (m_htiLogLevel) {
		DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiLogLevel, m_iLogLevel);
		DDV_MinMaxInt(pDX, m_iLogLevel, 1, 5);
	}
	if (m_htiVerbose)
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiVerbose, m_bVerbose);
	if (m_htiDebug2Disk) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiDebug2Disk, m_bDebug2Disk);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebug2Disk, m_bVerbose);
	}
	if (m_htiDebugSourceExchange) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiDebugSourceExchange, m_bDebugSourceExchange);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebugSourceExchange, m_bVerbose);
	}
	if (m_htiLogBannedClients) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogBannedClients, m_bLogBannedClients);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogBannedClients, m_bVerbose);
	}
	if (m_htiLogRatingDescReceived) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogRatingDescReceived, m_bLogRatingDescReceived);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogRatingDescReceived, m_bVerbose);
	}
	if (m_htiLogSecureIdent) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogSecureIdent, m_bLogSecureIdent);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogSecureIdent, m_bVerbose);
	}
	if (m_htiLogFilteredIPs) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogFilteredIPs, m_bLogFilteredIPs);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFilteredIPs, m_bVerbose);
	}
	if (m_htiLogFileSaving) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogFileSaving, m_bLogFileSaving);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFileSaving, m_bVerbose);
	}
	if (m_htiLogA4AF) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogA4AF, m_bLogA4AF);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogA4AF, m_bVerbose);
	}
	if (m_htiLogUlDlEvents) {
		DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiLogUlDlEvents, m_bLogUlDlEvents);
		m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogUlDlEvents, m_bVerbose);
	}

	/////////////////////////////////////////////////////////////////////////////
	// USS group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiDynUpEnabled, m_bDynUpEnabled);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDynUpMinUpload, m_iDynUpMinUpload);
	DDV_MinMaxInt(pDX, m_iDynUpMinUpload, 1, INT_MAX);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDynUpPingTolerance, m_iDynUpPingTolerance);
	DDV_MinMaxInt(pDX, m_iDynUpPingTolerance, 100, INT_MAX);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDynUpPingToleranceMilliseconds, m_iDynUpPingToleranceMilliseconds);
	DDV_MinMaxInt(pDX, m_iDynUpPingTolerance, 1, INT_MAX);
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiDynUpPingToleranceGroup, m_iDynUpRadioPingTolerance);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDynUpGoingUpDivider, m_iDynUpGoingUpDivider);
	DDV_MinMaxInt(pDX, m_iDynUpGoingUpDivider, 1, INT_MAX);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDynUpGoingDownDivider, m_iDynUpGoingDownDivider);
	DDV_MinMaxInt(pDX, m_iDynUpGoingDownDivider, 1, INT_MAX);
	DDX_TreeEdit(pDX, IDC_EXT_OPTS, m_htiDynUpNumberOfPings, m_iDynUpNumberOfPings);
	DDV_MinMaxInt(pDX, m_iDynUpNumberOfPings, 1, INT_MAX);

	/////////////////////////////////////////////////////////////////////////////
	// UPnP group
	//
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiCloseUPnPPorts, m_bCloseUPnPOnExit);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiSkipWANIPSetup, m_bSkipWANIPSetup);
	DDX_TreeCheck(pDX, IDC_EXT_OPTS, m_htiSkipWANPPPSetup, m_bSkipWANPPPSetup);

	/////////////////////////////////////////////////////////////////////////////
	// eMule Shared User
	//
	DDX_TreeRadio(pDX, IDC_EXT_OPTS, m_htiShareeMule, m_iShareeMule);
	m_ctrlTreeOptions.SetRadioButtonEnable(m_htiShareeMulePublicUser, thePrefs.GetWindowsVersion() >= _WINVER_VISTA_);
}

BOOL CPPgTweaks::OnInitDialog()
{
	m_iMaxConnPerFive = thePrefs.GetMaxConperFive();
	m_iMaxHalfOpen = thePrefs.GetMaxHalfConnections();
	m_bConditionalTCPAccept = thePrefs.GetConditionalTCPAccept();
	m_bAutoTakeEd2kLinks = thePrefs.AutoTakeED2KLinks();
	if (thePrefs.GetEnableVerboseOptions()) {
		m_bVerbose = thePrefs.m_bVerbose;
		m_bDebug2Disk = thePrefs.debug2disk;							// do *not* use the corresponding 'Get...' function here!
		m_bDebugSourceExchange = thePrefs.m_bDebugSourceExchange;		// do *not* use the corresponding 'Get...' function here!
		m_bLogBannedClients = thePrefs.m_bLogBannedClients;				// do *not* use the corresponding 'Get...' function here!
		m_bLogRatingDescReceived = thePrefs.m_bLogRatingDescReceived;	// do *not* use the corresponding 'Get...' function here!
		m_bLogSecureIdent = thePrefs.m_bLogSecureIdent;					// do *not* use the corresponding 'Get...' function here!
		m_bLogFilteredIPs = thePrefs.m_bLogFilteredIPs;					// do *not* use the corresponding 'Get...' function here!
		m_bLogFileSaving = thePrefs.m_bLogFileSaving;					// do *not* use the corresponding 'Get...' function here!
		m_bLogA4AF = thePrefs.m_bLogA4AF;							    // do *not* use the corresponding 'Get...' function here! // ZZ:DownloadManager
		m_bLogUlDlEvents = thePrefs.m_bLogUlDlEvents;
		m_iLogLevel = 5 - thePrefs.m_byLogLevel;
	}
	m_bLog2Disk = thePrefs.log2disk;
	m_bCreditSystem = thePrefs.m_bCreditSystem;
	m_iCommitFiles = thePrefs.m_iCommitFiles;
	m_iExtractMetaData = thePrefs.m_iExtractMetaData;
	m_bFilterLANIPs = thePrefs.filterLANIPs;
	m_bExtControls = thePrefs.m_bExtControls;
	m_uServerKeepAliveTimeout = thePrefs.m_dwServerKeepAliveTimeout / MIN2MS(1);
	m_bSparsePartFiles = thePrefs.GetSparsePartFiles();
	m_bImportParts = thePrefs.m_bImportParts;
	m_bFullAlloc = thePrefs.m_bAllocFull;
	m_bCheckDiskspace = thePrefs.checkDiskspace;
	m_bResolveShellLinks = thePrefs.GetResolveSharedShellLinks();
	m_fMinFreeDiskSpaceMB = (float)(thePrefs.m_uMinFreeDiskSpace / (1024.0 * 1024.0));
	m_sYourHostname = thePrefs.GetYourHostname();
	m_bFirewallStartup = ((thePrefs.GetWindowsVersion() == _WINVER_XP_) ? thePrefs.m_bOpenPortsOnStartUp : 0);
	m_bAutoArchDisable = !thePrefs.m_bAutomaticArcPreviewStart;

	m_bDynUpEnabled = thePrefs.m_bDynUpEnabled;
	m_iDynUpMinUpload = thePrefs.GetMinUpload();
	m_iDynUpPingTolerance = thePrefs.GetDynUpPingTolerance();
	m_iDynUpPingToleranceMilliseconds = thePrefs.GetDynUpPingToleranceMilliseconds();
	m_iDynUpRadioPingTolerance = static_cast<int>(thePrefs.IsDynUpUseMillisecondPingTolerance());
	m_iDynUpGoingUpDivider = thePrefs.GetDynUpGoingUpDivider();
	m_iDynUpGoingDownDivider = thePrefs.GetDynUpGoingDownDivider();
	m_iDynUpNumberOfPings = thePrefs.GetDynUpNumberOfPings();

	m_bCloseUPnPOnExit = thePrefs.CloseUPnPOnExit();
	m_bSkipWANIPSetup = thePrefs.GetSkipWANIPSetup();
	m_bSkipWANPPPSetup = thePrefs.GetSkipWANPPPSetup();

	m_iShareeMule = thePrefs.m_nCurrentUserDirMode;

	m_bA4AFSaveCpu = thePrefs.GetA4AFSaveCpu();

	m_ctrlTreeOptions.SetImageListColorFlags(theApp.m_iDfltImageListColorFlags);
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);
	m_ctrlTreeOptions.SetItemHeight(m_ctrlTreeOptions.GetItemHeight() + 2);

	m_uFileBufferSize = thePrefs.m_uFileBufferSize;
	m_ctlFileBuffSize.SetRange(16, 1024 + 512, TRUE);
	int iMin, iMax;
	m_ctlFileBuffSize.GetRange(iMin, iMax);
	m_ctlFileBuffSize.SetPos(m_uFileBufferSize / 1024);
	int iPage = 128;
	for (int i = ((iMin + iPage - 1) / iPage) * iPage; i < iMax; i += iPage)
		m_ctlFileBuffSize.SetTic(i);
	m_ctlFileBuffSize.SetPageSize(iPage);

	m_iQueueSize = thePrefs.m_iQueueSize;
	m_ctlQueueSize.SetRange(20, 100, TRUE);
	m_ctlQueueSize.SetPos((int)(m_iQueueSize / 100));
	m_ctlQueueSize.SetTicFreq(10);
	m_ctlQueueSize.SetPageSize(10);

	Localize();

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPgTweaks::OnKillActive()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit any possibly pending
	// data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();
	return CPropertyPage::OnKillActive();
}

BOOL CPPgTweaks::OnApply()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit any possibly pending
	// data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();

	if (!UpdateData())
		return FALSE;

	thePrefs.SetMaxConsPerFive(m_iMaxConnPerFive ? m_iMaxConnPerFive : DFLT_MAXCONPERFIVE);
	theApp.scheduler->original_cons5s = thePrefs.GetMaxConperFive();
	thePrefs.SetMaxHalfConnections(m_iMaxHalfOpen ? m_iMaxHalfOpen : DFLT_MAXHALFOPEN);
	thePrefs.m_bConditionalTCPAccept = m_bConditionalTCPAccept;

	if (thePrefs.AutoTakeED2KLinks() != m_bAutoTakeEd2kLinks) {
		thePrefs.autotakeed2klinks = m_bAutoTakeEd2kLinks;
		if (thePrefs.AutoTakeED2KLinks())
			Ask4RegFix(false, true, false);
		else
			RevertReg();
	}

	if (!thePrefs.log2disk && m_bLog2Disk)
		theLog.Open();
	else if (thePrefs.log2disk && !m_bLog2Disk)
		theLog.Close();
	thePrefs.log2disk = m_bLog2Disk;

	if (thePrefs.GetEnableVerboseOptions()) {
		if (!thePrefs.GetDebug2Disk() && m_bVerbose && m_bDebug2Disk)
			theVerboseLog.Open();
		else if (thePrefs.GetDebug2Disk() && (!m_bVerbose || !m_bDebug2Disk))
			theVerboseLog.Close();
		thePrefs.debug2disk = m_bDebug2Disk;

		thePrefs.m_bDebugSourceExchange = m_bDebugSourceExchange;
		thePrefs.m_bLogBannedClients = m_bLogBannedClients;
		thePrefs.m_bLogRatingDescReceived = m_bLogRatingDescReceived;
		thePrefs.m_bLogSecureIdent = m_bLogSecureIdent;
		thePrefs.m_bLogFilteredIPs = m_bLogFilteredIPs;
		thePrefs.m_bLogFileSaving = m_bLogFileSaving;
		thePrefs.m_bLogA4AF = m_bLogA4AF;
		thePrefs.m_bLogUlDlEvents = m_bLogUlDlEvents;
		thePrefs.m_byLogLevel = 5 - m_iLogLevel;

		thePrefs.m_bVerbose = m_bVerbose; // store after related options were stored!
	}

	thePrefs.m_bCreditSystem = m_bCreditSystem;
	thePrefs.m_iCommitFiles = m_iCommitFiles;
	thePrefs.m_iExtractMetaData = m_iExtractMetaData;
	thePrefs.filterLANIPs = m_bFilterLANIPs;
	thePrefs.m_uFileBufferSize = m_uFileBufferSize;
	thePrefs.m_iQueueSize = m_iQueueSize;

	bool bUpdateDLmenu = (thePrefs.m_bImportParts != m_bImportParts);
	thePrefs.m_bImportParts = m_bImportParts;
	if (thePrefs.m_bExtControls != m_bExtControls) {
		bUpdateDLmenu = true;
		thePrefs.m_bExtControls = m_bExtControls;
		theApp.emuledlg->searchwnd->CreateMenus();
		theApp.emuledlg->sharedfileswnd->sharedfilesctrl.CreateMenus();
	}
	if (bUpdateDLmenu)
		theApp.emuledlg->transferwnd->GetDownloadList()->CreateMenus();

	thePrefs.m_dwServerKeepAliveTimeout = MIN2MS(m_uServerKeepAliveTimeout);
	thePrefs.m_bSparsePartFiles = m_bSparsePartFiles;
	thePrefs.m_bAllocFull = m_bFullAlloc;
	thePrefs.checkDiskspace = m_bCheckDiskspace;
	thePrefs.m_bResolveSharedShellLinks = m_bResolveShellLinks;
	thePrefs.m_uMinFreeDiskSpace = (UINT)(m_fMinFreeDiskSpaceMB * (1024 * 1024));
	if (thePrefs.GetYourHostname() != m_sYourHostname) {
		thePrefs.SetYourHostname(m_sYourHostname);
		theApp.emuledlg->serverwnd->UpdateMyInfo();
	}
	thePrefs.m_bOpenPortsOnStartUp = m_bFirewallStartup;

	thePrefs.m_bDynUpEnabled = m_bDynUpEnabled;
	thePrefs.m_minupload = (uint32)m_iDynUpMinUpload;
	thePrefs.m_iDynUpPingTolerance = m_iDynUpPingTolerance;
	thePrefs.m_iDynUpPingToleranceMilliseconds = m_iDynUpPingToleranceMilliseconds;
	thePrefs.m_bDynUpUseMillisecondPingTolerance = (m_iDynUpRadioPingTolerance == 1);
	thePrefs.m_iDynUpGoingUpDivider = m_iDynUpGoingUpDivider;
	thePrefs.m_iDynUpGoingDownDivider = m_iDynUpGoingDownDivider;
	thePrefs.m_iDynUpNumberOfPings = m_iDynUpNumberOfPings;
	thePrefs.m_bAutomaticArcPreviewStart = !m_bAutoArchDisable;

	thePrefs.m_bCloseUPnPOnExit = m_bCloseUPnPOnExit;
	thePrefs.SetSkipWANIPSetup(m_bSkipWANIPSetup);
	thePrefs.SetSkipWANPPPSetup(m_bSkipWANPPPSetup);

	thePrefs.ChangeUserDirMode(m_iShareeMule);

	thePrefs.m_bA4AFSaveCpu = m_bA4AFSaveCpu;

	if (thePrefs.GetEnableVerboseOptions()) {
		theApp.emuledlg->serverwnd->ToggleDebugWindow();
		theApp.emuledlg->serverwnd->UpdateLogTabSelection();
	}
	theApp.downloadqueue->CheckDiskspace();

	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgTweaks::OnHScroll(UINT /*nSBCode*/, UINT /*nPos*/, CScrollBar *pScrollBar)
{
	if (pScrollBar->GetSafeHwnd() == m_ctlFileBuffSize.m_hWnd) {
		m_uFileBufferSize = m_ctlFileBuffSize.GetPos() * 1024;
		CString temp;
		temp.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_FILEBUFFERSIZE), (LPCTSTR)CastItoXBytes(m_uFileBufferSize));
		SetDlgItemText(IDC_FILEBUFFERSIZE_STATIC, temp);
		SetModified(TRUE);
	} else if (pScrollBar->GetSafeHwnd() == m_ctlQueueSize.m_hWnd) {
		m_iQueueSize = reinterpret_cast<CSliderCtrl*>(pScrollBar)->GetPos() * 100;
		CString temp;
		temp.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_QUEUESIZE), (LPCTSTR)GetFormatedUInt((ULONG)m_iQueueSize));
		SetDlgItemText(IDC_QUEUESIZE_STATIC, temp);
		SetModified(TRUE);
	}
}

void CPPgTweaks::LocalizeItemText(HTREEITEM item, UINT strid)
{
	if (item)
		m_ctrlTreeOptions.SetItemText(item, GetResString(strid));
}

void CPPgTweaks::LocalizeEditLabel(HTREEITEM item, UINT strid)
{
	if (item)
		m_ctrlTreeOptions.SetEditLabel(item, GetResString(strid));
}

void CPPgTweaks::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_TWEAK));
		SetDlgItemText(IDC_WARNING, GetResString(IDS_TWEAKS_WARNING));
		SetDlgItemText(IDC_PREFINI_STATIC, GetResString(IDS_PW_TWEAK));
		SetDlgItemText(IDC_OPENPREFINI, GetResString(IDS_OPENPREFINI));

		LocalizeEditLabel(m_htiDynUpGoingDownDivider, IDS_DYNUP_GOINGDOWNDIVIDER);
		LocalizeEditLabel(m_htiDynUpGoingUpDivider, IDS_DYNUP_GOINGUPDIVIDER);
		LocalizeEditLabel(m_htiDynUpMinUpload, IDS_DYNUP_MINUPLOAD);
		LocalizeEditLabel(m_htiDynUpNumberOfPings, IDS_DYNUP_NUMBEROFPINGS);
		LocalizeEditLabel(m_htiDynUpPingTolerance, IDS_DYNUP_PINGTOLERANCE);
		LocalizeEditLabel(m_htiLogLevel, IDS_LOG_LEVEL);
		LocalizeEditLabel(m_htiMaxCon5Sec, IDS_MAXCON5SECLABEL);
		LocalizeEditLabel(m_htiMaxHalfOpen, IDS_MAXHALFOPENCONS);
		LocalizeEditLabel(m_htiMinFreeDiskSpace, IDS_MINFREEDISKSPACE);
		LocalizeEditLabel(m_htiServerKeepAliveTimeout, IDS_SERVERKEEPALIVETIMEOUT);
		LocalizeEditLabel(m_htiYourHostname, IDS_YOURHOSTNAME);	// itsonlyme: hostnameSource
		LocalizeItemText(m_htiA4AFSaveCpu, IDS_A4AF_SAVE_CPU);
		LocalizeItemText(m_htiAutoArch, IDS_DISABLE_AUTOARCHPREV);
		LocalizeItemText(m_htiAutoTakeEd2kLinks, IDS_AUTOTAKEED2KLINKS);
		LocalizeItemText(m_htiCheckDiskspace, IDS_CHECKDISKSPACE);
		LocalizeItemText(m_htiCloseUPnPPorts, IDS_UPNPCLOSEONEXIT);
		LocalizeItemText(m_htiCommit, IDS_COMMITFILES);
		LocalizeItemText(m_htiCommitAlways, IDS_ALWAYS);
		LocalizeItemText(m_htiCommitNever, IDS_NEVER);
		LocalizeItemText(m_htiCommitOnShutdown, IDS_ONSHUTDOWN);
		LocalizeItemText(m_htiConditionalTCPAccept, IDS_CONDTCPACCEPT);
		LocalizeItemText(m_htiCreditSystem, IDS_USECREDITSYSTEM);
		LocalizeItemText(m_htiDebug2Disk, IDS_LOG2DISK);
		LocalizeItemText(m_htiDebugSourceExchange, IDS_DEBUG_SOURCE_EXCHANGE);
		LocalizeItemText(m_htiDynUp, IDS_DYNUP);
		LocalizeItemText(m_htiDynUpEnabled, IDS_DYNUPENABLED);
		LocalizeItemText(m_htiExtControls, IDS_SHOWEXTSETTINGS);
		LocalizeItemText(m_htiExtractMetaData, IDS_EXTRACT_META_DATA);
		LocalizeItemText(m_htiExtractMetaDataID3Lib, IDS_META_DATA_ID3LIB);
		//LocalizeItemText(m_htiExtractMetaDataMediaDet, IDS_META_DATA_MEDIADET);
		LocalizeItemText(m_htiExtractMetaDataNever, IDS_NEVER);
		LocalizeItemText(m_htiFilterLANIPs, IDS_PW_FILTER);
		LocalizeItemText(m_htiFirewallStartup, IDS_FO_PREF_STARTUP);
		LocalizeItemText(m_htiFullAlloc, IDS_FULLALLOC);
		LocalizeItemText(m_htiImportParts, IDS_ENABLEIMPORTPARTS);
		LocalizeItemText(m_htiLog2Disk, IDS_LOG2DISK);
		LocalizeItemText(m_htiLogA4AF, IDS_LOG_A4AF);
		LocalizeItemText(m_htiLogBannedClients, IDS_LOG_BANNED_CLIENTS);
		LocalizeItemText(m_htiLogFileSaving, IDS_LOG_FILE_SAVING);
		LocalizeItemText(m_htiLogFilteredIPs, IDS_LOG_FILTERED_IPS);
		LocalizeItemText(m_htiLogRatingDescReceived, IDS_LOG_RATING_RECV);
		LocalizeItemText(m_htiLogSecureIdent, IDS_LOG_SECURE_IDENT);
		LocalizeItemText(m_htiLogUlDlEvents, IDS_LOG_ULDL_EVENTS);
		LocalizeItemText(m_htiResolveShellLinks, IDS_RESOLVELINKS);
		LocalizeItemText(m_htiShareeMule, IDS_SHAREEMULELABEL);
		LocalizeItemText(m_htiShareeMuleMultiUser, IDS_SHAREEMULEMULTI);
		LocalizeItemText(m_htiShareeMuleOldStyle, IDS_SHAREEMULEOLD);
		LocalizeItemText(m_htiShareeMulePublicUser, IDS_SHAREEMULEPUBLIC);
		LocalizeItemText(m_htiSkipWANIPSetup, IDS_UPNPSKIPWANIP);
		LocalizeItemText(m_htiSkipWANPPPSetup, IDS_UPNPSKIPWANPPP);
		LocalizeItemText(m_htiSparsePartFiles, IDS_SPARSEPARTFILES);
		LocalizeItemText(m_htiTCPGroup, IDS_TCPIP_CONNS);
		LocalizeItemText(m_htiUPnP, IDS_UPNP);
		LocalizeItemText(m_htiVerbose, IDS_ENABLED);
		LocalizeItemText(m_htiVerboseGroup, IDS_VERBOSE);

		CString temp;
		temp.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_FILEBUFFERSIZE), (LPCTSTR)CastItoXBytes(m_uFileBufferSize));
		SetDlgItemText(IDC_FILEBUFFERSIZE_STATIC, temp);
		temp.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_QUEUESIZE), (LPCTSTR)GetFormatedUInt((ULONG)m_iQueueSize));
		SetDlgItemText(IDC_QUEUESIZE_STATIC, temp);
	}
}

void CPPgTweaks::OnDestroy()
{
	m_ctrlTreeOptions.DeleteAllItems();
	m_ctrlTreeOptions.DestroyWindow();
	m_bInitializedTreeOpts = false;
	m_htiTCPGroup = NULL;
	m_htiMaxCon5Sec = NULL;
	m_htiMaxHalfOpen = NULL;
	m_htiConditionalTCPAccept = NULL;
	m_htiAutoTakeEd2kLinks = NULL;
	m_htiVerboseGroup = NULL;
	m_htiVerbose = NULL;
	m_htiDebugSourceExchange = NULL;
	m_htiLogBannedClients = NULL;
	m_htiLogRatingDescReceived = NULL;
	m_htiLogSecureIdent = NULL;
	m_htiLogFilteredIPs = NULL;
	m_htiLogFileSaving = NULL;
	m_htiLogA4AF = NULL;
	m_htiLogLevel = NULL;
	m_htiLogUlDlEvents = NULL;
	m_htiCreditSystem = NULL;
	m_htiLog2Disk = NULL;
	m_htiDebug2Disk = NULL;
	m_htiCommit = NULL;
	m_htiCommitNever = NULL;
	m_htiCommitOnShutdown = NULL;
	m_htiCommitAlways = NULL;
	m_htiFilterLANIPs = NULL;
	m_htiExtControls = NULL;
	m_htiServerKeepAliveTimeout = NULL;
	m_htiSparsePartFiles = NULL;
	m_htiImportParts = NULL;
	m_htiFullAlloc = NULL;
	m_htiCheckDiskspace = NULL;
	m_htiMinFreeDiskSpace = NULL;
	m_htiYourHostname = NULL;
	m_htiFirewallStartup = NULL;
	m_htiDynUp = NULL;
	m_htiDynUpEnabled = NULL;
	m_htiDynUpMinUpload = NULL;
	m_htiDynUpPingTolerance = NULL;
	m_htiDynUpPingToleranceMilliseconds = NULL;
	m_htiDynUpPingToleranceGroup = NULL;
	m_htiDynUpRadioPingTolerance = NULL;
	m_htiDynUpRadioPingToleranceMilliseconds = NULL;
	m_htiDynUpGoingUpDivider = NULL;
	m_htiDynUpGoingDownDivider = NULL;
	m_htiDynUpNumberOfPings = NULL;
	m_htiA4AFSaveCpu = NULL;
	m_htiExtractMetaData = NULL;
	m_htiExtractMetaDataNever = NULL;
	m_htiExtractMetaDataID3Lib = NULL;
	m_htiAutoArch = NULL;
	m_htiUPnP = NULL;
	m_htiCloseUPnPPorts = NULL;
	m_htiSkipWANIPSetup = NULL;
	m_htiSkipWANPPPSetup = NULL;
	m_htiShareeMule = NULL;
	m_htiShareeMuleMultiUser = NULL;
	m_htiShareeMulePublicUser = NULL;
	m_htiShareeMuleOldStyle = NULL;
	//m_htiExtractMetaDataMediaDet = NULL;
	m_htiResolveShellLinks = NULL;

	CPropertyPage::OnDestroy();
}

LRESULT CPPgTweaks::OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == IDC_EXT_OPTS) {
		TREEOPTSCTRLNOTIFY *pton = (TREEOPTSCTRLNOTIFY*)lParam;
		if (m_htiVerbose && pton->hItem == m_htiVerbose) {
			BOOL bCheck;
			if (m_ctrlTreeOptions.GetCheckBox(m_htiVerbose, bCheck)) {
				if (m_htiDebug2Disk)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebug2Disk, bCheck);
				if (m_htiDebugSourceExchange)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiDebugSourceExchange, bCheck);
				if (m_htiLogBannedClients)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogBannedClients, bCheck);
				if (m_htiLogRatingDescReceived)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogRatingDescReceived, bCheck);
				if (m_htiLogSecureIdent)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogSecureIdent, bCheck);
				if (m_htiLogFilteredIPs)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFilteredIPs, bCheck);
				if (m_htiLogFileSaving)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogFileSaving, bCheck);
				if (m_htiLogA4AF)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogA4AF, bCheck);
				if (m_htiLogUlDlEvents)
					m_ctrlTreeOptions.SetCheckBoxEnable(m_htiLogUlDlEvents, bCheck);
			}
		} else if ((m_htiShareeMuleMultiUser  && pton->hItem == m_htiShareeMuleMultiUser)
				|| (m_htiShareeMulePublicUser && pton->hItem == m_htiShareeMulePublicUser)
				|| (m_htiShareeMuleOldStyle   && pton->hItem == m_htiShareeMuleOldStyle))
		{
			if (m_htiShareeMule && !m_bShowedWarning) {
				HTREEITEM tmp;
				int nIndex;
				m_ctrlTreeOptions.GetRadioButton(m_htiShareeMule, nIndex, tmp);
				if (nIndex != thePrefs.m_nCurrentUserDirMode) {
					// TODO offer cancel option
					LocMessageBox(IDS_SHAREEMULEWARNING, MB_ICONINFORMATION | MB_OK, 0);
					m_bShowedWarning = true;
				}
			}
		}
		SetModified();
	}
	return 0;
}

void CPPgTweaks::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_Extended_Settings);
}

BOOL CPPgTweaks::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgTweaks::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgTweaks::OnBnClickedOpenprefini()
{
	ShellOpenFile(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("preferences.ini"));
}