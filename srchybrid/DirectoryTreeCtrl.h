#pragma once
/////////////////////////////////////////////
// written by robert rostek - tecxx@rrs.at //
/////////////////////////////////////////////

#define MP_SHAREDFOLDERS_FIRST	46901

class CDirectoryTreeCtrl : public CTreeCtrl
{
	DECLARE_DYNAMIC(CDirectoryTreeCtrl)

public:
	// initialize control
	void Init();
	// get all shared directories
	void GetSharedDirectories(CStringList &list);
	// set shared directories
	void SetSharedDirectories(CStringList &list);

private:
	CImageList m_image;
	// add a new item
	HTREEITEM AddChildItem(HTREEITEM hRoot, const CString &strText);
	// add subdirectory items
	void AddSubdirectories(HTREEITEM hRoot, const CString &strDir);
	// return the full path of an item (like C:\abc\somewhere\inheaven\)
	CString GetFullPath(HTREEITEM hItem);
	// returns true if strDir has at least one subdirectory
	bool HasSubdirectories(const CString &strDir);
	// check status of an item has changed
	void CheckChanged(HTREEITEM hItem, bool bChecked);
	// returns true if a subdirectory of strDir is shared
	bool HasSharedSubdirectory(const CString &strDir);
	// when sharing a directory, make all parent directories bold
	void UpdateParentItems(HTREEITEM hChild);
	void ShareSubDirTree(HTREEITEM hItem, BOOL bRecurse);

	// share list access
	bool IsShared(const CString &strDir);
	void AddShare(const CString &strDir);
	void DelShare(const CString &strDir);
	void MarkChildren(HTREEITEM hChild, bool mark);

	CStringList m_lstShared;
	CString m_strLastRightClicked;
	bool m_bSelectSubDirs;

public:
	// construction / destruction
	CDirectoryTreeCtrl();
	virtual	~CDirectoryTreeCtrl();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnTvnItemexpanding(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnGetdispinfo(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnTvnDeleteItem(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg void OnRButtonDown(UINT, CPoint);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnDestroy();
};
