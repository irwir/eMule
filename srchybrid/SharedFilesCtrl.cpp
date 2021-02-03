//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "emuledlg.h"
#include "SharedFilesCtrl.h"
#include "OtherFunctions.h"
#include "FileInfoDialog.h"
#include "MetaDataDlg.h"
#include "ED2kLinkDlg.h"
#include "ArchivePreviewDlg.h"
#include "CommentDialog.h"
#include "HighColorTab.hpp"
#include "ListViewWalkerPropertySheet.h"
#include "UserMsgs.h"
#include "ResizableLib/ResizableSheet.h"
#include "KnownFile.h"
#include "MapKey.h"
#include "SharedFileList.h"
#include "MemDC.h"
#include "PartFile.h"
#include "MenuCmds.h"
#include "IrcWnd.h"
#include "SharedFilesWnd.h"
#include "Opcodes.h"
#include "InputBox.h"
#include "WebServices.h"
#include "TransferDlg.h"
#include "ClientList.h"
#include "UpDownClient.h"
#include "Collection.h"
#include "CollectionCreateDialog.h"
#include "CollectionViewDialog.h"
#include "SearchParams.h"
#include "SearchDlg.h"
#include "SearchResultsWnd.h"
#include "ToolTipCtrlX.h"
#include "kademlia/kademlia/kademlia.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "MediaInfo.h"
#include "Log.h"
#include "KnownFileList.h"
#include "VisualStylesXP.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool NeedArchiveInfoPage(const CSimpleArray<CObject*> *paItems);
void UpdateFileDetailsPages(CListViewPropertySheet *pSheet
	, CResizablePage *pArchiveInfo, CResizablePage *pMediaInfo, CResizablePage *pFileLink);


//////////////////////////////////////////////////////////////////////////////
// CSharedFileDetailsSheet

class CSharedFileDetailsSheet : public CListViewWalkerPropertySheet
{
	DECLARE_DYNAMIC(CSharedFileDetailsSheet)

	void Localize();
public:
	CSharedFileDetailsSheet(CTypedPtrList<CPtrList, CShareableFile*> &aFiles, UINT uInvokePage = 0, CListCtrlItemWalk *pListCtrl = NULL);
	virtual	~CSharedFileDetailsSheet() = default;

protected:
	CArchivePreviewDlg	m_wndArchiveInfo;
	CCommentDialog		m_wndFileComments;
	CED2kLinkDlg		m_wndFileLink;
	CFileInfoDialog		m_wndMediaInfo;
	CMetaDataDlg		m_wndMetaData;

	UINT m_uInvokePage;
	static LPCTSTR m_pPshStartPage;

	void UpdateTitle();

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
};

LPCTSTR CSharedFileDetailsSheet::m_pPshStartPage;

IMPLEMENT_DYNAMIC(CSharedFileDetailsSheet, CListViewWalkerPropertySheet)

BEGIN_MESSAGE_MAP(CSharedFileDetailsSheet, CListViewWalkerPropertySheet)
	ON_WM_DESTROY()
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
END_MESSAGE_MAP()

void CSharedFileDetailsSheet::Localize()
{
	m_wndMediaInfo.Localize();
	SetTabTitle(IDS_CONTENT_INFO, &m_wndMediaInfo, this);
	m_wndMetaData.Localize();
	SetTabTitle(IDS_META_DATA, &m_wndMetaData, this);
	m_wndFileLink.Localize();
	SetTabTitle(IDS_SW_LINK, &m_wndFileLink, this);
	m_wndFileComments.Localize();
	SetTabTitle(IDS_COMMENT, &m_wndFileComments, this);
	m_wndArchiveInfo.Localize();
	SetTabTitle(IDS_CONTENT_INFO, &m_wndArchiveInfo, this);
}

CSharedFileDetailsSheet::CSharedFileDetailsSheet(CTypedPtrList<CPtrList, CShareableFile*> &aFiles, UINT uInvokePage, CListCtrlItemWalk *pListCtrl)
	: CListViewWalkerPropertySheet(pListCtrl)
	, m_uInvokePage(uInvokePage)
{
	for (POSITION pos = aFiles.GetHeadPosition(); pos != NULL;)
		m_aItems.Add(aFiles.GetNext(pos));
	m_psh.dwFlags &= ~PSH_HASHELP;

	m_wndFileComments.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndFileComments.m_psp.dwFlags |= PSP_USEICONID;
	m_wndFileComments.m_psp.pszIcon = _T("FileComments");
	m_wndFileComments.SetFiles(&m_aItems);
	AddPage(&m_wndFileComments);

	m_wndArchiveInfo.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndArchiveInfo.m_psp.dwFlags |= PSP_USEICONID;
	m_wndArchiveInfo.m_psp.pszIcon = _T("ARCHIVE_PREVIEW");
	m_wndArchiveInfo.SetFiles(&m_aItems);

	m_wndMediaInfo.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMediaInfo.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMediaInfo.m_psp.pszIcon = _T("MEDIAINFO");
	m_wndMediaInfo.SetFiles(&m_aItems);
	if (NeedArchiveInfoPage(&m_aItems))
		AddPage(&m_wndArchiveInfo);
	else
		AddPage(&m_wndMediaInfo);

	m_wndMetaData.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndMetaData.m_psp.dwFlags |= PSP_USEICONID;
	m_wndMetaData.m_psp.pszIcon = _T("METADATA");
	if (m_aItems.GetSize() == 1 && thePrefs.IsExtControlsEnabled()) {
		m_wndMetaData.SetFiles(&m_aItems);
		AddPage(&m_wndMetaData);
	}

	m_wndFileLink.m_psp.dwFlags &= ~PSP_HASHELP;
	m_wndFileLink.m_psp.dwFlags |= PSP_USEICONID;
	m_wndFileLink.m_psp.pszIcon = _T("ED2KLINK");
	m_wndFileLink.SetFiles(&m_aItems);
	AddPage(&m_wndFileLink);

	LPCTSTR pPshStartPage = m_pPshStartPage;
	if (m_uInvokePage != 0)
		pPshStartPage = MAKEINTRESOURCE(m_uInvokePage);
	for (int i = (int)m_pages.GetCount(); --i >= 0;)
		if (GetPage(i)->m_psp.pszTemplate == pPshStartPage) {
			m_psh.nStartPage = i;
			break;
		}
}

void CSharedFileDetailsSheet::OnDestroy()
{
	if (m_uInvokePage == 0)
		m_pPshStartPage = GetPage(GetActiveIndex())->m_psp.pszTemplate;
	CListViewWalkerPropertySheet::OnDestroy();
}

BOOL CSharedFileDetailsSheet::OnInitDialog()
{
	EnableStackedTabs(FALSE);
	BOOL bResult = CListViewWalkerPropertySheet::OnInitDialog();
	HighColorTab::UpdateImageList(*this);
	InitWindowStyles(this);
	EnableSaveRestore(_T("SharedFileDetailsSheet")); // call this after(!) OnInitDialog
	Localize();
	UpdateTitle();
	return bResult;
}

LRESULT CSharedFileDetailsSheet::OnDataChanged(WPARAM, LPARAM)
{
	UpdateTitle();
	UpdateFileDetailsPages(this, &m_wndArchiveInfo, &m_wndMediaInfo, &m_wndFileLink);
	return 1;
}

void CSharedFileDetailsSheet::UpdateTitle()
{
	CString sTitle(GetResString(IDS_DETAILS));
	if (m_aItems.GetSize() == 1)
		sTitle.AppendFormat(_T(": %s"), (LPCTSTR)(static_cast<CAbstractFile*>(m_aItems[0])->GetFileName()));
	SetWindowText(sTitle);
}

BOOL CSharedFileDetailsSheet::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (wParam == ID_APPLY_NOW) {
		CSharedFilesCtrl *pSharedFilesCtrl = DYNAMIC_DOWNCAST(CSharedFilesCtrl, m_pListCtrl->GetListCtrl());
		if (pSharedFilesCtrl)
			for (int i = m_aItems.GetSize(); --i >= 0;)
				// so, and why does this not(!) work while the sheet is open ??
				pSharedFilesCtrl->UpdateFile(DYNAMIC_DOWNCAST(CKnownFile, m_aItems[i]));
	}
	return CListViewWalkerPropertySheet::OnCommand(wParam, lParam);
}


//////////////////////////////////////////////////////////////////////////////
// CSharedFilesCtrl

IMPLEMENT_DYNAMIC(CSharedFilesCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CSharedFilesCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, OnLvnGetInfoTip)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNmDblClk)
	ON_NOTIFY_REFLECT_EX(NM_CLICK, OnNMClick)
	ON_WM_CONTEXTMENU()
	ON_WM_KEYDOWN()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

CSharedFilesCtrl::CSharedFilesCtrl()
	: CListCtrlItemWalk(this)
	, m_aSortBySecondValue()
	, m_pDirectoryFilter()
	, nAICHHashing()
	, m_pHighlightedItem()
{
	SetGeneralPurposeFind(true);
	m_pToolTip = new CToolTipCtrlX;
	SetSkinKey(_T("SharedFilesLv"));
}

CSharedFilesCtrl::~CSharedFilesCtrl()
{
	while (!liTempShareableFilesInDir.IsEmpty())	// delete shareable files
		delete liTempShareableFilesInDir.RemoveHead();
	delete m_pToolTip;
}

void CSharedFilesCtrl::Init()
{
	SetPrefsKey(_T("SharedFilesCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	ASSERT((GetStyle() & LVS_SINGLESEL) == 0);

	InsertColumn(0, GetResString(IDS_DL_FILENAME), LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);
	InsertColumn(1, GetResString(IDS_DL_SIZE), LVCFMT_RIGHT, DFLT_SIZE_COL_WIDTH);
	InsertColumn(2, GetResString(IDS_TYPE), LVCFMT_LEFT, DFLT_FILETYPE_COL_WIDTH);
	InsertColumn(3, GetResString(IDS_PRIORITY), LVCFMT_LEFT, DFLT_PRIORITY_COL_WIDTH);
	InsertColumn(4, GetResString(IDS_FILEID), LVCFMT_LEFT, DFLT_HASH_COL_WIDTH, -1, true);
	InsertColumn(5, GetResString(IDS_SF_REQUESTS), LVCFMT_RIGHT, 100);
	InsertColumn(6, GetResString(IDS_SF_ACCEPTS), LVCFMT_RIGHT, 100, -1, true);
	InsertColumn(7, GetResString(IDS_SF_TRANSFERRED), LVCFMT_RIGHT, 120);
	InsertColumn(8, GetResString(IDS_SHARED_STATUS), LVCFMT_LEFT, DFLT_PARTSTATUS_COL_WIDTH);
	InsertColumn(9, GetResString(IDS_FOLDER), LVCFMT_LEFT, DFLT_FOLDER_COL_WIDTH, -1, true);
	InsertColumn(10, GetResString(IDS_COMPLSOURCES), LVCFMT_RIGHT, 60);
	InsertColumn(11, GetResString(IDS_SHAREDTITLE), LVCFMT_LEFT, 100);
	InsertColumn(12, GetResString(IDS_ARTIST), LVCFMT_LEFT, DFLT_ARTIST_COL_WIDTH, -1, true);
	InsertColumn(13, GetResString(IDS_ALBUM), LVCFMT_LEFT, DFLT_ALBUM_COL_WIDTH, -1, true);
	InsertColumn(14, GetResString(IDS_TITLE), LVCFMT_LEFT, DFLT_TITLE_COL_WIDTH, -1, true);
	InsertColumn(15, GetResString(IDS_LENGTH), LVCFMT_RIGHT, DFLT_LENGTH_COL_WIDTH, -1, true);
	InsertColumn(16, GetResString(IDS_BITRATE), LVCFMT_RIGHT, DFLT_BITRATE_COL_WIDTH, -1, true);
	InsertColumn(17, GetResString(IDS_CODEC), LVCFMT_LEFT, DFLT_CODEC_COL_WIDTH, -1, true);

	SetAllIcons();
	CreateMenus();
	LoadSettings();

	m_aSortBySecondValue[0] = true; // Requests:			Sort by 2nd value by default
	m_aSortBySecondValue[1] = true; // Accepted Requests:	Sort by 2nd value by default
	m_aSortBySecondValue[2] = true; // Transferred Data:	Sort by 2nd value by default
	m_aSortBySecondValue[3] = false; // Shared ED2K|Kad:	Sort by 1st value by default
	if (GetSortItem() >= 5 && GetSortItem() <= 7)
		m_aSortBySecondValue[GetSortItem() - 5] = GetSortSecondValue();
	else if (GetSortItem() == 11)
		m_aSortBySecondValue[3] = GetSortSecondValue();
	SetSortArrow();
	SortItems(SortProc, GetSortItem() + (GetSortAscending() ? 0 : 20) + (GetSortSecondValue() ? 100 : 0));

	CToolTipCtrl *tooltip = GetToolTips();
	if (tooltip) {
		m_pToolTip->SetFileIconToolTip(true);
		m_pToolTip->SubclassWindow(*tooltip);
		tooltip->ModifyStyle(0, TTS_NOPREFIX);
		tooltip->SetDelayTime(TTDT_AUTOPOP, SEC2MS(20));
		tooltip->SetDelayTime(TTDT_INITIAL, SEC2MS(thePrefs.GetToolTipDelay()));
	}

	m_ShareDropTarget.SetParent(this);
	VERIFY(m_ShareDropTarget.Register(this));
}

void CSharedFilesCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
	CreateMenus();
}

void CSharedFilesCtrl::SetAllIcons()
{
	ApplyImageList(NULL);
	m_ImageList.DeleteImageList();
	m_ImageList.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	m_ImageList.Add(CTempIconLoader(_T("EMPTY")));
	m_ImageList.Add(CTempIconLoader(_T("FileSharedServer")));
	m_ImageList.Add(CTempIconLoader(_T("FileSharedKad")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_NotRated")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Fake")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Poor")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Fair")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Good")));
	m_ImageList.Add(CTempIconLoader(_T("Rating_Excellent")));
	m_ImageList.Add(CTempIconLoader(_T("Collection_Search"))); // rating for comments are searched on kad
	m_ImageList.SetOverlayImage(m_ImageList.Add(CTempIconLoader(_T("FileCommentsOvl"))), 1);
	// Apply the image list also to the listview control, even if we use our own 'DrawItem'.
	// This is needed to give the listview control a chance to initialize the row height.
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) != 0);
	VERIFY(ApplyImageList(m_ImageList) == NULL);
}

void CSharedFilesCtrl::Localize()
{
	static const UINT uids[18] =
	{
		IDS_DL_FILENAME, IDS_DL_SIZE, IDS_TYPE, IDS_PRIORITY, IDS_FILEID, IDS_SF_REQUESTS
		, IDS_SF_ACCEPTS, IDS_SF_TRANSFERRED, IDS_SHARED_STATUS, IDS_FOLDER, IDS_COMPLSOURCES
		, IDS_SHAREDTITLE, IDS_ARTIST, IDS_ALBUM, IDS_TITLE, IDS_LENGTH, IDS_BITRATE, IDS_CODEC
	};

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;

	for (int i = 0; i < _countof(uids); ++i) {
		const CString &strRes(GetResString(uids[i]));
		hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
		pHeaderCtrl->SetItem(i, &hdi);
	}

	CreateMenus();

	for (int i = GetItemCount(); --i >= 0;)
		Update(i);

	ShowFilesCount();
}

void CSharedFilesCtrl::AddFile(const CShareableFile *file)
{
	if (theApp.IsClosing())
		return;
	// check filter conditions if we should show this file right now
	if (m_pDirectoryFilter != NULL) {
		ASSERT(file->IsKindOf(RUNTIME_CLASS(CKnownFile)) || m_pDirectoryFilter->m_eItemType == SDI_UNSHAREDDIRECTORY);
		switch (m_pDirectoryFilter->m_eItemType) {
		case SDI_ALL:
			// No filter
			break;
		case SDI_FILESYSTEMPARENT:
			return;
		case SDI_UNSHAREDDIRECTORY:
			// Items from the whole filesystem tree
			if (file->IsPartFile())
				return;
		case SDI_NO:
			// some shared directory
		case SDI_CATINCOMING:
			// Categories with special incoming dirs
			if (CompareDirectory(file->GetSharedDirectory(), m_pDirectoryFilter->m_strFullPath) != 0)
				return;
			break;
		case SDI_TEMP:
			// only temp files
			if (!file->IsPartFile())
				return;
			if (m_pDirectoryFilter->m_nCatFilter != -1 && (UINT)m_pDirectoryFilter->m_nCatFilter != ((CPartFile*)file)->GetCategory())
				return;
			break;
		case SDI_DIRECTORY:
			// any user selected shared dir but not incoming or temp
			if (file->IsPartFile())
				return;
			if (CompareDirectory(file->GetSharedDirectory(), thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) == 0)
				return;
			break;
		case SDI_INCOMING:
			// Main incoming directory
			if (CompareDirectory(file->GetSharedDirectory(), thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR)) != 0)
				return;
			// Hmm should we show all incoming files dirs or only those from the main incoming dir here?
			// hard choice, will only show the main for now
		}
	}
	if (IsFilteredItem(file))
		return;
	if (FindFile(file) >= 0) {
		// in the file system view the shared status might have changed so we need to update the item to redraw the checkbox
		if (m_pDirectoryFilter != NULL && m_pDirectoryFilter->m_eItemType == SDI_UNSHAREDDIRECTORY)
			UpdateFile(file);
		return;
	}

	// if we are in the file system view, this might be a CKnownFile which has to replace a CShareableFile
	// (in case we start sharing this file), so make sure to replace the old one instead of adding a new
	if (m_pDirectoryFilter != NULL && m_pDirectoryFilter->m_eItemType == SDI_UNSHAREDDIRECTORY && file->IsKindOf(RUNTIME_CLASS(CKnownFile)))
		for (POSITION pos = liTempShareableFilesInDir.GetHeadPosition(); pos != NULL; ) {
			const CShareableFile *pFile = liTempShareableFilesInDir.GetNext(pos);
			if (pFile->GetFilePath().CompareNoCase(file->GetFilePath()) == 0) {
				int iOldFile = FindFile(pFile);
				if (iOldFile >= 0) {
					SetItemData(iOldFile, (LPARAM)file);
					Update(iOldFile);
					ShowFilesCount();
					return;
				}
			}
		}

	int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, GetItemCount(), LPSTR_TEXTCALLBACK, 0, 0, 0, (LPARAM)file);
	if (iItem >= 0)
		Update(iItem);
}

void CSharedFilesCtrl::RemoveFile(const CShareableFile *file, bool bDeletedFromDisk)
{
	int iItem = FindFile(file);
	if (iItem >= 0) {
		if (!bDeletedFromDisk && m_pDirectoryFilter != NULL && m_pDirectoryFilter->m_eItemType == SDI_UNSHAREDDIRECTORY)
			// in the file system view we usually don't need to remove a file, if it becomes unshared it will
			// still be visible as its still in the file system and the knownfile object doesn't gets deleted neither
			// so to avoid having to reload the whole list we just update it instead of removing and re-finding
			UpdateFile(file);
		else
			DeleteItem(iItem);
		ShowFilesCount();
	}
}

void CSharedFilesCtrl::UpdateFile(const CShareableFile *file, bool bUpdateFileSummary)
{
	if (!theApp.IsClosing() && file) {
		int iItem = FindFile(file);
		if (iItem >= 0) {
			Update(iItem);
			if (bUpdateFileSummary && GetItemState(iItem, LVIS_SELECTED))
				theApp.emuledlg->sharedfileswnd->ShowSelectedFilesDetails(true); //force update
		}
	}
}

int CSharedFilesCtrl::FindFile(const CShareableFile *pFile)
{
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)pFile;
	return FindItem(&find);
}

void CSharedFilesCtrl::ReloadFileList()
{
	DeleteAllItems();
	theApp.emuledlg->sharedfileswnd->ShowSelectedFilesDetails();

	CCKey bufKey;
	for (POSITION pos = theApp.sharedfiles->m_Files_map.GetStartPosition(); pos != NULL;) {
		CKnownFile *cur_file;
		theApp.sharedfiles->m_Files_map.GetNextAssoc(pos, bufKey, cur_file);
		AddFile(cur_file);
	}
	if (m_pDirectoryFilter != NULL && m_pDirectoryFilter->m_eItemType == SDI_UNSHAREDDIRECTORY && !m_pDirectoryFilter->m_strFullPath.IsEmpty())
		AddShareableFiles(m_pDirectoryFilter->m_strFullPath);
	else
		while (!liTempShareableFilesInDir.IsEmpty())	// cleanup temp file list
			delete liTempShareableFilesInDir.RemoveHead();

	ShowFilesCount();
}

void CSharedFilesCtrl::ShowFilesCount()
{
	CString str;
	if (theApp.sharedfiles->GetHashingCount() + nAICHHashing > 0)
		str.Format(_T(" (%i, %s %i)"), (int)theApp.sharedfiles->GetCount(), (LPCTSTR)GetResString(IDS_HASHING), (int)(theApp.sharedfiles->GetHashingCount() + nAICHHashing));
	else
		str.Format(_T(" (%i)"), (int)theApp.sharedfiles->GetCount());
	theApp.emuledlg->sharedfileswnd->SetDlgItemText(IDC_TRAFFIC_TEXT, GetResString(IDS_SF_FILES) + str);
}

void CSharedFilesCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (theApp.IsClosing() || !lpDrawItemStruct->itemData)
		return;

	CMemoryDC dc(CDC::FromHandle(lpDrawItemStruct->hDC), &lpDrawItemStruct->rcItem);
	BOOL bCtrlFocused;
	InitItemMemDC(dc, lpDrawItemStruct, bCtrlFocused);
	CRect rcItem(lpDrawItemStruct->rcItem);
	CRect rcClient;
	GetClientRect(&rcClient);
	CShareableFile *file = reinterpret_cast<CShareableFile*>(lpDrawItemStruct->itemData);
	CKnownFile *pKnownFile = file->IsKindOf(RUNTIME_CLASS(CKnownFile)) ? static_cast<CKnownFile*>(file) : NULL;

	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	int iCount = pHeaderCtrl->GetItemCount();
	rcItem.right = rcItem.left - sm_iLabelOffset;
	rcItem.left += sm_iIconOffset;
	int iIconDrawWidth = theApp.GetSmallSytemIconSize().cx;
	for (int iCurrent = 0; iCurrent < iCount; ++iCurrent) {
		int iColumn = pHeaderCtrl->OrderToIndex(iCurrent);
		if (!IsColumnHidden(iColumn)) {
			UINT uDrawTextAlignment;
			int iColumnWidth = GetColumnWidth(iColumn, uDrawTextAlignment);
			rcItem.right += iColumnWidth;
			if (rcItem.left < rcItem.right && HaveIntersection(rcClient, rcItem)) {
				const CString &sItem(GetItemDisplayText(file, iColumn));
				switch (iColumn) {
				case 0: //file name
					{
						int iCheckboxDrawWidth = 0;
						if (CheckBoxesEnabled()) {
							CHECKBOXSTATES iState = (file == m_pHighlightedItem) ? CBS_UNCHECKEDHOT : CBS_UNCHECKEDNORMAL;
							int iNoStyleState = (file == m_pHighlightedItem) ? DFCS_PUSHED : 0;
							// no interacting with shell linked files or default shared directories
							if ((file->IsShellLinked() && theApp.sharedfiles->ShouldBeShared(file->GetSharedDirectory(), file->GetFilePath(), false))
								|| (theApp.sharedfiles->ShouldBeShared(file->GetSharedDirectory(), file->GetFilePath(), true)))
							{
								iState = CBS_CHECKEDDISABLED;
								iNoStyleState = DFCS_CHECKED | DFCS_INACTIVE;
							} else if (theApp.sharedfiles->ShouldBeShared(file->GetSharedDirectory(), file->GetFilePath(), false)) {
								iState = (file == m_pHighlightedItem) ? CBS_CHECKEDHOT : CBS_CHECKEDNORMAL;
								iNoStyleState = (file == m_pHighlightedItem) ? (DFCS_PUSHED | DFCS_CHECKED) : DFCS_CHECKED;
							} else if (!thePrefs.IsShareableDirectory(file->GetPath())) {
								iState = CBS_UNCHECKEDDISABLED;
								iNoStyleState = DFCS_INACTIVE;
							}

							HTHEME hTheme = (g_xpStyle.IsThemeActive() && g_xpStyle.IsAppThemed()) ? g_xpStyle.OpenThemeData(NULL, L"BUTTON") : NULL;

							RECT rcCheckBox(rcItem);
							rcCheckBox.left = rcItem.left;
							rcCheckBox.top += max((rcItem.Height() - 16) / 2, 0);
							rcCheckBox.right = rcItem.left + 16;
							rcCheckBox.bottom = rcCheckBox.top + 16;
							if (hTheme != NULL)
								g_xpStyle.DrawThemeBackground(hTheme, dc.GetSafeHdc(), BP_CHECKBOX, iState, &rcCheckBox, NULL);
							else
								dc.DrawFrameControl(&rcCheckBox, DFC_BUTTON, DFCS_BUTTONCHECK | iNoStyleState | DFCS_FLAT);
							rcItem.left += 2 + 16;
							iCheckboxDrawWidth += 2 + 16;
						}

						int iIconPosY = (rcItem.Height() > theApp.GetSmallSytemIconSize().cy) ? ((rcItem.Height() - theApp.GetSmallSytemIconSize().cy) / 2) : 0;
						int iImage = theApp.GetFileTypeSystemImageIdx(file->GetFileName());
						if (theApp.GetSystemImageList() != NULL)
							::ImageList_Draw(theApp.GetSystemImageList(), iImage, dc.GetSafeHdc(), rcItem.left, rcItem.top + iIconPosY, ILD_TRANSPARENT);
						if (!file->GetFileComment().IsEmpty() || file->GetFileRating())
							m_ImageList.Draw(dc, 0, POINT{ rcItem.left, rcItem.top + iIconPosY }, ILD_NORMAL | INDEXTOOVERLAYMASK(1));
						rcItem.left += iIconDrawWidth;

						if (thePrefs.ShowRatingIndicator() && (file->HasComment() || file->HasRating() || file->IsKadCommentSearchRunning())) {
							m_ImageList.Draw(dc, file->UserRating(true) + 3, POINT{ rcItem.left + 2, rcItem.top + iIconPosY }, ILD_NORMAL);
							rcItem.left += 2 + 16;
							iIconDrawWidth += 2 + 16;
						}
						rcItem.left += sm_iLabelOffset;
						rcItem.right -= sm_iSubItemInset;
						dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
						rcItem.left -= sm_iLabelOffset + iIconDrawWidth + iCheckboxDrawWidth;
						rcItem.right += sm_iSubItemInset;
					}
					break;
				case 8: //shared parts bar
					if (pKnownFile != NULL && pKnownFile->GetPartCount()) {
						++rcItem.top;
						--rcItem.bottom;
						pKnownFile->DrawShareStatusBar(dc, &rcItem, false, thePrefs.UseFlatBar());
						++rcItem.bottom;
						--rcItem.top;
					}
					break;
				case 11: //shared ed2k/kad
					if (pKnownFile != NULL) {
						if (pKnownFile->GetPublishedED2K())
							m_ImageList.Draw(dc, 1, rcItem.TopLeft(), ILD_NORMAL);
						if (IsSharedInKad(pKnownFile)) {
							rcItem.left += 16;
							m_ImageList.Draw(dc, IsSharedInKad(pKnownFile) ? 2 : 0, rcItem.TopLeft(), ILD_NORMAL);
							rcItem.left -= 16;
						}
					}
					break;
				default: //all other text columns
					dc.DrawText(sItem, -1, &rcItem, MLC_DT_TEXT | uDrawTextAlignment);
				}
			}
			rcItem.left += iColumnWidth;
		}
	}

	DrawFocusRect(dc, &lpDrawItemStruct->rcItem, lpDrawItemStruct->itemState & ODS_FOCUS, bCtrlFocused, lpDrawItemStruct->itemState & ODS_SELECTED);
}

CString CSharedFilesCtrl::GetItemDisplayText(const CShareableFile *file, int iSubItem) const
{
	CString sText;
	switch (iSubItem) {
	case 0:
		return file->GetFileName();
	case 1:
		return CastItoXBytes((uint64)file->GetFileSize());
	case 2:
		return file->GetFileTypeDisplayStr();
	case 9:
		sText = file->GetPath();
		unslosh(sText);
		//PathRemoveBackslash(sText);
		return sText;
	}

	if (file->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
		const CKnownFile *pKnownFile = static_cast<const CKnownFile*>(file);
		switch (iSubItem) {
		case 3:
			sText = pKnownFile->GetUpPriorityDisplayString();
			break;
		case 4:
			sText = md4str(pKnownFile->GetFileHash());
			break;
		case 5:
			sText.Format(_T("%u (%u)"), pKnownFile->statistic.GetRequests(), pKnownFile->statistic.GetAllTimeRequests());
			break;
		case 6:
			sText.Format(_T("%u (%u)"), pKnownFile->statistic.GetAccepts(), pKnownFile->statistic.GetAllTimeAccepts());
			break;
		case 7:
			sText.Format(_T("%s (%s)"), (LPCTSTR)CastItoXBytes(pKnownFile->statistic.GetTransferred()), (LPCTSTR)CastItoXBytes(pKnownFile->statistic.GetAllTimeTransferred()));
			break;
		case 8:
			sText.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_SHARED_STATUS), pKnownFile->GetPartCount());
			break;
		case 10:
			if (pKnownFile->m_nCompleteSourcesCountLo == pKnownFile->m_nCompleteSourcesCountHi)
				sText.Format(_T("%u"), pKnownFile->m_nCompleteSourcesCountLo);
			else if (pKnownFile->m_nCompleteSourcesCountLo == 0)
				sText.Format(_T("< %u"), pKnownFile->m_nCompleteSourcesCountHi);
			else
				sText.Format(_T("%u - %u"), pKnownFile->m_nCompleteSourcesCountLo, pKnownFile->m_nCompleteSourcesCountHi);
			break;
		case 11:
			sText.Format(_T("%s|%s"), (LPCTSTR)GetResString(pKnownFile->GetPublishedED2K() ? IDS_YES : IDS_NO), (LPCTSTR)GetResString(IsSharedInKad(pKnownFile) ? IDS_YES : IDS_NO));
			break;
		case 12:
			sText = pKnownFile->GetStrTagValue(FT_MEDIA_ARTIST);
			break;
		case 13:
			sText = pKnownFile->GetStrTagValue(FT_MEDIA_ALBUM);
			break;
		case 14:
			sText = pKnownFile->GetStrTagValue(FT_MEDIA_TITLE);
			break;
		case 15:
			{
				uint32 nMediaLength = pKnownFile->GetIntTagValue(FT_MEDIA_LENGTH);
				if (nMediaLength)
					SecToTimeLength(nMediaLength, sText);
			}
			break;
		case 16:
			{
				uint32 nBitrate = pKnownFile->GetIntTagValue(FT_MEDIA_BITRATE);
				if (nBitrate)
					sText.Format(_T("%u %s"), nBitrate, (LPCTSTR)GetResString(IDS_KBITSSEC));
			}
			break;
		case 17:
			sText = GetCodecDisplayName(pKnownFile->GetStrTagValue(FT_MEDIA_CODEC));
		}
	}
	return sText;
}

void CSharedFilesCtrl::OnContextMenu(CWnd*, CPoint point)
{
	// get merged settings
	bool bFirstItem = true;
	bool bContainsShareableFiles = false;
	bool bContainsOnlyShareableFile = true;
	bool bContainsUnshareableFile = false;
	int iSelectedItems = GetSelectedCount();
	int iCompleteFileSelected = -1;
	UINT uPrioMenuItem = 0;
	const CShareableFile *pSingleSelFile = NULL;
	for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
		const CShareableFile *pFile = reinterpret_cast<CShareableFile*>(GetItemData(GetNextSelectedItem(pos)));
		pSingleSelFile = bFirstItem ? pFile : NULL;

		int iCurCompleteFile = static_cast<uint16>(!pFile->IsPartFile());
		if (bFirstItem)
			iCompleteFileSelected = iCurCompleteFile;
		else if (iCompleteFileSelected != iCurCompleteFile)
			iCompleteFileSelected = -1;

		bContainsUnshareableFile = !pFile->IsShellLinked() && !pFile->IsPartFile() && (bContainsUnshareableFile || (theApp.sharedfiles->ShouldBeShared(pFile->GetSharedDirectory(), pFile->GetFilePath(), false)
			&& !theApp.sharedfiles->ShouldBeShared(pFile->GetSharedDirectory(), pFile->GetFilePath(), true)));

		if (pFile->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
			bContainsOnlyShareableFile = false;
			UINT uCurPrioMenuItem = 0;
			if (static_cast<const CKnownFile*>(pFile)->IsAutoUpPriority())
				uCurPrioMenuItem = MP_PRIOAUTO;
			else
				switch (static_cast<const CKnownFile*>(pFile)->GetUpPriority()) {
				case PR_VERYLOW:
					uCurPrioMenuItem = MP_PRIOVERYLOW;
					break;
				case PR_LOW:
					uCurPrioMenuItem = MP_PRIOLOW;
					break;
				case PR_NORMAL:
					uCurPrioMenuItem = MP_PRIONORMAL;
					break;
				case PR_HIGH:
					uCurPrioMenuItem = MP_PRIOHIGH;
					break;
				case PR_VERYHIGH:
					uCurPrioMenuItem = MP_PRIOVERYHIGH;
					break;
				default:
					ASSERT(0);
				}

			if (bFirstItem)
				uPrioMenuItem = uCurPrioMenuItem;
			else if (uPrioMenuItem != uCurPrioMenuItem)
				uPrioMenuItem = 0;
		} else
			bContainsShareableFiles = true;

		bFirstItem = false;
	}

	m_SharedFilesMenu.EnableMenuItem((UINT)m_PrioMenu.m_hMenu, (!bContainsShareableFiles && iSelectedItems > 0) ? MF_ENABLED : MF_GRAYED);
	m_PrioMenu.CheckMenuRadioItem(MP_PRIOVERYLOW, MP_PRIOAUTO, uPrioMenuItem, 0);

	bool bSingleCompleteFileSelected = (iSelectedItems == 1 && (iCompleteFileSelected == 1 || bContainsOnlyShareableFile));
	m_SharedFilesMenu.EnableMenuItem(MP_OPEN, bSingleCompleteFileSelected ? MF_ENABLED : MF_GRAYED);
	UINT uInsertedMenuItem = 0;
	static const TCHAR _szSkinPkgSuffix1[] = _T(".") EMULSKIN_BASEEXT _T(".zip");
	static const TCHAR _szSkinPkgSuffix2[] = _T(".") EMULSKIN_BASEEXT _T(".rar");
	if (bSingleCompleteFileSelected
		&& pSingleSelFile
		&& (pSingleSelFile->GetFilePath().Right(_countof(_szSkinPkgSuffix1) - 1).CompareNoCase(_szSkinPkgSuffix1) == 0
			|| pSingleSelFile->GetFilePath().Right(_countof(_szSkinPkgSuffix2) - 1).CompareNoCase(_szSkinPkgSuffix2) == 0))
	{
		MENUITEMINFO mii = {};
		mii.cbSize = (UINT)sizeof mii;
		mii.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.fType = MFT_STRING;
		mii.fState = MFS_ENABLED;
		mii.wID = MP_INSTALL_SKIN;
		const CString &strBuff(GetResString(IDS_INSTALL_SKIN));
		mii.dwTypeData = const_cast<LPTSTR>((LPCTSTR)strBuff);
		if (m_SharedFilesMenu.InsertMenuItem(MP_OPENFOLDER, &mii, FALSE))
			uInsertedMenuItem = mii.wID;
	}
	m_SharedFilesMenu.EnableMenuItem(MP_OPENFOLDER, bSingleCompleteFileSelected ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.EnableMenuItem(MP_RENAME, (!bContainsShareableFiles && bSingleCompleteFileSelected) ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.EnableMenuItem(MP_REMOVE, iCompleteFileSelected > 0 ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.EnableMenuItem(MP_UNSHAREFILE, bContainsUnshareableFile ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.SetDefaultItem(bSingleCompleteFileSelected ? MP_OPEN : -1);
	m_SharedFilesMenu.EnableMenuItem(MP_CMT, (!bContainsShareableFiles && iSelectedItems > 0) ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.EnableMenuItem(MP_DETAIL, iSelectedItems > 0 ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.EnableMenuItem(thePrefs.GetShowCopyEd2kLinkCmd() ? MP_GETED2KLINK : MP_SHOWED2KLINK, (!bContainsOnlyShareableFile && iSelectedItems > 0) ? MF_ENABLED : MF_GRAYED);
	m_SharedFilesMenu.EnableMenuItem(MP_FIND, GetItemCount() > 0 ? MF_ENABLED : MF_GRAYED);

	const CCollection *coll = pSingleSelFile ? static_cast<const CKnownFile*>(pSingleSelFile)->m_pCollection : NULL;
	m_CollectionsMenu.EnableMenuItem(MP_MODIFYCOLLECTION, (!bContainsShareableFiles && coll != NULL) ? MF_ENABLED : MF_GRAYED);
	m_CollectionsMenu.EnableMenuItem(MP_VIEWCOLLECTION, (!bContainsShareableFiles && coll != NULL) ? MF_ENABLED : MF_GRAYED);
	m_CollectionsMenu.EnableMenuItem(MP_SEARCHAUTHOR, (!bContainsShareableFiles && coll != NULL && !coll->GetAuthorKeyHashString().IsEmpty()) ? MF_ENABLED : MF_GRAYED);
#if defined(_DEBUG)
	if (thePrefs.IsExtControlsEnabled()) {
		//JOHNTODO: Not for release as we need kad lowID users in the network to see how well this work. Also, we do not support these links yet.
		bool bEnable = (iSelectedItems > 0 && theApp.IsConnected() && theApp.IsFirewalled() && theApp.clientlist->GetBuddy());
		m_SharedFilesMenu.EnableMenuItem(MP_GETKADSOURCELINK, (bEnable ? MF_ENABLED : MF_GRAYED));
	}
#endif
	m_SharedFilesMenu.EnableMenuItem(Irc_SetSendLink, (!bContainsOnlyShareableFile && iSelectedItems == 1 && theApp.emuledlg->ircwnd->IsConnected()) ? MF_ENABLED : MF_GRAYED);

	CTitleMenu WebMenu;
	WebMenu.CreateMenu();
	WebMenu.AddMenuTitle(NULL, true);
	int iWebMenuEntries = theWebServices.GetFileMenuEntries(&WebMenu);
	UINT flag2 = (iWebMenuEntries == 0 || iSelectedItems != 1) ? MF_GRAYED : MF_STRING;
	m_SharedFilesMenu.AppendMenu(flag2 | MF_POPUP, (UINT_PTR)WebMenu.m_hMenu, GetResString(IDS_WEBSERVICES), _T("WEB"));

	GetPopupMenuPos(*this, point);
	m_SharedFilesMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);

	m_SharedFilesMenu.RemoveMenu(m_SharedFilesMenu.GetMenuItemCount() - 1, MF_BYPOSITION);
	VERIFY(WebMenu.DestroyMenu());
	if (uInsertedMenuItem)
		VERIFY(m_SharedFilesMenu.RemoveMenu(uInsertedMenuItem, MF_BYCOMMAND));
}

BOOL CSharedFilesCtrl::OnCommand(WPARAM wParam, LPARAM)
{
	wParam = LOWORD(wParam);

	CTypedPtrList<CPtrList, CShareableFile*> selectedList;
	for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
		int index = GetNextSelectedItem(pos);
		if (index >= 0)
			selectedList.AddTail(reinterpret_cast<CShareableFile*>(GetItemData(index)));
	}

	if (wParam == MP_CREATECOLLECTION || wParam == MP_FIND || !selectedList.IsEmpty()) {
		CShareableFile *file = (selectedList.GetCount() == 1) ? selectedList.GetHead() : NULL;

		CKnownFile *pKnownFile;
		if (file != NULL && file->IsKindOf(RUNTIME_CLASS(CKnownFile)))
			pKnownFile = static_cast<CKnownFile*>(file);
		else
			pKnownFile = NULL;

		switch (wParam) {
		case Irc_SetSendLink:
			if (pKnownFile != NULL)
				theApp.emuledlg->ircwnd->SetSendFileString(pKnownFile->GetED2kLink());
			break;
		case MP_GETED2KLINK:
			{
				CString str;
				for (POSITION pos = selectedList.GetHeadPosition(); pos != NULL;) {
					CKnownFile *pfile = static_cast<CKnownFile*>(selectedList.GetNext(pos));
					if (pfile != NULL && pfile->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
						if (!str.IsEmpty())
							str += _T("\r\n");
						str += pfile->GetED2kLink();
					}
				}
				theApp.CopyTextToClipboard(str);
			}
			break;
#if defined(_DEBUG)
		//JOHNTODO: Not for release as we need kad lowID users in the network to see how well this works. Also, we do not support these links yet.
		case MP_GETKADSOURCELINK:
			{
				CString str;
				for (POSITION pos = selectedList.GetHeadPosition(); pos != NULL;) {
					const CKnownFile *pfile = static_cast<CKnownFile*>(selectedList.GetNext(pos));
					if (pfile->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
						if (!str.IsEmpty())
							str += _T("\r\n");
						str += theApp.CreateKadSourceLink(pfile);
					}
				}
				theApp.CopyTextToClipboard(str);
			}
			break;
#endif
		// file operations
		case MP_OPEN:
		case IDA_ENTER:
			if (file && !file->IsPartFile())
				OpenFile(file);
			break;
		case MP_INSTALL_SKIN:
			if (file && !file->IsPartFile())
				InstallSkin(file->GetFilePath());
			break;
		case MP_OPENFOLDER:
			if (file && !file->IsPartFile())
				ShellOpen(_T("explorer"), _T("/select,\"") + file->GetFilePath() + _T('\"'));
			break;
		case MP_RENAME:
		case MPG_F2:
			if (pKnownFile && !pKnownFile->IsPartFile()) {
				InputBox inputbox;
				inputbox.SetLabels(GetResNoAmp(IDS_RENAME), GetResString(IDS_DL_FILENAME), pKnownFile->GetFileName());
				inputbox.SetEditFilenameMode();
				inputbox.DoModal();
				const CString &newname(inputbox.GetInput());
				if (!inputbox.WasCancelled() && !newname.IsEmpty()) {
					// at least prevent users from specifying something like "..\dir\file"
					if (newname.FindOneOf(sBadFileNameChar) >= 0) {
						AfxMessageBox(GetErrorMessage(ERROR_BAD_PATHNAME));
						break;
					}

					CString newpath;
					PathCombine(newpath.GetBuffer(MAX_PATH), pKnownFile->GetPath(), newname);
					newpath.ReleaseBuffer();
					if (_trename(pKnownFile->GetFilePath(), newpath) != 0) {
						CString strError;
						strError.Format(GetResString(IDS_ERR_RENAMESF), (LPCTSTR)pKnownFile->GetFilePath(), (LPCTSTR)newpath, _tcserror(errno));
						AfxMessageBox(strError);
						break;
					}

					if (pKnownFile->IsKindOf(RUNTIME_CLASS(CPartFile))) {
						pKnownFile->SetFileName(newname);
						static_cast<CPartFile*>(pKnownFile)->SetFullName(newpath);
					} else {
						theApp.sharedfiles->RemoveKeywords(pKnownFile);
						pKnownFile->SetFileName(newname);
						theApp.sharedfiles->AddKeywords(pKnownFile);
					}
					pKnownFile->SetFilePath(newpath);
					UpdateFile(pKnownFile);
				}
			} else
				MessageBeep(MB_OK);
			break;
		case MP_REMOVE:
		case MPG_DELETE:
			{
				if (IDNO == LocMessageBox(IDS_CONFIRM_FILEDELETE, MB_ICONWARNING | MB_DEFBUTTON2 | MB_YESNO, 0))
					return TRUE;

				SetRedraw(FALSE);
				bool bRemovedItems = false;
				while (!selectedList.IsEmpty()) {
					CShareableFile *myfile = selectedList.RemoveHead();
					if (!myfile || myfile->IsPartFile())
						continue;

					bool delsucc = ShellDeleteFile(myfile->GetFilePath());
					if (delsucc) {
						if (myfile->IsKindOf(RUNTIME_CLASS(CKnownFile)))
							theApp.sharedfiles->RemoveFile(static_cast<CKnownFile*>(myfile), true);
						else
							RemoveFile(myfile, true);
						bRemovedItems = true;
						if (myfile->IsKindOf(RUNTIME_CLASS(CPartFile)))
							theApp.emuledlg->transferwnd->GetDownloadList()->ClearCompleted(static_cast<CPartFile*>(myfile));
					} else {
						CString strError;
						strError.Format(GetResString(IDS_ERR_DELFILE) + _T("\r\n\r\n%s"), (LPCTSTR)myfile->GetFilePath(), (LPCTSTR)GetErrorMessage(GetLastError()));
						AfxMessageBox(strError);
					}
				}
				SetRedraw(TRUE);
				if (bRemovedItems) {
					AutoSelectItem();
					// Depending on <no-idea> this does not always cause a
					// LVN_ITEMACTIVATE message sent. So, explicitly redraw
					// the item.
					theApp.emuledlg->sharedfileswnd->ShowSelectedFilesDetails();
					theApp.emuledlg->sharedfileswnd->OnSingleFileShareStatusChanged(); // might have been a single shared file
				}
			}
			break;
		case MP_UNSHAREFILE:
			{
				SetRedraw(FALSE);
				bool bUnsharedItems = false;
				while (!selectedList.IsEmpty()) {
					const CShareableFile *myfile = selectedList.RemoveHead();
					if (myfile && !myfile->IsPartFile() && theApp.sharedfiles->ShouldBeShared(myfile->GetPath(), myfile->GetFilePath(), false)
						&& !theApp.sharedfiles->ShouldBeShared(myfile->GetPath(), myfile->GetFilePath(), true))
					{
						bUnsharedItems |= theApp.sharedfiles->ExcludeFile(myfile->GetFilePath());
						ASSERT(bUnsharedItems);
					}
				}
				SetRedraw(TRUE);
				if (bUnsharedItems) {
					theApp.emuledlg->sharedfileswnd->ShowSelectedFilesDetails();
					theApp.emuledlg->sharedfileswnd->OnSingleFileShareStatusChanged();
					if (GetFirstSelectedItemPosition() == NULL)
						AutoSelectItem();
				}
			}
			break;
		case MP_CMT:
			ShowFileDialog(selectedList, IDD_COMMENT);
			break;
		case MPG_ALTENTER:
		case MP_DETAIL:
			ShowFileDialog(selectedList);
			break;
		case MP_FIND:
			OnFindStart();
			break;
		case MP_CREATECOLLECTION:
			{
				CCollection *pCollection = new CCollection();
				for (POSITION pos = selectedList.GetHeadPosition(); pos != NULL;) {
					CShareableFile *pFile = selectedList.GetNext(pos);
					if (pFile->IsKindOf(RUNTIME_CLASS(CKnownFile)))
						pCollection->AddFileToCollection(pFile, true);
				}
				CCollectionCreateDialog dialog;
				dialog.SetCollection(pCollection, true);
				dialog.DoModal();
				//We delete this collection object because when the newly created
				//collection file is added to the shared file list, it is read and verified
				//and which creates the collection object that is attached to that file.
				delete pCollection;
			}
			break;
		case MP_SEARCHAUTHOR:
			if (pKnownFile && pKnownFile->m_pCollection) {
				SSearchParams *pParams = new SSearchParams;
				pParams->strExpression = pKnownFile->m_pCollection->GetCollectionAuthorKeyString();
				pParams->eType = SearchTypeKademlia;
				pParams->strFileType = _T(ED2KFTSTR_EMULECOLLECTION);
				pParams->strSpecialTitle = pKnownFile->m_pCollection->m_sCollectionAuthorName;
				if (pParams->strSpecialTitle.GetLength() > 50) {
					pParams->strSpecialTitle.Truncate(50);
					pParams->strSpecialTitle += _T("...");
				}

				theApp.emuledlg->searchwnd->m_pwndResults->StartSearch(pParams);
			}
			break;
		case MP_VIEWCOLLECTION:
			if (pKnownFile && pKnownFile->m_pCollection) {
				CCollectionViewDialog dialog;
				dialog.SetCollection(pKnownFile->m_pCollection);
				dialog.DoModal();
			}
			break;
		case MP_MODIFYCOLLECTION:
			if (pKnownFile && pKnownFile->m_pCollection) {
				CCollectionCreateDialog dialog;
				CCollection *pCollection = new CCollection(pKnownFile->m_pCollection);
				dialog.SetCollection(pCollection, false);
				dialog.DoModal();
				delete pCollection;
			}
			break;
		case MP_SHOWED2KLINK:
			ShowFileDialog(selectedList, IDD_ED2KLINK);
			break;
		case MP_PRIOVERYLOW:
		case MP_PRIOLOW:
		case MP_PRIONORMAL:
		case MP_PRIOHIGH:
		case MP_PRIOVERYHIGH:
		case MP_PRIOAUTO:
			for (POSITION pos = selectedList.GetHeadPosition(); pos != NULL;) {
				CKnownFile *pfile = static_cast<CKnownFile*>(selectedList.GetNext(pos));
				if (pfile->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
					pfile->SetAutoUpPriority(wParam == MP_PRIOAUTO);
					switch (wParam) {
					case MP_PRIOVERYLOW:
						pfile->SetUpPriority(PR_VERYLOW);
						break;
					case MP_PRIOLOW:
						pfile->SetUpPriority(PR_LOW);
						break;
					case MP_PRIONORMAL:
						pfile->SetUpPriority(PR_NORMAL);
						break;
					case MP_PRIOHIGH:
						pfile->SetUpPriority(PR_HIGH);
						break;
					case MP_PRIOVERYHIGH:
						pfile->SetUpPriority(PR_VERYHIGH);
						break;
					case MP_PRIOAUTO:
						pfile->UpdateAutoUpPriority();
					}
					UpdateFile(pfile);
				}
			}
			break;
		default:
			if (file && wParam >= MP_WEBURL && wParam <= MP_WEBURL + 256)
				theWebServices.RunURL(file, (UINT)wParam);
		}
	}
	return TRUE;
}

void CSharedFilesCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	bool sortAscending;
	if (GetSortItem() == pNMListView->iSubItem)
		sortAscending = !GetSortAscending();
	else
		switch (pNMListView->iSubItem) {
		case 3:  // Priority
		case 5:  // Requests
		case 6:  // Accepted Requests
		case 7:  // Transferred Data
		case 10: // Complete Sources
		case 11: // Shared ed2k/kad
			// Keep the current 'm_aSortBySecondValue' for that column, but reset to 'descending'
			sortAscending = false;
			break;
		default:
			sortAscending = true;
		}

	// Ornis 4-way-sorting
	int adder = 0;
	if (pNMListView->iSubItem >= 5 && pNMListView->iSubItem <= 7) { // 5=IDS_SF_REQUESTS, 6=IDS_SF_ACCEPTS, 7=IDS_SF_TRANSFERRED
		ASSERT(pNMListView->iSubItem - 5 < _countof(m_aSortBySecondValue));
		if (GetSortItem() == pNMListView->iSubItem && !sortAscending) // check for 'descending' because the initial sort order is also 'descending'
			m_aSortBySecondValue[pNMListView->iSubItem - 5] = !m_aSortBySecondValue[pNMListView->iSubItem - 5];
		adder = m_aSortBySecondValue[pNMListView->iSubItem - 5] ? 100 : 0;
	} else if (pNMListView->iSubItem == 11) { // 11=IDS_SHAREDTITLE
		ASSERT(3 < _countof(m_aSortBySecondValue));
		if (GetSortItem() == pNMListView->iSubItem && !sortAscending) // check for 'descending' because the initial sort order is also 'descending'
			m_aSortBySecondValue[3] = !m_aSortBySecondValue[3];
		adder = m_aSortBySecondValue[3] ? 100 : 0;
	}

	// Sort table
	if (adder == 0)
		SetSortArrow(pNMListView->iSubItem, sortAscending);
	else
		SetSortArrow(pNMListView->iSubItem, sortAscending ? arrowDoubleUp : arrowDoubleDown);
	if (!sortAscending)
		adder += 20;
	UpdateSortHistory(pNMListView->iSubItem + adder, 20);
	SortItems(SortProc, pNMListView->iSubItem + adder);

	*pResult = 0;
}

int CALLBACK CSharedFilesCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const CShareableFile *item1 = reinterpret_cast<CShareableFile*>(lParam1);
	const CShareableFile *item2 = reinterpret_cast<CShareableFile*>(lParam2);

	bool bSortAscending;
	if (lParamSort >= 100)
		bSortAscending = lParamSort < 120;
	else
		bSortAscending = lParamSort < 20;
	LPARAM iColumn = bSortAscending ? lParamSort : lParamSort - 20;

	int iResult = 0;
	bool bExtColumn = false;

	switch (iColumn) {
	case 0: //file name
		iResult = CompareLocaleStringNoCase(item1->GetFileName(), item2->GetFileName());
		break;
	case 1: //file size
		iResult = CompareUnsigned64(item1->GetFileSize(), item2->GetFileSize());
		break;
	case 2: //file type
		iResult = item1->GetFileTypeDisplayStr().Compare(item2->GetFileTypeDisplayStr());
		// if the type is equal, sub-sort by extension
		if (iResult == 0) {
			LPCTSTR pszExt1 = PathFindExtension(item1->GetFileName());
			LPCTSTR pszExt2 = PathFindExtension(item2->GetFileName());
			if (!*pszExt1 ^ !*pszExt2)
				iResult = *pszExt1 ? -1 : 1;
			else
				iResult = *pszExt1 ? _tcsicmp(pszExt1, pszExt2) : 0;
		}
		break;
	case 9: //folder
		iResult = CompareLocaleStringNoCase(item1->GetPath(), item2->GetPath());
		break;
	default:
		bExtColumn = true;
	}

	if (bExtColumn) {
		if (item1->IsKindOf(RUNTIME_CLASS(CKnownFile)) && !item2->IsKindOf(RUNTIME_CLASS(CKnownFile)))
			iResult = -1;
		else if (!item1->IsKindOf(RUNTIME_CLASS(CKnownFile)) && item2->IsKindOf(RUNTIME_CLASS(CKnownFile)))
			iResult = 1;
		else if (item1->IsKindOf(RUNTIME_CLASS(CKnownFile)) && item2->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
			const CKnownFile *kitem1 = static_cast<const CKnownFile*>(item1);
			const CKnownFile *kitem2 = static_cast<const CKnownFile*>(item2);

			switch (iColumn) {
			case 3: //prio
				{
					uint8 p1 = kitem1->GetUpPriority() + 1;
					if (p1 == 5)
						p1 = 0;
					uint8 p2 = kitem2->GetUpPriority() + 1;
					if (p2 == 5)
						p2 = 0;
					iResult = p1 - p2;
				}
				break;
			case 4: //fileID
				iResult = memcmp(kitem1->GetFileHash(), kitem2->GetFileHash(), 16);
				break;
			case 5: //requests
				iResult = CompareUnsigned(kitem1->statistic.GetRequests(), kitem2->statistic.GetRequests());
				break;
			case 6: //acc requests
				iResult = CompareUnsigned(kitem1->statistic.GetAccepts(), kitem2->statistic.GetAccepts());
				break;
			case 7: //all transferred
				iResult = CompareUnsigned64(kitem1->statistic.GetTransferred(), kitem2->statistic.GetTransferred());
				break;
			case 10: //complete sources
				iResult = CompareUnsigned(kitem1->m_nCompleteSourcesCount, kitem2->m_nCompleteSourcesCount);
				break;
			case 11: //ed2k shared
				iResult = kitem1->GetPublishedED2K() - kitem2->GetPublishedED2K();
				break;
			case 12:
				iResult = CompareOptLocaleStringNoCaseUndefinedAtBottom(kitem1->GetStrTagValue(FT_MEDIA_ARTIST), kitem2->GetStrTagValue(FT_MEDIA_ARTIST), bSortAscending);
				break;
			case 13:
				iResult = CompareOptLocaleStringNoCaseUndefinedAtBottom(kitem1->GetStrTagValue(FT_MEDIA_ALBUM), kitem2->GetStrTagValue(FT_MEDIA_ALBUM), bSortAscending);
				break;
			case 14:
				iResult = CompareOptLocaleStringNoCaseUndefinedAtBottom(kitem1->GetStrTagValue(FT_MEDIA_TITLE), kitem2->GetStrTagValue(FT_MEDIA_TITLE), bSortAscending);
				break;
			case 15:
				iResult = CompareUnsignedUndefinedAtBottom(kitem1->GetIntTagValue(FT_MEDIA_LENGTH), kitem2->GetIntTagValue(FT_MEDIA_LENGTH), bSortAscending);
				break;
			case 16:
				iResult = CompareUnsignedUndefinedAtBottom(kitem1->GetIntTagValue(FT_MEDIA_BITRATE), kitem2->GetIntTagValue(FT_MEDIA_BITRATE), bSortAscending);
				break;
			case 17:
				iResult = CompareOptLocaleStringNoCaseUndefinedAtBottom(GetCodecDisplayName(kitem1->GetStrTagValue(FT_MEDIA_CODEC)), GetCodecDisplayName(kitem2->GetStrTagValue(FT_MEDIA_CODEC)), bSortAscending);
				break;

			case 105: //all requests
				iResult = CompareUnsigned(kitem1->statistic.GetAllTimeRequests(), kitem2->statistic.GetAllTimeRequests());
				break;
			case 106: //all accepted requests
				iResult = CompareUnsigned(kitem1->statistic.GetAllTimeAccepts(), kitem2->statistic.GetAllTimeAccepts());
				break;
			case 107: //all transferred
				iResult = CompareUnsigned64(kitem1->statistic.GetAllTimeTransferred(), kitem2->statistic.GetAllTimeTransferred());
				break;
			case 111: //kad shared
				{
					time_t tNow = time(NULL);
					int i2 = static_cast<int>(tNow < kitem2->GetLastPublishTimeKadSrc());
					int i1 = static_cast<int>(tNow < kitem1->GetLastPublishTimeKadSrc());
					iResult = i1 - i2;
				}
			}
		}
	}

	if (!bSortAscending)
		iResult = -iResult;

	//call secondary sort order, if this one results in equal
	if (iResult == 0) {
		int dwNextSort = theApp.emuledlg->sharedfileswnd->sharedfilesctrl.GetNextSortOrder((int)lParamSort);
		if (dwNextSort != -1)
			iResult = SortProc(lParam1, lParam2, dwNextSort);
	}

	return iResult;
}

void CSharedFilesCtrl::OpenFile(const CShareableFile *file)
{
	if (file->IsKindOf(RUNTIME_CLASS(CKnownFile)) && static_cast<const CKnownFile*>(file)->m_pCollection) {
		CCollectionViewDialog dialog;
		dialog.SetCollection(static_cast<const CKnownFile*>(file)->m_pCollection);
		dialog.DoModal();
	} else
		ShellDefaultVerb(file->GetFilePath());
}

void CSharedFilesCtrl::OnNmDblClk(LPNMHDR, LRESULT *pResult)
{
	int iSel = GetNextItem(-1, LVIS_SELECTED | LVIS_FOCUSED);
	if (iSel >= 0) {
		CShareableFile *file = reinterpret_cast<CShareableFile*>(GetItemData(iSel));
		if (file) {
			if (GetKeyState(VK_MENU) & 0x8000) {
				CTypedPtrList<CPtrList, CShareableFile*> aFiles;
				aFiles.AddHead(file);
				ShowFileDialog(aFiles);
			} else if (!file->IsPartFile())
				OpenFile(file);
		}
	}
	*pResult = 0;
}

void CSharedFilesCtrl::CreateMenus()
{
	if (m_PrioMenu)
		VERIFY(m_PrioMenu.DestroyMenu());
	if (m_CollectionsMenu)
		VERIFY(m_CollectionsMenu.DestroyMenu());
	if (m_SharedFilesMenu)
		VERIFY(m_SharedFilesMenu.DestroyMenu());

	m_PrioMenu.CreateMenu();
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOVERYLOW, GetResString(IDS_PRIOVERYLOW));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOLOW, GetResString(IDS_PRIOLOW));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIONORMAL, GetResString(IDS_PRIONORMAL));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOHIGH, GetResString(IDS_PRIOHIGH));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOVERYHIGH, GetResString(IDS_PRIORELEASE));
	m_PrioMenu.AppendMenu(MF_STRING, MP_PRIOAUTO, GetResString(IDS_PRIOAUTO));//UAP

	m_CollectionsMenu.CreateMenu();
	m_CollectionsMenu.AddMenuTitle(NULL, true);
	m_CollectionsMenu.AppendMenu(MF_STRING, MP_CREATECOLLECTION, GetResString(IDS_CREATECOLLECTION), _T("COLLECTION_ADD"));
	m_CollectionsMenu.AppendMenu(MF_STRING, MP_MODIFYCOLLECTION, GetResString(IDS_MODIFYCOLLECTION), _T("COLLECTION_EDIT"));
	m_CollectionsMenu.AppendMenu(MF_STRING, MP_VIEWCOLLECTION, GetResString(IDS_VIEWCOLLECTION), _T("COLLECTION_VIEW"));
	m_CollectionsMenu.AppendMenu(MF_STRING, MP_SEARCHAUTHOR, GetResString(IDS_SEARCHAUTHORCOLLECTION), _T("COLLECTION_SEARCH"));

	m_SharedFilesMenu.CreatePopupMenu();
	m_SharedFilesMenu.AddMenuTitle(GetResString(IDS_SHAREDFILES), true);

	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_OPEN, GetResString(IDS_OPENFILE), _T("OPENFILE"));
	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_OPENFOLDER, GetResString(IDS_OPENFOLDER), _T("OPENFOLDER"));
	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_RENAME, GetResString(IDS_RENAME) + _T("..."), _T("FILERENAME"));
	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_REMOVE, GetResString(IDS_DELETE), _T("DELETE"));
	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_UNSHAREFILE, GetResString(IDS_UNSHARE), _T("KADBOOTSTRAP")); // TODO: better icon
	if (thePrefs.IsExtControlsEnabled())
		m_SharedFilesMenu.AppendMenu(MF_STRING, Irc_SetSendLink, GetResString(IDS_IRC_ADDLINKTOIRC), _T("IRCCLIPBOARD"));

	m_SharedFilesMenu.AppendMenu(MF_STRING | MF_SEPARATOR);
	CString sPrio(GetResString(IDS_PRIORITY));
	sPrio.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_PW_CON_UPLBL));
	m_SharedFilesMenu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)m_PrioMenu.m_hMenu, sPrio, _T("FILEPRIORITY"));
	m_SharedFilesMenu.AppendMenu(MF_STRING | MF_SEPARATOR);

	m_SharedFilesMenu.AppendMenu(MF_STRING | MF_POPUP, (UINT_PTR)m_CollectionsMenu.m_hMenu, GetResString(IDS_META_COLLECTION), _T("AABCollectionFileType"));
	m_SharedFilesMenu.AppendMenu(MF_STRING | MF_SEPARATOR);

	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_DETAIL, GetResString(IDS_SHOWDETAILS), _T("FILEINFO"));
	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_CMT, GetResString(IDS_CMT_ADD), _T("FILECOMMENTS"));
	if (thePrefs.GetShowCopyEd2kLinkCmd())
		m_SharedFilesMenu.AppendMenu(MF_STRING, MP_GETED2KLINK, GetResString(IDS_DL_LINK1), _T("ED2KLINK"));
	else
		m_SharedFilesMenu.AppendMenu(MF_STRING, MP_SHOWED2KLINK, GetResString(IDS_DL_SHOWED2KLINK), _T("ED2KLINK"));
	m_SharedFilesMenu.AppendMenu(MF_STRING, MP_FIND, GetResString(IDS_FIND), _T("Search"));
	m_SharedFilesMenu.AppendMenu(MF_STRING | MF_SEPARATOR);

#if defined(_DEBUG)
	if (thePrefs.IsExtControlsEnabled()) {
		//JOHNTODO: Not for release as we need kad lowID users in the network to see how well this works. Also, we do not support these links yet.
		m_SharedFilesMenu.AppendMenu(MF_STRING, MP_GETKADSOURCELINK, _T("Copy eD2K Links To Clipboard (Kad)"));
		m_SharedFilesMenu.AppendMenu(MF_STRING | MF_SEPARATOR);
	}
#endif
}

void CSharedFilesCtrl::ShowComments(CShareableFile *file)
{
	if (file) {
		CTypedPtrList<CPtrList, CShareableFile*> aFiles;
		aFiles.AddHead(file);
		ShowFileDialog(aFiles, IDD_COMMENT);
	}
}

void CSharedFilesCtrl::OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult)
{
	if (!theApp.IsClosing()) {
		// Although we have an owner drawn listview control we store the text for the primary item in the listview, to be
		// capable of quick searching those items via the keyboard. Because our listview items may change their contents,
		// we do this via a text callback function. The listview control will send us the LVN_DISPINFO notification if
		// it needs to know the contents of the primary item.
		//
		// But, the listview control sends this notification all the time, even if we do not search for an item. At least
		// this notification is only sent for the visible items and not for all items in the list. Though, because this
		// function is invoked *very* often, do *NOT* put any time consuming code in here.
		//
		// Vista: That callback is used to get the strings for the label tips for the sub(!) items.
		//
		const LVITEMW &rItem = reinterpret_cast<NMLVDISPINFO*>(pNMHDR)->item;
		if (rItem.mask & LVIF_TEXT) {
			const CShareableFile *pFile = reinterpret_cast<CShareableFile*>(rItem.lParam);
			if (pFile != NULL)
				_tcsncpy_s(rItem.pszText, rItem.cchTextMax, GetItemDisplayText(pFile, rItem.iSubItem), _TRUNCATE);
		}
	}
	*pResult = 0;
}

void CSharedFilesCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == 'C' && GetKeyState(VK_CONTROL) < 0) {
		// Ctrl+C: Copy listview items to clipboard
		SendMessage(WM_COMMAND, MP_GETED2KLINK);
		return;
	}
	if (nChar == VK_F5)
		ReloadFileList();
	else if (nChar == VK_SPACE && CheckBoxesEnabled()) {
		// Toggle Checkboxes
		// selection and item position might change during processing (shouldn't though, but lets make sure), so first get all pointers instead using the selection pos directly
		SetRedraw(FALSE);
		CTypedPtrList<CPtrList, CShareableFile*> selectedList;
		for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;) {
			int index = GetNextSelectedItem(pos);
			if (index >= 0)
				selectedList.AddTail(reinterpret_cast<CShareableFile*>(GetItemData(index)));
		}
		while (!selectedList.IsEmpty())
			CheckBoxClicked(FindFile(selectedList.RemoveHead()));

		SetRedraw(TRUE);
	}

	CMuleListCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CSharedFilesCtrl::ShowFileDialog(CTypedPtrList<CPtrList, CShareableFile*> &aFiles, UINT uInvokePage)
{
	if (!aFiles.IsEmpty()) {
		CSharedFileDetailsSheet dialog(aFiles, uInvokePage, this);
		dialog.DoModal();
	}
}

void CSharedFilesCtrl::SetDirectoryFilter(CDirectoryItem *pNewFilter, bool bRefresh)
{
	if (m_pDirectoryFilter != pNewFilter) {
		m_pDirectoryFilter = pNewFilter;
		if (bRefresh)
			ReloadFileList();
	}
}

void CSharedFilesCtrl::OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVGETINFOTIP pGetInfoTip = reinterpret_cast<LPNMLVGETINFOTIP>(pNMHDR);
	LVHITTESTINFO hti;
	if (pGetInfoTip && pGetInfoTip->iSubItem == 0 && ::GetCursorPos(&hti.pt)) {
		ScreenToClient(&hti.pt);
		if (SubItemHitTest(&hti) == -1 || hti.iItem != pGetInfoTip->iItem || hti.iSubItem != 0) {
			// don' show the default label tip for the main item, if the mouse is not over the main item
			if ((pGetInfoTip->dwFlags & LVGIT_UNFOLDED) == 0 && pGetInfoTip->cchTextMax > 0 && pGetInfoTip->pszText[0] != _T('\0'))
				pGetInfoTip->pszText[0] = _T('\0');
			return;
		}

		const CShareableFile *pFile = reinterpret_cast<CShareableFile*>(GetItemData(pGetInfoTip->iItem));
		if (pFile && pGetInfoTip->pszText && pGetInfoTip->cchTextMax > 0) {
			CString strInfo = pFile->GetInfoSummary();
			strInfo += TOOLTIP_AUTOFORMAT_SUFFIX_CH;
			_tcsncpy(pGetInfoTip->pszText, strInfo, pGetInfoTip->cchTextMax);
			pGetInfoTip->pszText[pGetInfoTip->cchTextMax - 1] = _T('\0');
		}
	}
	*pResult = 0;
}

bool CSharedFilesCtrl::IsFilteredItem(const CShareableFile *pFile) const
{
	const CStringArray &rastrFilter = theApp.emuledlg->sharedfileswnd->m_astrFilter;
	if (!rastrFilter.IsEmpty()) {
		// filtering is done by text only for all columns to keep it consistent and simple for the user
		// even if that doesn't allows complex filters
		const CString &szFilterTarget(GetItemDisplayText(pFile, theApp.emuledlg->sharedfileswnd->GetFilterColumn()));

		for (INT_PTR i = rastrFilter.GetCount(); --i >= 0;) {
			LPCTSTR pszText = (LPCTSTR)rastrFilter[i];
			bool bAnd = (*pszText != _T('-'));
			if (!bAnd)
				++pszText;

			bool bFound = (stristr(szFilterTarget, pszText) != NULL);
			if (bAnd != bFound)
				return true;
		}
	}
	return false;
}

void CSharedFilesCtrl::SetToolTipsDelay(DWORD dwDelay)
{
	CToolTipCtrl *tooltip = GetToolTips();
	if (tooltip)
		tooltip->SetDelayTime(TTDT_INITIAL, dwDelay);
}

bool CSharedFilesCtrl::IsSharedInKad(const CKnownFile *file) const
{
	if (!Kademlia::CKademlia::IsConnected() || time(NULL) >= file->GetLastPublishTimeKadSrc())
		return false;
	if (!Kademlia::CKademlia::IsFirewalled())
		return true;
	return (theApp.clientlist->GetBuddy() && (file->GetLastPublishBuddy() == theApp.clientlist->GetBuddy()->GetIP()))
		|| (Kademlia::CKademlia::IsRunning() && !Kademlia::CUDPFirewallTester::IsFirewalledUDP(true) && Kademlia::CUDPFirewallTester::IsVerified());
}

void CSharedFilesCtrl::AddShareableFiles(const CString &strFromDir)
{
	while (!liTempShareableFilesInDir.IsEmpty())	// cleanup old file list
		delete liTempShareableFilesInDir.RemoveHead();

	CString strSearchPath(strFromDir);
	PathAddBackslash(strSearchPath.GetBuffer(strFromDir.GetLength() + 1));
	strSearchPath.ReleaseBuffer();
	CFileFind ff;
	bool end = !ff.FindFile(strSearchPath + _T('*'), 0);
	if (end) {
		DWORD dwError = ::GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
			DebugLogError(_T("Failed to find files for SharedFilesListCtrl in %s, %s"), (LPCTSTR)strFromDir, (LPCTSTR)GetErrorMessage(dwError));
		return;
	}

	SetRedraw(FALSE);
	while (!end) {
		end = !ff.FindNextFile();
		if (ff.IsDirectory() || ff.IsDots() || ff.IsSystem() || ff.IsTemporary() || ff.GetLength() == 0 || ff.GetLength() > MAX_EMULE_FILE_SIZE)
			continue;

		const CString &strFoundFileName(ff.GetFileName());
		const CString &strFoundFilePath(ff.GetFilePath());
		const CString &strFoundDirectory(strFoundFilePath.Left(ff.GetFilePath().ReverseFind(_T('\\')) + 1));
		ULONGLONG ullFoundFileSize = ff.GetLength();

		FILETIME tFoundFileTime;
		ff.GetLastWriteTime(&tFoundFileTime);
		// ignore real(!) LNK files
		if (ExtensionIs(strFoundFileName, _T(".lnk"))) {
			SHFILEINFO info;
			if (SHGetFileInfo(strFoundFilePath, 0, &info, sizeof info, SHGFI_ATTRIBUTES) && (info.dwAttributes & SFGAO_LINK))
				continue;
		}

		// ignore real(!) thumbs.db files -- seems that lot of ppl have 'thumbs.db' files without the 'System' file attribute
		if (strFoundFileName.CompareNoCase(_T("thumbs.db")) == 0) {
			// if that's a valid 'Storage' file, we declare it as a "thumbs.db" file.
			CComPtr<IStorage> pStorage;
			if (StgOpenStorage(strFoundFilePath, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &pStorage) == S_OK) {
				CComPtr<IEnumSTATSTG> pEnumSTATSTG;
				if (SUCCEEDED(pStorage->EnumElements(0, NULL, 0, &pEnumSTATSTG))) {
					STATSTG statstg = {};
					if (pEnumSTATSTG->Next(1, &statstg, 0) == S_OK) {
						CoTaskMemFree(statstg.pwcsName);
						statstg.pwcsName = NULL;
						continue;
					}
				}
			}
		}

		time_t fdate = (time_t)FileTimeToUnixTime(tFoundFileTime);
		if (fdate == 0)
			fdate = (time_t)-1;
		if (fdate == (time_t)-1) {
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, _T("Failed to get file date of \"%s\""), (LPCTSTR)strFoundFilePath);
		} else
			AdjustNTFSDaylightFileTime(fdate, strFoundFilePath);

		CKnownFile *toadd = theApp.knownfiles->FindKnownFile(strFoundFileName, fdate, ullFoundFileSize);
		if (toadd == NULL || theApp.sharedfiles->GetFileByID(toadd->GetFileHash()) == NULL) // check if already shared
			if (toadd != NULL) { // for known
				toadd->SetFilePath(strFoundFilePath);
				toadd->SetPath(strFoundDirectory);
				AddFile(toadd); // known, could be on the list already
			} else { // neither known nor shared, create
				CShareableFile *pNewTempFile = new CShareableFile();
				pNewTempFile->SetFilePath(strFoundFilePath);
				pNewTempFile->SetFileName(strFoundFileName);
				pNewTempFile->SetPath(strFoundDirectory);
				pNewTempFile->SetFileSize(ullFoundFileSize);
				uchar aucMD4[16] = {};
				pNewTempFile->SetFileHash(aucMD4);
				liTempShareableFilesInDir.AddTail(pNewTempFile);
				AddFile(pNewTempFile);
			}
	}
	SetRedraw(TRUE);
	ff.Close();
}

BOOL CSharedFilesCtrl::OnNMClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	if (CheckBoxesEnabled()) { // do we have checkboxes?
		NMLISTVIEW *pNMListView = reinterpret_cast<NM_LISTVIEW*>(pNMHDR);

		int iItem = HitTest(pNMListView->ptAction);
		if (iItem >= 0) {
			// determine if the checkbox was clicked
			CRect rcItem;
			if (GetItemRect(iItem, rcItem, LVIR_BOUNDS)) {
				CPoint pointHit = pNMListView->ptAction;
				ASSERT(rcItem.PtInRect(pointHit));
				rcItem.left += sm_iIconOffset;
				rcItem.right = rcItem.left + 16;
				rcItem.top += (rcItem.Height() > 16) ? ((rcItem.Height() - 16) / 2) : 0;
				rcItem.bottom = rcItem.top + 16;
				if (rcItem.PtInRect(pointHit)) {
					// user clicked on the checkbox
					CheckBoxClicked(iItem);
				}
			}

		}
	}

	return (BOOL)(*pResult = 0); // pass on to the parent window
}

void CSharedFilesCtrl::CheckBoxClicked(int iItem)
{
	if (iItem == -1) {
		ASSERT(0);
		return;
	}
	// check which state the checkbox (should) currently have
	const CShareableFile *pFile = reinterpret_cast<CShareableFile*>(GetItemData(iItem));
	if (pFile->IsShellLinked())
		return; // no interacting with shelllinked files
	if (theApp.sharedfiles->ShouldBeShared(pFile->GetPath(), pFile->GetFilePath(), false)) {
		// this is currently shared so unshare it
		if (theApp.sharedfiles->ShouldBeShared(pFile->GetPath(), pFile->GetFilePath(), true))
			return; // not allowed to unshare this file
		VERIFY(theApp.sharedfiles->ExcludeFile(pFile->GetFilePath()));
		// update GUI stuff
		ShowFilesCount();
		theApp.emuledlg->sharedfileswnd->ShowSelectedFilesDetails();
		theApp.emuledlg->sharedfileswnd->OnSingleFileShareStatusChanged();
		// no need to update the list itself, will be handled in the RemoveFile function
	} else {
		if (!thePrefs.IsShareableDirectory(pFile->GetPath()))
			return; // not allowed to share
		VERIFY(theApp.sharedfiles->AddSingleSharedFile(pFile->GetFilePath()));
		ShowFilesCount();
		theApp.emuledlg->sharedfileswnd->ShowSelectedFilesDetails();
		theApp.emuledlg->sharedfileswnd->OnSingleFileShareStatusChanged();
		UpdateFile(pFile);
	}
}

bool CSharedFilesCtrl::CheckBoxesEnabled() const
{
	return m_pDirectoryFilter != NULL && m_pDirectoryFilter->m_eItemType == SDI_UNSHAREDDIRECTORY;
}

void CSharedFilesCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
	// highlighting Checkboxes
	if (CheckBoxesEnabled()) {
		// are we currently on any checkbox?
		int iItem = HitTest(point);
		if (iItem >= 0) {
			CRect rcItem;
			if (GetItemRect(iItem, rcItem, LVIR_BOUNDS)) {
				ASSERT(rcItem.PtInRect(point));
				rcItem.left += sm_iIconOffset;
				rcItem.right = rcItem.left + 16;
				rcItem.top += (rcItem.Height() > 16) ? ((rcItem.Height() - 16) / 2) : 0;
				rcItem.bottom = rcItem.top + 16;
				if (rcItem.PtInRect(point)) {
					// is this checkbox already hot?
					if (m_pHighlightedItem != reinterpret_cast<CShareableFile*>(GetItemData(iItem))) {
						// update old highlighted item
						CShareableFile *pOldItem = m_pHighlightedItem;
						m_pHighlightedItem = reinterpret_cast<CShareableFile*>(GetItemData(iItem));
						UpdateFile(pOldItem, false);
						// highlight current item
						InvalidateRect(rcItem);
					}
					CMuleListCtrl::OnMouseMove(nFlags, point);
					return;
				}
			}
		}
		// no checkbox should be hot
		if (m_pHighlightedItem != NULL) {
			CShareableFile *pOldItem = m_pHighlightedItem;
			m_pHighlightedItem = NULL;
			UpdateFile(pOldItem, false);
		}
	}
	CMuleListCtrl::OnMouseMove(nFlags, point);
}

CSharedFilesCtrl::CShareDropTarget::CShareDropTarget()
{
	m_piDropHelper = NULL;
	m_pParent = NULL;
	m_bUseDnDHelper = SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper, (void**)&m_piDropHelper));
}

CSharedFilesCtrl::CShareDropTarget::~CShareDropTarget()
{
	if (m_piDropHelper != NULL)
		m_piDropHelper->Release();
}

DROPEFFECT CSharedFilesCtrl::CShareDropTarget::OnDragEnter(CWnd *pWnd, COleDataObject *pDataObject, DWORD /*dwKeyState*/, CPoint point)
{
	DROPEFFECT dwEffect = pDataObject->IsDataAvailable(CF_HDROP) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
	if (m_bUseDnDHelper) {
		IDataObject *piDataObj = pDataObject->GetIDataObject(FALSE);
		m_piDropHelper->DragEnter(pWnd->GetSafeHwnd(), piDataObj, &point, dwEffect);
	}
	return dwEffect;
}

DROPEFFECT CSharedFilesCtrl::CShareDropTarget::OnDragOver(CWnd*, COleDataObject *pDataObject, DWORD, CPoint point)
{
	DROPEFFECT dwEffect = pDataObject->IsDataAvailable(CF_HDROP) ? DROPEFFECT_COPY : DROPEFFECT_NONE;
	if (m_bUseDnDHelper)
		m_piDropHelper->DragOver(&point, dwEffect);
	return dwEffect;
}

BOOL CSharedFilesCtrl::CShareDropTarget::OnDrop(CWnd*, COleDataObject *pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	HGLOBAL hGlobal = pDataObject->GetGlobalData(CF_HDROP);
	if (hGlobal != NULL) {
		HDROP hDrop = (HDROP)GlobalLock(hGlobal);
		if (hDrop != NULL) {
			CString strFilePath;
			CFileFind ff;
			CStringList liToAddFiles; // all files too add
			CStringList liToAddDirs; // all directories to add
			bool bFromSingleDirectory = true; // are all files from within the same directory
			CString strSingleDirectory; // which would be this one

			UINT nFileCount = DragQueryFile(hDrop, UINT_MAX, NULL, 0);
			for (UINT nFile = 0; nFile < nFileCount; ++nFile) {
				if (DragQueryFile(hDrop, nFile, strFilePath.GetBuffer(MAX_PATH), MAX_PATH) > 0) {
					strFilePath.ReleaseBuffer();
					if (ff.FindFile(strFilePath, 0)) {
						ff.FindNextFile();
						CString ffpath(ff.GetFilePath());
						// just a quick pre check, complete check is done later in the share function itself
						if (ff.IsDots() || ff.IsSystem() || ff.IsTemporary()
							|| (!ff.IsDirectory() && (ff.GetLength() == 0 || ff.GetLength() > MAX_EMULE_FILE_SIZE))
							|| (ff.IsDirectory() && !thePrefs.IsShareableDirectory(ffpath + _T('\\')))
							|| (ff.IsDirectory() && theApp.sharedfiles->ShouldBeShared(ffpath + _T('\\'), _T(""), false))
							|| (!ff.IsDirectory() && theApp.sharedfiles->ShouldBeShared(ffpath, ffpath.Left(ffpath.ReverseFind(_T('\\')) + 1), false)))
						{
							DebugLog(_T("Drag&Drop'ed shared File ignored (%s)"), (LPCTSTR)ffpath);
							ff.Close();
							continue;
						}
						if (ff.IsDirectory()) {
							DEBUG_ONLY(DebugLog(_T("Drag'n'Drop'ed directory: %s\\"), (LPCTSTR)ffpath));
							liToAddDirs.AddTail(ffpath + _T('\\'));
						} else {
							DEBUG_ONLY(DebugLog(_T("Drag'n'Drop'ed file: %s"), (LPCTSTR)ffpath));
							liToAddFiles.AddTail(ffpath);
							if (bFromSingleDirectory) {
								if (strSingleDirectory.IsEmpty())
									strSingleDirectory = ffpath.Left(ffpath.ReverseFind(_T('\\')) + 1);
								else if (strSingleDirectory.CompareNoCase(ffpath.Left(ffpath.ReverseFind(_T('\\')) + 1)) != NULL)
									bFromSingleDirectory = false;
							}
						}
					} else
						DebugLogError(_T("Drag&Drop'ed shared File not found (%s)"), (LPCTSTR)strFilePath);

					ff.Close();

				} else {
					ASSERT(0);
					strFilePath.ReleaseBuffer();
				}
			}
			if (!liToAddFiles.IsEmpty() || !liToAddDirs.IsEmpty()) {
				// add the directories first as they could
				// make single file adds invalid if they are contained in one of those dirs already
				for (POSITION pos = liToAddDirs.GetHeadPosition(); pos != NULL; )
					VERIFY(theApp.sharedfiles->AddSingleSharedDirectory(liToAddDirs.GetNext(pos))); // should always succeed

				bool bHaveFiles = false;
				while (!liToAddFiles.IsEmpty())
					bHaveFiles |= theApp.sharedfiles->AddSingleSharedFile(liToAddFiles.RemoveHead()); // could fail, due to the dirs added above

				// GUI updates
				if (!liToAddDirs.IsEmpty())
					theApp.emuledlg->sharedfileswnd->m_ctlSharedDirTree.Reload(true);
				if (bHaveFiles)
					theApp.emuledlg->sharedfileswnd->OnSingleFileShareStatusChanged();
				m_pParent->ShowFilesCount();

				if (bHaveFiles && liToAddDirs.IsEmpty() && bFromSingleDirectory) {
					// if we added only files from the same directory, show and select this in the file system tree
					ASSERT(!strSingleDirectory.IsEmpty());
					VERIFY(theApp.emuledlg->sharedfileswnd->m_ctlSharedDirTree.ShowFileSystemDirectory(strSingleDirectory));
				} else if (!liToAddDirs.IsEmpty() && !bHaveFiles) {
					// only directories added, if only one select the specific shared dir, otherwise the Shared Directories section
					if (liToAddDirs.GetCount() == 1)
						theApp.emuledlg->sharedfileswnd->m_ctlSharedDirTree.ShowSharedDirectory(liToAddDirs.GetHead());
					else
						theApp.emuledlg->sharedfileswnd->m_ctlSharedDirTree.ShowSharedDirectory(_T(""));
				} else {
					// otherwise select the All Shared Files category
					theApp.emuledlg->sharedfileswnd->m_ctlSharedDirTree.ShowAllSharedFiles();
				}
			}
			GlobalUnlock(hGlobal);
		}
		GlobalFree(hGlobal);
	}

	if (m_bUseDnDHelper) {
		IDataObject *piDataObj = pDataObject->GetIDataObject(FALSE);
		m_piDropHelper->Drop(piDataObj, &point, dropEffect);
	}

	return TRUE;
}

void CSharedFilesCtrl::CShareDropTarget::OnDragLeave(CWnd*)
{
	if (m_bUseDnDHelper)
		m_piDropHelper->DragLeave();
}