#pragma once
#include "TreeOptionsCtrlEx.h"

class CPPgTweaks : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgTweaks)

	enum
	{
		IDD = IDD_PPG_TWEAKS
	};
	void LocalizeItemText(HTREEITEM item, UINT strid);
	void LocalizeEditLabel(HTREEITEM item, UINT strid);

public:
	CPPgTweaks();
	virtual	~CPPgTweaks() = default;

	void Localize();

protected:
	CSliderCtrl m_ctlFileBuffSize;
	CSliderCtrl m_ctlQueueSize;
	CTreeOptionsCtrlEx m_ctrlTreeOptions;
	CString m_sYourHostname;

	HTREEITEM m_htiA4AFSaveCpu;
	HTREEITEM m_htiAutoArch;
	HTREEITEM m_htiAutoTakeEd2kLinks;
	HTREEITEM m_htiCheckDiskspace;
	HTREEITEM m_htiCloseUPnPPorts;
	HTREEITEM m_htiCommit;
	HTREEITEM m_htiCommitAlways;
	HTREEITEM m_htiCommitNever;
	HTREEITEM m_htiCommitOnShutdown;
	HTREEITEM m_htiConditionalTCPAccept;
	HTREEITEM m_htiCreditSystem;
	HTREEITEM m_htiDebug2Disk;
	HTREEITEM m_htiDebugSourceExchange;
	HTREEITEM m_htiDynUp;
	HTREEITEM m_htiDynUpEnabled;
	HTREEITEM m_htiDynUpGoingDownDivider;
	HTREEITEM m_htiDynUpGoingUpDivider;
	HTREEITEM m_htiDynUpMinUpload;
	HTREEITEM m_htiDynUpNumberOfPings;
	HTREEITEM m_htiDynUpPingTolerance;
	HTREEITEM m_htiDynUpPingToleranceGroup;
	HTREEITEM m_htiDynUpPingToleranceMilliseconds;
	HTREEITEM m_htiDynUpRadioPingTolerance;
	HTREEITEM m_htiDynUpRadioPingToleranceMilliseconds;
	HTREEITEM m_htiExtControls;
	HTREEITEM m_htiExtractMetaData;
	HTREEITEM m_htiExtractMetaDataID3Lib;
	//HTREEITEM m_htiExtractMetaDataMediaDet;
	HTREEITEM m_htiExtractMetaDataNever;
	HTREEITEM m_htiFilterLANIPs;
	HTREEITEM m_htiFirewallStartup;
	HTREEITEM m_htiFullAlloc;
	HTREEITEM m_htiImportParts;
	HTREEITEM m_htiLog2Disk;
	HTREEITEM m_htiLogA4AF;
	HTREEITEM m_htiLogBannedClients;
	HTREEITEM m_htiLogFileSaving;
	HTREEITEM m_htiLogFilteredIPs;
	HTREEITEM m_htiLogLevel;
	HTREEITEM m_htiLogRatingDescReceived;
	HTREEITEM m_htiLogSecureIdent;
	HTREEITEM m_htiLogUlDlEvents;
	HTREEITEM m_htiMaxCon5Sec;
	HTREEITEM m_htiMaxHalfOpen;
	HTREEITEM m_htiMinFreeDiskSpace;
	HTREEITEM m_htiResolveShellLinks;
	HTREEITEM m_htiServerKeepAliveTimeout;
	HTREEITEM m_htiShareeMule;
	HTREEITEM m_htiShareeMuleMultiUser;
	HTREEITEM m_htiShareeMuleOldStyle;
	HTREEITEM m_htiShareeMulePublicUser;
	HTREEITEM m_htiSkipWANIPSetup;
	HTREEITEM m_htiSkipWANPPPSetup;
	HTREEITEM m_htiSparsePartFiles;
	HTREEITEM m_htiTCPGroup;
	HTREEITEM m_htiUPnP;
	HTREEITEM m_htiVerbose;
	HTREEITEM m_htiVerboseGroup;
	HTREEITEM m_htiYourHostname;

	float m_fMinFreeDiskSpaceMB;
	INT_PTR m_iQueueSize;
	UINT m_uFileBufferSize;
	UINT m_uServerKeepAliveTimeout;
	int m_iCommitFiles;
	int m_iDynUpGoingDownDivider;
	int m_iDynUpGoingUpDivider;
	int m_iDynUpMinUpload;
	int m_iDynUpNumberOfPings;
	int m_iDynUpPingTolerance;
	int m_iDynUpPingToleranceMilliseconds;
	int m_iDynUpRadioPingTolerance;
	int m_iExtractMetaData;
	int m_iLogLevel;
	int m_iMaxConnPerFive;
	int m_iMaxHalfOpen;
	int m_iShareeMule;

	bool m_bA4AFSaveCpu;
	bool m_bAutoArchDisable;
	bool m_bAutoTakeEd2kLinks;
	bool m_bCheckDiskspace;
	bool m_bCloseUPnPOnExit;
	bool m_bConditionalTCPAccept;
	bool m_bCreditSystem;
	bool m_bDebug2Disk;
	bool m_bDebugSourceExchange;
	bool m_bDynUpEnabled;
	bool m_bExtControls;
	bool m_bFilterLANIPs;
	bool m_bFirewallStartup;
	bool m_bFullAlloc;
	bool m_bImportParts;
	bool m_bInitializedTreeOpts;
	bool m_bLog2Disk;
	bool m_bLogA4AF;
	bool m_bLogBannedClients;
	bool m_bLogFileSaving;
	bool m_bLogFilteredIPs;
	bool m_bLogRatingDescReceived;
	bool m_bLogSecureIdent;
	bool m_bLogUlDlEvents;
	bool m_bResolveShellLinks;
	bool m_bShowedWarning;
	bool m_bSkipWANIPSetup;
	bool m_bSkipWANPPPSetup;
	bool m_bSparsePartFiles;
	bool m_bVerbose;

	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnKillActive();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnBnClickedOpenprefini();
};