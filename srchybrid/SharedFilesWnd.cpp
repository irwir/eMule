//this file is part of eMule
//Copyright (C)2002-2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "emule.h"
#include "emuleDlg.h"
#include "SharedFilesWnd.h"
#include "OtherFunctions.h"
#include "SharedFileList.h"
#include "KnownFileList.h"
#include "KnownFile.h"
#include "UserMsgs.h"
#include "HelpIDs.h"
#include "HighColorTab.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	SPLITTER_RANGE_MIN		100
#define	SPLITTER_RANGE_MAX		350

#define	SPLITTER_MARGIN			0
#define	SPLITTER_WIDTH			4


// CSharedFilesWnd dialog

IMPLEMENT_DYNAMIC(CSharedFilesWnd, CDialog)

BEGIN_MESSAGE_MAP(CSharedFilesWnd, CResizableDialog)
	ON_BN_CLICKED(IDC_RELOADSHAREDFILES, OnBnClickedReloadSharedFiles)
	ON_MESSAGE(UM_DELAYED_EVALUATE, OnChangeFilter)
	ON_NOTIFY(LVN_ITEMACTIVATE, IDC_SFLIST, OnLvnItemActivateSharedFiles)
	ON_NOTIFY(NM_CLICK, IDC_SFLIST, OnNmClickSharedFiles)
	ON_NOTIFY(TVN_SELCHANGED, IDC_SHAREDDIRSTREE, OnTvnSelChangedSharedDirsTree)
	ON_STN_DBLCLK(IDC_FILES_ICO, OnStnDblClickFilesIco)
	ON_WM_CTLCOLOR()
	ON_WM_HELPINFO()
	ON_WM_SYSCOLORCHANGE()
	ON_BN_CLICKED(IDC_SF_HIDESHOWDETAILS, OnBnClickedSfHideshowdetails)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_SFLIST, OnLvnItemchangedSflist)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

CSharedFilesWnd::CSharedFilesWnd(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CSharedFilesWnd::IDD, pParent)
	, icon_files()
	, m_nFilterColumn()
	, m_bDetailsVisible(true)
{
}

CSharedFilesWnd::~CSharedFilesWnd()
{
	m_ctlSharedListHeader.Detach();
	if (icon_files)
		VERIFY(::DestroyIcon(icon_files));
}

void CSharedFilesWnd::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SFLIST, sharedfilesctrl);
	DDX_Control(pDX, IDC_SHAREDDIRSTREE, m_ctlSharedDirTree);
	DDX_Control(pDX, IDC_SHAREDFILES_FILTER, m_ctlFilter);
}

BOOL CSharedFilesWnd::OnInitDialog()
{
	CResizableDialog::OnInitDialog();
	InitWindowStyles(this);
	SetAllIcons();
	sharedfilesctrl.Init();
	m_ctlSharedDirTree.Initialize(&sharedfilesctrl);
	if (thePrefs.GetUseSystemFontForMainControls())
		m_ctlSharedDirTree.SendMessage(WM_SETFONT, NULL, FALSE);

	m_ctlSharedListHeader.Attach(sharedfilesctrl.GetHeaderCtrl()->Detach());
	CArray<int, int> aIgnore; // ignored no-text columns for filter edit
	aIgnore.Add(8); // shared parts
	aIgnore.Add(11); // shared ed2k/kad
	m_ctlFilter.OnInit(&m_ctlSharedListHeader, &aIgnore);

	RECT rcSpl;
	m_ctlSharedDirTree.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);

	CRect rcFiles;
	sharedfilesctrl.GetWindowRect(rcFiles);
	ScreenToClient(rcFiles);
	VERIFY(m_dlgDetails.Create(this, DS_CONTROL | DS_SETFONT | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, WS_EX_CONTROLPARENT));
	m_dlgDetails.SetWindowPos(NULL, rcFiles.left, rcFiles.bottom + 4, rcFiles.Width() + 2, rcSpl.bottom - (rcFiles.bottom + 3), 0);
	AddAnchor(m_dlgDetails, BOTTOM_LEFT, BOTTOM_RIGHT);

	rcSpl.left = rcSpl.right + SPLITTER_MARGIN;
	rcSpl.right = rcSpl.left + SPLITTER_WIDTH;
	m_wndSplitter.Create(WS_CHILD | WS_VISIBLE, rcSpl, this, IDC_SPLITTER_SHAREDFILES);

	AddAnchor(m_ctlSharedDirTree, TOP_LEFT, BOTTOM_LEFT);
	AddAnchor(IDC_RELOADSHAREDFILES, TOP_RIGHT);

	AddAllOtherAnchors();

	int iPosStatInit = rcSpl.left;
	int iPosStatNew = thePrefs.GetSplitterbarPositionShared();
	if (iPosStatNew > SPLITTER_RANGE_MAX)
		iPosStatNew = SPLITTER_RANGE_MAX;
	else if (iPosStatNew < SPLITTER_RANGE_MIN)
		iPosStatNew = SPLITTER_RANGE_MIN;
	rcSpl.left = iPosStatNew;
	rcSpl.right = iPosStatNew + SPLITTER_WIDTH;
	if (iPosStatNew != iPosStatInit) {
		m_wndSplitter.MoveWindow(&rcSpl);
		DoResize(iPosStatNew - iPosStatInit);
	}

	GetDlgItem(IDC_SF_HIDESHOWDETAILS)->SetFont(&theApp.m_fontSymbol);
	GetDlgItem(IDC_SF_HIDESHOWDETAILS)->BringWindowToTop();
	ShowDetailsPanel(thePrefs.GetShowSharedFilesDetails());

	Localize();
	return TRUE;
}

void CSharedFilesWnd::DoResize(int iDelta)
{
	CSplitterControl::ChangeWidth(&m_ctlSharedDirTree, iDelta);
	CSplitterControl::ChangeWidth(&m_ctlFilter, iDelta);
	CSplitterControl::ChangePos(&sharedfilesctrl, -iDelta, 0);
	CSplitterControl::ChangeWidth(&sharedfilesctrl, -iDelta);
	bool bAntiFlicker = (m_dlgDetails.IsWindowVisible() != FALSE);
	if (bAntiFlicker)
		m_dlgDetails.SetRedraw(FALSE);
	CSplitterControl::ChangePos(&m_dlgDetails, -iDelta, 0);
	CSplitterControl::ChangeWidth(&m_dlgDetails, -iDelta);
	if (bAntiFlicker)
		m_dlgDetails.SetRedraw(TRUE);

	RECT rcSpl;
	m_wndSplitter.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);
	thePrefs.SetSplitterbarPositionShared(rcSpl.left);

	RemoveAnchor(m_wndSplitter);
	AddAnchor(m_wndSplitter, TOP_LEFT);
	RemoveAnchor(sharedfilesctrl);
	RemoveAnchor(m_ctlSharedDirTree);
	RemoveAnchor(m_ctlFilter);
	RemoveAnchor(m_dlgDetails);

	AddAnchor(sharedfilesctrl, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(m_ctlSharedDirTree, TOP_LEFT, BOTTOM_LEFT);
	AddAnchor(m_ctlFilter, TOP_LEFT);
	AddAnchor(m_dlgDetails, BOTTOM_LEFT, BOTTOM_RIGHT);

	RECT rcWnd;
	GetWindowRect(&rcWnd);
	ScreenToClient(&rcWnd);
	m_wndSplitter.SetRange(rcWnd.left + SPLITTER_RANGE_MIN, rcWnd.left + SPLITTER_RANGE_MAX);

	Invalidate();
	UpdateWindow();
}


void CSharedFilesWnd::Reload(bool bForceTreeReload)
{
	sharedfilesctrl.SetDirectoryFilter(NULL, false);
	m_ctlSharedDirTree.Reload(bForceTreeReload); // force a reload of the tree to update the 'accessible' state of each directory
	sharedfilesctrl.SetDirectoryFilter(m_ctlSharedDirTree.GetSelectedFilter(), false);
	theApp.sharedfiles->Reload();

	ShowSelectedFilesDetails();
}

void CSharedFilesWnd::OnStnDblClickFilesIco()
{
	theApp.emuledlg->ShowPreferences(IDD_PPG_DIRECTORIES);
}

void CSharedFilesWnd::OnBnClickedReloadSharedFiles()
{
	CWaitCursor curWait;
#ifdef _DEBUG
	if (GetKeyState(VK_CONTROL) < 0) {
		theApp.sharedfiles->RebuildMetaData();
		sharedfilesctrl.Invalidate();
		sharedfilesctrl.UpdateWindow();
		return;
	}
#endif
	Reload(true);
}

void CSharedFilesWnd::OnLvnItemActivateSharedFiles(LPNMHDR, LRESULT*)
{
	ShowSelectedFilesDetails();
}

void CSharedFilesWnd::OnNmClickSharedFiles(LPNMHDR pNMHDR, LRESULT *pResult)
{
	OnLvnItemActivateSharedFiles(pNMHDR, pResult);
	*pResult = 0;
}

BOOL CSharedFilesWnd::PreTranslateMessage(MSG *pMsg)
{
	if (theApp.emuledlg->m_pSplashWnd)
		return FALSE;
	switch (pMsg->message) {
	case WM_KEYDOWN:
		// Don't handle Ctrl+Tab in this window. It will be handled by main window.
		if (pMsg->wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0)
			return FALSE;
		if (pMsg->wParam == VK_ESCAPE)
			return FALSE;
		break;
	case WM_KEYUP:
		if (pMsg->hwnd == sharedfilesctrl.m_hWnd)
			OnLvnItemActivateSharedFiles(0, 0);
		break;
	case WM_MBUTTONUP:
		{
			CPoint point;
			if (!::GetCursorPos(&point))
				return FALSE;
			sharedfilesctrl.ScreenToClient(&point);
			int it = sharedfilesctrl.HitTest(point);
			if (it == -1)
				return FALSE;

			sharedfilesctrl.SetItemState(-1, 0, LVIS_SELECTED);
			sharedfilesctrl.SetItemState(it, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			sharedfilesctrl.SetSelectionMark(it);   // display selection mark correctly!
			sharedfilesctrl.ShowComments(reinterpret_cast<CShareableFile*>(sharedfilesctrl.GetItemData(it)));
			return TRUE;
		}
	}

	return CResizableDialog::PreTranslateMessage(pMsg);
}

void CSharedFilesWnd::OnSysColorChange()
{
	CResizableDialog::OnSysColorChange();
	SetAllIcons();
}

void CSharedFilesWnd::SetAllIcons()
{
	if (icon_files)
		VERIFY(::DestroyIcon(icon_files));
	icon_files = theApp.LoadIcon(_T("SharedFilesList"), 16, 16);
	static_cast<CStatic*>(GetDlgItem(IDC_FILES_ICO))->SetIcon(icon_files);
}

void CSharedFilesWnd::Localize()
{
	sharedfilesctrl.Localize();
	m_ctlSharedDirTree.Localize();
	m_ctlFilter.ShowColumnText(true);
	sharedfilesctrl.SetDirectoryFilter(NULL, true);
	SetDlgItemText(IDC_RELOADSHAREDFILES, GetResString(IDS_SF_RELOAD));
	m_dlgDetails.Localize();
}

void CSharedFilesWnd::OnTvnSelChangedSharedDirsTree(LPNMHDR, LRESULT *pResult)
{
	sharedfilesctrl.SetDirectoryFilter(m_ctlSharedDirTree.GetSelectedFilter(), !m_ctlSharedDirTree.IsCreatingTree());
	*pResult = 0;
}

LRESULT CSharedFilesWnd::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_PAINT:
		if (m_wndSplitter) {
			CRect rcWnd;
			GetWindowRect(rcWnd);
			if (rcWnd.Width() > 0) {
				RECT rcSpl;
				m_ctlSharedDirTree.GetWindowRect(&rcSpl);
				ScreenToClient(&rcSpl);
				rcSpl.left = rcSpl.right + SPLITTER_MARGIN;
				rcSpl.right = rcSpl.left + SPLITTER_WIDTH;

				RECT rcFilter;
				m_ctlFilter.GetWindowRect(&rcFilter);
				ScreenToClient(&rcFilter);
				rcSpl.top = rcFilter.top;
				m_wndSplitter.MoveWindow(&rcSpl, TRUE);
			}
		}
		break;
	case WM_NOTIFY:
		if (wParam == IDC_SPLITTER_SHAREDFILES) {
			SPC_NMHDR *pHdr = reinterpret_cast<SPC_NMHDR*>(lParam);
			DoResize(pHdr->delta);
		}
		break;
	case WM_SIZE:
		if (m_wndSplitter) {
			RECT rcWnd;
			GetWindowRect(&rcWnd);
			ScreenToClient(&rcWnd);
			m_wndSplitter.SetRange(rcWnd.left + SPLITTER_RANGE_MIN, rcWnd.left + SPLITTER_RANGE_MAX);
		}
	}
	return CResizableDialog::DefWindowProc(message, wParam, lParam);
}

LRESULT CSharedFilesWnd::OnChangeFilter(WPARAM wParam, LPARAM lParam)
{
	CWaitCursor curWait; // this may take a while

	bool bColumnDiff = (m_nFilterColumn != (uint32)wParam);
	m_nFilterColumn = (uint32)wParam;

	CStringArray astrFilter;
	CString strFullFilterExpr((LPCTSTR)lParam);
	for (int iPos = 0; iPos >= 0;) {
		const CString &strFilter(strFullFilterExpr.Tokenize(_T(" "), iPos));
		if (!strFilter.IsEmpty() && strFilter != _T("-"))
			astrFilter.Add(strFilter);
	}

	bool bFilterDiff = (astrFilter.GetCount() != m_astrFilter.GetCount());
	if (!bFilterDiff)
		for (int i = (int)astrFilter.GetCount(); --i >= 0;)
			if (astrFilter[i] != m_astrFilter[i]) {
				bFilterDiff = true;
				break;
			}

	if (bColumnDiff || bFilterDiff) {
		m_astrFilter.RemoveAll();
		m_astrFilter.Append(astrFilter);

		sharedfilesctrl.ReloadFileList();
	}
	return 0;
}

BOOL CSharedFilesWnd::OnHelpInfo(HELPINFO*)
{
	theApp.ShowHelp(eMule_FAQ_GUI_SharedFiles);
	return TRUE;
}

HBRUSH CSharedFilesWnd::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = theApp.emuledlg->GetCtlColor(pDC, pWnd, nCtlColor);
	return hbr ? hbr : __super::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CSharedFilesWnd::SetToolTipsDelay(DWORD dwDelay)
{
	sharedfilesctrl.SetToolTipsDelay(dwDelay);
}

void CSharedFilesWnd::ShowSelectedFilesDetails(bool bForce)
{
	CTypedPtrList<CPtrList, CShareableFile*> selectedList;
	UINT nItems = m_dlgDetails.GetItems().GetSize();
	if (m_bDetailsVisible) {
		int i = 0;
		for (POSITION pos = sharedfilesctrl.GetFirstSelectedItemPosition(); pos != NULL;) {
			int index = sharedfilesctrl.GetNextSelectedItem(pos);
			if (index >= 0) {
				CShareableFile *file = reinterpret_cast<CShareableFile*>(sharedfilesctrl.GetItemData(index));
				if (file != NULL) {
					selectedList.AddTail(file);
					if (nItems <= (UINT)i || m_dlgDetails.GetItems()[i] != file)
						bForce = true;
					++i;
				}
			}
		}
	}
	if (bForce || nItems != (UINT)selectedList.GetCount())
		m_dlgDetails.SetFiles(selectedList);
}

void CSharedFilesWnd::ShowDetailsPanel(bool bShow)
{
	m_bDetailsVisible = bShow;
	thePrefs.SetShowSharedFilesDetails(bShow);
	RemoveAnchor(sharedfilesctrl);
	RemoveAnchor(IDC_SF_HIDESHOWDETAILS);

	RECT rcSpl;
	CRect rcFiles, rcDetailDlg, rcButton;

	sharedfilesctrl.GetWindowRect(rcFiles);
	ScreenToClient(rcFiles);

	m_dlgDetails.GetWindowRect(rcDetailDlg);

	CWnd &button = *GetDlgItem(IDC_SF_HIDESHOWDETAILS);
	button.GetWindowRect(rcButton);

	m_ctlSharedDirTree.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);

	button.GetWindowRect(rcButton);
	if (bShow) {
		sharedfilesctrl.SetWindowPos(NULL, 0, 0, rcFiles.Width(), rcSpl.bottom - rcFiles.top - rcDetailDlg.Height() - 2, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
		m_dlgDetails.ShowWindow(SW_SHOW);
		button.SetWindowPos(NULL, rcFiles.right - rcButton.Width() + 1, rcSpl.bottom - rcDetailDlg.Height() + 2, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
		button.SetWindowText(_T("6"));
	} else {
		m_dlgDetails.ShowWindow(SW_HIDE);
		sharedfilesctrl.SetWindowPos(NULL, 0, 0, rcFiles.Width(), rcSpl.bottom - rcFiles.top - rcButton.Height() + 1, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
		button.SetWindowPos(NULL, rcFiles.right - rcButton.Width() + 1, rcSpl.bottom - rcButton.Height() + 1, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);
		button.SetWindowText(_T("5"));
	}

	sharedfilesctrl.SetFocus();
	AddAnchor(sharedfilesctrl, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_SF_HIDESHOWDETAILS, BOTTOM_RIGHT);
	ShowSelectedFilesDetails();
}

void CSharedFilesWnd::OnBnClickedSfHideshowdetails()
{
	ShowDetailsPanel(!m_bDetailsVisible);
}

void CSharedFilesWnd::OnLvnItemchangedSflist(LPNMHDR, LRESULT *pResult)
{
	ShowSelectedFilesDetails();
	*pResult = 0;
}

void CSharedFilesWnd::OnShowWindow(BOOL bShow, UINT)
{
	if (bShow)
		ShowSelectedFilesDetails(true);
}


/////////////////////////////////////////////////////////////////////////////////////////////
// CSharedFileDetailsModelessSheet
IMPLEMENT_DYNAMIC(CSharedFileDetailsModelessSheet, CListViewPropertySheet)

BEGIN_MESSAGE_MAP(CSharedFileDetailsModelessSheet, CListViewPropertySheet)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_WM_CREATE()
END_MESSAGE_MAP()

int CSharedFileDetailsModelessSheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// skip CResizableSheet::OnCreate because we don't need the styles and stuff which are set there
	//CreateSizeGrip(FALSE); // create grip but don't show it <- Do not do this with the new library!
	return CPropertySheet::OnCreate(lpCreateStruct);
}

bool NeedArchiveInfoPage(const CSimpleArray<CObject*> *paItems);
void UpdateFileDetailsPages(CListViewPropertySheet *pSheet, CResizablePage *pArchiveInfo
		, CResizablePage *pMediaInfo, CResizablePage *pFileLink);

CSharedFileDetailsModelessSheet::CSharedFileDetailsModelessSheet()
{
	m_psh.dwFlags &= ~PSH_HASHELP;
	m_psh.dwFlags |= PSH_MODELESS;

	m_wndStatistics.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndStatistics.m_psp.dwFlags |= PSP_USEICONID;
	m_wndStatistics.m_psp.pszIcon = _T("StatsDetail");
	m_wndStatistics.SetFiles(&m_aItems);
	AddPage(&m_wndStatistics);

	m_wndArchiveInfo.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndArchiveInfo.m_psp.dwFlags |= PSP_USEICONID;
	m_wndArchiveInfo.m_psp.pszIcon = _T("ARCHIVE_PREVIEW");
	m_wndArchiveInfo.SetReducedDialog();
	m_wndArchiveInfo.SetFiles(&m_aItems);

	m_wndMediaInfo.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMediaInfo.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMediaInfo.m_psp.pszIcon = _T("MEDIAINFO");
	m_wndMediaInfo.SetReducedDialog();
	m_wndMediaInfo.SetFiles(&m_aItems);
	if (NeedArchiveInfoPage(&m_aItems))
		AddPage(&m_wndArchiveInfo);
	else
		AddPage(&m_wndMediaInfo);

	m_wndFileLink.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndFileLink.m_psp.dwFlags |= PSP_USEICONID;
	m_wndFileLink.m_psp.pszIcon = _T("ED2KLINK");
	m_wndFileLink.SetReducedDialog();
	m_wndFileLink.SetFiles(&m_aItems);
	AddPage(&m_wndFileLink);

	m_wndMetaData.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMetaData.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMetaData.m_psp.pszIcon = _T("METADATA");
	m_wndMetaData.SetFiles(&m_aItems);
	if (thePrefs.IsExtControlsEnabled())
		AddPage(&m_wndMetaData);

	/*LPCTSTR pPshStartPage = m_pPshStartPage;
	if (m_uInvokePage != 0)
		pPshStartPage = MAKEINTRESOURCE(m_uInvokePage);
	for (int i = (int)m_pages.GetCount(); --i >= 0;)
		if (GetPage(i)->m_psp.pszTemplate == pPshStartPage) {
			m_psh.nStartPage = i;
			break;
		}*/
}

BOOL CSharedFileDetailsModelessSheet::OnInitDialog()
{
	EnableStackedTabs(FALSE);
	BOOL bResult = CListViewPropertySheet::OnInitDialog();
	HighColorTab::UpdateImageList(*this);
	InitWindowStyles(this);
	return bResult;
}

void  CSharedFileDetailsModelessSheet::SetFiles(CTypedPtrList<CPtrList, CShareableFile*> &aFiles)
{
	m_aItems.RemoveAll();
	for (POSITION pos = aFiles.GetHeadPosition(); pos != NULL;)
		m_aItems.Add(aFiles.GetNext(pos));
	ChangedData();
}

void CSharedFileDetailsModelessSheet::Localize()
{
	m_wndStatistics.Localize();
	SetTabTitle(IDS_SF_STATISTICS, &m_wndStatistics, this);
	m_wndFileLink.Localize();
	SetTabTitle(IDS_SW_LINK, &m_wndFileLink, this);
	m_wndArchiveInfo.Localize();
	SetTabTitle(IDS_CONTENT_INFO, &m_wndArchiveInfo, this);
	m_wndMediaInfo.Localize();
	SetTabTitle(IDS_CONTENT_INFO, &m_wndMediaInfo, this);
	m_wndMetaData.Localize();
	SetTabTitle(IDS_META_DATA, &m_wndMetaData, this);
}

LRESULT CSharedFileDetailsModelessSheet::OnDataChanged(WPARAM, LPARAM)
{
	//When using up/down keys in shared files list, "Content" tab grabs focus on archives
	CWnd *pFocused = GetFocus();
	UpdateFileDetailsPages(this, &m_wndArchiveInfo, &m_wndMediaInfo, &m_wndFileLink);
	if (pFocused) //try to stay in file list
		pFocused->SetFocus();
	return TRUE;
}