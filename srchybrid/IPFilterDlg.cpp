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
#include "emuleDlg.h"
#include "ServerWnd.h"
#include "ServerListCtrl.h"
#include "IPFilterDlg.h"
#include "IPFilter.h"
#include "OtherFunctions.h"
#include "Preferences.h"
#include "MenuCmds.h"
#include "ZipFile.h"
#include "GZipFile.h"
#include "RarFile.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//////////////////////////////////////////////////////////////////////////////
// COLUMN_INIT -- List View Columns

enum EIPFilterCols
{
	IPFILTER_COL_START = 0,
	IPFILTER_COL_END,
	IPFILTER_COL_LEVEL,
	IPFILTER_COL_HITS,
	IPFILTER_COL_DESC
};

static LCX_COLUMN_INIT s_aColumns[] =
{
	{ IPFILTER_COL_START,	_T("Start"),		IDS_IP_START,	LVCFMT_LEFT,	-1, 0, ASCENDING,  NONE, _T("255.255.255.255") },
	{ IPFILTER_COL_END,		_T("End"),			IDS_IP_END,		LVCFMT_LEFT,	-1, 1, ASCENDING,  NONE, _T("255.255.255.255")},
	{ IPFILTER_COL_LEVEL,	_T("Level"),		IDS_IP_LEVEL,	LVCFMT_RIGHT,	-1, 2, ASCENDING,  NONE, _T("999") },
	{ IPFILTER_COL_HITS,	_T("Hits"),			IDS_IP_HITS,	LVCFMT_RIGHT,	-1, 3, DESCENDING, NONE, _T("99999") },
	{ IPFILTER_COL_DESC,	_T("Description"),	IDS_IP_DESC,	LVCFMT_LEFT,	-1, 4, ASCENDING,  NONE, _T("long long long long long long long long file name") },
};

#define	PREF_INI_SECTION	_T("IPFilterDlg")

int CIPFilterDlg::sm_iSortColumn = IPFILTER_COL_HITS;

IMPLEMENT_DYNAMIC(CIPFilterDlg, CDialog)

BEGIN_MESSAGE_MAP(CIPFilterDlg, CResizableDialog)
	ON_BN_CLICKED(IDC_APPEND, OnBnClickedAppend)
	ON_BN_CLICKED(IDC_REMOVE, OnBnClickedDelete)
	ON_BN_CLICKED(IDC_SAVE, OnBnClickedSave)
	ON_COMMAND(MP_COPYSELECTED, OnCopyIPFilter)
	ON_COMMAND(MP_FIND, OnFind)
	ON_COMMAND(MP_REMOVE, OnDeleteIPFilter)
	ON_COMMAND(MP_SELECTALL, OnSelectAllIPFilter)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_IPFILTER, OnLvnColumnClickIPFilter)
	ON_NOTIFY(LVN_DELETEITEM, IDC_IPFILTER, OnLvnDeleteItemIPFilter)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_IPFILTER, OnLvnGetDispInfoIPFilter)
	ON_NOTIFY(LVN_KEYDOWN, IDC_IPFILTER, OnLvnKeyDownIPFilter)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CIPFilterDlg::CIPFilterDlg(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CIPFilterDlg::IDD, pParent)
	, m_pMenuIPFilter()
	, m_icoDlg()
	, m_uIPFilterItems()
	, m_ppIPFilterItems()
	, m_ulFilteredIPs()
{
	m_ipfilter.m_pParent = this;
	m_ipfilter.SetRegistryKey(PREF_INI_SECTION);
	m_ipfilter.SetRegistryPrefix(_T("IPfilters_"));
	m_ipfilter.m_pfnFindItem = FindItem;
	m_ipfilter.m_lFindItemParam = (DWORD_PTR)this;
}

CIPFilterDlg::~CIPFilterDlg()
{
	free(m_ppIPFilterItems);
	delete m_pMenuIPFilter;
	sm_iSortColumn = m_ipfilter.GetSortColumn();
	if (m_icoDlg)
		VERIFY(::DestroyIcon(m_icoDlg));
}

void CIPFilterDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_IPFILTER, m_ipfilter);
}

static int s_lParamSort = 0;

int __cdecl CompareIPFilterItems(const void *lParam1, const void *lParam2) noexcept
{
#define COMPARE_NUM(a,b)	((a) < (b))			\
							  ? -1				\
							  : ( ((b) < (a))	\
									? 1			\
									: 0			\
								)

	int iResult;

	switch (s_lParamSort) {
	case IPFILTER_COL_START:
		iResult = COMPARE_NUM((*((SIPFilter**)lParam1))->start, (*((SIPFilter**)lParam2))->start);
		break;
	case IPFILTER_COL_END:
		iResult = COMPARE_NUM((*((SIPFilter**)lParam1))->end, (*((SIPFilter**)lParam2))->end);
		break;
	case IPFILTER_COL_LEVEL:
		iResult = COMPARE_NUM((*((SIPFilter**)lParam1))->level, (*((SIPFilter**)lParam2))->level);
		break;
	case IPFILTER_COL_HITS:
		iResult = COMPARE_NUM((*((SIPFilter**)lParam1))->hits, (*((SIPFilter**)lParam2))->hits);
		break;
	case IPFILTER_COL_DESC:
		iResult = _stricmp/*CompareLocaleStringNoCase*/((*((SIPFilter**)lParam1))->desc, (*((SIPFilter**)lParam2))->desc);
		break;
	default:
		ASSERT(0);
		return 0;
	}
#undef COMPARE_NUM

	return (s_aColumns[s_lParamSort].eSortOrder == DESCENDING) ? -iResult : iResult;
}

void CIPFilterDlg::SortIPFilterItems()
{
	// Update (sort, if needed) the listview items
	if (m_ipfilter.GetSortColumn() != -1) {
		s_lParamSort = m_ipfilter.GetSortColumn();
		qsort((void*)m_ppIPFilterItems, m_uIPFilterItems, sizeof(*m_ppIPFilterItems), CompareIPFilterItems);
	}
}

void CIPFilterDlg::OnLvnColumnClickIPFilter(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	m_ipfilter.UpdateSortOrder(pNMLV, _countof(s_aColumns), s_aColumns);
	SortIPFilterItems();
	m_ipfilter.Invalidate();
	m_ipfilter.UpdateWindow();
	*pResult = 0;
}

BOOL CIPFilterDlg::OnInitDialog()
{
	CResizableDialog::OnInitDialog();
	InitWindowStyles(this);

	AddAnchor(IDC_IPFILTER, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_TOTAL_IPFILTER_LABEL, BOTTOM_LEFT);
	AddAnchor(IDC_TOTAL_IPFILTER, BOTTOM_LEFT);
	AddAnchor(IDC_TOTAL_IPS_LABEL, BOTTOM_LEFT);
	AddAnchor(IDC_TOTAL_IPS, BOTTOM_LEFT);
	AddAnchor(IDC_REMOVE, BOTTOM_RIGHT);
	AddAnchor(IDC_APPEND, BOTTOM_RIGHT);
	AddAnchor(IDC_SAVE, BOTTOM_RIGHT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	EnableSaveRestore(PREF_INI_SECTION);

	ASSERT(m_ipfilter.GetStyle() & LVS_OWNERDATA);
	// Win98: Explicitly set to Unicode to receive Unicode notifications.
	m_ipfilter.SendMessage(CCM_SETUNICODEFORMAT, TRUE);
	m_ipfilter.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_GRIDLINES);
	m_ipfilter.EnableHdrCtrlSortBitmaps();
	m_ipfilter.ReadColumnStats(_countof(s_aColumns), s_aColumns);
	m_ipfilter.CreateColumns(_countof(s_aColumns), s_aColumns);
	m_ipfilter.InitColumnOrders(_countof(s_aColumns), s_aColumns);
	m_ipfilter.UpdateSortColumn(_countof(s_aColumns), s_aColumns);

	SetIcon(m_icoDlg = theApp.LoadIcon(_T("IPFilter")), FALSE);

	InitIPFilters();

	m_pMenuIPFilter = new CMenu();
	if (m_pMenuIPFilter->CreatePopupMenu()) {
		m_pMenuIPFilter->AppendMenu(MF_ENABLED | MF_STRING, MP_COPYSELECTED, GetResString(IDS_COPY));
		m_pMenuIPFilter->AppendMenu(MF_ENABLED | MF_STRING, MP_REMOVE, GetResString(IDS_REMOVE));
		m_pMenuIPFilter->AppendMenu(MF_SEPARATOR);
		m_pMenuIPFilter->AppendMenu(MF_ENABLED | MF_STRING, MP_SELECTALL, GetResString(IDS_SELECTALL));
		m_pMenuIPFilter->AppendMenu(MF_SEPARATOR);
		m_pMenuIPFilter->AppendMenu(MF_ENABLED | MF_STRING, MP_FIND, GetResString(IDS_FIND));
	}
	m_ipfilter.m_pMenu = m_pMenuIPFilter;
	m_ipfilter.m_pParent = this;

	// localize
	SetWindowText(GetResString(IDS_IPFILTER));
	SetDlgItemText(IDC_STATICIPLABEL, GetResString(IDS_IP_RULES));
	SetDlgItemText(IDC_TOTAL_IPFILTER_LABEL, GetResString(IDS_TOTAL_IPFILTER_LABEL));
	SetDlgItemText(IDC_TOTAL_IPS_LABEL, GetResString(IDS_TOTAL_IPS_LABEL));
	SetDlgItemText(IDC_REMOVE, GetResString(IDS_DELETESELECTED));
	SetDlgItemText(IDC_APPEND, GetResString(IDS_APPEND));
	SetDlgItemText(IDC_SAVE, GetResString(IDS_SAVE));
	SetDlgItemText(IDOK, GetResString(IDS_FD_CLOSE));

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

void CIPFilterDlg::InitIPFilters()
{
	CWaitCursor curWait;

	m_ulFilteredIPs = 0;
	free(m_ppIPFilterItems);
	m_ppIPFilterItems = NULL;

	const CIPFilterArray &ipfilter = theApp.ipfilter->GetIPFilter();
	m_uIPFilterItems = (UINT)ipfilter.GetCount();
	m_ppIPFilterItems = (const SIPFilter**)malloc(sizeof(*m_ppIPFilterItems) * m_uIPFilterItems);
	if (m_ppIPFilterItems == NULL)
		m_uIPFilterItems = 0;
	else
		for (UINT i = 0; i < m_uIPFilterItems; ++i) {
			const SIPFilter *pFilter = ipfilter[i];
			m_ppIPFilterItems[i] = pFilter;
			m_ulFilteredIPs += pFilter->end - pFilter->start + 1;
		}
	SortIPFilterItems();
	m_ipfilter.SetItemCount(m_uIPFilterItems);
	SetDlgItemText(IDC_TOTAL_IPFILTER, GetFormatedUInt(m_uIPFilterItems));
	SetDlgItemText(IDC_TOTAL_IPS, GetFormatedUInt(m_ulFilteredIPs));
}

void CIPFilterDlg::OnLvnGetDispInfoIPFilter(LPNMHDR pNMHDR, LRESULT *pResult)
{
	const LVITEMW &rItem = reinterpret_cast<NMLVDISPINFO*>(pNMHDR)->item;
	if (rItem.mask & LVIF_TEXT) // *have* to check that flag!!
		switch (rItem.iSubItem) {
		case IPFILTER_COL_START:
			if (rItem.cchTextMax > 0) {
				_tcsncpy(rItem.pszText, (LPCTSTR)ipstr(htonl(m_ppIPFilterItems[rItem.iItem]->start)), rItem.cchTextMax);
				rItem.pszText[rItem.cchTextMax - 1] = _T('\0');
			}
			break;
		case IPFILTER_COL_END:
			if (rItem.cchTextMax > 0) {
				_tcsncpy(rItem.pszText, (LPCTSTR)ipstr(htonl(m_ppIPFilterItems[rItem.iItem]->end)), rItem.cchTextMax);
				rItem.pszText[rItem.cchTextMax - 1] = _T('\0');
			}
			break;
		case IPFILTER_COL_LEVEL:
			if (rItem.cchTextMax > 0) {
				_tcsncpy(rItem.pszText, _itot(m_ppIPFilterItems[rItem.iItem]->level, rItem.pszText, 10), rItem.cchTextMax);
				rItem.pszText[rItem.cchTextMax - 1] = _T('\0');
			}
			break;
		case IPFILTER_COL_HITS:
			if (rItem.cchTextMax > 0) {
				_tcsncpy(rItem.pszText, _itot(m_ppIPFilterItems[rItem.iItem]->hits, rItem.pszText, 10), rItem.cchTextMax);
				rItem.pszText[rItem.cchTextMax - 1] = _T('\0');
			}
			break;
		case IPFILTER_COL_DESC:
			if (rItem.cchTextMax > 0) {
				_tcsncpy(rItem.pszText, (CString)m_ppIPFilterItems[rItem.iItem]->desc, rItem.cchTextMax);
				rItem.pszText[rItem.cchTextMax - 1] = _T('\0');
			}
		}

	*pResult = 0;
}

void CIPFilterDlg::OnCopyIPFilter()
{
	CWaitCursor curWait;
	int iSelected = 0;
	CString strData;
	for (POSITION pos = m_ipfilter.GetFirstSelectedItemPosition(); pos != NULL;) {
		int iItem = m_ipfilter.GetNextSelectedItem(pos);
		if (!strData.IsEmpty())
			strData += _T("\r\n");

		strData.AppendFormat(_T("%-15s - %-15s  Hits=%-5s  %s")
			, (LPCTSTR)m_ipfilter.GetItemText(iItem, IPFILTER_COL_START)
			, (LPCTSTR)m_ipfilter.GetItemText(iItem, IPFILTER_COL_END)
			, (LPCTSTR)m_ipfilter.GetItemText(iItem, IPFILTER_COL_HITS)
			, (LPCTSTR)m_ipfilter.GetItemText(iItem, IPFILTER_COL_DESC));
		++iSelected;
	}

	if (!strData.IsEmpty()) {
		if (iSelected > 1)
			strData += _T("\r\n");
		theApp.CopyTextToClipboard(strData);
	}
}

void CIPFilterDlg::OnSelectAllIPFilter()
{
	m_ipfilter.SelectAllItems();
}

void CIPFilterDlg::OnBnClickedAppend()
{
	// Save/Restore the current directory
	TCHAR szCurDir[MAX_PATH];
	DWORD dwCurDirLen = GetCurrentDirectory(_countof(szCurDir), szCurDir);
	if (dwCurDirLen == 0 || dwCurDirLen >= _countof(szCurDir))
		szCurDir[0] = _T('\0');

	CString strFilePath;			  // Do NOT localize that string
	if (DialogBrowseFile(strFilePath, _T("All IP Filter Files (*ipfilter.dat;*ip.prefix;*.p2b;*.p2p;*.p2p.txt;*.zip;*.gz;*.rar)|*ipfilter.dat;*ip.prefix;*.p2b;*.p2p;*.p2p.txt;*.zip;*.gz;*.rar|eMule IP Filter Files (*ipfilter.dat;*ip.prefix)|*ipfilter.dat;*ip.prefix|Peer Guardian Files (*.p2b;*.p2p;*.p2p.txt)|*.p2b;*.p2p;*.p2p.txt|Text Files (*.txt)|*.txt|ZIP Files (*.zip;*.gz)|*.zip;*.gz|RAR Files (*.rar)|*.rar|All Files (*.*)|*.*||"))) {
		CWaitCursor curWait;

		CString szExt(PathFindExtension(strFilePath));
		szExt.MakeLower();
		bool bIsArchiveFile = szExt.Compare(_T(".zip")) == 0 || szExt.Compare(_T(".rar")) == 0 || szExt.Compare(_T(".gz")) == 0;
		bool bExtractedArchive = false;

		CString strTempUnzipFilePath;
		if (szExt.Compare(_T(".zip")) == 0) {
			CZIPFile zip;
			if (zip.Open(strFilePath)) {
				CZIPFile::File *zfile = zip.GetFile(_T("ipfilter.dat"));
				if (zfile == NULL)
					zfile = zip.GetFile(_T("guarding.p2p"));
				if (zfile == NULL)
					zfile = zip.GetFile(_T("guardian.p2p"));
				if (zfile) {
					_tmakepathlimit(strTempUnzipFilePath.GetBuffer(MAX_PATH), NULL, thePrefs.GetMuleDirectory(EMULE_CONFIGDIR), DFLT_IPFILTER_FILENAME, _T(".unzip.tmp"));
					strTempUnzipFilePath.ReleaseBuffer();
					if (zfile->Extract(strTempUnzipFilePath)) {
						strFilePath = strTempUnzipFilePath;
						bExtractedArchive = true;
					} else {
						CString strError;
						strError.Format(_T("Failed to extract IP filter file from ZIP file \"%s\"."), (LPCTSTR)strFilePath);
						AfxMessageBox(strError, MB_ICONERROR);
					}
				} else {
					CString strError;
					strError.Format(_T("Failed to find IP filter file \"guarding.p2p\", \"guardian.p2p\" or \"ipfilter.dat\" in ZIP file \"%s\"."), (LPCTSTR)strFilePath);
					AfxMessageBox(strError, MB_ICONERROR);
				}
				zip.Close();
			} else {
				CString strError;
				strError.Format(_T("Failed to open file \"%s\".\r\n\r\nInvalid file format?"), (LPCTSTR)strFilePath);
				AfxMessageBox(strError, MB_ICONERROR);
			}
		} else if (szExt.Compare(_T(".rar")) == 0) {
			CRARFile rar;
			if (rar.Open(strFilePath)) {
				CString strFile;
				if (rar.GetNextFile(strFile)
					&& (strFile.CompareNoCase(_T("ipfilter.dat")) == 0
						|| strFile.CompareNoCase(_T("guarding.p2p")) == 0
						|| strFile.CompareNoCase(_T("guardian.p2p")) == 0))
				{
					_tmakepathlimit(strTempUnzipFilePath.GetBuffer(MAX_PATH), NULL, thePrefs.GetMuleDirectory(EMULE_CONFIGDIR), DFLT_IPFILTER_FILENAME, _T(".unzip.tmp"));
					strTempUnzipFilePath.ReleaseBuffer();
					if (rar.Extract(strTempUnzipFilePath)) {
						strFilePath = strTempUnzipFilePath;
						bExtractedArchive = true;
					} else {
						CString strError;
						strError.Format(_T("Failed to extract IP filter file from RAR file \"%s\"."), (LPCTSTR)strFilePath);
						AfxMessageBox(strError, MB_ICONERROR);
					}
				} else {
					CString strError;
					strError.Format(_T("Failed to find IP filter file \"guarding.p2p\", \"guardian.p2p\" or \"ipfilter.dat\" in RAR file \"%s\"."), (LPCTSTR)strFilePath);
					AfxMessageBox(strError, MB_ICONERROR);
				}
				rar.Close();
			} else {
				CString strError;
				strError.Format(_T("Failed to open file \"%s\".\r\n\r\nInvalid file format?\r\n\r\nDownload latest version of UNRAR.DLL from http://www.rarlab.com and copy UNRAR.DLL into eMule installation folder."), (LPCTSTR)strFilePath);
				AfxMessageBox(strError, MB_ICONERROR);
			}
		} else if (szExt.Compare(_T(".gz")) == 0) {
			CGZIPFile gz;
			if (gz.Open(strFilePath)) {
				_tmakepathlimit(strTempUnzipFilePath.GetBuffer(MAX_PATH), NULL, thePrefs.GetMuleDirectory(EMULE_CONFIGDIR), DFLT_IPFILTER_FILENAME, _T(".unzip.tmp"));
				strTempUnzipFilePath.ReleaseBuffer();

				// add filename and extension of uncompressed file to temporary file
				const CString &strUncompressedFileName = gz.GetUncompressedFileName();
				if (!strUncompressedFileName.IsEmpty())
					strTempUnzipFilePath.AppendFormat(_T(".%s"), (LPCTSTR)strUncompressedFileName);

				if (gz.Extract(strTempUnzipFilePath)) {
					strFilePath = strTempUnzipFilePath;
					bExtractedArchive = true;
				}
				gz.Close();
			} else {
				CString strError;
				strError.Format(_T("Failed to open file \"%s\".\r\n\r\nInvalid file format?"), (LPCTSTR)strFilePath);
				AfxMessageBox(strError, MB_ICONERROR);
			}
		}

		if ((!bIsArchiveFile || bExtractedArchive) && theApp.ipfilter->AddFromFile(strFilePath, true)) {
			if (thePrefs.GetFilterServerByIP())
				theApp.emuledlg->serverwnd->serverlistctrl.RemoveAllFilteredServers();
			InitIPFilters();
			m_ipfilter.Update(-1);
		}

		if (!strTempUnzipFilePath.IsEmpty())
			VERIFY(_tremove(strTempUnzipFilePath) == 0);
	}

	if (szCurDir[0] != _T('\0'))
		VERIFY(SetCurrentDirectory(szCurDir));
}

void CIPFilterDlg::OnLvnDeleteItemIPFilter(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	ASSERT(m_uIPFilterItems > 0);
	if (m_uIPFilterItems > 0) {
		ASSERT((UINT)pNMLV->iItem < m_uIPFilterItems);
		if ((UINT)pNMLV->iItem < m_uIPFilterItems - 1)
			memmove(m_ppIPFilterItems + pNMLV->iItem, m_ppIPFilterItems + pNMLV->iItem + 1, (m_uIPFilterItems - (pNMLV->iItem + 1)) * sizeof(*m_ppIPFilterItems));
		--m_uIPFilterItems;
		const SIPFilter **ppNewIPFilterItems = (const SIPFilter**)realloc(m_ppIPFilterItems, sizeof(*m_ppIPFilterItems) * m_uIPFilterItems);
		if (ppNewIPFilterItems != NULL)
			m_ppIPFilterItems = ppNewIPFilterItems;
		//else: Keep the old list
	}

	*pResult = 0;
}

void CIPFilterDlg::OnBnClickedDelete()
{
	if (m_ipfilter.GetSelectedCount() == 0)
		return;
	if (LocMessageBox(IDS_DELETEIPFILTERS, MB_YESNOCANCEL, 0) != IDYES)
		return;
	CWaitCursor curWait;

	if (m_ipfilter.GetSelectedCount() == m_uIPFilterItems) {
		theApp.ipfilter->RemoveAllIPFilters();
		theApp.ipfilter->SetModified();
		m_uIPFilterItems = 0;
		free(m_ppIPFilterItems);
		m_ppIPFilterItems = NULL;
		m_ipfilter.SetItemCount(m_uIPFilterItems);
		m_ulFilteredIPs = 0;
	} else {
		CUIntArray aItems;
		for (POSITION pos = m_ipfilter.GetFirstSelectedItemPosition(); pos != NULL;) {
			int iItem = m_ipfilter.GetNextSelectedItem(pos);
			const SIPFilter *pFilter = m_ppIPFilterItems[iItem];
			if (pFilter) {
				ULONG ulIPRange = pFilter->end - pFilter->start + 1;
				if (theApp.ipfilter->RemoveIPFilter(pFilter)) {
					theApp.ipfilter->SetModified();
					aItems.Add(iItem);
					m_ulFilteredIPs -= ulIPRange;
				}
			}
		}

		m_ipfilter.SetRedraw(FALSE);
		for (INT_PTR i = aItems.GetCount() - 1; i >= 0; --i)
			m_ipfilter.DeleteItem(aItems[i]);
		if (!aItems.IsEmpty()) {
			int iNextSelItem = aItems[0];
			if (iNextSelItem >= m_ipfilter.GetItemCount())
				--iNextSelItem;
			if (iNextSelItem >= 0) {
				m_ipfilter.SetItemState(iNextSelItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_ipfilter.SetSelectionMark(iNextSelItem);
			}
		}
		m_ipfilter.SetRedraw();
	}
	ASSERT(m_uIPFilterItems == (UINT)m_ipfilter.GetItemCount());
	SetDlgItemText(IDC_TOTAL_IPFILTER, GetFormatedUInt(m_ipfilter.GetItemCount()));
	SetDlgItemText(IDC_TOTAL_IPS, GetFormatedUInt(m_ulFilteredIPs));
}

void CIPFilterDlg::OnDeleteIPFilter()
{
	OnBnClickedDelete();
}

void CIPFilterDlg::OnLvnKeyDownIPFilter(LPNMHDR pNMHDR, LRESULT *pResult)
{
	LPNMLVKEYDOWN pLVKeyDow = reinterpret_cast<LPNMLVKEYDOWN>(pNMHDR);

	if (pLVKeyDow->wVKey == VK_DELETE)
		OnDeleteIPFilter();
	else if (pLVKeyDow->wVKey == 'C' && GetKeyState(VK_CONTROL) < 0)
		OnCopyIPFilter();
	*pResult = 0;
}

void CIPFilterDlg::OnDestroy()
{
	m_ipfilter.WriteColumnStats(_countof(s_aColumns), s_aColumns);
	CResizableDialog::OnDestroy();
}

void CIPFilterDlg::OnBnClickedSave()
{
	CWaitCursor curWait;
	try {
		theApp.ipfilter->SaveToDefaultFile();
	} catch (const CString &err) {
		AfxMessageBox(err, MB_ICONERROR);
	}
}

bool CIPFilterDlg::FindItem(const CListCtrlX &lv, int iItem, DWORD_PTR lParam)
{
	if (lv.GetFindColumn() != IPFILTER_COL_START)
		return CListCtrlX::FindItem(lv, iItem, lParam);

	const CIPFilterDlg *dlg = reinterpret_cast<CIPFilterDlg*>(lParam);
	ASSERT_VALID(dlg);
	u_long ip = htonl(inet_addr((CStringA)lv.GetFindText()));
	const SIPFilter *filter = dlg->m_ppIPFilterItems[iItem];
	return (ip >= filter->start && ip <= filter->end);
}

void CIPFilterDlg::OnFind()
{
	m_ipfilter.OnFindStart();
}