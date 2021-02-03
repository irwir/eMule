#pragma once
#include "ResizableLib/ResizableSheet.h"
#include "ListCtrlItemWalk.h"

class CListViewPropertySheet : public CResizableSheet
{
	DECLARE_DYNAMIC(CListViewPropertySheet)

public:
	CListViewPropertySheet() = default;
	explicit CListViewPropertySheet(UINT nIDCaption, CWnd *pParentWnd = NULL, UINT iSelectPage = 0);
	explicit CListViewPropertySheet(LPCTSTR pszCaption, CWnd *pParentWnd = NULL, UINT iSelectPage = 0);
	~CListViewPropertySheet() = default;

	CPtrArray& GetPages()								{ return m_pages; }
	const	CSimpleArray<CObject*>& GetItems() const	{ return m_aItems; }
	void	InsertPage(int iIndex, CPropertyPage *pPage);

protected:
	CSimpleArray<CObject*> m_aItems;
	void ChangedData();
	DECLARE_MESSAGE_MAP()
};

// CListViewWalkerPropertySheet

class CListViewWalkerPropertySheet : public CListViewPropertySheet
{
	DECLARE_DYNAMIC(CListViewWalkerPropertySheet)

public:
	explicit CListViewWalkerPropertySheet(CListCtrlItemWalk *pListCtrl)
		: m_pListCtrl(pListCtrl)
	{
	}
	~CListViewWalkerPropertySheet() = default;

	explicit CListViewWalkerPropertySheet(UINT nIDCaption, CWnd *pParentWnd = NULL, UINT iSelectPage = 0);
	explicit CListViewWalkerPropertySheet(LPCTSTR pszCaption, CWnd *pParentWnd = NULL, UINT iSelectPage = 0);
	virtual BOOL OnInitDialog();

protected:
	CListCtrlItemWalk *m_pListCtrl;
	CButton m_ctlPrev;
	CButton m_ctlNext;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnNext();
	afx_msg void OnPrev();
};