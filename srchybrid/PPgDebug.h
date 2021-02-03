#pragma once
#include "stdafx.h"
#include "emule.h"
#include "TreeOptionsCtrlEx.h"

#define	MAX_DETAIL_ITEMS	7
#define	MAX_INTEGER_ITEMS	1

class CPPgDebug : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgDebug)

	enum
	{
		IDD = IDD_PPG_DEBUG
	};

public:
	CPPgDebug();
	virtual	~CPPgDebug() = default;

protected:
	HTREEITEM m_htiServer;
	HTREEITEM m_htiClient;

	// detail level items
	HTREEITEM m_cb[MAX_DETAIL_ITEMS];
	HTREEITEM m_lv[MAX_DETAIL_ITEMS];
	BOOL m_checks[MAX_DETAIL_ITEMS];
	int m_levels[MAX_DETAIL_ITEMS];

	// integer items
	HTREEITEM m_htiInteger[MAX_INTEGER_ITEMS];
	int m_iValInteger[MAX_INTEGER_ITEMS];

	CTreeOptionsCtrlEx m_ctrlTreeOptions;
	bool m_bInitializedTreeOpts;

	void ClearAllMembers();

	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnKillActive();
	virtual void DoDataExchange(CDataExchange *pDX);	// DDX/DDV support
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg LRESULT OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM);
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
};