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
#include "CommentDialogLst.h"
#include "PartFile.h"
#include "UpDownClient.h"
#include "UserMsgs.h"
#include "kademlia/kademlia/kademlia.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Search.h"
#include "searchlist.h"
#include "InputBox.h"
#include "DownloadQueue.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(CCommentDialogLst, CResizablePage)

BEGIN_MESSAGE_MAP(CCommentDialogLst, CResizablePage)
	ON_BN_CLICKED(IDOK, OnBnClickedApply)
	ON_BN_CLICKED(IDC_SEARCHKAD, OnBnClickedSearchKad)
	ON_BN_CLICKED(IDC_EDITCOMMENTFILTER, OnBnClickedFilter)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CCommentDialogLst::CCommentDialogLst()
	: CResizablePage(CCommentDialogLst::IDD)
	, m_paFiles()
	, m_timer()
	, m_bDataChanged()
{
	m_strCaption = GetResString(IDS_CMT_READALL);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

void CCommentDialogLst::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LST, m_lstComments);
}

void CCommentDialogLst::OnBnClickedApply()
{
	CResizablePage::OnOK();
}

void CCommentDialogLst::OnTimer(UINT_PTR /*nIDEvent*/)
{
	RefreshData(false);
}

BOOL CCommentDialogLst::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(IDC_LST, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_SEARCHKAD, BOTTOM_RIGHT);
	AddAnchor(IDC_EDITCOMMENTFILTER, BOTTOM_LEFT);

	m_lstComments.Init();
	Localize();

	// start time for calling 'RefreshData'
	VERIFY((m_timer = SetTimer(301, SEC2MS(5), NULL)) != 0);

	return TRUE;
}

BOOL CCommentDialogLst::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;
	if (m_bDataChanged) {
		RefreshData();
		RefreshData();
		m_bDataChanged = false;
	}
	return TRUE;
}

LRESULT CCommentDialogLst::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CCommentDialogLst::OnDestroy()
{
	if (m_timer) {
		KillTimer(m_timer);
		m_timer = 0;
	}
}

void CCommentDialogLst::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_CMT_READALL, this);

	SetDlgItemText(IDC_SEARCHKAD, GetResString(IDS_SEARCHKAD));
	SetDlgItemText(IDC_EDITCOMMENTFILTER, GetResString(IDS_EDITSPAMFILTER));
}

void CCommentDialogLst::RefreshData(bool deleteOld)
{
	if (deleteOld)
		m_lstComments.DeleteAllItems();

	bool kadsearchable = true;
	for (int i = 0; i < m_paFiles->GetSize(); ++i) {
		CAbstractFile *file = static_cast<CAbstractFile*>((*m_paFiles)[i]);
		if (file->IsPartFile()) {
			for (POSITION pos = static_cast<CPartFile*>(file)->srclist.GetHeadPosition(); pos != NULL;) {
				const CUpDownClient *cur_src = static_cast<CPartFile*>(file)->srclist.GetNext(pos);
				if (cur_src->HasFileRating() || !cur_src->GetFileComment().IsEmpty())
					m_lstComments.AddItem(cur_src);
			}
		}

		const CTypedPtrList<CPtrList, Kademlia::CEntry*> &list = file->getNotes();
		for (POSITION pos = list.GetHeadPosition(); pos != NULL;)
			m_lstComments.AddItem(list.GetNext(pos));
		if (file->IsPartFile())
			static_cast<CPartFile*>(file)->UpdateFileRatingCommentAvail();

		// check if note searches are running for this file(s)
		if (Kademlia::CSearchManager::AlreadySearchingFor(Kademlia::CUInt128(file->GetFileHash())))
			kadsearchable = false;
	}

	CWnd *pWndFocus = GetFocus();
	if (Kademlia::CKademlia::IsConnected()) {
		SetDlgItemText(IDC_SEARCHKAD, GetResString(kadsearchable ? IDS_SEARCHKAD : IDS_KADSEARCHACTIVE));
		GetDlgItem(IDC_SEARCHKAD)->EnableWindow(kadsearchable);
	} else {
		SetDlgItemText(IDC_SEARCHKAD, GetResString(IDS_SEARCHKAD));
		GetDlgItem(IDC_SEARCHKAD)->EnableWindow(FALSE);
	}
	if (pWndFocus && pWndFocus->m_hWnd == GetDlgItem(IDC_SEARCHKAD)->m_hWnd)
		m_lstComments.SetFocus();
}

void CCommentDialogLst::OnBnClickedSearchKad()
{
	if (Kademlia::CKademlia::IsConnected()) {
		bool bSkipped = false;
		int iMaxSearches = min(m_paFiles->GetSize(), KADEMLIATOTALFILE);
		for (int i = 0; i < iMaxSearches; ++i) {
			CAbstractFile *file = static_cast<CAbstractFile*>((*m_paFiles)[i]);
			if (file)
				if (!Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::NOTES, true, Kademlia::CUInt128(file->GetFileHash())))
					bSkipped = true;
				else {
					theApp.searchlist->SetNotesSearchStatus(file->GetFileHash(), true);
					file->SetKadCommentSearchRunning(true);
				}
		}
		if (bSkipped)
			LocMessageBox(IDS_KADSEARCHALREADY, MB_OK | MB_ICONINFORMATION, 0);
	}
	RefreshData();
}

void CCommentDialogLst::OnBnClickedFilter()
{
	InputBox inputbox;
	inputbox.SetLabels(GetResString(IDS_EDITSPAMFILTERCOMMENTS), GetResString(IDS_FILTERCOMMENTSLABEL), thePrefs.GetCommentFilter());
	inputbox.DoModal();
	if (!inputbox.WasCancelled()) {
		CString strCommentFilters(inputbox.GetInput());
		strCommentFilters.MakeLower();
		CString strNewCommentFilters;
		for (int iPos = 0; iPos >= 0;) {
			CString strFilter = strCommentFilters.Tokenize(_T("|"), iPos);
			if (!strFilter.IsEmpty()) {
				if (!strNewCommentFilters.IsEmpty())
					strNewCommentFilters += _T('|');
				strNewCommentFilters += strFilter.Trim();
			}
		}

		if (thePrefs.GetCommentFilter() != strNewCommentFilters) {
			thePrefs.SetCommentFilter(strNewCommentFilters);
			theApp.downloadqueue->RefilterAllComments();
			RefreshData();
		}
	}
}