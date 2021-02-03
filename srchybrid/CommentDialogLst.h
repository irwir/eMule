#pragma once
#include "ResizableLib/ResizablePage.h"
#include "CommentListCtrl.h"

class CPartFile;


///////////////////////////////////////////////////////////////////////////////
// CCommentDialogLst

class CCommentDialogLst : public CResizablePage
{
	DECLARE_DYNAMIC(CCommentDialogLst)

	enum
	{
		IDD = IDD_COMMENTLST
	};

public:
	CCommentDialogLst();
	virtual	~CCommentDialogLst() = default;
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void Localize();

protected:
	CCommentListCtrl m_lstComments;
	const CSimpleArray<CObject*> *m_paFiles;
	UINT_PTR m_timer;
	bool m_bDataChanged;

	void RefreshData(bool deleteOld = true);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedApply();
	afx_msg void OnBnClickedSearchKad();
	afx_msg void OnBnClickedFilter();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};