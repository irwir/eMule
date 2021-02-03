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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include "emule.h"
#include "CommentDialog.h"
#include "KnownFile.h"
#include "PartFile.h"
#include "Opcodes.h"
#include "UpDownClient.h"
#include "kademlia/kademlia/kademlia.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Search.h"
#include "UserMsgs.h"
#include "searchlist.h"
#include "sharedfilelist.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	IDT_REFRESH	301

// CommentDialog dialog

IMPLEMENT_DYNAMIC(CCommentDialog, CResizablePage)

BEGIN_MESSAGE_MAP(CCommentDialog, CResizablePage)
	ON_BN_CLICKED(IDC_RESET, OnBnClickedReset)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_EN_CHANGE(IDC_CMT_TEXT, OnEnChangeCmtText)
	ON_CBN_SELENDOK(IDC_RATELIST, OnCbnSelendokRatelist)
	ON_CBN_SELCHANGE(IDC_RATELIST, OnCbnSelchangeRatelist)
	ON_BN_CLICKED(IDC_SEARCHKAD, OnBnClickedSearchKad)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CCommentDialog::CCommentDialog()
	: CResizablePage(CCommentDialog::IDD)
	, m_paFiles()
	, m_bDataChanged()
	, m_bMergedComment()
	, m_bSelf()
	, m_timer()
	, m_bEnabled(true)
{
	m_strCaption = GetResString(IDS_COMMENT);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

void CCommentDialog::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RATELIST, m_ratebox);
	DDX_Control(pDX, IDC_LST, m_lstComments);
}

void CCommentDialog::OnTimer(UINT_PTR /*nIDEvent*/)
{
	RefreshData(false);
}

BOOL CCommentDialog::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(IDC_LST, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_CMT_LQUEST, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_CMT_LAIDE, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_CMT_TEXT, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_RATEQUEST, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_RATEHELP, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_USERCOMMENTS, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_RESET, TOP_RIGHT);
	AddAnchor(IDC_SEARCHKAD, BOTTOM_RIGHT);

	AddAllOtherAnchors();

	m_lstComments.Init();
	Localize();

	// start timer for calling 'RefreshData'
	VERIFY((m_timer = SetTimer(IDT_REFRESH, SEC2MS(5), NULL)) != 0);

	return TRUE;
}

BOOL CCommentDialog::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;
	if (m_bDataChanged) {
		bool bContainsSharedKnownFile = false;
		int iRating = -1;
		m_bMergedComment = false;
		CString strComment;
		for (int i = 0; i < m_paFiles->GetSize(); ++i) {
			if (!(*m_paFiles)[i]->IsKindOf(RUNTIME_CLASS(CKnownFile)))
				continue;
			CKnownFile *file = static_cast<CKnownFile*>((*m_paFiles)[i]);
			// we actually could show, add and even search for comments on KAD for known, but not shared, files,
			// but we don't publish comments entered by the user if the file is not shared (which might be changed at some point)
			// so make sure we don't let him think he can comment and disable the dialog for such files
			if (theApp.sharedfiles->GetFileByID(file->GetFileHash()) == NULL)
				continue;
			bContainsSharedKnownFile = true;
			if (i == 0) {
				strComment = file->GetFileComment();
				iRating = file->GetFileRating();
			} else {
				if (!m_bMergedComment && strComment.Compare(file->GetFileComment()) != 0) {
					strComment.Empty();
					m_bMergedComment = true;
				}
				if (iRating >= 0 && (UINT)iRating != file->GetFileRating())
					iRating = -1;
			}
		}
		m_bSelf = true;
		SetDlgItemText(IDC_CMT_TEXT, strComment);
		static_cast<CEdit*>(GetDlgItem(IDC_CMT_TEXT))->SetLimitText(MAXFILECOMMENTLEN);
		m_ratebox.SetCurSel(iRating);
		m_bSelf = false;
		EnableDialog(bContainsSharedKnownFile);

		m_bDataChanged = false;

		RefreshData();
	}

	return TRUE;
}

LRESULT CCommentDialog::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CCommentDialog::OnBnClickedReset()
{
	SetDlgItemText(IDC_CMT_TEXT, _T(""));
	m_bMergedComment = false;
	m_ratebox.SetCurSel(0);
}

BOOL CCommentDialog::OnApply()
{
	if (m_bEnabled && !m_bDataChanged) {
		CString strComment;
		GetDlgItemText(IDC_CMT_TEXT, strComment);
		int iRating = m_ratebox.GetCurSel();
		for (int i = 0; i < m_paFiles->GetSize(); ++i)
			if ((*m_paFiles)[i]->IsKindOf(RUNTIME_CLASS(CKnownFile))) {
				CKnownFile *file = static_cast<CKnownFile*>((*m_paFiles)[i]);
				if (theApp.sharedfiles->GetFileByID(file->GetFileHash()) != NULL) {
					if (!strComment.IsEmpty() || !m_bMergedComment)
						file->SetFileComment(strComment);
					if (iRating >= 0)
						file->SetFileRating(iRating);
				}
			}
	}
	return CResizablePage::OnApply();
}

void CCommentDialog::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_COMMENT, this);

	SetDlgItemText(IDC_RESET, GetResString(IDS_PW_RESET));

	SetDlgItemText(IDC_CMT_LQUEST, GetResString(IDS_CMT_QUEST));
	SetDlgItemText(IDC_CMT_LAIDE, GetResString(IDS_CMT_AIDE));

	SetDlgItemText(IDC_RATEQUEST, GetResString(IDS_CMT_RATEQUEST));
	SetDlgItemText(IDC_RATEHELP, GetResString(IDS_CMT_RATEHELP));

	SetDlgItemText(IDC_USERCOMMENTS, GetResString(IDS_COMMENT));
	SetDlgItemText(IDC_SEARCHKAD, GetResString(IDS_SEARCHKAD));

	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("Rating_NotRated")));
	iml.Add(CTempIconLoader(_T("Rating_Fake")));
	iml.Add(CTempIconLoader(_T("Rating_Poor")));
	iml.Add(CTempIconLoader(_T("Rating_Fair")));
	iml.Add(CTempIconLoader(_T("Rating_Good")));
	iml.Add(CTempIconLoader(_T("Rating_Excellent")));
	m_ratebox.SetImageList(&iml);
	m_imlRating.DeleteImageList();
	m_imlRating.Attach(iml.Detach());

	m_ratebox.ResetContent();
	m_ratebox.AddItem(GetResString(IDS_CMT_NOTRATED), 0);
	m_ratebox.AddItem(GetResString(IDS_CMT_FAKE), 1);
	m_ratebox.AddItem(GetResString(IDS_CMT_POOR), 2);
	m_ratebox.AddItem(GetResString(IDS_CMT_FAIR), 3);
	m_ratebox.AddItem(GetResString(IDS_CMT_GOOD), 4);
	m_ratebox.AddItem(GetResString(IDS_CMT_EXCELLENT), 5);
	UpdateHorzExtent(m_ratebox, 16); // adjust dropped width to ensure all strings are fully visible

	RefreshData();
}

void CCommentDialog::OnDestroy()
{
	m_imlRating.DeleteImageList();
	CResizablePage::OnDestroy();
	if (m_timer) {
		KillTimer(m_timer);
		m_timer = 0;
	}
}

void CCommentDialog::OnEnChangeCmtText()
{
	if (!m_bSelf)
		SetModified();
}

void CCommentDialog::OnCbnSelendokRatelist()
{
	if (!m_bSelf)
		SetModified();
}

void CCommentDialog::OnCbnSelchangeRatelist()
{
	if (!m_bSelf)
		SetModified();
}

void CCommentDialog::RefreshData(bool deleteOld)
{
	if (deleteOld)
		m_lstComments.DeleteAllItems();

	if (!m_bEnabled)
		return;

	bool kadsearchable = true;
	for (int i = 0; i < m_paFiles->GetSize(); ++i) {
		CAbstractFile *file = static_cast<CAbstractFile*>((*m_paFiles)[i]);
		if (file->IsPartFile()) {
			for (POSITION pos = static_cast<CPartFile*>(file)->srclist.GetHeadPosition(); pos != NULL; ) {
				CUpDownClient *cur_src = static_cast<CPartFile*>(file)->srclist.GetNext(pos);
				if (cur_src->HasFileRating() || !cur_src->GetFileComment().IsEmpty())
					m_lstComments.AddItem(cur_src);
			}
		} else if (!file->IsKindOf(RUNTIME_CLASS(CKnownFile)))
			continue;
		else if (theApp.sharedfiles->GetFileByID(file->GetFileHash()) == NULL)
			continue;

		const CTypedPtrList<CPtrList, Kademlia::CEntry*> &list = file->getNotes();
		for (POSITION pos = list.GetHeadPosition(); pos != NULL;)
			m_lstComments.AddItem(list.GetNext(pos));

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

void CCommentDialog::OnBnClickedSearchKad()
{
	if (m_bEnabled && Kademlia::CKademlia::IsConnected()) {
		bool bSkipped = false;
		int iMaxSearches = min(m_paFiles->GetSize(), KADEMLIATOTALFILE);
		for (int i = 0; i < iMaxSearches; ++i) {
			CAbstractFile *file = static_cast<CAbstractFile*>((*m_paFiles)[i]);
			if (file && file->IsKindOf(RUNTIME_CLASS(CKnownFile)) && theApp.sharedfiles->GetFileByID(file->GetFileHash()) != NULL) {
				if (!Kademlia::CSearchManager::PrepareLookup(Kademlia::CSearch::NOTES, true, Kademlia::CUInt128(file->GetFileHash())))
					bSkipped = true;
				else {
					theApp.searchlist->SetNotesSearchStatus(file->GetFileHash(), true);
					file->SetKadCommentSearchRunning(true);
				}
			}
		}
		if (bSkipped)
			LocMessageBox(IDS_KADSEARCHALREADY, MB_OK | MB_ICONINFORMATION, 0);
	}
	RefreshData();
}

void CCommentDialog::EnableDialog(bool bEnabled)
{
	if (m_bEnabled == bEnabled)
		return;
	m_bEnabled = bEnabled;
	GetDlgItem(IDC_LST)->EnableWindow(static_cast<BOOL>(m_bEnabled));
	GetDlgItem(IDC_CMT_TEXT)->EnableWindow(static_cast<BOOL>(m_bEnabled));
	GetDlgItem(IDC_RATELIST)->EnableWindow(static_cast<BOOL>(m_bEnabled));
	GetDlgItem(IDC_RESET)->EnableWindow(static_cast<BOOL>(m_bEnabled));
	GetDlgItem(IDC_SEARCHKAD)->EnableWindow(static_cast<BOOL>(m_bEnabled));
}