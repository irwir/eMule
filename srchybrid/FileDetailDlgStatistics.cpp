//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "filedetaildlgstatistics.h"
#include "UserMsgs.h"
#include "KnownFile.h"
#include "KnownFileList.h"
#include "SharedFileList.h"
#include "UploadQueue.h"
#include "emuledlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define REFRESH_TIMER_ID	9042

IMPLEMENT_DYNAMIC(CFileDetailDlgStatistics, CResizablePage)

BEGIN_MESSAGE_MAP(CFileDetailDlgStatistics, CResizablePage)
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_WM_DESTROY()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_TIMER()
END_MESSAGE_MAP()

CFileDetailDlgStatistics::CFileDetailDlgStatistics()
	: CResizablePage(CFileDetailDlgStatistics::IDD)
	, m_paFiles()
	, m_bDataChanged()
	, nLastRequestCount()
	, m_hRefreshTimer()
{
	m_strCaption = GetResString(IDS_SF_STATISTICS);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

void CFileDetailDlgStatistics::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_POPBAR, pop_bar);
	DDX_Control(pDX, IDC_POPBAR2, pop_baraccept);
	DDX_Control(pDX, IDC_POPBAR3, pop_bartrans);
	DDX_Control(pDX, IDC_POPBAR4, pop_bar2);
	DDX_Control(pDX, IDC_POPBAR5, pop_baraccept2);
	DDX_Control(pDX, IDC_POPBAR6, pop_bartrans2);
}

BOOL CFileDetailDlgStatistics::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(pop_bar, TOP_LEFT, TOP_RIGHT);
	AddAnchor(pop_baraccept, TOP_LEFT, TOP_RIGHT);
	AddAnchor(pop_bartrans, TOP_LEFT, TOP_RIGHT);
	AddAnchor(pop_bar2, TOP_LEFT, TOP_RIGHT);
	AddAnchor(pop_baraccept2, TOP_LEFT, TOP_RIGHT);
	AddAnchor(pop_bartrans2, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_STATISTICS_GB_TOTAL, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_STATISTICS_GB_SESSION, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_FS_POPULARITY_LBL, TOP_RIGHT);
	AddAnchor(IDC_FS_POPULARITY_VAL, TOP_RIGHT);
	AddAnchor(IDC_FS_ONQUEUE_LBL, TOP_RIGHT);
	AddAnchor(IDC_FS_ONQUEUE_VAL, TOP_RIGHT);
	AddAnchor(IDC_FS_UPLOADING_LBL, TOP_RIGHT);
	AddAnchor(IDC_FS_UPLOADING_VAL, TOP_RIGHT);
	AddAnchor(IDC_FS_POPULARITY2_LBL, TOP_RIGHT);
	AddAnchor(IDC_FS_POPULARITY2_VAL, TOP_RIGHT);

	AddAllOtherAnchors();

	pop_bar.SetGradientColors(RGB(255, 255, 240), RGB(255, 255, 0));
	pop_bar.SetTextColor(RGB(20, 70, 255));
	pop_baraccept.SetGradientColors(RGB(255, 255, 240), RGB(255, 255, 0));
	pop_baraccept.SetTextColor(RGB(20, 70, 255));
	pop_bartrans.SetGradientColors(RGB(255, 255, 240), RGB(255, 255, 0));
	pop_bartrans.SetTextColor(RGB(20, 70, 255));
	pop_bar2.SetGradientColors(RGB(255, 255, 240), RGB(255, 255, 0));
	pop_bar2.SetTextColor(RGB(20, 70, 255));
	pop_baraccept2.SetGradientColors(RGB(255, 255, 240), RGB(255, 255, 0));
	pop_baraccept2.SetTextColor(RGB(20, 70, 255));
	pop_bartrans2.SetGradientColors(RGB(255, 255, 240), RGB(255, 255, 0));
	pop_bartrans2.SetTextColor(RGB(20, 70, 255));

	pop_baraccept.SetShowPercent();
	pop_bar.SetShowPercent();
	pop_bartrans.SetShowPercent();
	pop_baraccept2.SetShowPercent();
	pop_bar2.SetShowPercent();
	pop_bartrans2.SetShowPercent();
	Localize();
	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CFileDetailDlgStatistics::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;
	if (m_hRefreshTimer == 0) {
		m_hRefreshTimer = SetTimer(REFRESH_TIMER_ID, SEC2MS(3), NULL);
		m_bDataChanged = true;
	}
	return TRUE;
}

BOOL CFileDetailDlgStatistics::OnKillActive()
{
	if (m_hRefreshTimer) {
		KillTimer(m_hRefreshTimer);
		m_hRefreshTimer = 0;
	}
	return CResizablePage::OnKillActive();
}

LRESULT CFileDetailDlgStatistics::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CFileDetailDlgStatistics::RefreshData()
{
	if (m_paFiles == NULL)
		return;
	uint64 uTransferred = 0;
	uint64 uAllTimeTransferred = 0;
	const CKnownFile *pTheFile = NULL;
	int iFiles = 0;
	UINT uRequests = 0;
	UINT uAccepted = 0;
	UINT uAllTimeRequests = 0;
	UINT uAllTimeAccepted = 0;
	for (int i = m_paFiles->GetSize(); --i >= 0;) {
		if (!(*m_paFiles)[i]->IsKindOf(RUNTIME_CLASS(CKnownFile)))
			continue;
		const CKnownFile *pFile = static_cast<CKnownFile*>((*m_paFiles)[i]);
		if (theApp.sharedfiles->GetFileByIdentifier(pFile->GetFileIdentifierC()) == NULL)
			continue;
		if (!iFiles)
			pTheFile = pFile;
		++iFiles;

		uTransferred += pFile->statistic.GetTransferred();
		uRequests += pFile->statistic.GetRequests();
		uAccepted += pFile->statistic.GetAccepts();

		uAllTimeTransferred += pFile->statistic.GetAllTimeTransferred();
		uAllTimeRequests += pFile->statistic.GetAllTimeRequests();
		uAllTimeAccepted += pFile->statistic.GetAllTimeAccepts();
	}

	if (iFiles != 0) {
		pop_bartrans.SetEmpty(false);
		pop_bartrans.SetRange32(0, (int)(theApp.knownfiles->transferred / 1024));
		pop_bartrans.SetPos((int)(uTransferred / 1024));
		SetDlgItemText(IDC_STRANSFERRED, CastItoXBytes(uTransferred));

		pop_bar.SetEmpty(false);
		pop_bar.SetRange32(0, theApp.knownfiles->requested);
		pop_bar.SetPos(uRequests);
		SetDlgItemInt(IDC_SREQUESTED, uRequests, FALSE);

		pop_baraccept.SetEmpty(false);
		pop_baraccept.SetRange32(0, theApp.knownfiles->accepted);
		pop_baraccept.SetPos(uAccepted);
		SetDlgItemInt(IDC_SACCEPTED, uAccepted, FALSE);

		pop_bartrans2.SetEmpty(false);
		pop_bartrans2.SetRange32(0, (int)(theApp.knownfiles->m_nTransferredTotal / (1024 * 1024)));
		pop_bartrans2.SetPos((int)(uAllTimeTransferred / (1024 * 1024)));

		pop_bar2.SetEmpty(false);
		pop_bar2.SetRange32(0, theApp.knownfiles->m_nRequestedTotal);
		pop_bar2.SetPos(uAllTimeRequests);

		pop_baraccept2.SetEmpty(false);
		pop_baraccept2.SetRange32(0, theApp.knownfiles->m_nAcceptedTotal);
		pop_baraccept2.SetPos(uAllTimeAccepted);

		SetDlgItemText(IDC_STRANSFERRED2, (LPCTSTR)CastItoXBytes(uAllTimeTransferred));
		SetDlgItemInt(IDC_SREQUESTED2, uAllTimeRequests, FALSE);
		SetDlgItemInt(IDC_SACCEPTED2, uAllTimeAccepted, FALSE);

		uint32 nQueueCount = theApp.uploadqueue->GetWaitingUserForFileCount(*m_paFiles, !m_bDataChanged);
		if (nQueueCount != _UI32_MAX)
			SetDlgItemInt(IDC_FS_ONQUEUE_VAL, nQueueCount, FALSE);

		SetDlgItemText(IDC_FS_UPLOADING_VAL, CastItoXBytes(theApp.uploadqueue->GetDatarateForFile(*m_paFiles), false, true));


		if (iFiles == 1) {
			if (m_bDataChanged || nLastRequestCount != theApp.knownfiles->m_nRequestedTotal) {
				nLastRequestCount = theApp.knownfiles->m_nRequestedTotal;
				uint32 nSession = 0, nTotal = 0;
				if (theApp.sharedfiles->GetPopularityRank(pTheFile, nSession, nTotal)) {
					SetDlgItemInt(IDC_FS_POPULARITY_VAL, nSession, FALSE);
					SetDlgItemInt(IDC_FS_POPULARITY2_VAL, nTotal, FALSE);
				}
			}
		} else {
			SetDlgItemText(IDC_FS_POPULARITY_VAL, _T("-"));
			SetDlgItemText(IDC_FS_POPULARITY2_VAL, _T("-"));
		}
	} else {
		SetDlgItemText(IDC_STRANSFERRED, _T("-"));
		SetDlgItemText(IDC_SREQUESTED, _T("-"));
		SetDlgItemText(IDC_SACCEPTED, _T("-"));
		SetDlgItemText(IDC_STRANSFERRED2, _T("-"));
		SetDlgItemText(IDC_SREQUESTED2, _T("-"));
		SetDlgItemText(IDC_SACCEPTED2, _T("-"));
		SetDlgItemText(IDC_FS_POPULARITY_VAL, _T("-"));
		SetDlgItemText(IDC_FS_POPULARITY2_VAL, _T("-"));
		SetDlgItemText(IDC_FS_ONQUEUE_VAL, _T("-"));
		SetDlgItemText(IDC_FS_UPLOADING_VAL, _T("-"));

		pop_bartrans.SetEmpty(true, true);
		pop_bar.SetEmpty(true, true);
		pop_baraccept.SetEmpty(true, true);
		pop_bartrans2.SetEmpty(true, true);
		pop_bar2.SetEmpty(true, true);
		pop_baraccept2.SetEmpty(true, true);
	}
}


void CFileDetailDlgStatistics::OnDestroy()
{
	if (m_hRefreshTimer) {
		KillTimer(m_hRefreshTimer);
		m_hRefreshTimer = 0;
	}
	CResizablePage::OnDestroy();
}

void CFileDetailDlgStatistics::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_SF_STATISTICS, this);

	SetDlgItemText(IDC_STATISTICS_GB_SESSION, GetResString(IDS_SF_CURRENT));
	SetDlgItemText(IDC_STATISTICS_GB_TOTAL, GetResString(IDS_SF_TOTAL));
	SetDlgItemText(IDC_FSTATIC6, GetResString(IDS_SF_TRANS));
	SetDlgItemText(IDC_FSTATIC5, GetResString(IDS_SF_ACCEPTED));
	SetDlgItemText(IDC_FSTATIC4, GetResString(IDS_SF_REQUESTS) + _T(':'));
	SetDlgItemText(IDC_FSTATIC9, GetResString(IDS_SF_TRANS));
	SetDlgItemText(IDC_FSTATIC8, GetResString(IDS_SF_ACCEPTED));
	SetDlgItemText(IDC_FSTATIC7, GetResString(IDS_SF_REQUESTS) + _T(':'));
	SetDlgItemText(IDC_FS_POPULARITY_LBL, GetResString(IDS_POPULARITY) + _T(':'));
	SetDlgItemText(IDC_FS_POPULARITY2_LBL, GetResString(IDS_POPULARITY) + _T(':'));
	SetDlgItemText(IDC_FS_ONQUEUE_LBL, GetResString(IDS_ONQUEUE) + _T(':'));
	SetDlgItemText(IDC_FS_UPLOADING_LBL, GetResString(IDS_UPLOADING) + _T(':'));
}

void CFileDetailDlgStatistics::OnSysColorChange()
{
	const DWORD clr3Dface = ::GetSysColor(COLOR_3DFACE);
	pop_bar.SetBkColor(clr3Dface);
	pop_baraccept.SetBkColor(clr3Dface);
	pop_bartrans.SetBkColor(clr3Dface);
	pop_bar2.SetBkColor(clr3Dface);
	pop_baraccept2.SetBkColor(clr3Dface);
	pop_bartrans2.SetBkColor(clr3Dface);
	CResizablePage::OnSysColorChange();
}

void CFileDetailDlgStatistics::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == m_hRefreshTimer) {
		if (theApp.IsClosing() || !GetParent()->IsWindowVisible()
			|| theApp.emuledlg->GetActiveDialog() != (CWnd*)theApp.emuledlg->sharedfileswnd)
		{
			KillTimer(m_hRefreshTimer);
			m_hRefreshTimer = 0;
		} else if (m_bDataChanged) {
			RefreshData();
			m_bDataChanged = false;
		}
	} else
		CResizablePage::OnTimer(nIDEvent);
}