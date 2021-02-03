#pragma once
#include "Preferences.h"
#include "resource.h"

class CIni;
class CMemoryDC;

///////////////////////////////////////////////////////////////////////////////
// CMuleListCtrl

#define MLC_DT_TEXT (DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS)

#define DFLT_FILENAME_COL_WIDTH		260
#define DFLT_FILETYPE_COL_WIDTH		 60
#define	DFLT_CLIENTNAME_COL_WIDTH	150
#define	DFLT_CLIENTSOFT_COL_WIDTH	100
#define	DFLT_SIZE_COL_WIDTH			 65
#define	DFLT_HASH_COL_WIDTH			220
#define	DFLT_DATARATE_COL_WIDTH		 65
#define	DFLT_PRIORITY_COL_WIDTH		 60
#define	DFLT_PARTSTATUS_COL_WIDTH	170
#define	DFLT_ARTIST_COL_WIDTH		100
#define	DFLT_ALBUM_COL_WIDTH		100
#define	DFLT_TITLE_COL_WIDTH		100
#define	DFLT_LENGTH_COL_WIDTH		 50
#define	DFLT_BITRATE_COL_WIDTH		 65
#define	DFLT_CODEC_COL_WIDTH		 50
#define	DFLT_FOLDER_COL_WIDTH		260

class CMuleListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CMuleListCtrl)

public:
	CMuleListCtrl(PFNLVCOMPARE pfnCompare = SortProc, DWORD dwParamSort = 0);
	virtual	~CMuleListCtrl();

	// Default sort proc, this does nothing
	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

	// Sets the list name, used for settings in "preferences.ini"
	void SetPrefsKey(LPCTSTR lpszName);

	// Save to preferences
	void SaveSettings();

	// Load from preferences
	void LoadSettings();

	DWORD SetExtendedStyle(DWORD dwNewStyle);

	// Hide the column
	void HideColumn(int iColumn);

	// Unhide the column
	void ShowColumn(int iColumn);

	// Check to see if the column is hidden
	bool IsColumnHidden(int iColumn) const
	{
		if (iColumn < 1 || iColumn >= m_iColumnsTracked)
			return false;
		return m_aColumns[iColumn].bHidden;
	}

	// Get the correct column width even if column is hidden
	int GetColumnWidth(int iColumn) const
	{
		if (iColumn < 0 || iColumn >= m_iColumnsTracked)
			return 0;
		if (m_aColumns[iColumn].bHidden)
			return m_aColumns[iColumn].iWidth;
		return CListCtrl::GetColumnWidth(iColumn);
	}

	// Get the column width and the alignment flags for 'DrawText'
	int GetColumnWidth(int iColumn, UINT &uDrawTextAlignment) const
	{
		if (iColumn < 0 || iColumn >= m_iColumnsTracked) {
			uDrawTextAlignment = DT_LEFT;
			return 0;
		}
		ASSERT(!m_aColumns[iColumn].bHidden);
		LVCOLUMN lvcol;
		lvcol.mask = LVCF_FMT | LVCF_WIDTH;
		if (!CListCtrl::GetColumn(iColumn, &lvcol)) {
			uDrawTextAlignment = DT_LEFT;
			return 0;
		}
		switch (lvcol.fmt & LVCFMT_JUSTIFYMASK) {
		default:
		case LVCFMT_LEFT:
			uDrawTextAlignment = DT_LEFT;
			break;
		case LVCFMT_RIGHT:
			uDrawTextAlignment = DT_RIGHT;
			break;
		case LVCFMT_CENTER:
			uDrawTextAlignment = DT_CENTER;
		}
		return lvcol.cx;
	}

	// Call SetRedraw to allow changes to be redrawn or to prevent changes from being redrawn.
	void SetRedraw(BOOL bRedraw = TRUE)
	{
		if (bRedraw) {
			if (m_iRedrawCount > 0 && --m_iRedrawCount == 0)
				CListCtrl::SetRedraw(TRUE);
		} else {
			if (m_iRedrawCount++ == 0)
				CListCtrl::SetRedraw(FALSE);
		}
	}

	// Sorts the list
	BOOL SortItems(PFNLVCOMPARE pfnCompare, DWORD_PTR dwData)
	{
		return CListCtrl::SortItems(pfnCompare, dwData);
	}

	// Sorts the list
	BOOL SortItems(DWORD dwData)
	{
		return CListCtrl::SortItems(m_SortProc, dwData);
	}

	// Sets the sorting procedure
	void SetSortProcedure(PFNLVCOMPARE funcSortProcedure)
	{
		m_SortProc = funcSortProcedure;
	}

	// Gets the sorting procedure
	PFNLVCOMPARE GetSortProcedure()
	{
		return m_SortProc;
	}

	// Retrieves the data (lParam) associated with a particular item.
	DWORD_PTR GetItemData(int iItem);

	// Retrieves the number of items in the control.
	int GetItemCount() const
	{
		return static_cast<int>(m_Params.GetCount());
	};

	enum ArrowType
	{
		arrowDown = IDB_DOWN,
		arrowUp = IDB_UP,
		arrowDoubleDown = IDB_DOWN2X,
		arrowDoubleUp = IDB_UP2X
	};

	int	GetSortType(ArrowType at);
	ArrowType GetArrowType(int iat);
	int GetSortItem() const
	{
		return m_iCurrentSortItem;
	}
	bool GetSortAscending() const
	{
		return m_atSortArrow == arrowUp || m_atSortArrow == arrowDoubleUp;
	}
	bool GetSortSecondValue() const
	{
		return m_atSortArrow == arrowDoubleDown || m_atSortArrow == arrowDoubleUp;
	}
	// Places a sort arrow in a column
	void SetSortArrow(int iColumn, ArrowType atType);
	void SetSortArrow()
	{
		SetSortArrow(m_iCurrentSortItem, m_atSortArrow);
	}
	void SetSortArrow(int iColumn, bool bAscending)
	{
		SetSortArrow(iColumn, bAscending ? arrowUp : arrowDown);
	}
	int GetNextSortOrder(int dwCurrentSortOrder) const;
	void UpdateSortHistory(int dwNewOrder, int dwInverseValue = 100);

	// General purpose listview find dialog+functions (optional)
	void	SetGeneralPurposeFind(bool bEnable, bool bCanSearchInAllColumns = true)
	{
		m_bGeneralPurposeFind = bEnable;
		m_bCanSearchInAllColumns = bCanSearchInAllColumns;
	}
	void	DoFind(int iStartItem, int iDirection /*1=down, 0 = up*/, BOOL bShowError);
	void	DoFindNext(BOOL bShowError);

	enum EUpdateMode
	{
		lazy,
		direct,
		none
	};
	EUpdateMode SetUpdateMode(EUpdateMode eUpdateMode);
	void SetAutoSizeWidth(int iAutoSizeWidth);

	int InsertColumn(int nCol, LPCTSTR lpszColumnHeading, int nFormat = LVCFMT_LEFT, int nWidth = -1, int nSubItem = -1, bool bHiddenByDefault = false);

	HIMAGELIST ApplyImageList(HIMAGELIST himl);
	void AutoSelectItem();
	void SetSkinKey(LPCTSTR pszKey)
	{
		m_strSkinKey = pszKey;
	}
	const CString& GetSkinKey() const
	{
		return m_strSkinKey;
	}

protected:
	virtual void PreSubclassWindow();
	virtual BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT *pResult);
	virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT *pResult);
	virtual BOOL PreTranslateMessage(MSG *pMsg);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnSysColorChange();
	afx_msg void OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnEndScrollList(LPNMHDR pNMHDR, LRESULT *pResult);

	int UpdateLocation(int iItem);
	int MoveItem(int iOldIndex, int iNewIndex);
	void SetColors();
	void DrawFocusRect(CDC *pDC, LPCRECT rcItem, BOOL bItemFocused, BOOL bCtrlFocused, BOOL bItemSelected);
	void InitItemMemDC(CMemoryDC *dc, LPDRAWITEMSTRUCT lpDrawItemStruct, BOOL &bCtrlFocused);

	static inline bool HaveIntersection(const CRect &rc1, const CRect &rc2)
	{
		return (rc1.left   < rc2.right
			 && rc1.top    < rc2.bottom
			 && rc1.right  > rc2.left
			 && rc1.bottom > rc2.top);
	}

	CString         m_Name;
	PFNLVCOMPARE    m_SortProc;
	DWORD           m_dwParamSort;
	CString			m_strSkinKey;
	COLORREF        m_crWindow;
	COLORREF        m_crWindowText;
	COLORREF        m_crWindowTextBk;
	COLORREF        m_crHighlight;
	COLORREF		m_crHighlightText;
	COLORREF		m_crGlow;
	COLORREF        m_crFocusLine;
	COLORREF        m_crNoHighlight;
	COLORREF        m_crNoFocusLine;
	NMLVCUSTOMDRAW  m_lvcd;
	BOOL            m_bCustomDraw;
	CImageList		m_imlHeaderCtrl;
	CList<int, int>	m_liSortHistory;
	UINT			m_uIDAccel;
	HACCEL			m_hAccel;
	enum EUpdateMode m_eUpdateMode;
	int				m_iAutoSizeWidth;
	static const int sm_iIconOffset;
	static const int sm_iLabelOffset;
	static const int sm_iSubItemInset;


	// General purpose listview find dialog+functions (optional)
	bool m_bGeneralPurposeFind;
	bool m_bCanSearchInAllColumns;
	CString m_strFindText;
	bool m_bFindMatchCase;
	int m_iFindDirection;
	int m_iFindColumn;
	void OnFindStart();
	void OnFindNext();
	void OnFindPrev();

private:
	static int	IndexToOrder(CHeaderCtrl *pHeader, int iIndex);

	struct MULE_COLUMN
	{
		int iWidth;
		int iLocation;
		bool bHidden;
	};

	int          m_iColumnsTracked;
	MULE_COLUMN *m_aColumns;

	int GetHiddenColumnCount() const
	{
		int iHidden = 0;
		for (int i = m_iColumnsTracked; --i >= 0;)
			iHidden += static_cast<int>(m_aColumns[i].bHidden);
		return iHidden;
	}

	int		  m_iCurrentSortItem;
	ArrowType m_atSortArrow;

	int		  m_iRedrawCount;
	CList<DWORD_PTR, DWORD_PTR> m_Params;

	DWORD_PTR GetParamAt(POSITION pos, int iPos)
	{
		DWORD_PTR lParam = m_Params.GetAt(pos);
		if (lParam == 0xFEEBDEEF) //same as MLC_MAGIC!
			m_Params.SetAt(pos, lParam = CListCtrl::GetItemData(iPos));
		return lParam;
	}

	CList<int> m_liDefaultHiddenColumns;
};

//void GetContextMenuPosition(CListCtrl &lv, CPoint &point);