#pragma once
#include "ResizableLib/ResizableDialog.h"
#include "ListCtrlX.h"

struct SIPFilter;

class CIPFilterDlg : public CResizableDialog
{
	DECLARE_DYNAMIC(CIPFilterDlg)

	enum
	{
		IDD = IDD_IPFILTER
	};

public:
	explicit CIPFilterDlg(CWnd *pParent = NULL);   // standard constructor
	virtual	~CIPFilterDlg();

protected:
	static int sm_iSortColumn;
	CMenu *m_pMenuIPFilter;
	CListCtrlX m_ipfilter;
	HICON m_icoDlg;
	UINT m_uIPFilterItems;
	const SIPFilter **m_ppIPFilterItems;
	ULONG m_ulFilteredIPs;

	void SortIPFilterItems();
	void InitIPFilters();
	static bool FindItem(const CListCtrlX &lv, int iItem, DWORD_PTR lParam);

	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
//	afx_msg void OnContextMenu(CWnd*, CPoint);
	afx_msg void OnLvnColumnClickIPFilter(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnKeyDownIPFilter(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedAppend();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedSave();
	afx_msg void OnCopyIPFilter();
	afx_msg void OnDeleteIPFilter();
	afx_msg void OnSelectAllIPFilter();
	afx_msg void OnFind();
	afx_msg void OnLvnGetDispInfoIPFilter(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnDeleteItemIPFilter(LPNMHDR pNMHDR, LRESULT *pResult);
};