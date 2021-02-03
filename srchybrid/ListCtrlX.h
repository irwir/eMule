#pragma once

//////////////////////////////////////////////////////////////////////////////
// LCX_SORT_ORDER

typedef enum
{
	DESCENDING = 0,
	ASCENDING,
	NONE
} LCX_SORT_ORDER;


// Sort state image list
#define LCX_SORT_STATE_IMAGE_WIDTH	8
#define LCX_SORT_STATE_IMAGE_HEIGHT 7

#define LCX_IDX_SORT_IMG_ASCENDING	0	// Arrow-Up (same symbol and meaning as MS Outlook)
#define LCX_IDX_SORT_IMG_DESCENDING 1	// Arrow-Down (same symbol and meaning as MS Outlook)
#define LCX_SORT_STATE_IMAGES		2


//////////////////////////////////////////////////////////////////////////////
// LCX_COLUMN_INIT

typedef struct
{
	int				iColID;
	LPCTSTR			pszHeading;
	UINT			uHeadResID;		// optional, set to 0, if not needed
	UINT			uFormat;
	int				iWidth;
	int				iOrder;
	LCX_SORT_ORDER	eDfltSortOrder;
	LCX_SORT_ORDER	eSortOrder;		// changed during runtime
	LPCTSTR			pszSample;
} LCX_COLUMN_INIT;


//////////////////////////////////////////////////////////////////////////////
// Common List Ctrl helpers
void ReadColumnStats(int iColumns, LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection, LPCTSTR pszPrefix);
void WriteColumnStats(CListCtrl &lv, int iColumns, const LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection, LPCTSTR pszPrefix);

void InitColumnOrders(CListCtrl &lv, int iColumns, const LCX_COLUMN_INIT *pColumns);
void SetItemFocus(CListCtrl &ctl);
void UpdateHdrImageList(CListCtrl &lv, CImageList &imlHdr, UINT uIDHdrImgList, const CSize &sizeHdrImgListIcon, int iHdrImgListImages);
void CreateItemReport(CListCtrl &lv, CString &rstrReport);


/////////////////////////////////////////////////////////////////////////////
// CListCtrlX window

class CListCtrlX;
typedef bool (*LCX_FINDITEMFN)(const CListCtrlX &lv, int iItem, DWORD_PTR lParam);

class CListCtrlX : public CListCtrl
{
// Construction
public:
	CListCtrlX();
	virtual	~CListCtrlX() = default;

// Attributes
public:
	CWnd *m_pParent;
	CMenu *m_pMenu;
	LCX_FINDITEMFN m_pfnFindItem;
	DWORD_PTR m_lFindItemParam;
	UINT m_uIDMenu;
	UINT m_uIDAccel;
	BOOL m_bRouteMenuCmdsToMainFrame;

	void SetRegistryKey(LPCTSTR pszRegKey)		{ m_strRegKey = pszRegKey; }
	void SetRegistryPrefix(LPCTSTR pszPrefix)	{ m_strRegPrefix = pszPrefix; }

	void EnableHdrCtrlSortBitmaps(BOOL bUseHdrCtrlSortBitmaps = TRUE);
	void SetHdrImgList(UINT uResID, int cx, int cy, int iImages);
	int GetSortBitmapWidth() const				{ return m_bUseHdrCtrlSortBitmaps ? 12 : LCX_SORT_STATE_IMAGE_WIDTH; }

	int GetSortColumn() const					{ return m_iSortColumn; }
	void SetSortColumn(int iColumns, LCX_COLUMN_INIT *pColumns, int iSortColumn);
	void UpdateSortColumn(int iColumns, LCX_COLUMN_INIT *pColumns);

// Operations
public:
	void CreateColumns(int iColumns, LCX_COLUMN_INIT *pColumns);
	void ReadColumnStats(int iColumns, LCX_COLUMN_INIT *pColumns);
	void ReadColumnStats(int iColumns, LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection);
	void WriteColumnStats(int iColumns, const LCX_COLUMN_INIT *pColumns);
	void WriteColumnStats(int iColumns, const LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection);

	void InitColumnOrders(int iColumns, const LCX_COLUMN_INIT *pColumns);

	void SelectAllItems();
	void DeselectAllItems();
	void CheckSelectedItems(int nCurrItem);

	void UpdateSortOrder(LPNMLISTVIEW pnmlv, int iColumns, LCX_COLUMN_INIT *pColumns);
	void UpdateHdrCtrlSortBitmap(int iSortedColumn, LCX_SORT_ORDER eSortOrder);

	void UpdateHdrImageList();
	void ApplyImageList(HIMAGELIST himl);

	void OnFindStart();
	void OnFindNext();
	void OnFindPrev();
	int GetFindColumn() const					{ return m_iFindColumn; }
	const CString& GetFindText() const			{ return m_strFindText; }
	bool GetFindMatchCase() const				{ return m_bFindMatchCase; }
	static bool FindItem(const CListCtrlX &lv, int iItem, DWORD_PTR);

protected:
	CString m_strRegKey;
	CString m_strRegPrefix;
	CString m_strFindText;
	HACCEL m_hAccel;

	CImageList m_imlHdr;
	CSize m_sizeHdrImgListIcon;
	UINT m_uIDHdrImgList;
	int m_iHdrImgListImages;

	int m_iFindDirection;
	int m_iFindColumn;
	int m_iSortColumn;
	BOOL m_bUseHdrCtrlSortBitmaps;
	bool m_bFindMatchCase;
	void DoFindNext(BOOL bShowError);
	void DoFind(int iStartItem, int iDirection /*1=down, 0 = up*/, BOOL bShowError);

	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual void PreSubclassWindow();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSysColorChange();
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnInitMenuPopup(CMenu *pPopupMenu, UINT nIndex, BOOL bSysMenu);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg BOOL OnHdrBeginDrag(UINT, LPNMHDR, LRESULT*);
	afx_msg BOOL OnHdrEndDrag(UINT, LPNMHDR, LRESULT*);
	afx_msg LRESULT OnCopy(WPARAM, LPARAM);
};