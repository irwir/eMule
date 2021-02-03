#pragma once
#include "TreeOptionsCtrlEx.h"

class CPPgIRC : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgIRC)

	enum
	{
		IDD = IDD_PPG_IRC
	};
	void LocalizeItemText(HTREEITEM item, UINT strid);

public:
	CPPgIRC();
	virtual	~CPPgIRC() = default;

	void Localize();

protected:
	bool m_bTimeStamp;
	bool m_bSoundEvents;
	bool m_bMiscMessage;
	bool m_bJoinMessage;
	bool m_bPartMessage;
	bool m_bQuitMessage;
	bool m_bPingPongMessage;
	bool m_bEmuleAddFriend;
	bool m_bEmuleAllowAddFriend;
	bool m_bEmuleSendLink;
	bool m_bAcceptLinks;
	bool m_bIRCAcceptLinksFriendsOnly;
	bool m_bHelpChannel;
	bool m_bChannelsOnConnect;
	bool m_bIRCEnableSmileys;
	bool m_bIRCEnableUTF8;

	CTreeOptionsCtrlEx m_ctrlTreeOptions;
	bool m_bInitializedTreeOpts;
	HTREEITEM m_htiSoundEvents;
	HTREEITEM m_htiTimeStamp;
	HTREEITEM m_htiInfoMessage;
	HTREEITEM m_htiMiscMessage;
	HTREEITEM m_htiJoinMessage;
	HTREEITEM m_htiPartMessage;
	HTREEITEM m_htiQuitMessage;
	HTREEITEM m_htiPingPongMessage;
	HTREEITEM m_htiEmuleProto;
	HTREEITEM m_htiEmuleAddFriend;
	HTREEITEM m_htiEmuleAllowAddFriend;
	HTREEITEM m_htiEmuleSendLink;
	HTREEITEM m_htiAcceptLinks;
	HTREEITEM m_htiAcceptLinksFriends;
	HTREEITEM m_htiHelpChannel;
	HTREEITEM m_htiChannelsOnConnect;
	HTREEITEM m_htiSmileys;
	HTREEITEM m_htiUTF8;

	void LoadSettings();
	void UpdateControls();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnKillActive();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg void OnSettingsChange()			{ SetModified(); }
	afx_msg void OnBtnClickPerform();
	afx_msg LRESULT OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};
