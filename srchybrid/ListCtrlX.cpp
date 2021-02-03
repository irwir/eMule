#include "stdafx.h"
#include "emule.h"
#include "ListCtrlX.h"
#include "ListViewSearchDlg.h"
#include "OtherFunctions.h"
#include "MenuCmds.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CListCtrlX

BEGIN_MESSAGE_MAP(CListCtrlX, CListCtrl)
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_WM_INITMENUPOPUP()
	ON_WM_MENUSELECT()
	ON_WM_CONTEXTMENU()
	ON_NOTIFY_EX(HDN_BEGINDRAG, 0, OnHdrBeginDrag)
	ON_NOTIFY_EX(HDN_ENDDRAG, 0, OnHdrEndDrag)
	ON_WM_KEYDOWN()
	ON_MESSAGE(WM_COPY, OnCopy)
END_MESSAGE_MAP()

CListCtrlX::CListCtrlX()
	: m_pParent()
	, m_pMenu()
	, m_pfnFindItem(FindItem)
	, m_lFindItemParam()
	, m_uIDMenu(UINT_MAX)
	, m_uIDAccel(UINT_MAX)
	, m_bRouteMenuCmdsToMainFrame()
	, m_hAccel()
	, m_uIDHdrImgList(UINT_MAX)
	, m_iHdrImgListImages()
	, m_iFindDirection(1)
	, m_iFindColumn()
	, m_iSortColumn(-1)
	, m_bUseHdrCtrlSortBitmaps()
	, m_bFindMatchCase()
{
}

void CListCtrlX::ReadColumnStats(int iColumns, LCX_COLUMN_INIT *pColumns)
{
	ASSERT(!m_strRegKey.IsEmpty());
	ReadColumnStats(iColumns, pColumns, m_strRegKey);
}

void CListCtrlX::ReadColumnStats(int iColumns, LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection)
{
	::ReadColumnStats(iColumns, pColumns, pszSection, m_strRegPrefix);

	DWORD dwVal = theApp.GetProfileInt(pszSection, m_strRegPrefix + _T("_SortColumn"), -1);
	m_iSortColumn = (short)LOWORD(dwVal);
	if (m_iSortColumn >= 0 && m_iSortColumn < iColumns) {
		LCX_SORT_ORDER eSortOrder = (LCX_SORT_ORDER)(short)HIWORD(dwVal);
		if (eSortOrder == ASCENDING || eSortOrder == DESCENDING)
			pColumns[m_iSortColumn].eSortOrder = eSortOrder;

		if (pColumns[m_iSortColumn].eSortOrder == NONE)
			pColumns[m_iSortColumn].eSortOrder = pColumns[m_iSortColumn].eDfltSortOrder != NONE ? pColumns[m_iSortColumn].eDfltSortOrder : ASCENDING;
	} else
		m_iSortColumn = -1;
}

void ReadColumnStats(int iColumns, LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection, LPCTSTR pszPrefix)
{
	for (int iCol = 0; iCol < iColumns; ++iCol) {
		DWORD dwVal = theApp.GetProfileInt(pszSection, CString(pszPrefix) + pColumns[iCol].pszHeading, -1);
		if (dwVal != _UI32_MAX) {
			pColumns[iCol].iWidth = (short)LOWORD(dwVal);
			if ((pColumns[iCol].iOrder = (short)HIWORD(dwVal)) < 0)
				pColumns[iCol].iOrder = 0; // COMCTL crashes on negative 'iOrder' values!
		}
	}
}

void CListCtrlX::WriteColumnStats(int iColumns, const LCX_COLUMN_INIT *pColumns)
{
	ASSERT(!m_strRegKey.IsEmpty());
	WriteColumnStats(iColumns, pColumns, m_strRegKey);
}

void CListCtrlX::WriteColumnStats(int iColumns, const LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection)
{
	::WriteColumnStats(*this, iColumns, pColumns, pszSection, m_strRegPrefix);

	DWORD dwVal = MAKELONG(m_iSortColumn, pColumns[m_iSortColumn].eSortOrder);
	theApp.WriteProfileInt(pszSection, m_strRegPrefix + _T("_SortColumn"), dwVal);
}

void WriteColumnStats(CListCtrl &lv, int iColumns, const LCX_COLUMN_INIT *pColumns, LPCTSTR pszSection, LPCTSTR pszPrefix)
{
	for (int iCol = 0; iCol < iColumns; ++iCol) {
		LVCOLUMN lvc;
		lvc.mask = LVCF_WIDTH | LVCF_ORDER;
		if (lv.GetColumn(iCol, &lvc) && (lvc.cx != pColumns[iCol].iWidth || lvc.iOrder != pColumns[iCol].iOrder)) {
			DWORD dwVal = MAKELONG(lvc.cx, lvc.iOrder);
			theApp.WriteProfileInt(pszSection, CString(pszPrefix) + pColumns[iCol].pszHeading, dwVal);
		}
	}
}

int CListCtrlX::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CListCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	// If we want to handle the VK_RETURN key, we have to do that via accelerators!
	if (m_uIDAccel != UINT_MAX) {
		m_hAccel = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(m_uIDAccel));
		ASSERT(m_hAccel);
	}

	return 0;
}

void CListCtrlX::PreSubclassWindow()
{
	CListCtrl::PreSubclassWindow();

	// If we want to handle the VK_RETURN key, we have to do that via accelerators!
	if (m_uIDAccel != UINT_MAX) {
		m_hAccel = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(m_uIDAccel));
		ASSERT(m_hAccel);
	}

	if (thePrefs.GetUseSystemFontForMainControls())
		SendMessage(WM_SETFONT, NULL, FALSE);
}

BOOL CListCtrlX::PreTranslateMessage(MSG *pMsg)
{
	if (m_hAccel && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST) {
		// If we want to handle the VK_RETURN key, we have to do that via accelerators!
		if (TranslateAccelerator(m_hWnd, m_hAccel, pMsg))
			return TRUE;
	}

	return CListCtrl::PreTranslateMessage(pMsg);
}

void CListCtrlX::CreateColumns(int iColumns, LCX_COLUMN_INIT *pColumns)
{
	InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, 0);
	for (int iCol = 0; iCol < iColumns; ++iCol) {
		CString strHeading;
		if (pColumns[iCol].uHeadResID)
			strHeading = GetResString(pColumns[iCol].uHeadResID);
		if (strHeading.IsEmpty())
			strHeading = pColumns[iCol].pszHeading;

		int iColWidth;
		if (pColumns[iCol].iWidth >= 0)
			iColWidth = pColumns[iCol].iWidth;
		else if (pColumns[iCol].pszSample) {
			// Get the 'Optimal Column Width'
			int iWidthSample = GetStringWidth(pColumns[iCol].pszSample);
			int iWidthHeader = GetStringWidth(strHeading);
			iWidthHeader += 30; // if using the COMCTL 6.0 header bitmaps (up/down arrows), we need more space

			iColWidth = 6 + __max(iWidthSample, iWidthHeader) + 6;	// left+right margin
			if (pColumns[iCol].uFormat & LVCFMT_RIGHT) // right-justified text(!)
				iColWidth += 4;
		} else
			iColWidth = 0;

		LVCOLUMN lvc;
		lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
		lvc.pszText = const_cast<LPTSTR>((LPCTSTR)strHeading);
		lvc.cx = iColWidth;
		lvc.fmt = pColumns[iCol].uFormat;
		lvc.iSubItem = pColumns[iCol].iColID;
		InsertColumn(pColumns[iCol].iColID + 1/*skip dummy column*/, &lvc);
	}
	DeleteColumn(0);
}

void CListCtrlX::EnableHdrCtrlSortBitmaps(BOOL bUseHdrCtrlSortBitmaps)
{
	if (bUseHdrCtrlSortBitmaps && theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 0, 0, 0))
		m_bUseHdrCtrlSortBitmaps = TRUE;
	else {
#ifdef IDB_SORT_STATES
		SetHdrImgList(IDB_SORT_STATES, LCX_SORT_STATE_IMAGE_WIDTH, LCX_SORT_STATE_IMAGE_HEIGHT, LCX_SORT_STATE_IMAGES);
#else
		//ASSERT(0);
#endif
	}
}

void CListCtrlX::SetHdrImgList(UINT uResID, int cx, int cy, int iImages)
{
	m_uIDHdrImgList = uResID;
	m_sizeHdrImgListIcon.cx = cx;
	m_sizeHdrImgListIcon.cy = cy;
	m_iHdrImgListImages = iImages;
}

void CListCtrlX::SetSortColumn(int iColumns, LCX_COLUMN_INIT *pColumns, int iSortColumn)
{
	UNREFERENCED_PARAMETER(iColumns);
	ASSERT(iSortColumn < iColumns);

	m_iSortColumn = iSortColumn;
	pColumns[m_iSortColumn].eSortOrder = pColumns[m_iSortColumn].eDfltSortOrder;

	UpdateSortColumn(iColumns, pColumns);
}

void CListCtrlX::UpdateSortColumn(int iColumns, LCX_COLUMN_INIT *pColumns)
{
	UNREFERENCED_PARAMETER(iColumns);
	ASSERT(m_iSortColumn < iColumns);

	UpdateHdrCtrlSortBitmap(m_iSortColumn, pColumns[m_iSortColumn].eSortOrder);
}

void CListCtrlX::UpdateSortOrder(LPNMLISTVIEW pnmlv, int iColumns, LCX_COLUMN_INIT *pColumns)
{
	ASSERT(pnmlv->iItem == -1);
	ASSERT(pnmlv->iSubItem >= 0 && pnmlv->iSubItem < iColumns);
	(void)iColumns;

	// Get sorting order for a column
	if (pnmlv->iSubItem == m_iSortColumn) {
		// The sorting order is toggled, only if the user has clicked on the
		// same column which was used for the prev. sorting (like Windows
		// Explorer)
		if (pColumns[m_iSortColumn].eSortOrder == ASCENDING)
			pColumns[m_iSortColumn].eSortOrder = DESCENDING;
		else
			pColumns[m_iSortColumn].eSortOrder = ASCENDING;
	} else {
		// Every time the user has clicked a column which was *not* used for
		// the prev. sorting, use the 'default' sorting order (like Windows
		// Explorer resp. MS Outlook)
		m_iSortColumn = pnmlv->iSubItem;
		pColumns[m_iSortColumn].eSortOrder = pColumns[m_iSortColumn].eDfltSortOrder;
	}

	UpdateHdrCtrlSortBitmap(m_iSortColumn, pColumns[m_iSortColumn].eSortOrder);
}

void CListCtrlX::UpdateHdrCtrlSortBitmap(int iSortedColumn, LCX_SORT_ORDER eSortOrder)
{
	//////////////////////////////////////////////////////////////////////////
	// Update the listview column headers to show the current sorting order
	//
	CHeaderCtrl *pHdrCtrl = GetHeaderCtrl();
	if (pHdrCtrl != NULL) {
		int iColumns = pHdrCtrl->GetItemCount();
		for (int i = 0; i < iColumns; ++i) {
			HDITEM hdi;
			hdi.mask = HDI_FORMAT;
			pHdrCtrl->GetItem(i, &hdi);

			if (i == iSortedColumn) {
				if (m_bUseHdrCtrlSortBitmaps) {
					hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
					hdi.fmt |= (eSortOrder == ASCENDING) ? HDF_SORTUP : HDF_SORTDOWN;
				} else if (m_uIDHdrImgList != UINT_MAX) {
					hdi.mask |= HDI_FORMAT | HDI_IMAGE;
					hdi.fmt |= HDF_IMAGE | HDF_BITMAP_ON_RIGHT;
					hdi.iImage = (eSortOrder == ASCENDING) ? LCX_IDX_SORT_IMG_ASCENDING : LCX_IDX_SORT_IMG_DESCENDING;
				}
				pHdrCtrl->SetItem(i, &hdi);
			} else {
				if (m_bUseHdrCtrlSortBitmaps)
					hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
				else
					hdi.fmt &= ~(HDF_IMAGE | HDF_BITMAP_ON_RIGHT);
				pHdrCtrl->SetItem(i, &hdi);
			}
		}
	}
}

void CListCtrlX::UpdateHdrImageList()
{
	if (m_uIDHdrImgList != UINT_MAX
		&& m_sizeHdrImgListIcon.cx > 0
		&& m_sizeHdrImgListIcon.cy > 0
		&& m_iHdrImgListImages > 0)
	{
		::UpdateHdrImageList(*this, m_imlHdr, m_uIDHdrImgList, m_sizeHdrImgListIcon, m_iHdrImgListImages);
	}
}

void UpdateHdrImageList(CListCtrl &lv, CImageList &imlHdr, UINT uIDHdrImgList
	, const CSize &sizeHdrImgListIcon, int iHdrImgListImages)
{
	CHeaderCtrl *pHdrCtrl = lv.GetHeaderCtrl();
	if (!pHdrCtrl)
		return;
	HINSTANCE hInstRes = AfxFindResourceHandle(MAKEINTRESOURCE(uIDHdrImgList), RT_BITMAP);
	if (!hInstRes)
		return;
	// Explicitly map the bitmap (which comes in default 3D colors) to the current system colors
	HBITMAP hbmHdr = (HBITMAP)::LoadImage(hInstRes, MAKEINTRESOURCE(uIDHdrImgList), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADMAP3DCOLORS);
	if (hbmHdr != NULL) {
		CBitmap bmSortStates;
		bmSortStates.Attach(hbmHdr);

		// Create image list with a mask (NOTE: The mask is needed for WinXP!)
		HIMAGELIST himlHeaderOld = imlHdr.Detach();
		if (imlHdr.Create(sizeHdrImgListIcon.cx, sizeHdrImgListIcon.cy, ILC_COLOR | ILC_MASK, iHdrImgListImages, 0)) {
			// Fill images and masks into image list
			VERIFY(imlHdr.Add(&bmSortStates, RGB(255, 0, 255)) >= 0);

			// To avoid drawing problems (which occur only with an image list *with* a mask) while
			// resizing list view columns which have the header control bitmap right aligned, set
			// the background color of the image list.
			if (theApp.m_ullComCtrlVer < MAKEDLLVERULL(6, 0, 0, 0))
				imlHdr.SetBkColor(::GetSysColor(COLOR_BTNFACE));

			// When setting the image list for the header control, this *ALWAYS* returns
			// the image list which was set for the list view control!!!
			pHdrCtrl->SetImageList(&imlHdr);
			if (himlHeaderOld != NULL)
				::ImageList_Destroy(himlHeaderOld);

#if defined(HDM_GETBITMAPMARGIN) && defined(HDM_SETBITMAPMARGIN)
			int iBmpMargin = (int)pHdrCtrl->SendMessage(HDM_GETBITMAPMARGIN); // Win2000: default is '6' (3*GetSystemMetrics(SM_CXEDGE))
			// Use same bitmap margin as Windows (W2K) Explorer -- this saves some pixels which
			// may be required for rather small column titles!
			int iNewBmpMargin = ::GetSystemMetrics(SM_CXEDGE) + ::GetSystemMetrics(SM_CXEDGE) / 2;
			if (iNewBmpMargin < iBmpMargin)
				pHdrCtrl->SendMessage(HDM_SETBITMAPMARGIN, iNewBmpMargin);
#endif
		}
	}
}

void CListCtrlX::ApplyImageList(HIMAGELIST himl)
{
	SendMessage(LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himl);
	if (m_imlHdr.m_hImageList != NULL) {
		// Must *again* set the image list for the header control, because LVM_SETIMAGELIST
		// always resets any already specified header control image lists!
		GetHeaderCtrl()->SetImageList(&m_imlHdr);
	}
}

BOOL CListCtrlX::OnHdrBeginDrag(UINT, LPNMHDR, LRESULT*)
{
	if (theApp.m_ullComCtrlVer < MAKEDLLVERULL(6, 0, 0, 0)) {
		CImageList *piml = GetHeaderCtrl()->GetImageList();
		if (piml != NULL)
			piml->SetBkColor(::GetSysColor(COLOR_3DSHADOW));
	}
	return FALSE; // *Force* default processing of notification!
}

BOOL CListCtrlX::OnHdrEndDrag(UINT, LPNMHDR, LRESULT*)
{
	if (theApp.m_ullComCtrlVer < MAKEDLLVERULL(6, 0, 0, 0)) {
		CImageList *piml = GetHeaderCtrl()->GetImageList();
		if (piml != NULL)
			piml->SetBkColor(::GetSysColor(COLOR_BTNFACE));
	}
	return FALSE; // *Force* default processing of notification!
}

void CListCtrlX::SelectAllItems()
{
	SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
}

void CListCtrlX::DeselectAllItems()
{
	SetItemState(-1, 0, LVIS_SELECTED);
}

void CListCtrlX::OnSysColorChange()
{
	CListCtrl::OnSysColorChange();
	UpdateHdrImageList();
}

void CListCtrlX::OnSetFocus(CWnd *pOldWnd)
{
	CListCtrl::OnSetFocus(pOldWnd);
	SetItemFocus(*this);
}

void CListCtrlX::OnDestroy()
{
	CListCtrl::OnDestroy();
	m_imlHdr.DeleteImageList();
}

void CListCtrlX::OnInitMenuPopup(CMenu *pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CListCtrl::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

	if (!m_bRouteMenuCmdsToMainFrame) {
		// NOTE: To enable the 'OnUpdateCmdUI'-stuff for a dialog or a control we
		// must explicitly call an appropriate function (which was stolen from
		// 'CFrameWnd') - otherwise out 'OnUpdateCmdUI'-callbacks would not be called.
		//WndInitMenuPopupUpdateCmdUI(this, pPopupMenu, nIndex, bSysMenu);
	}
}

void CListCtrlX::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu)
{
	CListCtrl::OnMenuSelect(nItemID, nFlags, hSysMenu);

	if (!m_bRouteMenuCmdsToMainFrame) {
		// To enable the display of the statusbar message strings for the
		// currently selected menu item, we must explicitly call an appropriate
		// function (which is stolen from 'CFrameWnd').
		//WndMenuSelectUpdateMessageText(nItemID, nFlags, hSysMenu);
	}
}

void CListCtrlX::OnContextMenu(CWnd*, CPoint point)
{
	if (!PointInClient(*this, point) || (m_uIDMenu == UINT_MAX && m_pMenu == NULL)) {
		Default();
		return;
	}

	CMenu *pMenu;
	if (m_pMenu != NULL)
		pMenu = m_pMenu;
	else {
		CMenu menuPopup;
		pMenu = menuPopup.LoadMenu(m_uIDMenu) ? menuPopup.GetSubMenu(0) : NULL;
	}

	if (pMenu != NULL) {
		// If the context menu was not opened using the right mouse button,
		// but the keyboard (Shift+F10), get a useful position for the context menu.
		GetPopupMenuPos(*this, point);
		pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON
			, point.x, point.y
			, m_bRouteMenuCmdsToMainFrame ? AfxGetMainWnd() : (m_pParent ? m_pParent : this));
	}
}

void CListCtrlX::InitColumnOrders(int iColumns, const LCX_COLUMN_INIT *pColumns)
{
	::InitColumnOrders(*this, iColumns, pColumns);
}

void CListCtrlX::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_SPACE) {
		if (GetExtendedStyle() & LVS_EX_CHECKBOXES) {
			// Check *all* selected items
			int iItem = GetNextItem(-1, LVNI_FOCUSED);
			if (iItem >= 0 && GetKeyState(VK_CONTROL) >= 0)
				CheckSelectedItems(iItem);
		}
	} else if (GetKeyState(VK_CONTROL) < 0) {
		if (nChar == 'A') {
			//////////////////////////////////////////////////////////////////
			// Ctrl+A: Select all items
			//
			if ((GetStyle() & LVS_SINGLESEL) == 0)
				SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
		/*else if (nChar == 'C') {
			//////////////////////////////////////////////////////////////////
			// Ctrl+C: Copy listview items to clipboard
			//
			OnCopy(0, 0);*/
		} else if (nChar == 'F') {
			//////////////////////////////////////////////////////////////////
			// Ctrl+F: Search item
			//
			OnFindStart();
		}
	} else if (nChar == VK_F3) {
		if (GetKeyState(VK_SHIFT) & 0x8000) {
			//////////////////////////////////////////////////////////////////
			// Shift+F3: Search previous
			//
			OnFindPrev();
		} else {
			//////////////////////////////////////////////////////////////////
			// F3: Search next
			//
			OnFindNext();
		}
	}

	CListCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CListCtrlX::CheckSelectedItems(int nCurrItem)
{
	ASSERT(GetExtendedStyle() & LVS_EX_CHECKBOXES);

	// first check if this item is selected
	LVITEM lvi;
	lvi.iItem = nCurrItem;
	lvi.iSubItem = 0;
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_SELECTED;
	GetItem(&lvi);
	if (lvi.state & LVIS_SELECTED) {
		// new check state is the reverse of the current state,
		BOOL bCheck = !GetCheck(nCurrItem);
		int iSel = -1;
		while ((iSel = GetNextItem(iSel, LVNI_SELECTED)) >= 0)
			if (iSel != nCurrItem)
				SetCheck(iSel, bCheck);
	}
}

void CreateItemReport(CListCtrl &lv, CString &rstrReport)
{
	// Get nr. of listview columns
	CHeaderCtrl *hdr = lv.GetHeaderCtrl();
	int iCols = hdr->GetItemCount();
	if (iCols == 0)
		return;
	// Get max. chars per column
	int *paiColWidths;
	try {
		paiColWidths = new int[iCols]();
	} catch (...) {
		return;
	}
	TCHAR szItem[512];
	int iItems = lv.GetItemCount();

//	memset(paiColWidths, 0, (sizeof *paiColWidths) * iCols);
	for (int iCol = 0; iCol < iCols; ++iCol) {
		LVCOLUMN lvc;
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.pszText = szItem;
		lvc.cchTextMax = _countof(szItem);
		if (lv.GetColumn(iCol, &lvc) && lvc.cx > 0) {
			szItem[_countof(szItem) - 1] = _T('\0');
			int iLen = (int)_tcslen(lvc.pszText);
			if (iLen > paiColWidths[iCol])
				paiColWidths[iCol] = iLen;

			for (int iItem = 0; iItem < iItems; ++iItem) {
				LVITEM lvi;
				lvi.mask = LVIF_TEXT;
				lvi.iItem = iItem;
				lvi.iSubItem = iCol;
				lvi.pszText = szItem;
				lvi.cchTextMax = _countof(szItem);
				if (lv.GetItem(&lvi)) {
					szItem[_countof(szItem) - 1] = _T('\0');
					iLen = (int)_tcslen(lvi.pszText);
					if (iLen > paiColWidths[iCol])
						paiColWidths[iCol] = iLen;
				}
			}
		}
	}

	CString strLine;
	for (int iCol = 0; iCol < iCols; ++iCol) {
		if (paiColWidths[iCol] > 0) {
			LVCOLUMN lvc;
			lvc.mask = LVCF_TEXT;
			lvc.pszText = szItem;
			lvc.cchTextMax = _countof(szItem);
			if (lv.GetColumn(iCol, &lvc)) {
				szItem[_countof(szItem) - 1] = _T('\0');
				strLine.AppendFormat(_T("%-*s"), paiColWidths[iCol] + 2, szItem);
			}
		}
	}
	if (!strLine.IsEmpty()) {
		if (!rstrReport.IsEmpty())
			rstrReport += _T("\r\n");
		rstrReport.AppendFormat(_T("%s\r\n%s"), (LPCTSTR)strLine, (LPCTSTR)CString(_T('-'), strLine.GetLength()));
	}

	for (int iItem = 0; iItem < iItems; ++iItem) {
		for (int iCol = 0; iCol < iCols; ++iCol) {
			if (paiColWidths[iCol] > 0) {
				LVITEM lvi;
				lvi.mask = LVIF_TEXT;
				lvi.iItem = iItem;
				lvi.iSubItem = iCol;
				lvi.pszText = szItem;
				lvi.cchTextMax = _countof(szItem);
				if (lv.GetItem(&lvi)) {
					szItem[_countof(szItem) - 1] = _T('\0');
					strLine.AppendFormat(_T("%-*s"), paiColWidths[iCol] + 2, szItem);
				}
			}
		}

		if (!strLine.IsEmpty()) {
			if (!rstrReport.IsEmpty())
				rstrReport += _T("\r\n");
			rstrReport += strLine;
		}
	}

	delete[] paiColWidths;

	if (!rstrReport.IsEmpty())
		rstrReport += _T("\r\n");
}

LRESULT CListCtrlX::OnCopy(WPARAM, LPARAM)
{
	CString strReport;
	CreateItemReport(*this, strReport);
	if (!strReport.IsEmpty())
		theApp.CopyTextToClipboard(strReport);
	return 0;
}

void InitColumnOrders(CListCtrl &lv, int iColumns, const LCX_COLUMN_INIT *pColumns)
{
	ASSERT(lv.GetHeaderCtrl()->GetItemCount() == iColumns);
	LPINT piOrders;
	try {
		piOrders = new INT[iColumns];
	} catch (...) {
		return;
	}
	for (int iCol = 0; iCol < iColumns; ++iCol) {
		int j;
		for (j = 0; j < iColumns; ++j) {
			if (pColumns[j].iOrder == iCol) {
				piOrders[iCol] = j;
				break;
			}
		}
		if (j >= iColumns) {
			ASSERT(0);
			piOrders[iCol] = iCol;
		}
	}
	VERIFY(lv.SetColumnOrderArray(iColumns, piOrders));
	delete[] piOrders;
}

void SetItemFocus(CListCtrl &ctl)
{
	if (ctl.GetItemCount() > 0) {
		int iSel = ctl.GetNextItem(-1, LVNI_FOCUSED);
		if (iSel < 0) {
			iSel = ctl.GetNextItem(-1, LVNI_SELECTED);
			if (iSel < 0)
				iSel = 0;
			ctl.SetItemState(iSel, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
			ctl.SetSelectionMark(iSel);
		}
	}
}

bool CListCtrlX::FindItem(const CListCtrlX &lv, int iItem, DWORD_PTR)
{
	CString strItemText(lv.GetItemText(iItem, lv.GetFindColumn()));
	if (!strItemText.IsEmpty()) {
		if (lv.GetFindMatchCase()
			? _tcsstr(strItemText, lv.GetFindText()) != NULL
			: stristr(strItemText, lv.GetFindText()) != NULL)
			return true;
	}
	return false;
}

void CListCtrlX::DoFind(int iStartItem, int iDirection /*1=down, 0 = up*/, BOOL bShowError)
{
	CWaitCursor curHourglass;

	if (iStartItem < 0) {
		MessageBeep(MB_OK);
		return;
	}

	int iNumItems = iDirection ? GetItemCount() : 0;
	int iItem = iStartItem;
	while (iDirection ? iItem < iNumItems : iItem >= 0) {
		if ((*m_pfnFindItem)(*this, iItem, m_lFindItemParam)) {
			// Deselect all listview entries
			DeselectAllItems();

			// Select the found listview entry
			SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			SetSelectionMark(iItem);
			EnsureVisible(iItem, FALSE/*bPartialOK*/);
			SetFocus();

			return;
		}

		if (iDirection)
			++iItem;
		else
			--iItem;
	}

	if (bShowError)
		AfxMessageBox(_T("No matching entry found."), MB_ICONINFORMATION);
	else
		MessageBeep(MB_OK);
}

void CListCtrlX::OnFindStart()
{
	CListViewSearchDlg dlg;
	dlg.m_pListView = this;
	dlg.m_strFindText = m_strFindText;
	dlg.m_iSearchColumn = m_iFindColumn;
	if (dlg.DoModal() == IDOK && !dlg.m_strFindText.IsEmpty()) {
		m_strFindText = dlg.m_strFindText;
		m_iFindColumn = dlg.m_iSearchColumn;

		DoFindNext(TRUE/*bShowError*/);
	}
}

void CListCtrlX::OnFindNext()
{
	DoFindNext(FALSE/*bShowError*/);
}

void CListCtrlX::DoFindNext(BOOL bShowError)
{
	int iStartItem = GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
	if (iStartItem == -1)
		iStartItem = 0;
	else
		iStartItem += (m_iFindDirection ? 1 : -1);
	DoFind(iStartItem, m_iFindDirection, bShowError);
}

void CListCtrlX::OnFindPrev()
{
	int iStartItem = GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
	if (iStartItem == -1)
		iStartItem = 0;
	else
		iStartItem += (!m_iFindDirection ? 1 : -1);

	DoFind(iStartItem, !m_iFindDirection, FALSE/*bShowError*/);
}
