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
#include "eMule.h"
#include "FileDetailDialogInfo.h"
#include "UserMsgs.h"
#include "PartFile.h"
#include "Preferences.h"
#include "shahashset.h"
#include "UpDownClient.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	IDT_REFRESH	301

///////////////////////////////////////////////////////////////////////////////
// CFileDetailDialogInfo dialog

LPCTSTR CFileDetailDialogInfo::sm_pszNotAvail = _T("-");

IMPLEMENT_DYNAMIC(CFileDetailDialogInfo, CResizablePage)

BEGIN_MESSAGE_MAP(CFileDetailDialogInfo, CResizablePage)
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_MESSAGE(UM_DATA_CHANGED, OnDataChanged)
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

CFileDetailDialogInfo::CFileDetailDialogInfo()
	: CResizablePage(CFileDetailDialogInfo::IDD)
	, m_paFiles()
	, m_timer()
	, m_bDataChanged()
	, m_bShowFileTypeWarning()
{
	m_strCaption = GetResString(IDS_FD_GENERAL);
	m_psp.pszTitle = m_strCaption;
	m_psp.dwFlags |= PSP_USETITLE;
}

void CFileDetailDialogInfo::OnTimer(UINT_PTR /*nIDEvent*/)
{
	RefreshData();
}

void CFileDetailDialogInfo::DoDataExchange(CDataExchange *pDX)
{
	CResizablePage::DoDataExchange(pDX);
}

BOOL CFileDetailDialogInfo::OnInitDialog()
{
	CResizablePage::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(IDC_FD_X0, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_FD_X6, TOP_LEFT, TOP_RIGHT);
	AddAnchor(IDC_FD_X8, TOP_LEFT, TOP_RIGHT);

	AddAllOtherAnchors();
	Localize();

	// no need to refresh data explicitly because
	// 'OnSetActive' will be called right after 'OnInitDialog'
	
	// start timer for calling 'RefreshData'
	VERIFY((m_timer = SetTimer(IDT_REFRESH, SEC2MS(5), NULL)) != 0);

	return TRUE;
}

BOOL CFileDetailDialogInfo::OnSetActive()
{
	if (!CResizablePage::OnSetActive())
		return FALSE;
	if (m_bDataChanged) {
		RefreshData();
		m_bDataChanged = false;
	}
	return TRUE;
}

LRESULT CFileDetailDialogInfo::OnDataChanged(WPARAM, LPARAM)
{
	m_bDataChanged = true;
	return 1;
}

void CFileDetailDialogInfo::RefreshData()
{
	CString str;

	if (m_paFiles->GetSize() == 1) {
		CPartFile *file = static_cast<CPartFile*>((*m_paFiles)[0]);
		const CString &fname(file->GetFileName());

		// if file is completed, we output the 'file path' and not the 'part.met file path'
		if (file->GetStatus(true) == PS_COMPLETE)
			SetDlgItemText(IDC_FD_X2, GetResString(IDS_DL_FILENAME));

		SetDlgItemText(IDC_FNAME, fname);
		SetDlgItemText(IDC_METFILE, file->GetFullName());
		SetDlgItemText(IDC_FHASH, md4str(file->GetFileHash()));

		if (file->GetTransferringSrcCount() > 0)
			str.Format(GetResString(IDS_PARTINFOS2), file->GetTransferringSrcCount());
		else
			str = file->getPartfileStatus();
		SetDlgItemText(IDC_PFSTATUS, str);

		str.Format(_T("%u;  %s: %u (%.1f%%)"), file->GetPartCount(), (LPCTSTR)GetResString(IDS_AVAILABLE), file->GetAvailablePartCount(), (file->GetAvailablePartCount()*100.0) / file->GetPartCount());
		SetDlgItemText(IDC_PARTCOUNT, str);

		// date created
		if (file->GetCrFileDate() != 0) {
			str.Format(_T("%s   ") + GetResString(IDS_TIMEBEFORE),
				(LPCTSTR)file->GetCrCFileDate().Format(thePrefs.GetDateTimeFormat()),
				(LPCTSTR)CastSecondsToLngHM(time(NULL) - file->GetCrFileDate()));
		} else
			str = GetResString(IDS_UNKNOWN);
		SetDlgItemText(IDC_FILECREATED, str);

		// active download time
		time_t nDlActiveTime = file->GetDlActiveTime();
		str = nDlActiveTime ? CastSecondsToLngHM(nDlActiveTime) : GetResString(IDS_UNKNOWN);
		SetDlgItemText(IDC_DL_ACTIVE_TIME, str);

		// last seen complete
		struct tm tmTemp;
		struct tm *ptimLastSeenComplete = file->lastseencomplete.GetLocalTm(&tmTemp);
		if (file->lastseencomplete == 0 || ptimLastSeenComplete == NULL)
			str.Format(GetResString(IDS_NEVER));
		else {
			str.Format(_T("%s   ") + GetResString(IDS_TIMEBEFORE)
				, (LPCTSTR)file->lastseencomplete.Format(thePrefs.GetDateTimeFormat())
				, (LPCTSTR)CastSecondsToLngHM(time(NULL) - safe_mktime(ptimLastSeenComplete)));
		}
		SetDlgItemText(IDC_LASTSEENCOMPL, str);

		// last receive
		if (file->GetFileDate() != 0 && file->GetRealFileSize() > 0ull) {
			// 'Last Modified' sometimes is up to 2 seconds greater than the current time ???
			// If it's related to the FAT32 seconds time resolution the max. failure should still be only 1 sec.
			// Happens at least on FAT32 with very high download speed.
			time_t tLastModified = file->GetFileDate();
			time_t tNow = time(NULL);
			time_t tAgo;
			if (tNow >= tLastModified)
				tAgo = tNow - tLastModified;
			else {
				TRACE("tNow = %s\n", (LPCTSTR)CTime(tNow).Format("%X"));
				TRACE("tLMd = %s, +%u\n", (LPCTSTR)CTime(tLastModified).Format("%X"), tLastModified - tNow);
				TRACE("\n");
				tAgo = 0;
			}
			str.Format(_T("%s   ") + GetResString(IDS_TIMEBEFORE)
				, (LPCTSTR)file->GetCFileDate().Format(thePrefs.GetDateTimeFormat())
				, (LPCTSTR)CastSecondsToLngHM(tAgo));
		} else
			str = GetResString(IDS_NEVER);
		SetDlgItemText(IDC_LASTRECEIVED, str);

		// AICH Hash
		switch (file->GetAICHRecoveryHashSet()->GetStatus()) {
		case AICH_TRUSTED:
		case AICH_VERIFIED:
		case AICH_HASHSETCOMPLETE:
			if (file->GetAICHRecoveryHashSet()->HasValidMasterHash()) {
				SetDlgItemText(IDC_FD_AICHHASH, file->GetAICHRecoveryHashSet()->GetMasterHash().GetString());
				break;
			}
		default:
			SetDlgItemText(IDC_FD_AICHHASH, GetResString(IDS_UNKNOWN));
		}

		// file type
		CString ext;
		bool showwarning = false;
		int pos = fname.ReverseFind(_T('.'));
		if (fname.ReverseFind(_T('\\')) < pos)
			ext = fname.Mid(pos + 1).MakeUpper();

		EFileType bycontent = GetFileTypeEx(file, false, true);
		if (bycontent != FILETYPE_UNKNOWN) {
			str.Format(_T("%s  (%s)"), (LPCTSTR)GetFileTypeName(bycontent), (LPCTSTR)GetResString(IDS_VERIFIED));

			switch (IsExtensionTypeOf(bycontent, ext)) {
			case -1:
				showwarning = true;
				str.AppendFormat(_T(" - %s: %s"), (LPCTSTR)GetResString(IDS_INVALIDFILEEXT), (LPCTSTR)ext);
				break;
			case 0:
				str.AppendFormat(_T(" - %s: %s"), (LPCTSTR)GetResString(IDS_UNKNOWNFILEEXT), (LPCTSTR)ext);
			}
		} else {
			// not verified
			if (pos >= 0) {
				str = fname.Mid(pos + 1).MakeUpper();
				str.AppendFormat(_T("  (%s)"), (LPCTSTR)GetResString(IDS_UNVERIFIED));
			} else
				str = GetResString(IDS_UNKNOWN);
		}
		m_bShowFileTypeWarning = showwarning;
		SetDlgItemText(IDC_FD_X11, str);
	} else {
		SetDlgItemText(IDC_FNAME, sm_pszNotAvail);
		SetDlgItemText(IDC_METFILE, sm_pszNotAvail);
		SetDlgItemText(IDC_FHASH, sm_pszNotAvail);

		SetDlgItemText(IDC_PFSTATUS, sm_pszNotAvail);
		SetDlgItemText(IDC_PARTCOUNT, sm_pszNotAvail);
		SetDlgItemText(IDC_FD_X11, sm_pszNotAvail);

		SetDlgItemText(IDC_FILECREATED, sm_pszNotAvail);
		SetDlgItemText(IDC_DL_ACTIVE_TIME, sm_pszNotAvail);
		SetDlgItemText(IDC_LASTSEENCOMPL, sm_pszNotAvail);
		SetDlgItemText(IDC_LASTRECEIVED, sm_pszNotAvail);
		SetDlgItemText(IDC_FD_AICHHASH, sm_pszNotAvail);
	}

	uint64 uFileSize = 0;
	uint64 uRealFileSize = 0;
	uint64 uTransferred = 0;
	uint64 uCorrupted = 0;
	uint32 uRecoveredParts = 0;
	uint64 uCompression = 0;
	uint64 uCompleted = 0;
	int iMD4HashsetAvailable = 0;
	int iAICHHashsetAvailable = 0;
	uint32 uDataRate = 0;
	UINT uSources = 0;
	UINT uValidSources = 0;
	UINT uNNPSources = 0;
	UINT uA4AFSources = 0;
	for (int i = m_paFiles->GetSize(); --i >= 0;) {
		CPartFile *file = static_cast<CPartFile*>((*m_paFiles)[i]);

		uFileSize += (uint64)file->GetFileSize();
		uRealFileSize += (uint64)file->GetRealFileSize();
		uTransferred += (uint64)file->GetTransferred();
		uCorrupted += file->GetCorruptionLoss();
		uRecoveredParts += file->GetRecoveredPartsByICH();
		uCompression += file->GetCompressionGain();
		uDataRate += file->GetDatarate();
		uCompleted += (uint64)file->GetCompletedSize();
		iMD4HashsetAvailable += static_cast<int>(file->GetFileIdentifier().HasExpectedMD4HashCount());
		iAICHHashsetAvailable += static_cast<int>(file->GetFileIdentifier().HasExpectedAICHHashCount());

		if (file->IsPartFile()) {
			uSources += file->GetSourceCount();
			uValidSources += file->GetValidSourcesCount();
			uNNPSources += file->GetSrcStatisticsValue(DS_NONEEDEDPARTS);
			uA4AFSources += file->GetSrcA4AFCount();
		}
	}

	str.Format(_T("%s  (%s %s);  %s %s"), (LPCTSTR)CastItoXBytes(uFileSize), (LPCTSTR)GetFormatedUInt64(uFileSize), (LPCTSTR)GetResString(IDS_BYTES), (LPCTSTR)GetResString(IDS_ONDISK), (LPCTSTR)CastItoXBytes(uRealFileSize));
	SetDlgItemText(IDC_FSIZE, str);

	if (iAICHHashsetAvailable == 0 && iMD4HashsetAvailable == 0)
		str = GetResString(IDS_NO);
	else if (m_paFiles->GetSize() == 1) {
		LPCTSTR p;
		if (iAICHHashsetAvailable != 0 && iMD4HashsetAvailable != 0)
			p = _T(" (eD2K + AICH)");
		else if (iAICHHashsetAvailable != 0)
			p = _T(" (AICH)");
		else //if (iMD4HashsetAvailable != 0)
			p = _T(" (eD2K)");
		str = GetResString(IDS_YES) + p;
	} else if (iMD4HashsetAvailable == m_paFiles->GetSize() && iMD4HashsetAvailable == iAICHHashsetAvailable)
		str = GetResString(IDS_YES) + _T(" (eD2K + AICH)");
	else
		str.Empty();
	SetDlgItemText(IDC_HASHSET, str);

	str.Format(GetResString(IDS_SOURCESINFO), uSources, uValidSources, uNNPSources, uA4AFSources);
	SetDlgItemText(IDC_SOURCECOUNT, str);

	SetDlgItemText(IDC_DATARATE, CastItoXBytes(uDataRate, false, true));

	SetDlgItemText(IDC_TRANSFERRED, CastItoXBytes(uTransferred));

	str.Format(_T("%s (%.1f%%)"), (LPCTSTR)CastItoXBytes(uCompleted), uFileSize != 0 ? (uCompleted * 100.0 / uFileSize) : 0.0);
	SetDlgItemText(IDC_COMPLSIZE, str);

	str.Format(_T("%s (%.1f%%)"), (LPCTSTR)CastItoXBytes(uCorrupted), uTransferred != 0 ? (uCorrupted * 100.0 / uTransferred) : 0.0);
	SetDlgItemText(IDC_CORRUPTED, str);

	str.Format(_T("%s (%.1f%%)"), (LPCTSTR)CastItoXBytes(uFileSize - uCompleted), uFileSize != 0 ? ((uFileSize - uCompleted) * 100.0 / uFileSize) : 0.0);
	SetDlgItemText(IDC_REMAINING, str);

	str.Format(_T("%u %s"), uRecoveredParts, (LPCTSTR)GetResString(IDS_FD_PARTS));
	SetDlgItemText(IDC_RECOVERED, str);

	str.Format(_T("%s (%.1f%%)"), (LPCTSTR)CastItoXBytes(uCompression), (uTransferred ? uCompression * 100.0 / uTransferred : 0.0));
	SetDlgItemText(IDC_COMPRESSION, str);
}

void CFileDetailDialogInfo::OnDestroy()
{
	if (m_timer) {
		KillTimer(m_timer);
		m_timer = 0;
	}
}

void CFileDetailDialogInfo::Localize()
{
	if (!m_hWnd)
		return;
	SetTabTitle(IDS_FD_GENERAL, this);

	SetDlgItemText(IDC_FD_X0, GetResString(IDS_FD_GENERAL));
	SetDlgItemText(IDC_FD_X1, GetResString(IDS_SW_NAME) + _T(':'));
	SetDlgItemText(IDC_FD_X2, GetResString(IDS_FD_MET));
	SetDlgItemText(IDC_FD_X3, GetResString(IDS_FD_HASH));
	SetDlgItemText(IDC_FD_X4, GetResString(IDS_DL_SIZE) + _T(':'));
	SetDlgItemText(IDC_FD_X9, GetResString(IDS_FD_PARTS) + _T(':'));
	SetDlgItemText(IDC_FD_X5, GetResString(IDS_STATUS) + _T(':'));
	SetDlgItemText(IDC_FD_X6, GetResString(IDS_FD_TRANSFER));
	SetDlgItemText(IDC_FD_X7, GetResString(IDS_DL_SOURCES) + _T(':'));
	SetDlgItemText(IDC_FD_X14, GetResString(IDS_FD_TRANS));
	SetDlgItemText(IDC_FD_X12, GetResString(IDS_FD_COMPSIZE));
	SetDlgItemText(IDC_FD_X13, GetResString(IDS_FD_DATARATE));
	SetDlgItemText(IDC_FD_X15, GetResString(IDS_LASTSEENCOMPL));
	SetDlgItemText(IDC_FD_LASTCHANGE, GetResString(IDS_FD_LASTCHANGE));
	SetDlgItemText(IDC_FD_X8, GetResString(IDS_FD_TIMEDATE));
	SetDlgItemText(IDC_FD_X16, GetResString(IDS_FD_DOWNLOADSTARTED));
	SetDlgItemText(IDC_DL_ACTIVE_TIME_LBL, GetResString(IDS_DL_ACTIVE_TIME) + _T(':'));
	SetDlgItemText(IDC_HSAV, GetResString(IDS_HSAV) + _T(':'));
	SetDlgItemText(IDC_FD_CORR, GetResString(IDS_FD_CORR) + _T(':'));
	SetDlgItemText(IDC_FD_RECOV, GetResString(IDS_FD_RECOV) + _T(':'));
	SetDlgItemText(IDC_FD_COMPR, GetResString(IDS_FD_COMPR) + _T(':'));
	SetDlgItemText(IDC_FD_XAICH, GetResString(IDS_AICHHASH) + _T(':'));
	SetDlgItemText(IDC_REMAINING_TEXT, GetResString(IDS_DL_REMAINS) + _T(':'));
	SetDlgItemText(IDC_FD_X10, GetResString(IDS_TYPE) + _T(':'));
}

HBRUSH CFileDetailDialogInfo::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CResizablePage::OnCtlColor(pDC, pWnd, nCtlColor);
	if (m_bShowFileTypeWarning && pWnd->GetDlgCtrlID() == IDC_FD_X11)
		pDC->SetTextColor(RGB(255, 0, 0));
	return hbr;
}