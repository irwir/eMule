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
#include "StatisticsDlg.h"
#include "UploadQueue.h"
#include "Statistics.h"
#include "emuledlg.h"
#include "OtherFunctions.h"
#include "WebServer.h"
#include "DownloadQueue.h"
#include "ClientList.h"
#include "Preferences.h"
#include "ListenSocket.h"
#include "ServerList.h"
#include "SharedFileList.h"
#include "UpDownClient.h"
#include "UserMsgs.h"
#include "HelpIDs.h"
#include "Kademlia/Kademlia/kademlia.h"
#include "Kademlia/Kademlia/Prefs.h"
#include "kademlia/kademlia/UDPFirewallTester.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef _DEBUG
extern _CRT_ALLOC_HOOK g_pfnPrevCrtAllocHook;
#endif


// CStatisticsDlg dialog

IMPLEMENT_DYNAMIC(CStatisticsDlg, CDialog)

BEGIN_MESSAGE_MAP(CStatisticsDlg, CResizableDialog)
	ON_WM_SHOWWINDOW()
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BNMENU, OnMenuButtonClicked)
	ON_WM_SYSCOLORCHANGE()
	ON_WM_CTLCOLOR()
	ON_STN_DBLCLK(IDC_SCOPE_D, OnStnDblclickScopeD)
	ON_STN_DBLCLK(IDC_SCOPE_U, OnStnDblclickScopeU)
	ON_STN_DBLCLK(IDC_STATSSCOPE, OnStnDblclickStatsscope)
	ON_MESSAGE(UM_OSCOPEPOSITION, OnOscopePositionMsg)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

CStatisticsDlg::CStatisticsDlg(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CStatisticsDlg::IDD, pParent)
	, m_DownloadOMeter(3)
	, m_UploadOMeter(5)
	, m_Statistics(4)
	, m_oldcx()
	, m_oldcy()
	, m_TimeToolTips()
{
}

CStatisticsDlg::~CStatisticsDlg()
{
	delete m_TimeToolTips;

#ifdef _DEBUG
	for (POSITION pos = blockFiles.GetStartPosition(); pos != NULL;) {
		const unsigned char *fileName;
		HTREEITEM *pTag;
		blockFiles.GetNextAssoc(pos, fileName, pTag);
		delete pTag;
	}
#endif
}

void CStatisticsDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATTREE, m_stattree);
}

void CStatisticsDlg::OnSysColorChange()
{
	CResizableDialog::OnSysColorChange();
	SetAllIcons();
}

void CStatisticsDlg::SetAllIcons()
{
	InitWindowStyles(this);

	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("StatsGeneric")));		// Dots & Arrow (Default icon for stats)
	iml.Add(CTempIconLoader(_T("TransferUpDown")));		// Transfer
	iml.Add(CTempIconLoader(_T("Connection")));			// Connection
	iml.Add(CTempIconLoader(_T("StatsClients")));		// Clients
	iml.Add(CTempIconLoader(_T("Server")));				// Server
	iml.Add(CTempIconLoader(_T("SharedFiles")));		// Shared Files
	iml.Add(CTempIconLoader(_T("Upload")));				// Transfer > Upload
	iml.Add(CTempIconLoader(_T("Download")));			// Transfer > Download
	iml.Add(CTempIconLoader(_T("StatsDetail")));		// Session Sections
	iml.Add(CTempIconLoader(_T("StatsCumulative")));	// Cumulative Sections
	iml.Add(CTempIconLoader(_T("StatsRecords")));		// Records
	iml.Add(CTempIconLoader(_T("TransferUpDown")));		// Connection > General
	iml.Add(CTempIconLoader(_T("StatsTime")));			// Time Section
	iml.Add(CTempIconLoader(_T("StatsProjected")));		// Time > Averages and Projections
	iml.Add(CTempIconLoader(_T("StatsDay")));			// Time > Averages and Projections > Daily
	iml.Add(CTempIconLoader(_T("StatsMonth")));			// Time > Averages and Projections > Monthly
	iml.Add(CTempIconLoader(_T("StatsYear")));			// Time > Averages and Projections > Yearly
	iml.Add(CTempIconLoader(_T("HardDisk")));			// Diskspace
	m_stattree.SetImageList(&iml, TVSIL_NORMAL);
	imagelistStatTree.DeleteImageList();
	imagelistStatTree.Attach(iml.Detach());

	COLORREF crBk = ::GetSysColor(COLOR_WINDOW);
	COLORREF crFg = ::GetSysColor(COLOR_WINDOWTEXT);

	theApp.LoadSkinColorAlt(_T("StatisticsTvBk"), _T("DefLvBk"), crBk);
	theApp.LoadSkinColorAlt(_T("StatisticsTvFg"), _T("DefLvFg"), crFg);

	m_stattree.SetBkColor(crBk);
	m_stattree.SetTextColor(crFg);
	// can't use 'TVM_SETLINECOLOR' because the color may differ from the one used in "StatsGeneric" item image.
	//m_stattree.SendMessage(TVM_SETLINECOLOR, 0, (LPARAM)crFg);
}

BOOL CStatisticsDlg::OnInitDialog()
{
	CResizableDialog::OnInitDialog();
	EnableWindow(FALSE);
	SetAllIcons();
	m_bTreepaneHidden = false;

	if (theApp.m_fontSymbol.m_hObject) {
		GetDlgItem(IDC_BNMENU)->SetFont(&theApp.m_fontSymbol);
		SetDlgItemText(IDC_BNMENU, _T("6")); // show a down-arrow
	}

	// Win98: Explicitly set to Unicode to receive Unicode notifications.
	m_stattree.SendMessage(CCM_SETUNICODEFORMAT, TRUE);
	if (thePrefs.GetUseSystemFontForMainControls())
		m_stattree.SendMessage(WM_SETFONT, NULL, FALSE);
	CreateMyTree();

	// Setup download-scope
	CRect rcDown;
	GetDlgItem(IDC_SCOPE_D)->GetWindowRect(rcDown);
	GetDlgItem(IDC_SCOPE_D)->DestroyWindow();
	ScreenToClient(rcDown);
	m_DownloadOMeter.Create(WS_VISIBLE | WS_CHILD, rcDown, this, IDC_SCOPE_D);
	SetARange(true, thePrefs.GetMaxGraphDownloadRate());
	m_DownloadOMeter.SetYUnits(GetResString(IDS_KBYTESPERSEC));

	// Setup upload-scope
	CRect rcUp;
	GetDlgItem(IDC_SCOPE_U)->GetWindowRect(rcUp);
	GetDlgItem(IDC_SCOPE_U)->DestroyWindow();
	ScreenToClient(rcUp);
	// compensate rounding errors due to dialog units, make each of the 3 panes the same height
	rcUp.top = rcDown.bottom + 4;
	rcUp.bottom = rcUp.top + rcDown.Height();
	m_UploadOMeter.Create(WS_VISIBLE | WS_CHILD, rcUp, this, IDC_SCOPE_U);
	SetARange(false, thePrefs.GetMaxGraphUploadRate(true));
	m_UploadOMeter.SetYUnits(GetResString(IDS_KBYTESPERSEC));

	// Setup additional graph scope
	CRect rcConn;
	GetDlgItem(IDC_STATSSCOPE)->GetWindowRect(rcConn);
	GetDlgItem(IDC_STATSSCOPE)->DestroyWindow();
	ScreenToClient(rcConn);
	// compensate rounding errors due to dialog units, make each of the 3 panes the same height
	rcConn.top = rcUp.bottom + 4;
	rcConn.bottom = rcConn.top + rcDown.Height();
	m_Statistics.Create(WS_VISIBLE | WS_CHILD, rcConn, this, IDC_STATSSCOPE);
	m_Statistics.SetRanges(0, thePrefs.GetStatsMax());
	m_Statistics.autofitYscale = false;
	// Set the trend ratio of the Active Connections trend in the Connection Statistics scope.
	m_Statistics.SetTrendRatio(0, thePrefs.GetStatsConnectionsGraphRatio());

	m_Statistics.SetYUnits(_T(""));
	m_Statistics.SetXUnits(GetResString(IDS_TIME));

	RepaintMeters();
	m_Statistics.SetBackgroundColor(thePrefs.GetStatsColor(0));
	m_Statistics.SetGridColor(thePrefs.GetStatsColor(1));

	m_DownloadOMeter.InvalidateCtrl();
	m_UploadOMeter.InvalidateCtrl();
	m_Statistics.InvalidateCtrl();

	if (thePrefs.GetStatsInterval() == 0)
		m_stattree.EnableWindow(false);

	UpdateData(FALSE);

	EnableWindow(TRUE);

	m_ilastMaxConnReached = 0;
	CRect rcW;
	GetWindowRect(rcW);
	ScreenToClient(rcW);

	RECT rcTree;
	m_stattree.GetWindowRect(&rcTree);
	ScreenToClient(&rcTree);
	m_DownloadOMeter.GetWindowRect(rcDown);
	ScreenToClient(rcDown);
	m_UploadOMeter.GetWindowRect(&rcUp);
	ScreenToClient(&rcUp);

	RECT rcStat;
	m_Statistics.GetWindowRect(&rcStat);
	ScreenToClient(&rcStat);

	//vertical splitter
	CRect rcSpl(rcTree.right, rcW.top + 2, rcTree.right + 4, rcW.bottom - 5);
	m_wndSplitterstat.Create(WS_CHILD | WS_VISIBLE, rcSpl, this, IDC_SPLITTER_STAT);
	int PosStatVinitX = rcSpl.left;
	int PosStatVnewX = thePrefs.GetSplitterbarPositionStat() * rcW.Width() / 100;
	int maxX = rcW.right - 13;
	int minX = rcW.left + 8;
	if (thePrefs.GetSplitterbarPositionStat() > 90)
		PosStatVnewX = maxX;
	else if (thePrefs.GetSplitterbarPositionStat() < 10)
		PosStatVnewX = minX;
	rcSpl.left = PosStatVnewX;
	rcSpl.right = PosStatVnewX + 4;
	m_wndSplitterstat.MoveWindow(rcSpl);

	//HR splitter
	rcSpl = { rcDown.left, rcDown.bottom, rcDown.right, rcDown.bottom + 4 };
	m_wndSplitterstat_HR.Create(WS_CHILD | WS_VISIBLE, rcSpl, this, IDC_SPLITTER_STAT_HR);
	int PosStatVinitZ = rcSpl.top;
	int PosStatVnewZ = thePrefs.GetSplitterbarPositionStat_HR() * rcW.Height() / 100;
	int maxZ = rcW.bottom - 14;
	int minZ = 0;
	if (thePrefs.GetSplitterbarPositionStat_HR() > 90)
		PosStatVnewZ = maxZ;
	else if (thePrefs.GetSplitterbarPositionStat_HR() < 10)
		PosStatVnewZ = minZ;
	rcSpl.top = PosStatVnewZ;
	rcSpl.bottom = PosStatVnewZ + 4;
	m_wndSplitterstat_HR.MoveWindow(rcSpl);

	//HL splitter
	rcSpl = { rcUp.left, rcUp.bottom, rcUp.right, rcUp.bottom + 4 };
	m_wndSplitterstat_HL.Create(WS_CHILD | WS_VISIBLE, rcSpl, this, IDC_SPLITTER_STAT_HL);
	int PosStatVinitY = rcSpl.top;
	int PosStatVnewY = thePrefs.GetSplitterbarPositionStat_HL() * rcW.Height() / 100;
	int maxY = rcW.bottom - 9;
	int minY = 10;
	if (thePrefs.GetSplitterbarPositionStat_HL() > 90)
		PosStatVnewY = maxY;
	else if (thePrefs.GetSplitterbarPositionStat_HL() < 10)
		PosStatVnewY = minY;
	rcSpl.top = PosStatVnewY;
	rcSpl.bottom = PosStatVnewY + 4;
	m_wndSplitterstat_HL.MoveWindow(rcSpl);

	DoResize_V(PosStatVnewX - PosStatVinitX);
	DoResize_HL(PosStatVnewY - PosStatVinitY);
	DoResize_HR(PosStatVnewZ - PosStatVinitZ);

	Localize();
	ShowStatistics(true);

	m_TimeToolTips = new CToolTipCtrl();
	m_TimeToolTips->Create(this);
	m_TimeToolTips->AddTool(GetDlgItem(IDC_SCOPE_D), _T(""), NULL, 0);
	m_TimeToolTips->AddTool(GetDlgItem(IDC_SCOPE_U), _T(""), NULL, 0);
	m_TimeToolTips->AddTool(GetDlgItem(IDC_STATSSCOPE), _T(""), NULL, 0);
	// Any Autopop-Time which is specified higher than ~30 sec. will get reset to 5 sec.
	m_TimeToolTips->SetDelayTime(TTDT_AUTOPOP, 30000);
	m_TimeToolTips->SetDelayTime(TTDT_INITIAL, 30000);
	m_TimeToolTips->SetDelayTime(TTDT_RESHOW, 30000);
	EnableToolTips(TRUE);

	return true;
}

void CStatisticsDlg::initCSize()
{
	UINT x = thePrefs.GetSplitterbarPositionStat();
	UINT y = thePrefs.GetSplitterbarPositionStat_HL();
	UINT z = thePrefs.GetSplitterbarPositionStat_HR();
	if (x > 90)
		x = 100;
	else if (x < 10)
		x = 0;
	if (y > 90)
		y = 100;
	else if (y < 10)
		y = 0;
	if (z > 90)
		z = 100;
	else if (z < 10)
		z = 0;

	RemoveAnchor(IDC_BNMENU);
	AddAnchor(IDC_BNMENU, ANCHOR(0, 0));

	//StatTitle
	RemoveAnchor(IDC_STATIC_LASTRESET);
	AddAnchor(IDC_STATIC_LASTRESET, ANCHOR(0, 0), ANCHOR(x, 0));
	//m_stattree
	RemoveAnchor(m_stattree);
	AddAnchor(m_stattree, ANCHOR(0, 0), ANCHOR(x, 100));

	//graph
	RemoveAnchor(m_DownloadOMeter);
	AddAnchor(m_DownloadOMeter, ANCHOR(x, 0), ANCHOR(100, z));

	RemoveAnchor(m_UploadOMeter);
	AddAnchor(m_UploadOMeter, ANCHOR(x, z), ANCHOR(100, y));

	RemoveAnchor(m_Statistics);
	AddAnchor(m_Statistics, ANCHOR(x, y), ANCHOR(100, 100));

	//set range
	RECT rcW;
	GetWindowRect(&rcW);
	ScreenToClient(&rcW);
	RECT rcHR;
	m_wndSplitterstat_HR.GetWindowRect(&rcHR);
	ScreenToClient(&rcHR);
	RECT rcHL;
	m_wndSplitterstat_HL.GetWindowRect(&rcHL);
	ScreenToClient(&rcHL);

	m_wndSplitterstat.SetRange(rcW.left + 11, rcW.right - 11);
	m_wndSplitterstat_HL.SetRange(rcHR.bottom + 5, rcW.bottom - 7);
	m_wndSplitterstat_HR.SetRange(rcW.top + 3, rcHL.top - 5);
}


void CStatisticsDlg::DoResize_HL(int delta)
{
	if (!delta)
		return;
	m_DownloadOMeter.InvalidateCtrl(true);
	CSplitterControl::ChangeHeight(&m_UploadOMeter, delta, CW_TOPALIGN);
	CSplitterControl::ChangeHeight(&m_Statistics, -delta, CW_BOTTOMALIGN);

	CRect rcW;
	GetWindowRect(rcW);
	ScreenToClient(rcW);

	RECT rcSpl;
	m_UploadOMeter.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);
	thePrefs.SetSplitterbarPositionStat_HL(rcSpl.bottom * 100 / rcW.Height());

	initCSize();

	ShowInterval();
	Invalidate();
	UpdateWindow();
}

void CStatisticsDlg::DoResize_HR(int delta)
{
	if (!delta)
		return;
	CSplitterControl::ChangeHeight(&m_DownloadOMeter, delta, CW_TOPALIGN);
	CSplitterControl::ChangeHeight(&m_UploadOMeter, -delta, CW_BOTTOMALIGN);
	m_Statistics.InvalidateCtrl(true);

	CRect rcW;
	GetWindowRect(rcW);
	ScreenToClient(rcW);

	RECT rcSpl;
	m_DownloadOMeter.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);
	thePrefs.SetSplitterbarPositionStat_HR(rcSpl.bottom * 100 / rcW.Height());

	initCSize();

	ShowInterval();
	Invalidate();
	UpdateWindow();
}

void CStatisticsDlg::DoResize_V(int delta)
{
	if (!delta)
		return;
	CSplitterControl::ChangeWidth(GetDlgItem(IDC_STATIC_LASTRESET), delta);
	CSplitterControl::ChangeWidth(&m_stattree, delta);
	CSplitterControl::ChangeWidth(&m_DownloadOMeter, -delta, CW_RIGHTALIGN);
	CSplitterControl::ChangeWidth(&m_UploadOMeter, -delta, CW_RIGHTALIGN);
	CSplitterControl::ChangeWidth(&m_Statistics, -delta, CW_RIGHTALIGN);

	CRect rcW;
	GetWindowRect(rcW);
	ScreenToClient(rcW);

	RECT rcSpl;
	m_stattree.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);
	thePrefs.SetSplitterbarPositionStat(rcSpl.right * 100 / rcW.Width());

	if (rcSpl.left == rcSpl.right) {
		m_stattree.ShowWindow(SW_HIDE);
		GetDlgItem(IDC_BNMENU)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_STATIC_LASTRESET)->ShowWindow(SW_HIDE);
		m_bTreepaneHidden = true;
	} else if (m_bTreepaneHidden) {
		m_bTreepaneHidden = false;
		m_stattree.ShowWindow(SW_SHOW);
		GetDlgItem(IDC_BNMENU)->ShowWindow(SW_SHOW);
		GetDlgItem(IDC_STATIC_LASTRESET)->ShowWindow(SW_SHOW);
	}

	initCSize();

	ShowInterval();
	Invalidate();
	UpdateWindow();
}

LRESULT CStatisticsDlg::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_PAINT:
		{
			CRect rcW;
			GetWindowRect(rcW);
			if (!rcW.IsRectEmpty()) {
				ScreenToClient(rcW);

				RECT rctree;
				m_stattree.GetWindowRect(&rctree);
				ScreenToClient(&rctree);
				RECT rcSpl = { rctree.right, rcW.top + 2, rctree.right + 4, rcW.bottom - 5 };
				m_wndSplitterstat.MoveWindow(&rcSpl, true);

				RECT rcUp;
				m_UploadOMeter.GetWindowRect(&rcUp);
				ScreenToClient(&rcUp);
				rcSpl = RECT{ rcUp.left, rcUp.bottom, rcUp.right, rcUp.bottom + 4 };
				m_wndSplitterstat_HL.MoveWindow(&rcSpl, true);

				RECT rcDown;
				m_DownloadOMeter.GetWindowRect(&rcDown);
				ScreenToClient(&rcDown);
				rcSpl = RECT{ rcDown.left, rcDown.bottom, rcDown.right, rcDown.bottom + 4 };
				m_wndSplitterstat_HR.MoveWindow(&rcSpl, true);
			}
		}
		break;
	case WM_NOTIFY:
		switch(wParam) {
		case IDC_SPLITTER_STAT:
			DoResize_V(reinterpret_cast<SPC_NMHDR *>(lParam)->delta);
			break;
		case IDC_SPLITTER_STAT_HL:
			DoResize_HL(reinterpret_cast<SPC_NMHDR *>(lParam)->delta);
			break;
		case IDC_SPLITTER_STAT_HR:
			DoResize_HR(reinterpret_cast<SPC_NMHDR *>(lParam)->delta);
		}
		break;
	case WM_WINDOWPOSCHANGED:
		if (m_wndSplitterstat)
			m_wndSplitterstat.Invalidate();
		if (m_wndSplitterstat_HL)
			m_wndSplitterstat_HL.Invalidate();
		if (m_wndSplitterstat_HR)
			m_wndSplitterstat_HR.Invalidate();
		break;
	case WM_SIZE:
		{
			CRect rcW;
			GetWindowRect(rcW);
			//set range
			if (!rcW.IsRectEmpty()) {
				ScreenToClient(rcW);
				long splitposstat = thePrefs.GetSplitterbarPositionStat() * rcW.Width() / 100;
				long splitposstat_HL = thePrefs.GetSplitterbarPositionStat_HL() * rcW.Height() / 100;
				long splitposstat_HR = thePrefs.GetSplitterbarPositionStat_HR() * rcW.Height() / 100;
				RECT rcSpl;

				if (m_DownloadOMeter) {
					rcSpl = RECT{ splitposstat, rcW.top + 2, splitposstat + 4, rcW.bottom - 5 };
					m_wndSplitterstat.MoveWindow(&rcSpl, true);
					m_wndSplitterstat.SetRange(rcW.left + 11, rcW.right - 11);
				}
				if (m_wndSplitterstat_HR) {
					rcSpl = RECT{ splitposstat + 7, splitposstat_HR, rcW.right - 14, splitposstat_HR + 4 };
					m_wndSplitterstat_HR.MoveWindow(&rcSpl, true);
					m_wndSplitterstat_HR.SetRange(rcW.top + 3, splitposstat_HL - 4);
				}
				if (m_wndSplitterstat_HL) {
					rcSpl = RECT{ splitposstat + 7, splitposstat_HL, rcW.right - 14, splitposstat_HL + 4 };
					m_wndSplitterstat_HL.MoveWindow(&rcSpl, true);
					m_wndSplitterstat_HL.SetRange(splitposstat_HR + 14, rcW.bottom - 7);
				}
			}
		}
	}
	return CResizableDialog::DefWindowProc(message, wParam, lParam);
}

void CStatisticsDlg::RepaintMeters()
{
	CString Buffer;
	m_DownloadOMeter.SetBackgroundColor(thePrefs.GetStatsColor(0));	// Background
	m_DownloadOMeter.SetGridColor(thePrefs.GetStatsColor(1));		// Grid
	m_DownloadOMeter.SetPlotColor(thePrefs.GetStatsColor(4), 0);	// Download session
	m_DownloadOMeter.SetPlotColor(thePrefs.GetStatsColor(3), 1);	// Download average
	m_DownloadOMeter.SetPlotColor(thePrefs.GetStatsColor(2), 2);	// Download current
	m_DownloadOMeter.SetBarsPlot(thePrefs.GetFillGraphs(), 2);

	m_UploadOMeter.SetBackgroundColor(thePrefs.GetStatsColor(0));
	m_UploadOMeter.SetGridColor(thePrefs.GetStatsColor(1));			// Grid
	m_UploadOMeter.SetPlotColor(thePrefs.GetStatsColor(7), 0);		// Upload session
	m_UploadOMeter.SetPlotColor(thePrefs.GetStatsColor(6), 1);		// Upload average
	m_UploadOMeter.SetPlotColor(thePrefs.GetStatsColor(5), 2);		// Upload current
	m_UploadOMeter.SetPlotColor(thePrefs.GetStatsColor(14), 3);		// Upload current (excl. overhead)
	m_UploadOMeter.SetPlotColor(thePrefs.GetStatsColor(13), 4);		// Upload friend slots
	m_UploadOMeter.SetBarsPlot(thePrefs.GetFillGraphs(), 2);

	m_Statistics.SetBackgroundColor(thePrefs.GetStatsColor(0));
	m_Statistics.SetGridColor(thePrefs.GetStatsColor(1));
	m_Statistics.SetPlotColor(thePrefs.GetStatsColor(8), 0);	// Active Connections
	m_Statistics.SetPlotColor(thePrefs.GetStatsColor(10), 1);	// Active Uploads
	m_Statistics.SetPlotColor(thePrefs.GetStatsColor(9), 2);	// Total Uploads
	m_Statistics.SetPlotColor(thePrefs.GetStatsColor(12), 3);	// Active Downloads
	m_Statistics.SetBarsPlot(thePrefs.GetFillGraphs(), 0);

	m_DownloadOMeter.SetYUnits(GetResString(IDS_ST_DOWNLOAD));
	m_DownloadOMeter.SetLegendLabel(GetResString(IDS_ST_SESSION), 0);			// Download session
	Buffer.Format(_T(" (%u %s)"), thePrefs.GetStatsAverageMinutes(), (LPCTSTR)GetResString(IDS_MINS));
	m_DownloadOMeter.SetLegendLabel(GetResString(IDS_AVG) + Buffer, 1);			// Download average
	m_DownloadOMeter.SetLegendLabel(GetResString(IDS_ST_CURRENT), 2);			// Download current

	m_UploadOMeter.SetYUnits(GetResString(IDS_ST_UPLOAD));
	m_UploadOMeter.SetLegendLabel(GetResString(IDS_ST_SESSION), 0);				// Upload session
	Buffer.Format(_T(" (%u %s)"), thePrefs.GetStatsAverageMinutes(), (LPCTSTR)GetResString(IDS_MINS));
	m_UploadOMeter.SetLegendLabel(GetResString(IDS_AVG) + Buffer, 1);			// Upload average
	m_UploadOMeter.SetLegendLabel(GetResString(IDS_ST_ULCURRENT), 2);			// Upload current
	m_UploadOMeter.SetLegendLabel(GetResString(IDS_ST_ULSLOTSNOOVERHEAD), 3);	// Upload current (excl. overhead)
	m_UploadOMeter.SetLegendLabel(GetResString(IDS_ST_ULFRIEND), 4);			// Upload friend slots

	m_Statistics.SetYUnits(GetResString(IDS_CONNECTIONS));
	Buffer.Format(_T("%s (1:%u)"), (LPCTSTR)GetResString(IDS_ST_ACTIVEC), thePrefs.GetStatsConnectionsGraphRatio());
	m_Statistics.SetLegendLabel(Buffer, 0);										// Active Connections
	m_Statistics.SetLegendLabel(GetResString(IDS_ST_ACTIVEU_ZZ), 1);			// Active Uploads
	m_Statistics.SetLegendLabel(GetResString(IDS_SP_TOTALUL), 2);				// Total Uploads
	m_Statistics.SetLegendLabel(GetResString(IDS_ST_ACTIVED), 3);				// Active Downloads
}

void CStatisticsDlg::SetCurrentRate(float uploadrate, float downloadrate)
{
	if (theApp.IsClosing())
		return;

	// Download
	double m_dPlotDataDown[3];
	m_dPlotDataDown[0] = theStats.GetAvgDownloadRate(AVG_SESSION);
	m_dPlotDataDown[1] = theStats.GetAvgDownloadRate(AVG_TIME);
	m_dPlotDataDown[2] = downloadrate;
	m_DownloadOMeter.AppendPoints(m_dPlotDataDown);

	// Upload
	double m_dPlotDataUp[5];
	m_dPlotDataUp[0] = theStats.GetAvgUploadRate(AVG_SESSION);
	m_dPlotDataUp[1] = theStats.GetAvgUploadRate(AVG_TIME);
	// current rate to network (standardPackets + controlPackets)
	m_dPlotDataUp[2] = uploadrate;
	// current rate (excl. overhead)
	m_dPlotDataUp[3] = uploadrate - theStats.GetUpDatarateOverhead() / 1024.0f;
	// current rate to friends
	m_dPlotDataUp[4] = uploadrate - theApp.uploadqueue->GetToNetworkDatarate() / 1024.0f;
	m_UploadOMeter.AppendPoints(m_dPlotDataUp);

	// Connections
	CDownloadQueue::SDownloadStats myStats;
	theApp.downloadqueue->GetDownloadSourcesStats(myStats);
	m_dPlotDataMore[0] = theApp.listensocket->GetActiveConnections();
	m_dPlotDataMore[1] = (double)theApp.uploadqueue->GetActiveUploadsCount();
	m_dPlotDataMore[2] = (double)theApp.uploadqueue->GetUploadQueueLength();
	m_dPlotDataMore[3] = myStats.a[1];
	m_Statistics.AppendPoints(m_dPlotDataMore);

	// Web Server
	UpDown updown;
	updown.upload = uploadrate;
	updown.download = downloadrate;
	updown.connections = theApp.listensocket->GetActiveConnections();
	theApp.webserver->AddStatsLine(updown);
}


void CStatisticsDlg::ShowStatistics(bool forceUpdate)
{
	m_stattree.SetRedraw(false);
	CString cbuffer;
	// Set Tree Values

	// TRANSFER SECTION
	// If a section is not expanded, don't waste CPU cycles updating it.
	if (forceUpdate || m_stattree.IsExpanded(h_transfer)) {
		uint32	statGoodSessions = 0;
		uint32	statBadSessions = 0;
		double	percentSessions = 0;
		// Transfer Ratios
		if (theStats.sessionReceivedBytes > 0 && theStats.sessionSentBytes > 0) {
			// Session
			if (theStats.sessionReceivedBytes < theStats.sessionSentBytes)
				cbuffer.Format(_T("%s %.2f : 1"), (LPCTSTR)GetResString(IDS_STATS_SRATIO), (float)theStats.sessionSentBytes / theStats.sessionReceivedBytes);
			else
				cbuffer.Format(_T("%s 1 : %.2f"), (LPCTSTR)GetResString(IDS_STATS_SRATIO), (float)theStats.sessionReceivedBytes / theStats.sessionSentBytes);
		} else
			cbuffer.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_STATS_SRATIO), (LPCTSTR)GetResString(IDS_FSTAT_WAITING)); // Localize
		m_stattree.SetItemText(trans[0], cbuffer);

		if (theStats.sessionReceivedBytes > 0 && theStats.sessionSentBytes > 0) {
			// Session
			if (theStats.sessionSentBytes > theStats.sessionSentBytesToFriend && theStats.sessionReceivedBytes < theStats.sessionSentBytes - theStats.sessionSentBytesToFriend)
				cbuffer.Format(_T("%s %.2f : 1"), (LPCTSTR)GetResString(IDS_STATS_FRATIO), (float)(theStats.sessionSentBytes - theStats.sessionSentBytesToFriend) / theStats.sessionReceivedBytes);
			else
				cbuffer.Format(_T("%s 1 : %.2f"), (LPCTSTR)GetResString(IDS_STATS_FRATIO), (float)theStats.sessionReceivedBytes / (theStats.sessionSentBytes - theStats.sessionSentBytesToFriend));
		} else
			cbuffer.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_STATS_FRATIO), (LPCTSTR)GetResString(IDS_FSTAT_WAITING)); // Localize
		m_stattree.SetItemText(trans[1], cbuffer);

		if ((thePrefs.GetTotalDownloaded() > 0 && thePrefs.GetTotalUploaded() > 0) || (theStats.sessionReceivedBytes > 0 && theStats.sessionSentBytes > 0)) {
			// Cumulative
			if ((theStats.sessionReceivedBytes + thePrefs.GetTotalDownloaded()) < (theStats.sessionSentBytes + thePrefs.GetTotalUploaded()))
				cbuffer.Format(_T("%s %.2f : 1"), (LPCTSTR)GetResString(IDS_STATS_CRATIO), (float)(theStats.sessionSentBytes + thePrefs.GetTotalUploaded()) / (theStats.sessionReceivedBytes + thePrefs.GetTotalDownloaded()));
			else
				cbuffer.Format(_T("%s 1 : %.2f"), (LPCTSTR)GetResString(IDS_STATS_CRATIO), (float)(theStats.sessionReceivedBytes + thePrefs.GetTotalDownloaded()) / (theStats.sessionSentBytes + thePrefs.GetTotalUploaded()));
		} else
			cbuffer.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_STATS_CRATIO), (LPCTSTR)GetResString(IDS_FSTAT_WAITING)); // Localize
		m_stattree.SetItemText(trans[2], cbuffer);
		
		// TRANSFER -> DOWNLOADS SECTION
		if (forceUpdate || m_stattree.IsExpanded(h_download)) {
			uint64 DownOHTotal = 0;
			uint64 DownOHTotalPackets = 0;
			CDownloadQueue::SDownloadStats myStats;
			theApp.downloadqueue->GetDownloadSourcesStats(myStats);
			// TRANSFER -> DOWNLOADS -> SESSION SECTION
			if (forceUpdate || m_stattree.IsExpanded(h_down_session)) {
				// Downloaded Data
				cbuffer.Format(GetResString(IDS_STATS_DDATA), (LPCTSTR)CastItoXBytes(theStats.sessionReceivedBytes));
				m_stattree.SetItemText(down_S[0], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(down_S[0])) {
					// Downloaded Data By Client
					if (forceUpdate || m_stattree.IsExpanded(hdown_scb)) {
						int i = 0;
						double percentClientTransferred;
						uint64 DownDataTotal = thePrefs.GetDownSessionClientData();
						uint64 DownDataClient = thePrefs.GetDownData_EMULE();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_EDONKEYHYBRID();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eD Hybrid: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_EDONKEY();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eDonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_AMULE();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("aMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_MLDONKEY();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("MLdonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_SHAREAZA();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("Shareaza: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_EMULECOMPAT();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eM Compat: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetDownData_URL();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("URL: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_scb[i], cbuffer);
					}
					// Downloaded Data By Port
					if (forceUpdate || m_stattree.IsExpanded(hdown_spb)) {
						int i = 0;
						uint64	PortDataDefault = thePrefs.GetDownDataPort_4662();
						uint64	PortDataOther = thePrefs.GetDownDataPort_OTHER();
						uint64	PortDataPeerCache = thePrefs.GetDownDataPort_PeerCache();
						uint64	PortDataTotal = thePrefs.GetDownSessionDataPort();
						double	percentPortTransferred;

						if (PortDataTotal != 0 && PortDataDefault != 0)
							percentPortTransferred = 100.0 * PortDataDefault / PortDataTotal;
						else
							percentPortTransferred = 0;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), (LPCTSTR)CastItoXBytes(PortDataDefault), percentPortTransferred);
						m_stattree.SetItemText(down_spb[i], cbuffer);
						++i;

						if (PortDataTotal != 0 && PortDataOther != 0)
							percentPortTransferred = 100.0 * PortDataOther / PortDataTotal;
						else
							percentPortTransferred = 0;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataOther), percentPortTransferred);
						m_stattree.SetItemText(down_spb[i], cbuffer);
						++i;

						if (PortDataTotal != 0 && PortDataPeerCache != 0)
							percentPortTransferred = 100.0 * PortDataPeerCache / PortDataTotal;
						else
							percentPortTransferred = 0;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), thePrefs.GetPeerCacheShow() ? _T("PeerCache") : (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataPeerCache), percentPortTransferred);
						m_stattree.SetItemText(down_spb[i], cbuffer);
					}
				}
				// Completed Downloads
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_COMPDL), thePrefs.GetDownSessionCompletedFiles());
				m_stattree.SetItemText(down_S[1], cbuffer);
				// Active Downloads
				cbuffer.Format(GetResString(IDS_STATS_ACTDL), myStats.a[1]);
				m_stattree.SetItemText(down_S[2], cbuffer);
				// Found Sources
				cbuffer.Format(GetResString(IDS_STATS_FOUNDSRC), myStats.a[0]);
				m_stattree.SetItemText(down_S[3], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(down_S[3])) {
					int i = 0;
					// Sources By Status
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_ONQUEUE), myStats.a[2]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_QUEUEFULL), myStats.a[3]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_NONEEDEDPARTS), myStats.a[4]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_ASKING), myStats.a[5]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_RECHASHSET), myStats.a[6]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_CONNECTING), myStats.a[7]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_CONNVIASERVER), myStats.a[8]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_TOOMANYCONNS), myStats.a[9]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_NOCONNECTLOW2LOW), myStats.a[10]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_PROBLEMATIC), myStats.a[12]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_BANNED), myStats.a[13]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_ASKED4ANOTHERFILE), myStats.a[15]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_UNKNOWN), myStats.a[11]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;

					// where from? (3)
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_VIAED2KSQ), myStats.a[16]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_VIAKAD), myStats.a[17]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_VIASE), myStats.a[18]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_VIAPASSIVE), myStats.a[14]);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("eD2K: %u (%.1f%%)"), myStats.a[19], myStats.a[0] ? (myStats.a[19] * 100.0 / myStats.a[0]) : 0.0);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("Kad: %u (%.1f%%)"), myStats.a[20], myStats.a[0] ? (myStats.a[20] * 100.0 / myStats.a[0]) : 0.0);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;
					cbuffer.Format(_T("eD2K/Kad: %u (%.1f%%)"), myStats.a[21], myStats.a[0] ? (myStats.a[21] * 100.0 / myStats.a[0]) : 0.0);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;

					cbuffer.Format(_T("%s: %s, %s: %s (%.1f%%)"), (LPCTSTR)GetResString(IDS_UDPREASKS), (LPCTSTR)CastItoIShort(theApp.downloadqueue->GetUDPFileReasks()), (LPCTSTR)GetResString(IDS_UFAILED), (LPCTSTR)CastItoIShort(theApp.downloadqueue->GetFailedUDPFileReasks()), theApp.downloadqueue->GetUDPFileReasks() ? (theApp.downloadqueue->GetFailedUDPFileReasks() * 100.0 / theApp.downloadqueue->GetUDPFileReasks()) : 0.0);
					m_stattree.SetItemText(down_sources[i], cbuffer);
					++i;

					cbuffer.Format(_T("%s: %s (%s + %s)"), (LPCTSTR)GetResString(IDS_DEADSOURCES), (LPCTSTR)CastItoIShort(static_cast<uint32>(theApp.clientlist->m_globDeadSourceList.GetDeadSourcesCount() + myStats.a[22])), (LPCTSTR)CastItoIShort(static_cast<uint32>(theApp.clientlist->m_globDeadSourceList.GetDeadSourcesCount())), (LPCTSTR)CastItoIShort(static_cast<uint32>(myStats.a[22])));
					m_stattree.SetItemText(down_sources[i], cbuffer);
				}
				// Set Download Sessions
				statGoodSessions = thePrefs.GetDownS_SuccessfulSessions() + myStats.a[1]; // Add Active Downloads
				statBadSessions = thePrefs.GetDownS_FailedSessions();
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_DLSES), statGoodSessions + statBadSessions);
				m_stattree.SetItemText(down_S[4], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(down_S[4])) {
					// Set Successful Download Sessions and Average Downloaded Per Session
					if (statGoodSessions > 0) {
						percentSessions = 100.0 * statGoodSessions / (statGoodSessions + statBadSessions);
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATADLSES), (LPCTSTR)CastItoXBytes(theStats.sessionReceivedBytes / statGoodSessions));
					} else {
						percentSessions = 0;
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATADLSES), (LPCTSTR)CastItoXBytes((uint32)0));
					}
					m_stattree.SetItemText(down_ssessions[2], cbuffer); // Set Avg DL/Session
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_SDLSES), statGoodSessions, percentSessions);
					m_stattree.SetItemText(down_ssessions[0], cbuffer); // Set Succ Sessions
					// Set Failed Download Sessions (Avoid Division)
					if (percentSessions != 0 && statBadSessions > 0)
						percentSessions = 100 - percentSessions; // There were some good sessions and bad ones...
					else if (percentSessions == 0 && statBadSessions > 0)
						percentSessions = 100; // There were bad sessions and no good ones, must be 100%
					else
						percentSessions = 0; // No sessions at all, or no bad ones.
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_FDLSES), statBadSessions, percentSessions);
					m_stattree.SetItemText(down_ssessions[1], cbuffer);
					// Set Average Download Time
					cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDLTIME), (LPCTSTR)CastSecondsToLngHM(thePrefs.GetDownS_AvgTime()));
					m_stattree.SetItemText(down_ssessions[3], cbuffer);
				}
				// Set Gain Due To Compression
				cbuffer.Format(GetResString(IDS_STATS_GAINCOMP) + _T(" (%.1f%%)"), (LPCTSTR)CastItoXBytes(thePrefs.GetSesSavedFromCompression()), theStats.sessionReceivedBytes != 0 ? (thePrefs.GetSesSavedFromCompression() * 100.0 / theStats.sessionReceivedBytes) : 0.0);
				m_stattree.SetItemText(down_S[5], cbuffer);
				// Set Lost Due To Corruption
				cbuffer.Format(GetResString(IDS_STATS_LOSTCORRUPT) + _T(" (%.1f%%)"), (LPCTSTR)CastItoXBytes(thePrefs.GetSesLostFromCorruption()), theStats.sessionReceivedBytes != 0 ? (thePrefs.GetSesLostFromCorruption() * 100.0 / theStats.sessionReceivedBytes) : 0.0);
				m_stattree.SetItemText(down_S[6], cbuffer);
				// Set Parts Saved Due To ICH
				cbuffer.Format(GetResString(IDS_STATS_ICHSAVED), thePrefs.GetSesPartsSavedByICH());
				m_stattree.SetItemText(down_S[7], cbuffer);

				// Calculate downline OH totals
				DownOHTotal = theStats.GetDownDataOverheadFileRequest()
					+ theStats.GetDownDataOverheadSourceExchange()
					+ theStats.GetDownDataOverheadServer()
					+ theStats.GetDownDataOverheadKad()
					+ theStats.GetDownDataOverheadOther();
				DownOHTotalPackets = theStats.GetDownDataOverheadFileRequestPackets()
					+ theStats.GetDownDataOverheadSourceExchangePackets()
					+ theStats.GetDownDataOverheadServerPackets()
					+ theStats.GetDownDataOverheadKadPackets()
					+ theStats.GetDownDataOverheadOtherPackets();

				// Downline Overhead
				cbuffer.Format(GetResString(IDS_TOVERHEAD), (LPCTSTR)CastItoXBytes(DownOHTotal), (LPCTSTR)CastItoIShort(DownOHTotalPackets));
				m_stattree.SetItemText(hdown_soh, cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(hdown_soh)) {
					int i = 0;
					// Set down session file req OH
					cbuffer.Format(GetResString(IDS_FROVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadFileRequest()), (LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadFileRequestPackets()));
					m_stattree.SetItemText(down_soh[i], cbuffer);
					++i;
					// Set down session source exch OH
					cbuffer.Format(GetResString(IDS_SSOVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadSourceExchange()), (LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadSourceExchangePackets()));
					m_stattree.SetItemText(down_soh[i], cbuffer);
					++i;
					// Set down session server OH
					cbuffer.Format(GetResString(IDS_SOVERHEAD),
						(LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadServer()),
						(LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadServerPackets()));
					m_stattree.SetItemText(down_soh[i], cbuffer);
					++i;
					// Set down session Kad OH
					cbuffer.Format(GetResString(IDS_KADOVERHEAD),
						(LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadKad()),
						(LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadKadPackets()));
					m_stattree.SetItemText(down_soh[i], cbuffer);
				}
			}
			// TRANSFER -> DOWNLOADS -> CUMULATIVE SECTION
			if (forceUpdate || m_stattree.IsExpanded(h_down_total)) {
				// Downloaded Data
				uint64 ullCumReceived = theStats.sessionReceivedBytes + thePrefs.GetTotalDownloaded();
				cbuffer.Format(GetResString(IDS_STATS_DDATA), (LPCTSTR)CastItoXBytes(ullCumReceived));
				m_stattree.SetItemText(down_T[0], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(down_T[0])) {
					// Downloaded Data By Client
					if (forceUpdate || m_stattree.IsExpanded(hdown_tcb)) {
						int i = 0;
						uint64 DownDataTotal = thePrefs.GetDownTotalClientData();
						uint64 DownDataClient = thePrefs.GetCumDownData_EMULE();
						double percentClientTransferred;
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_EDONKEYHYBRID();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eD Hybrid: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_EDONKEY();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eDonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_AMULE();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("aMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_MLDONKEY();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("MLdonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_SHAREAZA();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("Shareaza: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_EMULECOMPAT();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("eM Compat: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
						++i;

						DownDataClient = thePrefs.GetCumDownData_URL();
						if (DownDataTotal != 0 && DownDataClient != 0)
							percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
						else
							percentClientTransferred = 0;
						cbuffer.Format(_T("URL: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
						m_stattree.SetItemText(down_tcb[i], cbuffer);
					}
					// Downloaded Data By Port
					if (forceUpdate || m_stattree.IsExpanded(hdown_tpb)) {
						int i = 0;
						uint64	PortDataDefault = thePrefs.GetCumDownDataPort_4662();
						uint64	PortDataOther = thePrefs.GetCumDownDataPort_OTHER();
						uint64	PortDataPeerCache = thePrefs.GetCumDownDataPort_PeerCache();
						uint64	PortDataTotal = thePrefs.GetDownTotalPortData();
						double	percentPortTransferred;

						if (PortDataTotal != 0 && PortDataDefault != 0)
							percentPortTransferred = 100.0 * PortDataDefault / PortDataTotal;
						else
							percentPortTransferred = 0;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), (LPCTSTR)CastItoXBytes(PortDataDefault), percentPortTransferred);
						m_stattree.SetItemText(down_tpb[i], cbuffer);
						++i;

						if (PortDataTotal != 0 && PortDataOther != 0)
							percentPortTransferred = 100.0 * PortDataOther / PortDataTotal;
						else
							percentPortTransferred = 0;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataOther), percentPortTransferred);
						m_stattree.SetItemText(down_tpb[i], cbuffer);
						++i;

						if (PortDataTotal != 0 && PortDataPeerCache != 0)
							percentPortTransferred = 100.0 * PortDataPeerCache / PortDataTotal;
						else
							percentPortTransferred = 0;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), thePrefs.GetPeerCacheShow() ? _T("PeerCache") : (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataPeerCache), percentPortTransferred);
						m_stattree.SetItemText(down_tpb[i], cbuffer);
					}
				}
				// Set Cum Completed Downloads
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_COMPDL), thePrefs.GetDownCompletedFiles());
				m_stattree.SetItemText(down_T[1], cbuffer);
				// Set Cum Download Sessions
				statGoodSessions = thePrefs.GetDownC_SuccessfulSessions() + myStats.a[1]; // Need to reset these from the session section.  Declared up there.
				statBadSessions = thePrefs.GetDownC_FailedSessions(); // ^^^^^^^^^^^^^^
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_DLSES), statGoodSessions + statBadSessions);
				m_stattree.SetItemText(down_T[2], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(down_T[2])) {
					// Set Cum Successful Download Sessions & Cum Average Download Per Sessions (Save an if-else statement)
					if (statGoodSessions > 0) {
						percentSessions = 100.0 * statGoodSessions / (statGoodSessions + statBadSessions);
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATADLSES), (LPCTSTR)CastItoXBytes(ullCumReceived / statGoodSessions));
					} else {
						percentSessions = 0;
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATADLSES), (LPCTSTR)CastItoXBytes((uint32)0));
					}
					m_stattree.SetItemText(down_tsessions[2], cbuffer); // Set Avg DL/Session
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_SDLSES), statGoodSessions, percentSessions);
					m_stattree.SetItemText(down_tsessions[0], cbuffer); // Set Successful Sessions
					// Set Cum Failed Download Sessions
					if (percentSessions != 0 && statBadSessions > 0)
						percentSessions = 100 - percentSessions; // There were some good sessions and bad ones...
					else if (percentSessions == 0 && statBadSessions > 0)
						percentSessions = 100; // There were bad sessions and no good ones, must be 100%
					else
						percentSessions = 0; // No sessions at all, or no bad ones.
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_FDLSES), statBadSessions, percentSessions);
					m_stattree.SetItemText(down_tsessions[1], cbuffer);
					// Set Cumulative Average Download Time
					uint32 avgDownTime = thePrefs.GetDownS_AvgTime();
					if (thePrefs.GetDownC_AvgTime() <= 0)
						thePrefs.SetDownCAvgTime(avgDownTime);
					avgDownTime = (uint32)(avgDownTime + thePrefs.GetDownC_AvgTime()) / 2;
					cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDLTIME), (LPCTSTR)CastSecondsToLngHM(avgDownTime));
					m_stattree.SetItemText(down_tsessions[3], cbuffer);
				}
				// Set Cumulative Gained Due To Compression
				uint64 ullCumCompressed = thePrefs.GetSesSavedFromCompression() + thePrefs.GetCumSavedFromCompression();
				cbuffer.Format(GetResString(IDS_STATS_GAINCOMP) + _T(" (%.1f%%)"), (LPCTSTR)CastItoXBytes(ullCumCompressed), ullCumReceived != 0 ? (ullCumCompressed * 100.0 / ullCumReceived) : 0.0);
				m_stattree.SetItemText(down_T[3], cbuffer);
				// Set Cumulative Lost Due To Corruption
				uint64 ullCumCorrupted = thePrefs.GetSesLostFromCorruption() + thePrefs.GetCumLostFromCorruption();
				cbuffer.Format(GetResString(IDS_STATS_LOSTCORRUPT) + _T(" (%.1f%%)"), (LPCTSTR)CastItoXBytes(ullCumCorrupted), ullCumReceived != 0 ? (ullCumCorrupted * 100.0 / ullCumReceived) : 0.0);
				m_stattree.SetItemText(down_T[4], cbuffer);
				// Set Cumulative Saved Due To ICH
				cbuffer.Format(GetResString(IDS_STATS_ICHSAVED), thePrefs.GetSesPartsSavedByICH() + thePrefs.GetCumPartsSavedByICH());
				m_stattree.SetItemText(down_T[5], cbuffer);

				if (DownOHTotal == 0 || DownOHTotalPackets == 0) {
					DownOHTotal = theStats.GetDownDataOverheadFileRequest()
						+ theStats.GetDownDataOverheadSourceExchange()
						+ theStats.GetDownDataOverheadServer()
						+ theStats.GetDownDataOverheadKad()
						+ theStats.GetDownDataOverheadOther();
					DownOHTotalPackets = theStats.GetDownDataOverheadFileRequestPackets()
						+ theStats.GetDownDataOverheadSourceExchangePackets()
						+ theStats.GetDownDataOverheadServerPackets()
						+ theStats.GetDownDataOverheadKadPackets()
						+ theStats.GetDownDataOverheadOtherPackets();
				}
				// Total Overhead
				cbuffer.Format(GetResString(IDS_TOVERHEAD), (LPCTSTR)CastItoXBytes(DownOHTotal + thePrefs.GetDownOverheadTotal()), (LPCTSTR)CastItoIShort(DownOHTotalPackets + thePrefs.GetDownOverheadTotalPackets()));
				m_stattree.SetItemText(hdown_toh, cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(hdown_toh)) {
					int i = 0;
					// File Request Overhead
					cbuffer.Format(GetResString(IDS_FROVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadFileRequest() + thePrefs.GetDownOverheadFileReq()), (LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadFileRequestPackets() + thePrefs.GetDownOverheadFileReqPackets()));
					m_stattree.SetItemText(down_toh[i], cbuffer);
					++i;
					// Source Exchange Overhead
					cbuffer.Format(GetResString(IDS_SSOVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadSourceExchange() + thePrefs.GetDownOverheadSrcEx()), (LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadSourceExchangePackets() + thePrefs.GetDownOverheadSrcExPackets()));
					m_stattree.SetItemText(down_toh[i], cbuffer);
					++i;
					// Server Overhead
					cbuffer.Format(GetResString(IDS_SOVERHEAD),
						(LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadServer() +
							thePrefs.GetDownOverheadServer(), false, false),
							(LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadServerPackets() +
								thePrefs.GetDownOverheadServerPackets()));
					m_stattree.SetItemText(down_toh[i], cbuffer);
					++i;
					// Kad Overhead
					cbuffer.Format(GetResString(IDS_KADOVERHEAD)
						, (LPCTSTR)CastItoXBytes(theStats.GetDownDataOverheadKad() + thePrefs.GetDownOverheadKad())
						, (LPCTSTR)CastItoIShort(theStats.GetDownDataOverheadKadPackets() + thePrefs.GetDownOverheadKadPackets()));
					m_stattree.SetItemText(down_toh[i], cbuffer);
				}
			} // - End Transfer -> Downloads -> Cumulative Section
		} // - End Transfer -> Downloads Section
		// TRANSFER-> UPLOADS SECTION
		if (forceUpdate || m_stattree.IsExpanded(h_upload)) {
			uint64 UpOHTotal = 0;
			uint64 UpOHTotalPackets = 0;
			// TRANSFER -> UPLOADS -> SESSION SECTION
			if (forceUpdate || m_stattree.IsExpanded(h_up_session)) {
				// Uploaded Data
				cbuffer.Format(GetResString(IDS_STATS_UDATA), (LPCTSTR)CastItoXBytes(theStats.sessionSentBytes));
				m_stattree.SetItemText(up_S[0], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(up_S[0])) {
					// Uploaded Data By Client
					if (forceUpdate || m_stattree.IsExpanded(hup_scb)) {
						int i = 0;
						uint64 UpDataTotal = thePrefs.GetUpSessionClientData();

						uint64 UpDataClient = thePrefs.GetUpData_EMULE();
						double percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetUpData_EDONKEYHYBRID();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eD Hybrid: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetUpData_EDONKEY();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eDonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetUpData_AMULE();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("aMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetUpData_MLDONKEY();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("MLdonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetUpData_SHAREAZA();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("Shareaza: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetUpData_EMULECOMPAT();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eM Compat: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_scb[i], cbuffer);
					}
					// Uploaded Data By Port
					if (forceUpdate || m_stattree.IsExpanded(hup_spb)) {
						int i = 0;
						uint64	PortDataTotal = thePrefs.GetUpSessionPortData();

						uint64	PortDataDefault = thePrefs.GetUpDataPort_4662();
						double	percentPortTransferred = !PortDataTotal ? 0 : 100.0 * PortDataDefault / PortDataTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), (LPCTSTR)CastItoXBytes(PortDataDefault), percentPortTransferred);
						m_stattree.SetItemText(up_spb[i], cbuffer);
						++i;

						uint64	PortDataOther = thePrefs.GetUpDataPort_OTHER();
						percentPortTransferred = !PortDataTotal ? 0 : 100.0 * PortDataOther / PortDataTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataOther), percentPortTransferred);
						m_stattree.SetItemText(up_spb[i], cbuffer);
						++i;

						uint64	PortDataPeerCache = thePrefs.GetUpDataPort_PeerCache();
						percentPortTransferred = !PortDataTotal ? 0 : 100.0 * PortDataPeerCache / PortDataTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), thePrefs.GetPeerCacheShow() ? _T("PeerCache") : (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataPeerCache), percentPortTransferred);
						m_stattree.SetItemText(up_spb[i], cbuffer);
					}
					// Uploaded Data By Source
					if (forceUpdate || m_stattree.IsExpanded(hup_ssb)) {
						int i = 0;
						uint64	DataSourceTotal = thePrefs.GetUpSessionDataFile();

						uint64	DataSourceFile = thePrefs.GetUpData_File();
						double	percentFileTransferred = !DataSourceTotal ? 0 : 100.0 * DataSourceFile / DataSourceTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DSFILE), (LPCTSTR)CastItoXBytes(DataSourceFile), percentFileTransferred);
						m_stattree.SetItemText(up_ssb[i], cbuffer);
						++i;

						uint64	DataSourcePF = thePrefs.GetUpData_Partfile();
						percentFileTransferred = !DataSourceTotal ? 0 : 100.0 * DataSourcePF / DataSourceTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DSPF), (LPCTSTR)CastItoXBytes(DataSourcePF), percentFileTransferred);
						m_stattree.SetItemText(up_ssb[i], cbuffer);
					}
				}

				// Amount of data uploaded to friends
				cbuffer.Format(GetResString(IDS_STATS_UDATA_FRIENDS), (LPCTSTR)CastItoXBytes(theStats.sessionSentBytesToFriend));
				m_stattree.SetItemText(up_S[1], cbuffer);

				// Set fully Active Uploads
				cbuffer.Format(GetResString(IDS_STATS_ACTUL_ZZ), static_cast<unsigned>(theApp.uploadqueue->GetActiveUploadsCount())); //theApp.uploadqueue->GetUploadQueueLength()
				m_stattree.SetItemText(up_S[2], cbuffer);

				// Set Total Uploads
				cbuffer.Format(GetResString(IDS_STATS_TOTALUL), static_cast<unsigned>(theApp.uploadqueue->GetUploadQueueLength()));
				m_stattree.SetItemText(up_S[3], cbuffer);

				// Set Queue Length
				cbuffer.Format(GetResString(IDS_STATS_WAITINGUSERS), static_cast<unsigned>(theApp.uploadqueue->GetWaitingUserCount()));
				m_stattree.SetItemText(up_S[4], cbuffer);

				// Set Upload Sessions
				statGoodSessions = theApp.uploadqueue->GetSuccessfullUpCount() + (uint32)theApp.uploadqueue->GetUploadQueueLength();
				statBadSessions = theApp.uploadqueue->GetFailedUpCount();
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_ULSES), statGoodSessions + statBadSessions);
				m_stattree.SetItemText(up_S[5], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(up_S[5])) {
					// Set Successful Upload Sessions & Average Uploaded Per Session
					if (statGoodSessions > 0) { // Black holes are when God divided by 0
						percentSessions = 100.0*statGoodSessions / (statGoodSessions + statBadSessions);
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATAULSES), (LPCTSTR)CastItoXBytes(theStats.sessionSentBytes / statGoodSessions));
					} else {
						percentSessions = 0;
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATAULSES), (LPCTSTR)GetResString(IDS_FSTAT_WAITING));
					}
					m_stattree.SetItemText(up_ssessions[2], cbuffer);
					cbuffer.Format(GetResString(IDS_STATS_SUCCUPCOUNT), statGoodSessions, percentSessions);
					m_stattree.SetItemText(up_ssessions[0], cbuffer);
					// Set Failed Upload Sessions
					if (percentSessions != 0 && statBadSessions > 0)
						percentSessions = 100 - percentSessions; // There were some good sessions and bad ones...
					else if (percentSessions == 0 && statBadSessions > 0)
						percentSessions = 100; // There were bad sessions and no good ones, must be 100%
					else
						percentSessions = 0; // No sessions at all, or no bad ones.
					cbuffer.Format(GetResString(IDS_STATS_FAILUPCOUNT), statBadSessions, percentSessions);
					m_stattree.SetItemText(up_ssessions[1], cbuffer);
					// Set Avg Upload time
					cbuffer.Format(GetResString(IDS_STATS_AVEUPTIME), (LPCTSTR)CastSecondsToLngHM(theApp.uploadqueue->GetAverageUpTime()));
					m_stattree.SetItemText(up_ssessions[3], cbuffer);
				}
				// Calculate Upline OH Totals
				UpOHTotal = theStats.GetUpDataOverheadFileRequest()
					+ theStats.GetUpDataOverheadSourceExchange()
					+ theStats.GetUpDataOverheadServer()
					+ theStats.GetUpDataOverheadKad()
					+ theStats.GetUpDataOverheadOther();
				UpOHTotalPackets = theStats.GetUpDataOverheadFileRequestPackets()
					+ theStats.GetUpDataOverheadSourceExchangePackets()
					+ theStats.GetUpDataOverheadServerPackets()
					+ theStats.GetUpDataOverheadKadPackets()
					+ theStats.GetUpDataOverheadOtherPackets();
				// Total Upline Overhead
				cbuffer.Format(GetResString(IDS_TOVERHEAD), (LPCTSTR)CastItoXBytes(UpOHTotal), (LPCTSTR)CastItoIShort(UpOHTotalPackets));
				m_stattree.SetItemText(hup_soh, cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(hup_soh)) {
					int i = 0;
					// File Request Overhead
					cbuffer.Format(GetResString(IDS_FROVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadFileRequest()), (LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadFileRequestPackets()));
					m_stattree.SetItemText(up_soh[i], cbuffer);
					++i;
					// Source Exchanged Overhead
					cbuffer.Format(GetResString(IDS_SSOVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadSourceExchange()), (LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadSourceExchangePackets()));
					m_stattree.SetItemText(up_soh[i], cbuffer);
					++i;
					// Server Overhead
					cbuffer.Format(GetResString(IDS_SOVERHEAD)
						, (LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadServer())
						, (LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadServerPackets()));
					m_stattree.SetItemText(up_soh[i], cbuffer);
					++i;
					// Kad Overhead
					cbuffer.Format(GetResString(IDS_KADOVERHEAD)
						, (LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadKad())
						, (LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadKadPackets()));
					m_stattree.SetItemText(up_soh[i], cbuffer);
				}
			} // - End Transfer -> Uploads -> Session Section
			// TRANSFER -> UPLOADS -> CUMULATIVE SECTION
			if (forceUpdate || m_stattree.IsExpanded(h_up_total)) {
				// Uploaded Data
				cbuffer.Format(GetResString(IDS_STATS_UDATA), (LPCTSTR)CastItoXBytes(theStats.sessionSentBytes + thePrefs.GetTotalUploaded()));
				m_stattree.SetItemText(up_T[0], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(up_T[0])) {
					// Uploaded Data By Client
					if (forceUpdate || m_stattree.IsExpanded(hup_tcb)) {
						int i = 0;
						uint64 UpDataTotal = thePrefs.GetUpTotalClientData();

						uint64 UpDataClient = thePrefs.GetCumUpData_EMULE();
						double percentClientTransferred;
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetCumUpData_EDONKEYHYBRID();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eD Hybrid: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetCumUpData_EDONKEY();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eDonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetCumUpData_AMULE();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("aMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetCumUpData_MLDONKEY();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("MLdonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetCumUpData_SHAREAZA();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("Shareaza: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
						++i;

						UpDataClient = thePrefs.GetCumUpData_EMULECOMPAT();
						percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
						cbuffer.Format(_T("eM Compat: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
						m_stattree.SetItemText(up_tcb[i], cbuffer);
					}
					// Uploaded Data By Port
					if (forceUpdate || m_stattree.IsExpanded(hup_tpb)) {
						int i = 0;
						uint64	PortDataTotal = thePrefs.GetUpTotalPortData();

						uint64	PortDataDefault = thePrefs.GetCumUpDataPort_4662();
						double	percentPortTransferred = !PortDataTotal ? 0 : 100.0 * PortDataDefault / PortDataTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), (LPCTSTR)CastItoXBytes(PortDataDefault), percentPortTransferred);
						m_stattree.SetItemText(up_tpb[i], cbuffer);
						++i;

						uint64	PortDataOther = thePrefs.GetCumUpDataPort_OTHER();
						percentPortTransferred = !PortDataTotal ? 0 : 100.0 * PortDataOther / PortDataTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataOther), percentPortTransferred);
						m_stattree.SetItemText(up_tpb[i], cbuffer);
						++i;

						uint64	PortDataPeerCache = thePrefs.GetCumUpDataPort_PeerCache();
						percentPortTransferred = !PortDataTotal ? 0 : 100.0 * PortDataPeerCache / PortDataTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), thePrefs.GetPeerCacheShow() ? _T("PeerCache") : (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataPeerCache), percentPortTransferred);
						m_stattree.SetItemText(up_tpb[i], cbuffer);
					}
					// Uploaded Data By Source
					if (forceUpdate || m_stattree.IsExpanded(hup_tsb)) {
						int i = 0;
						uint64	DataSourceTotal = thePrefs.GetUpTotalDataFile();

						uint64	DataSourceFile = thePrefs.GetCumUpData_File();
						double	percentFileTransferred = !DataSourceTotal ? 0 : 100.0 * DataSourceFile / DataSourceTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DSFILE), (LPCTSTR)CastItoXBytes(DataSourceFile), percentFileTransferred);
						m_stattree.SetItemText(up_tsb[i], cbuffer);
						++i;

						uint64	DataSourcePF = thePrefs.GetCumUpData_Partfile();
						percentFileTransferred = !DataSourceTotal ? 0 : 100.0 * DataSourcePF / DataSourceTotal;
						cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DSPF), (LPCTSTR)CastItoXBytes(DataSourcePF), percentFileTransferred);
						m_stattree.SetItemText(up_tsb[i], cbuffer);
					}
				}
				// Upload Sessions
				statGoodSessions = theApp.uploadqueue->GetSuccessfullUpCount() + thePrefs.GetUpSuccessfulSessions() + (uint32)theApp.uploadqueue->GetUploadQueueLength();
				statBadSessions = theApp.uploadqueue->GetFailedUpCount() + thePrefs.GetUpFailedSessions();
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_ULSES), statGoodSessions + statBadSessions);
				m_stattree.SetItemText(up_T[1], cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(up_T[1])) {
					// Set Successful Upload Sessions & Average Uploaded Per Session
					if (statGoodSessions > 0) { // Black holes are when God divided by 0
						percentSessions = 100.0*statGoodSessions / (statGoodSessions + statBadSessions);
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATAULSES), (LPCTSTR)CastItoXBytes((theStats.sessionSentBytes + thePrefs.GetTotalUploaded()) / statGoodSessions));
					} else {
						percentSessions = 0;
						cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_AVGDATAULSES), (LPCTSTR)GetResString(IDS_FSTAT_WAITING));
					}
					m_stattree.SetItemText(up_tsessions[2], cbuffer);
					cbuffer.Format(GetResString(IDS_STATS_SUCCUPCOUNT), statGoodSessions, percentSessions);
					m_stattree.SetItemText(up_tsessions[0], cbuffer);
					// Set Failed Upload Sessions
					if (percentSessions != 0 && statBadSessions > 0)
						percentSessions = 100 - percentSessions; // There were some good sessions and bad ones...
					else if (percentSessions == 0 && statBadSessions > 0)
						percentSessions = 100; // There were bad sessions and no good ones, must be 100%
					else
						percentSessions = 0; // No sessions at all, or no bad ones.
					cbuffer.Format(GetResString(IDS_STATS_FAILUPCOUNT), statBadSessions, percentSessions);
					m_stattree.SetItemText(up_tsessions[1], cbuffer);
					// Set Avg Upload time
					uint32 avguptime = theApp.uploadqueue->GetAverageUpTime();
					if (thePrefs.GetUpAvgTime() <= 0)
						thePrefs.SetUpAvgTime(avguptime);
					avguptime = (avguptime + thePrefs.GetUpAvgTime()) / 2;
					cbuffer.Format(GetResString(IDS_STATS_AVEUPTIME), (LPCTSTR)CastSecondsToLngHM(avguptime));
					m_stattree.SetItemText(up_tsessions[3], cbuffer);
				}

				if (UpOHTotal == 0 || UpOHTotalPackets == 0) {
					// Calculate Upline OH Totals
					UpOHTotal = theStats.GetUpDataOverheadFileRequest()
						+ theStats.GetUpDataOverheadSourceExchange()
						+ theStats.GetUpDataOverheadServer()
						+ theStats.GetUpDataOverheadKad()
						+ theStats.GetUpDataOverheadOther();
					UpOHTotalPackets = theStats.GetUpDataOverheadFileRequestPackets()
						+ theStats.GetUpDataOverheadSourceExchangePackets()
						+ theStats.GetUpDataOverheadServerPackets()
						+ theStats.GetUpDataOverheadKadPackets()
						+ theStats.GetUpDataOverheadOtherPackets();
				}
				// Set Cumulative Total Overhead
				cbuffer.Format(GetResString(IDS_TOVERHEAD), (LPCTSTR)CastItoXBytes(UpOHTotal + thePrefs.GetUpOverheadTotal()), (LPCTSTR)CastItoIShort(UpOHTotalPackets + thePrefs.GetUpOverheadTotalPackets()));
				m_stattree.SetItemText(hup_toh, cbuffer);
				if (forceUpdate || m_stattree.IsExpanded(hup_toh)) {
					int i = 0;
					// Set up total file req OH
					cbuffer.Format(GetResString(IDS_FROVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadFileRequest() + thePrefs.GetUpOverheadFileReq()), (LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadFileRequestPackets() + thePrefs.GetUpOverheadFileReqPackets()));
					m_stattree.SetItemText(up_toh[i], cbuffer);
					++i;
					// Set up total source exch OH
					cbuffer.Format(GetResString(IDS_SSOVERHEAD), (LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadSourceExchange() + thePrefs.GetUpOverheadSrcEx()), (LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadSourceExchangePackets() + thePrefs.GetUpOverheadSrcExPackets()));
					m_stattree.SetItemText(up_toh[i], cbuffer);
					++i;
					// Set up total server OH
					cbuffer.Format(GetResString(IDS_SOVERHEAD),
						(LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadServer()
							+ thePrefs.GetUpOverheadServer(), false, false),
							(LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadServerPackets()
								+ thePrefs.GetUpOverheadServerPackets()));
					m_stattree.SetItemText(up_toh[i], cbuffer);
					++i;
					// Set up total Kad OH
					cbuffer.Format(GetResString(IDS_KADOVERHEAD),
						(LPCTSTR)CastItoXBytes(theStats.GetUpDataOverheadKad() +
							thePrefs.GetUpOverheadKad(), false, false),
							(LPCTSTR)CastItoIShort(theStats.GetUpDataOverheadKadPackets() +
								thePrefs.GetUpOverheadKadPackets()));
					m_stattree.SetItemText(up_toh[i], cbuffer);
				}
			} // - End Transfer -> Uploads -> Cumulative Section
		} // - End Transfer -> Uploads Section
	} // - END TRANSFER SECTION


	// CONNECTION SECTION
	if (forceUpdate || m_stattree.IsExpanded(h_connection)) {
		// CONNECTION -> SESSION SECTION
		if (forceUpdate || m_stattree.IsExpanded(h_conn_session)) {
			// CONNECTION -> SESSION -> GENERAL SECTION
			if (forceUpdate || m_stattree.IsExpanded(hconn_sg)) {
				int i = 0;
				// Server Reconnects
				cbuffer.Format(GetResString(IDS_STATS_RECONNECTS), theStats.reconnects ? theStats.reconnects - 1 : 0);
				m_stattree.SetItemText(conn_sg[i], cbuffer);
				++i;
				// Active Connections
				cbuffer.Format(_T("%s: %u (%s:%u | %s:%u | %s:%u)"), (LPCTSTR)GetResString(IDS_SF_ACTIVECON), theApp.listensocket->GetActiveConnections(), (LPCTSTR)GetResString(IDS_HALF), theApp.listensocket->GetTotalHalfCon(), (LPCTSTR)GetResString(IDS_CONCOMPL), theApp.listensocket->GetTotalComp(), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), theApp.listensocket->GetActiveConnections() - theApp.listensocket->GetTotalHalfCon() - theApp.listensocket->GetTotalComp());
				m_stattree.SetItemText(conn_sg[i], cbuffer);
				++i;
				// Average Connections
				cbuffer.Format(_T("%s: %i"), (LPCTSTR)GetResString(IDS_SF_AVGCON), (int)theApp.listensocket->GetAverageConnections());
				m_stattree.SetItemText(conn_sg[i], cbuffer);
				++i;
				// Peak Connections
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_SF_PEAKCON), theApp.listensocket->GetPeakConnections());
				m_stattree.SetItemText(conn_sg[i], cbuffer);
				++i;
				// Connect Limit Reached
				uint32 m_itemp = theApp.listensocket->GetMaxConnectionReached();
				if (m_itemp != m_ilastMaxConnReached) {
					cbuffer.Format(_T("%s: %u : %s"), (LPCTSTR)GetResString(IDS_SF_MAXCONLIMITREACHED), m_itemp, (LPCTSTR)CTime::GetCurrentTime().Format(_T("%c")));
					m_stattree.SetItemText(conn_sg[i], cbuffer);
					m_ilastMaxConnReached = m_itemp;
				} else if (m_itemp == 0) {
					cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_SF_MAXCONLIMITREACHED), m_itemp);
					m_stattree.SetItemText(conn_sg[i], cbuffer);
				}
			} // - End Connection -> Session -> General Section
			// CONNECTION -> SESSION -> UPLOADS SECTION
			if (forceUpdate || m_stattree.IsExpanded(hconn_su)) {
				int i = 0;
				// Upload Rate
				cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_ST_UPLOAD), (LPCTSTR)CastItoXBytes(theStats.rateUp, true, true));
				m_stattree.SetItemText(conn_su[i], cbuffer);
				++i;
				// Average Upload Rate
				cbuffer.Format(GetResString(IDS_STATS_AVGUL), (LPCTSTR)CastItoXBytes(theStats.GetAvgUploadRate(AVG_SESSION), true, true));
				m_stattree.SetItemText(conn_su[i], cbuffer);
				++i;
				// Max Upload Rate
				cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_MAXUL), (LPCTSTR)CastItoXBytes(theStats.maxUp, true, true));
				m_stattree.SetItemText(conn_su[i], cbuffer);
				++i;
				// Max Average Upload Rate
				float myAverageUpRate = theStats.GetAvgUploadRate(AVG_SESSION);
				if (myAverageUpRate > theStats.maxUpavg)
					theStats.maxUpavg = myAverageUpRate;
				cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_MAXAVGUL), (LPCTSTR)CastItoXBytes(theStats.maxUpavg, true, true));
				m_stattree.SetItemText(conn_su[i], cbuffer);
			} // - End Connection -> Session -> Uploads Section
			// CONNECTION -> SESSION -> DOWNLOADShead SECTION
			if (forceUpdate || m_stattree.IsExpanded(hconn_sd)) {
				int i = 0;
				// Download Rate
				cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_ST_DOWNLOAD), (LPCTSTR)CastItoXBytes(theStats.rateDown, true, true));
				m_stattree.SetItemText(conn_sd[i], cbuffer);
				++i;
				// Average Download Rate
				cbuffer.Format(GetResString(IDS_STATS_AVGDL), (LPCTSTR)CastItoXBytes(theStats.GetAvgDownloadRate(AVG_SESSION), true, true));
				m_stattree.SetItemText(conn_sd[i], cbuffer);
				++i;
				// Max Download Rate
				cbuffer.Format(GetResString(IDS_STATS_MAXDL), (LPCTSTR)CastItoXBytes(theStats.maxDown, true, true));
				m_stattree.SetItemText(conn_sd[i], cbuffer);
				++i;
				// Max Average Download Rate
				float myAverageDownRate = theStats.GetAvgDownloadRate(AVG_SESSION);
				if (myAverageDownRate > theStats.maxDownavg)
					theStats.maxDownavg = myAverageDownRate;
				cbuffer.Format(GetResString(IDS_STATS_MAXAVGDL), (LPCTSTR)CastItoXBytes(theStats.maxDownavg, true, true));
				m_stattree.SetItemText(conn_sd[i], cbuffer);
			} // - End Connection -> Session -> Downloads Section
		} // - End Connection -> Session Section
		// CONNECTION -> CUMULATIVE SECTION
		if (forceUpdate || m_stattree.IsExpanded(h_conn_total)) {
			// CONNECTION -> CUMULATIVE -> GENERAL SECTION
			if (forceUpdate || m_stattree.IsExpanded(hconn_tg)) {
				int i = 0;
				// Server Reconnects
				cbuffer.Format(GetResString(IDS_STATS_RECONNECTS), thePrefs.GetConnNumReconnects() + theStats.reconnects - static_cast<uint32>(theStats.reconnects > 0));
				m_stattree.SetItemText(conn_tg[i], cbuffer);
				++i;
				// Average Connections
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_SF_AVGCON), (theApp.listensocket->GetActiveConnections() + thePrefs.GetConnAvgConnections()) / 2);
				m_stattree.SetItemText(conn_tg[i], cbuffer);
				++i;
				// Peak Connections
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_SF_PEAKCON), thePrefs.GetConnPeakConnections());
				m_stattree.SetItemText(conn_tg[i], cbuffer);
				++i;
				// Connection Limit Reached
				cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_SF_MAXCONLIMITREACHED), theApp.listensocket->GetMaxConnectionReached() + thePrefs.GetConnMaxConnLimitReached());
				m_stattree.SetItemText(conn_tg[i], cbuffer);
			} // - End Connection -> Cumulative -> General Section
			// CONNECTION -> CUMULATIVE -> UPLOADS SECTION
			if (forceUpdate || m_stattree.IsExpanded(hconn_tu)) {
				int i = 0;
				// Average Upload Rate
				cbuffer.Format(GetResString(IDS_STATS_AVGUL), (LPCTSTR)CastItoXBytes(theStats.cumUpavg, true, true));
				m_stattree.SetItemText(conn_tu[i], cbuffer);
				++i;
				// Max Upload Rate
				cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_MAXUL), (LPCTSTR)CastItoXBytes(theStats.maxcumUp, true, true));
				m_stattree.SetItemText(conn_tu[i], cbuffer);
				++i;
				// Max Average Upload Rate
				cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_MAXAVGUL), (LPCTSTR)CastItoXBytes(theStats.maxcumUpavg, true, true));
				m_stattree.SetItemText(conn_tu[i], cbuffer);
			} // - End Connection -> Cumulative -> Uploads Section
			// CONNECTION -> CUMULATIVE -> DOWNLOADS SECTION
			if (forceUpdate || m_stattree.IsExpanded(hconn_td)) {
				int i = 0;
				// Average Download Rate
				cbuffer.Format(GetResString(IDS_STATS_AVGDL), (LPCTSTR)CastItoXBytes(theStats.cumDownavg, true, true));
				m_stattree.SetItemText(conn_td[i], cbuffer);
				++i;
				// Max Download Rate
				cbuffer.Format(GetResString(IDS_STATS_MAXDL), (LPCTSTR)CastItoXBytes(theStats.maxcumDown, true, true));
				m_stattree.SetItemText(conn_td[i], cbuffer);
				++i;
				// Max Average Download Rate
				cbuffer.Format(GetResString(IDS_STATS_MAXAVGDL), (LPCTSTR)CastItoXBytes(theStats.maxcumDownavg, true, true));
				m_stattree.SetItemText(conn_td[i], cbuffer);
			} // - End Connection -> Cumulative -> Downloads Section
		} // - End Connection -> Cumulative Section
	} // - END CONNECTION SECTION


	// TIME STATISTICS SECTION
	if (forceUpdate || m_stattree.IsExpanded(h_time)) {
		// Statistics Last Reset
		cbuffer.Format(GetResString(IDS_STATS_LASTRESETSTATIC), (LPCTSTR)thePrefs.GetStatsLastResetStr(false));
		m_stattree.SetItemText(tvitime[0], cbuffer);
		// Time Since Last Reset
		time_t timeDiff;
		if (thePrefs.GetStatsLastResetLng()) {
			time_t timeNow;
			time(&timeNow);
			timeDiff = timeNow - thePrefs.GetStatsLastResetLng(); // In seconds
			cbuffer.Format(GetResString(IDS_STATS_TIMESINCERESET), (LPCTSTR)CastSecondsToLngHM(timeDiff));
		} else {
			timeDiff = 0;
			cbuffer.Format(GetResString(IDS_STATS_TIMESINCERESET), (LPCTSTR)GetResString(IDS_UNKNOWN));
		}
		m_stattree.SetItemText(tvitime[1], cbuffer);
		// TIME STATISTICS -> SESSION SECTION
		if (forceUpdate || m_stattree.IsExpanded(htime_s)) {
			int i = 0;
			// Run Time
			time_t sessionRunTime = (::GetTickCount() - theStats.starttime) / SEC2MS(1);
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_RUNTIME), (LPCTSTR)CastSecondsToLngHM(sessionRunTime));
			m_stattree.SetItemText(tvitime_s[i], cbuffer);
			++i;
			if (!sessionRunTime)
				sessionRunTime = 1;
			// Transfer Time
			cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_TRANSTIME), (LPCTSTR)CastSecondsToLngHM(theStats.GetTransferTime()), (100.0 * theStats.GetTransferTime()) / sessionRunTime);
			m_stattree.SetItemText(tvitime_s[i], cbuffer);
			if (forceUpdate || m_stattree.IsExpanded(tvitime_s[i])) {
				int x = 0;
				// Upload Time
				cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_UPTIME), (LPCTSTR)CastSecondsToLngHM(theStats.GetUploadTime()), (100.0 * theStats.GetUploadTime()) / sessionRunTime);
				m_stattree.SetItemText(tvitime_st[x], cbuffer);
				++x;
				// Download Time
				cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DOWNTIME), (LPCTSTR)CastSecondsToLngHM(theStats.GetDownloadTime()), (100.0 * theStats.GetDownloadTime()) / sessionRunTime);
				m_stattree.SetItemText(tvitime_st[x], cbuffer);
			}
			++i;
			// Current Server Duration
			cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_CURRSRVDUR), (LPCTSTR)CastSecondsToLngHM(theStats.time_thisServerDuration), (100.0 * theStats.time_thisServerDuration) / sessionRunTime);
			m_stattree.SetItemText(tvitime_s[i], cbuffer);
			++i;
			// Total Server Duration
			cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_TOTALSRVDUR), (LPCTSTR)CastSecondsToLngHM(theStats.GetServerDuration()), (100.0 * theStats.GetServerDuration()) / sessionRunTime);
			m_stattree.SetItemText(tvitime_s[i], cbuffer);
		}
		// TIME STATISTICS -> CUMULATIVE SECTION
		if (forceUpdate || m_stattree.IsExpanded(htime_t)) {
			int i = 0;
			// Run Time
			time_t totalRunTime = (::GetTickCount() - theStats.starttime) / SEC2MS(1) + thePrefs.GetConnRunTime();
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_RUNTIME), (LPCTSTR)CastSecondsToLngHM(totalRunTime));
			m_stattree.SetItemText(tvitime_t[i], cbuffer);
			++i;
			if (!totalRunTime)
				totalRunTime = 1;
			// Transfer Time
			cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_TRANSTIME), (LPCTSTR)CastSecondsToLngHM(theStats.GetTransferTime() + thePrefs.GetConnTransferTime()), (100.0 * (theStats.GetTransferTime() + thePrefs.GetConnTransferTime())) / totalRunTime);
			m_stattree.SetItemText(tvitime_t[i], cbuffer);
			if (forceUpdate || m_stattree.IsExpanded(tvitime_t[i])) {
				int x = 0;
				// Upload Time
				cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_UPTIME), (LPCTSTR)CastSecondsToLngHM(theStats.GetUploadTime() + thePrefs.GetConnUploadTime()), (100.0 * (theStats.GetUploadTime() + thePrefs.GetConnUploadTime())) / totalRunTime);
				m_stattree.SetItemText(tvitime_tt[x], cbuffer);
				++x;
				// Download Time
				cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DOWNTIME), (LPCTSTR)CastSecondsToLngHM(theStats.GetDownloadTime() + thePrefs.GetConnDownloadTime()), (100.0 * (theStats.GetDownloadTime() + thePrefs.GetConnDownloadTime())) / totalRunTime);
				m_stattree.SetItemText(tvitime_tt[x], cbuffer);
			}
			++i;
			// Total Server Duration
			cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_TOTALSRVDUR), (LPCTSTR)CastSecondsToLngHM(theStats.GetServerDuration() + thePrefs.GetConnServerDuration()), (100.0 * (theStats.GetServerDuration() + thePrefs.GetConnServerDuration())) / totalRunTime);
			m_stattree.SetItemText(tvitime_t[i], cbuffer);
		}
		// TIME STATISTICS -> PROJECTED AVERAGES SECTION
		if ((forceUpdate || m_stattree.IsExpanded(htime_aap)) && timeDiff > 0) {
			double avgModifier[3];
			avgModifier[0] = (double)DAY2S(1) / timeDiff; // Days
			avgModifier[1] = (double)2629746 / timeDiff; // Months - 1/12 of Gregorian year
			avgModifier[2] = (double)31556952 / timeDiff; // Years - Gregorian year in seconds
			// TIME STATISTICS -> PROJECTED AVERAGES -> TIME PERIODS
			// This section is completely scalable.  Might add "Week" to it in the future.
			// For each time period that we are calculating a projected average for...
			for (int mx = 0; mx < 3; ++mx) {
				if (forceUpdate || m_stattree.IsExpanded(time_aaph[mx])) {
					// TIME STATISTICS -> PROJECTED AVERAGES -> TIME PERIOD -> UPLOADS SECTION
					if (forceUpdate || m_stattree.IsExpanded(time_aap_hup[mx])) {
						// Uploaded Data
						cbuffer.Format(GetResString(IDS_STATS_UDATA), (LPCTSTR)CastItoXBytes(((double)(theStats.sessionSentBytes + thePrefs.GetTotalUploaded()))*avgModifier[mx]));
						m_stattree.SetItemText(time_aap_up[mx][0], cbuffer);
						if (forceUpdate || m_stattree.IsExpanded(time_aap_up[mx][0])) {
							// Uploaded Data By Client
							if (forceUpdate || m_stattree.IsExpanded(time_aap_up_hd[mx][0])) {
								int i = 0;
								uint64 UpDataTotal = (uint64)(thePrefs.GetUpTotalClientData() * avgModifier[mx]);
								uint64 UpDataClient = (uint64)(thePrefs.GetCumUpData_EMULE() * avgModifier[mx]);
								double percentClientTransferred;
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("eMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
								++i;

								UpDataClient = (uint64)(thePrefs.GetCumUpData_EDONKEYHYBRID() * avgModifier[mx]);
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("eD Hybrid: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
								++i;

								UpDataClient = (uint64)(thePrefs.GetCumUpData_EDONKEY() * avgModifier[mx]);
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("eDonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
								++i;

								UpDataClient = (uint64)(thePrefs.GetCumUpData_AMULE() * avgModifier[mx]);
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("aMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
								++i;

								UpDataClient = (uint64)(thePrefs.GetCumUpData_MLDONKEY() * avgModifier[mx]);
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("MLdonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
								++i;

								UpDataClient = (uint64)(thePrefs.GetCumUpData_SHAREAZA() * avgModifier[mx]);
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("Shareaza: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
								++i;

								UpDataClient = (uint64)(thePrefs.GetCumUpData_EMULECOMPAT() * avgModifier[mx]);
								percentClientTransferred = !UpDataTotal ? 0 : 100.0 * UpDataClient / UpDataTotal;
								cbuffer.Format(_T("eM Compat: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(UpDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_up_dc[mx][i], cbuffer);
							}
							// Uploaded Data By Port
							if (forceUpdate || m_stattree.IsExpanded(time_aap_up_hd[mx][1])) {
								int i = 0;
								uint64	PortDataDefault = (uint64)(thePrefs.GetCumUpDataPort_4662() * avgModifier[mx]);
								uint64	PortDataOther = (uint64)(thePrefs.GetCumUpDataPort_OTHER() * avgModifier[mx]);
								uint64	PortDataPeerCache = (uint64)(thePrefs.GetCumUpDataPort_PeerCache() * avgModifier[mx]);
								uint64	PortDataTotal = (uint64)(thePrefs.GetUpTotalPortData() * avgModifier[mx]);
								double	percentPortTransferred = 0;

								if (PortDataTotal != 0 && PortDataDefault != 0)
									percentPortTransferred = 100.0 * PortDataDefault / PortDataTotal;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), (LPCTSTR)CastItoXBytes(PortDataDefault), percentPortTransferred);
								m_stattree.SetItemText(time_aap_up_dp[mx][i], cbuffer);
								++i;

								if (PortDataTotal != 0 && PortDataOther != 0)
									percentPortTransferred = 100.0 * PortDataOther / PortDataTotal;
								else
									percentPortTransferred = 0;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataOther), percentPortTransferred);
								m_stattree.SetItemText(time_aap_up_dp[mx][i], cbuffer);
								++i;

								if (PortDataTotal != 0 && PortDataPeerCache != 0)
									percentPortTransferred = 100.0 * PortDataPeerCache / PortDataTotal;
								else
									percentPortTransferred = 0;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), thePrefs.GetPeerCacheShow() ? _T("PeerCache") : (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataPeerCache), percentPortTransferred);
								m_stattree.SetItemText(time_aap_up_dp[mx][i], cbuffer);
							}
							// Uploaded Data By Source
							if (forceUpdate || m_stattree.IsExpanded(time_aap_up_hd[mx][2])) {
								int i = 0;
								uint64	DataSourceTotal = (uint64)(thePrefs.GetUpTotalDataFile() * avgModifier[mx]);

								uint64	DataSourceFile = (uint64)(thePrefs.GetCumUpData_File() * avgModifier[mx]);
								double	percentFileTransferred = !DataSourceTotal ? 0 : 100.0 * DataSourceFile / DataSourceTotal;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DSFILE), (LPCTSTR)CastItoXBytes(DataSourceFile), percentFileTransferred);
								m_stattree.SetItemText(time_aap_up_ds[mx][i], cbuffer);
								++i;

								uint64	DataSourcePF = (uint64)(thePrefs.GetCumUpData_Partfile() * avgModifier[mx]);
								percentFileTransferred = !DataSourceTotal ? 0 : 100.0 * DataSourcePF / DataSourceTotal;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_DSPF), (LPCTSTR)CastItoXBytes(DataSourcePF), percentFileTransferred);
								m_stattree.SetItemText(time_aap_up_ds[mx][i], cbuffer);
							}
						}
						// Upload Sessions
						uint32 statGoodSessions = (uint32)((theApp.uploadqueue->GetSuccessfullUpCount() + thePrefs.GetUpSuccessfulSessions() + theApp.uploadqueue->GetUploadQueueLength()) * avgModifier[mx]);
						uint32 statBadSessions = (uint32)((theApp.uploadqueue->GetFailedUpCount() + thePrefs.GetUpFailedSessions()) * avgModifier[mx]);
						cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_ULSES), statGoodSessions + statBadSessions);
						m_stattree.SetItemText(time_aap_up[mx][1], cbuffer);
						if (forceUpdate || m_stattree.IsExpanded(time_aap_up[mx][1])) {
							double percentSessions;
							// Set Successful Upload Sessions
							if (statGoodSessions > 0)
								percentSessions = (100.0 * statGoodSessions) / (statGoodSessions + statBadSessions);
							else
								percentSessions = 0;
							cbuffer.Format(GetResString(IDS_STATS_SUCCUPCOUNT), statGoodSessions, percentSessions);
							m_stattree.SetItemText(time_aap_up_s[mx][0], cbuffer);
							// Set Failed Upload Sessions
							if (percentSessions != 0 && statBadSessions > 0)
								percentSessions = 100 - percentSessions; // There were some good sessions and bad ones...
							else if (percentSessions == 0 && statBadSessions > 0)
								percentSessions = 100; // There were bad sessions and no good ones, must be 100%
							else
								percentSessions = 0; // No sessions at all, or no bad ones.
							cbuffer.Format(GetResString(IDS_STATS_FAILUPCOUNT), statBadSessions, percentSessions);
							m_stattree.SetItemText(time_aap_up_s[mx][1], cbuffer);
						}

						// Calculate Upline OH Totals
						uint64 UpOHTotal = (uint64)((theStats.GetUpDataOverheadFileRequest()
							+ theStats.GetUpDataOverheadSourceExchange()
							+ theStats.GetUpDataOverheadServer()
							+ theStats.GetUpDataOverheadKad()
							+ theStats.GetUpDataOverheadOther()
							) * avgModifier[mx]);
						uint64 UpOHTotalPackets = (uint64)((theStats.GetUpDataOverheadFileRequestPackets()
							+ theStats.GetUpDataOverheadSourceExchangePackets()
							+ theStats.GetUpDataOverheadServerPackets()
							+ theStats.GetUpDataOverheadKadPackets()
							+ theStats.GetUpDataOverheadOtherPackets()
							) * avgModifier[mx]);

						// Set Cumulative Total Overhead
						cbuffer.Format(GetResString(IDS_TOVERHEAD), (LPCTSTR)CastItoXBytes(UpOHTotal + ((uint64)thePrefs.GetUpOverheadTotal() * avgModifier[mx])), (LPCTSTR)CastItoIShort((uint64)(UpOHTotalPackets + ((uint64)thePrefs.GetUpOverheadTotalPackets() * avgModifier[mx]))));
						m_stattree.SetItemText(time_aap_up[mx][2], cbuffer);
						if (forceUpdate || m_stattree.IsExpanded(time_aap_up[mx][2])) {
							int i = 0;
							// Set up total file req OH
							cbuffer.Format(GetResString(IDS_FROVERHEAD), (LPCTSTR)CastItoXBytes((uint64)(theStats.GetUpDataOverheadFileRequest() + thePrefs.GetUpOverheadFileReq()) * avgModifier[mx]), (LPCTSTR)CastItoIShort((uint64)(theStats.GetUpDataOverheadFileRequestPackets() + thePrefs.GetUpOverheadFileReqPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_up_oh[mx][i], cbuffer);
							++i;
							// Set up total source exch OH
							cbuffer.Format(GetResString(IDS_SSOVERHEAD), (LPCTSTR)CastItoXBytes((uint64)(theStats.GetUpDataOverheadSourceExchange() + thePrefs.GetUpOverheadSrcEx()) * avgModifier[mx]), (LPCTSTR)CastItoIShort((uint64)(theStats.GetUpDataOverheadSourceExchangePackets() + thePrefs.GetUpOverheadSrcExPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_up_oh[mx][i], cbuffer);
							++i;
							// Set up total server OH
							cbuffer.Format(GetResString(IDS_SOVERHEAD)
								, (LPCTSTR)CastItoXBytes((theStats.GetUpDataOverheadServer()
									+ thePrefs.GetUpOverheadServer()) * avgModifier[mx])
								, (LPCTSTR)CastItoIShort((uint64)(theStats.GetUpDataOverheadServerPackets()
									+ thePrefs.GetUpOverheadServerPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_up_oh[mx][i], cbuffer);
							++i;
							// Set up total Kad OH
							cbuffer.Format(GetResString(IDS_KADOVERHEAD)
								, (LPCTSTR)CastItoXBytes((uint64)(theStats.GetUpDataOverheadKad()
									+ thePrefs.GetUpOverheadKad()) * avgModifier[mx])
								, (LPCTSTR)CastItoIShort((uint64)(theStats.GetUpDataOverheadKadPackets()
									+ thePrefs.GetUpOverheadKadPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_up_oh[mx][i], cbuffer);
						}
					} // - End Time Statistics -> Projected Averages -> Time Period -> Uploads Section
					// TIME STATISTICS -> PROJECTED AVERAGES -> TIME PERIOD -> DOWNLOADS SECTION
					if (forceUpdate || m_stattree.IsExpanded(time_aap_hdown[mx])) {
						CDownloadQueue::SDownloadStats myStats;
						theApp.downloadqueue->GetDownloadSourcesStats(myStats);
						// Downloaded Data
						cbuffer.Format(GetResString(IDS_STATS_DDATA), (LPCTSTR)CastItoXBytes((uint64)(theStats.sessionReceivedBytes + thePrefs.GetTotalDownloaded()) * avgModifier[mx]));
						m_stattree.SetItemText(time_aap_down[mx][0], cbuffer);
						if (forceUpdate || m_stattree.IsExpanded(time_aap_down[mx][0])) {
							// Downloaded Data By Client
							if (forceUpdate || m_stattree.IsExpanded(time_aap_down_hd[mx][0])) {
								int i = 0;
								uint64 DownDataTotal = (uint64)(thePrefs.GetDownTotalClientData() * avgModifier[mx]);
								uint64 DownDataClient = (uint64)(thePrefs.GetCumDownData_EMULE() * avgModifier[mx]);
								double percentClientTransferred;
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("eMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_EDONKEYHYBRID() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("eD Hybrid: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_EDONKEY() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("eDonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_AMULE() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("aMule: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_MLDONKEY() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("MLdonkey: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_SHAREAZA() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("Shareaza: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_EMULECOMPAT() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("eM Compat: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
								++i;

								DownDataClient = (uint64)(thePrefs.GetCumDownData_URL() * avgModifier[mx]);
								if (DownDataTotal != 0 && DownDataClient != 0)
									percentClientTransferred = 100.0 * DownDataClient / DownDataTotal;
								else
									percentClientTransferred = 0;
								cbuffer.Format(_T("URL: %s (%1.1f%%)"), (LPCTSTR)CastItoXBytes(DownDataClient), percentClientTransferred);
								m_stattree.SetItemText(time_aap_down_dc[mx][i], cbuffer);
							}
							// Downloaded Data By Port
							if (forceUpdate || m_stattree.IsExpanded(time_aap_down_hd[mx][1])) {
								int i = 0;
								uint64	PortDataDefault = (uint64)(thePrefs.GetCumDownDataPort_4662() * avgModifier[mx]);
								uint64	PortDataOther = (uint64)(thePrefs.GetCumDownDataPort_OTHER() * avgModifier[mx]);
								uint64	PortDataPeerCache = (uint64)(thePrefs.GetCumDownDataPort_PeerCache() * avgModifier[mx]);
								uint64	PortDataTotal = (uint64)(thePrefs.GetDownTotalPortData() * avgModifier[mx]);
								double	percentPortTransferred;

								if (PortDataTotal != 0 && PortDataDefault != 0)
									percentPortTransferred = 100.0 * PortDataDefault / PortDataTotal;
								else
									percentPortTransferred = 0;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), (LPCTSTR)CastItoXBytes(PortDataDefault), percentPortTransferred);
								m_stattree.SetItemText(time_aap_down_dp[mx][i], cbuffer);
								++i;

								if (PortDataTotal != 0 && PortDataOther != 0)
									percentPortTransferred = 100.0 * PortDataOther / PortDataTotal;
								else
									percentPortTransferred = 0;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataOther), percentPortTransferred);
								m_stattree.SetItemText(time_aap_down_dp[mx][i], cbuffer);
								++i;

								if (PortDataTotal != 0 && PortDataPeerCache != 0)
									percentPortTransferred = 100.0 * PortDataPeerCache / PortDataTotal;
								else
									percentPortTransferred = 0;
								cbuffer.Format(_T("%s: %s (%1.1f%%)"), thePrefs.GetPeerCacheShow() ? _T("PeerCache") : (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), (LPCTSTR)CastItoXBytes(PortDataPeerCache), percentPortTransferred);
								m_stattree.SetItemText(time_aap_down_dp[mx][i], cbuffer);
							}
						}
						// Set Cum Completed Downloads
						cbuffer.Format(_T("%s: %I64u"), (LPCTSTR)GetResString(IDS_STATS_COMPDL), (uint64)(thePrefs.GetDownCompletedFiles() * avgModifier[mx]));
						m_stattree.SetItemText(time_aap_down[mx][1], cbuffer);
						// Set Cum Download Sessions
						uint32	statGoodSessions = (uint32)((thePrefs.GetDownC_SuccessfulSessions() + myStats.a[1]) * avgModifier[mx]);
						uint32	statBadSessions = (uint32)(thePrefs.GetDownC_FailedSessions() * avgModifier[mx]);
						cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_DLSES), statGoodSessions + statBadSessions);
						m_stattree.SetItemText(time_aap_down[mx][2], cbuffer);
						if (forceUpdate || m_stattree.IsExpanded(time_aap_down[mx][2])) {
							double	percentSessions;
							// Set Cum Successful Download Sessions
							if (statGoodSessions > 0)
								percentSessions = 100.0 * statGoodSessions / (statGoodSessions + statBadSessions);
							else
								percentSessions = 0;
							cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_SDLSES), statGoodSessions, percentSessions);
							m_stattree.SetItemText(time_aap_down_s[mx][0], cbuffer); // Set Successful Sessions
							// Set Cum Failed Download Sessions
							if (percentSessions != 0 && statBadSessions > 0)
								percentSessions = 100 - percentSessions; // There were some good sessions and bad ones...
							else if (percentSessions == 0 && statBadSessions > 0)
								percentSessions = 100; // There were bad sessions and no good ones, must be 100%
							else
								percentSessions = 0; // No sessions at all, or no bad ones.
							cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_FDLSES), statBadSessions, percentSessions);
							m_stattree.SetItemText(time_aap_down_s[mx][1], cbuffer);
						}
						// Set Cumulative Gained Due To Compression
						cbuffer.Format(GetResString(IDS_STATS_GAINCOMP), (LPCTSTR)CastItoXBytes((uint64)(thePrefs.GetSesSavedFromCompression() + thePrefs.GetCumSavedFromCompression()) * avgModifier[mx]));
						m_stattree.SetItemText(time_aap_down[mx][3], cbuffer);
						// Set Cumulative Lost Due To Corruption
						cbuffer.Format(GetResString(IDS_STATS_LOSTCORRUPT), (LPCTSTR)CastItoXBytes((uint64)(thePrefs.GetSesLostFromCorruption() + thePrefs.GetCumLostFromCorruption()) * avgModifier[mx]));
						m_stattree.SetItemText(time_aap_down[mx][4], cbuffer);
						// Set Cumulative Saved Due To ICH
						cbuffer.Format(GetResString(IDS_STATS_ICHSAVED), (uint32)((thePrefs.GetSesPartsSavedByICH() + thePrefs.GetCumPartsSavedByICH()) * avgModifier[mx]));
						m_stattree.SetItemText(time_aap_down[mx][5], cbuffer);

						uint64 DownOHTotal = theStats.GetDownDataOverheadFileRequest()
							+ theStats.GetDownDataOverheadSourceExchange()
							+ theStats.GetDownDataOverheadServer()
							+ theStats.GetDownDataOverheadKad()
							+ theStats.GetDownDataOverheadOther();
						uint64 DownOHTotalPackets = theStats.GetDownDataOverheadFileRequestPackets()
							+ theStats.GetDownDataOverheadSourceExchangePackets()
							+ theStats.GetDownDataOverheadServerPackets()
							+ theStats.GetDownDataOverheadKadPackets()
							+ theStats.GetDownDataOverheadOtherPackets();
						// Total Overhead
						cbuffer.Format(GetResString(IDS_TOVERHEAD), (LPCTSTR)CastItoXBytes((uint64)(DownOHTotal + thePrefs.GetDownOverheadTotal()) * avgModifier[mx]), (LPCTSTR)CastItoIShort((uint64)(DownOHTotalPackets + thePrefs.GetDownOverheadTotalPackets()) * avgModifier[mx]));
						m_stattree.SetItemText(time_aap_down[mx][6], cbuffer);
						if (forceUpdate || m_stattree.IsExpanded(time_aap_down[mx][6])) {
							int i = 0;
							// File Request Overhead
							cbuffer.Format(GetResString(IDS_FROVERHEAD), (LPCTSTR)CastItoXBytes((uint64)(theStats.GetDownDataOverheadFileRequest() + thePrefs.GetDownOverheadFileReq()) * avgModifier[mx]), (LPCTSTR)CastItoIShort((uint64)(theStats.GetDownDataOverheadFileRequestPackets() + thePrefs.GetDownOverheadFileReqPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_down_oh[mx][i], cbuffer);
							++i;
							// Source Exchange Overhead
							cbuffer.Format(GetResString(IDS_SSOVERHEAD), (LPCTSTR)CastItoXBytes((uint64)(theStats.GetDownDataOverheadSourceExchange() + thePrefs.GetDownOverheadSrcEx()) * avgModifier[mx]), (LPCTSTR)CastItoIShort((uint64)(theStats.GetDownDataOverheadSourceExchangePackets() + thePrefs.GetDownOverheadSrcExPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_down_oh[mx][i], cbuffer);
							++i;
							// Server Overhead
							cbuffer.Format(GetResString(IDS_SOVERHEAD)
								, (LPCTSTR)CastItoXBytes((uint64)(theStats.GetDownDataOverheadServer()
									+ thePrefs.GetDownOverheadServer()) * avgModifier[mx])
								, (LPCTSTR)CastItoIShort((uint64)(theStats.GetDownDataOverheadServerPackets() +
									+ thePrefs.GetDownOverheadServerPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_down_oh[mx][i], cbuffer);
							++i;
							// Kad Overhead
							cbuffer.Format(GetResString(IDS_KADOVERHEAD)
								, (LPCTSTR)CastItoXBytes((uint64)(theStats.GetDownDataOverheadKad()
									+ thePrefs.GetDownOverheadKad()) * avgModifier[mx])
								, (LPCTSTR)CastItoIShort((uint64)(theStats.GetDownDataOverheadKadPackets()
									+ thePrefs.GetDownOverheadKadPackets()) * avgModifier[mx]));
							m_stattree.SetItemText(time_aap_down_oh[mx][i], cbuffer);
						}
					} // - End Time Statistics -> Projected Averages -> Time Period -> Downloads Section
				} // - End Time Statistics -> Projected Averages -> Time Period Sections
			} // - End Time Statistics -> Projected Averages Section
		} // - End Time Statistics -> Projected Averages Section Loop
	} // - END TIME STATISTICS SECTION


	// CLIENTS SECTION
	//		Note:	This section now has dynamic tree items.  This technique
	//				may appear in other areas, however, there is usually an
	//				advantage to displaying 0 data items.  Here, with the versions
	//				being displayed the way they are, it makes sense.
	//				Who wants to stare at totally blank tree items?  ;)
	if (forceUpdate || m_stattree.IsExpanded(h_clients)) {
		CMap<uint32, uint32, uint32, uint32>	clientVersionEDonkey;
		CMap<uint32, uint32, uint32, uint32>	clientVersionEDonkeyHybrid;
		CMap<uint32, uint32, uint32, uint32>	clientVersionEMule;
		CMap<uint32, uint32, uint32, uint32>	clientVersionAMule;
		uint32									totalclient;
		int										myStats[NUM_CLIENTLIST_STATS];

		theApp.clientlist->GetStatistics(totalclient
			, myStats
			, clientVersionEDonkey
			, clientVersionEDonkeyHybrid
			, clientVersionEMule
			, clientVersionAMule);

		cbuffer.Format(_T("%s: %u "), (LPCTSTR)GetResString(IDS_CLIENTLIST), totalclient);
		m_stattree.SetItemText(cligen[5], cbuffer);

		int SIclients = myStats[12] + myStats[13];
		cbuffer.Format(_T("%s: %i (%.1f%%) : %i (%.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_SECUREIDENT), myStats[12], (SIclients > 0) ? (100.0 * myStats[12] / SIclients) : 0, myStats[13], (SIclients > 0) ? (100.0 * myStats[13] / SIclients) : 0.0);
		m_stattree.SetItemText(cligen[3], cbuffer);

		double perc = totalclient > 0 ? 100.0 / totalclient : 0.0;
		cbuffer.Format(_T("%s: %i (%.1f%%)"), (LPCTSTR)GetResString(IDS_IDLOW), myStats[14], perc*myStats[14]);
		m_stattree.SetItemText(cligen[4], cbuffer);

		// CLIENTS -> CLIENT SOFTWARE SECTION
		if (forceUpdate || m_stattree.IsExpanded(hclisoft)) {
			cbuffer.Format(_T("eMule: %i (%1.1f%%)"), myStats[2], perc*myStats[2]);
			m_stattree.SetItemText(clisoft[0], cbuffer);
			cbuffer.Format(_T("eD Hybrid: %i (%1.1f%%)"), myStats[4], perc*myStats[4]);
			m_stattree.SetItemText(clisoft[1], cbuffer);
			cbuffer.Format(_T("eDonkey: %i (%1.1f%%)"), myStats[1], perc*myStats[1]);
			m_stattree.SetItemText(clisoft[2], cbuffer);
			cbuffer.Format(_T("aMule: %i (%1.1f%%)"), myStats[10], perc*myStats[10]);
			m_stattree.SetItemText(clisoft[3], cbuffer);
			cbuffer.Format(_T("MLdonkey: %i (%1.1f%%)"), myStats[3], perc*myStats[3]);
			m_stattree.SetItemText(clisoft[4], cbuffer);
			cbuffer.Format(_T("Shareaza: %i (%1.1f%%)"), myStats[11], perc*myStats[11]);
			m_stattree.SetItemText(clisoft[5], cbuffer);
			cbuffer.Format(_T("eM Compat: %i (%1.1f%%)"), myStats[5], perc*myStats[5]);
			m_stattree.SetItemText(clisoft[6], cbuffer);
			cbuffer.Format(GetResString(IDS_STATS_UNKNOWNCLIENT), myStats[0]);
			cbuffer.AppendFormat(_T(" (%1.1f%%)"), perc * myStats[0]);
			m_stattree.SetItemText(clisoft[7], cbuffer);

			// CLIENTS -> CLIENT SOFTWARE -> EMULE SECTION
			if (forceUpdate || m_stattree.IsExpanded(clisoft[0]) || cli_lastCount[0] == 0) {
				uint32 verCount = 0;

				//--- find top 4 eMule client versions ---
				uint32 currtopcnt = 0;
				uint32 currtopver = 0;
				uint32 totalOther = 0;
				for (uint32 i = 0; i < MAX_SUB_CLIENT_VERSIONS; ++i) {
					uint32 topver = 0;
					uint32 topcnt = 0;
					double topper = 0.0;
					for (POSITION pos = clientVersionEMule.GetStartPosition(); pos != NULL;) {
						uint32 ver;
						uint32 cnt;
						clientVersionEMule.GetNextAssoc(pos, ver, cnt);
						if (currtopcnt < cnt) {
							topper = (double)cnt / myStats[2];
							topver = ver;
							topcnt = cnt;
							currtopcnt = cnt;
							currtopver = ver;
						} else if (currtopcnt == cnt && currtopver < ver) {
							topver = ver;
							currtopver = ver;
						}
					}
					currtopcnt = 0;
					currtopver = 0;
					clientVersionEMule.RemoveKey(topver);

					if (!topcnt)
						continue;
					
					UINT verMaj = topver / (100 * 10 * 100);
					UINT verMin = (topver - (verMaj * 100 * 10 * 100)) / (100 * 10);
					UINT verUp = (topver - (verMaj * 100 * 10 * 100) - (verMin * 100 * 10)) / (100);
					if (topver >= MAKE_CLIENT_VERSION(0, 40, 0) || verUp != 0) {
						if (verUp <= 'z' - 'a')
							cbuffer.Format(_T("v%u.%u%c: %u (%1.1f%%)"), verMaj, verMin, _T('a') + verUp, topcnt, topper * 100);
						else
							cbuffer.Format(_T("v%u.%u.%u: %u (%1.1f%%)"), verMaj, verMin, verUp, topcnt, topper * 100);
					} else
						cbuffer.Format(_T("v%u.%u: %u (%1.1f%%)"), verMaj, verMin, topcnt, topper * 100);
					
					if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
						totalOther += topcnt;

					if (i >= cli_lastCount[0]) {
						if (i == MAX_SUB_CLIENT_VERSIONS / 2)
							cli_other[0] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), clisoft[0]);
						if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 0 + i] = m_stattree.InsertItem(cbuffer, cli_other[0]);
						else
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 0 + i] = m_stattree.InsertItem(cbuffer, clisoft[0]);
					} else
						m_stattree.SetItemText(cli_versions[MAX_SUB_CLIENT_VERSIONS * 0 + i], cbuffer);

					++verCount;
				}
				if (verCount > MAX_SUB_CLIENT_VERSIONS / 2) {
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_MINORCLIENTS), totalOther, 100.0 * totalOther / myStats[2]);
					m_stattree.SetItemText(cli_other[0], cbuffer);
				}
				if (verCount < cli_lastCount[0])
					for (uint32 i = 0; i < cli_lastCount[0] - verCount; ++i) {
						m_stattree.DeleteItem(cli_versions[cli_lastCount[0] + (MAX_SUB_CLIENT_VERSIONS * 0 - 1) - i]);
						if (cli_lastCount[0] + (MAX_SUB_CLIENT_VERSIONS * 0 - 1) - i == MAX_SUB_CLIENT_VERSIONS / 2)
							m_stattree.DeleteItem(cli_other[0]);
					}

				cli_lastCount[0] = verCount;
			} // End Clients -> Client Software -> eMule Section


			// CLIENTS -> CLIENT SOFTWARE -> eD HYBRID SECTION
			if (forceUpdate || m_stattree.IsExpanded(clisoft[1]) || cli_lastCount[1] == 0) {
				uint32 verCount = 0;

				//--- find top 4 eD Hybrid client versions ---
				uint32 currtopcnt = 0;
				uint32 currtopver = 0;
				uint32 totalOther = 0;
				for (uint32 i = 0; i < MAX_SUB_CLIENT_VERSIONS; ++i) {
					uint32 topver = 0;
					uint32 topcnt = 0;
					double topper = 0.0;
					for (POSITION pos = clientVersionEDonkeyHybrid.GetStartPosition(); pos != NULL;) {
						uint32 ver;
						uint32 cnt;
						clientVersionEDonkeyHybrid.GetNextAssoc(pos, ver, cnt);
						if (currtopcnt < cnt) {
							topper = (double)cnt / myStats[4];
							topver = ver;
							topcnt = cnt;
							currtopcnt = cnt;
							currtopver = ver;
						} else if (currtopcnt == cnt && currtopver < ver) {
							topver = ver;
							currtopver = ver;
						}
					}
					currtopcnt = 0;
					currtopver = 0;
					clientVersionEDonkeyHybrid.RemoveKey(topver);

					if (!topcnt)
						continue;

					UINT verMaj = topver / (100 * 10 * 100);
					UINT verMin = (topver - (verMaj * 100 * 10 * 100)) / (100 * 10);
					UINT verUp = (topver - (verMaj * 100 * 10 * 100) - (verMin * 100 * 10)) / (100);
					cbuffer.Format(_T("v%u.%u.%u: %u (%1.1f%%)"), verMaj, verMin, verUp, topcnt, topper * 100);

					if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
						totalOther += topcnt;

					if (i >= cli_lastCount[1]) {
						if (i == MAX_SUB_CLIENT_VERSIONS / 2)
							cli_other[1] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), clisoft[1]);
						if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 1 + i] = m_stattree.InsertItem(cbuffer, cli_other[1]);
						else
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 1 + i] = m_stattree.InsertItem(cbuffer, clisoft[1]);
					} else
						m_stattree.SetItemText(cli_versions[MAX_SUB_CLIENT_VERSIONS * 1 + i], cbuffer);

					++verCount;
				}
				if (verCount > MAX_SUB_CLIENT_VERSIONS / 2) {
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_MINORCLIENTS), totalOther, 100.0 * totalOther / myStats[4]);
					m_stattree.SetItemText(cli_other[1], cbuffer);
				}
				if (verCount < cli_lastCount[1])
					for (uint32 i = 0; i < cli_lastCount[1] - verCount; ++i) {
						m_stattree.DeleteItem(cli_versions[cli_lastCount[1] + (MAX_SUB_CLIENT_VERSIONS * 1 - 1) - i]);
						if (cli_lastCount[1] + (MAX_SUB_CLIENT_VERSIONS * 1 - 1) - i == MAX_SUB_CLIENT_VERSIONS / 2)
							m_stattree.DeleteItem(cli_other[1]);
					}

				cli_lastCount[1] = verCount;
			} // End Clients -> Client Software -> eD Hybrid Section


			// CLIENTS -> CLIENT SOFTWARE -> EDONKEY SECTION
			if (forceUpdate || m_stattree.IsExpanded(clisoft[2]) || cli_lastCount[2] == 0) {
				uint32 verCount = 0;

				//--- find top 4 eDonkey client versions ---
				uint32 currtopcnt = 0;
				uint32 currtopver = 0;
				uint32 totalOther = 0;
				for (uint32 i = 0; i < MAX_SUB_CLIENT_VERSIONS; ++i) {
					uint32 topver = 0;
					uint32 topcnt = 0;
					double topper = 0.0;
					for (POSITION pos = clientVersionEDonkey.GetStartPosition(); pos != NULL;) {
						uint32 ver;
						uint32 cnt;
						clientVersionEDonkey.GetNextAssoc(pos, ver, cnt);
						if (currtopcnt < cnt) {
							topper = (double)cnt / myStats[1];
							topver = ver;
							topcnt = cnt;
							currtopcnt = cnt;
							currtopver = ver;
						} else if (currtopcnt == cnt && currtopver < ver) {
							topver = ver;
							currtopver = ver;
						}
					}
					currtopcnt = 0;
					currtopver = 0;
					clientVersionEDonkey.RemoveKey(topver);

					if (!topcnt)
						continue;
					
					UINT verMaj = topver / (100 * 10 * 100);
					UINT verMin = (topver - (verMaj * 100 * 10 * 100)) / (100 * 10);
					UINT verUp = (topver - (verMaj * 100 * 10 * 100) - (verMin * 100 * 10)) / (100);
					cbuffer.Format(_T("v%u.%u.%u: %u (%1.1f%%)"), verMaj, verMin, verUp, topcnt, topper * 100);

					if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
						totalOther += topcnt;

					if (i >= cli_lastCount[2]) {
						if (i == MAX_SUB_CLIENT_VERSIONS / 2)
							cli_other[2] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), clisoft[2]);
						if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 2 + i] = m_stattree.InsertItem(cbuffer, cli_other[2]);
						else
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 2 + i] = m_stattree.InsertItem(cbuffer, clisoft[2]);
					} else
						m_stattree.SetItemText(cli_versions[MAX_SUB_CLIENT_VERSIONS * 2 + i], cbuffer);

					++verCount;
				}
				if (verCount > MAX_SUB_CLIENT_VERSIONS / 2) {
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_MINORCLIENTS), totalOther, 100.0 * totalOther / myStats[1]);
					m_stattree.SetItemText(cli_other[2], cbuffer);
				}
				if (verCount < cli_lastCount[2])
					for (uint32 i = 0; i < cli_lastCount[2] - verCount; ++i) {
						m_stattree.DeleteItem(cli_versions[cli_lastCount[2] + (MAX_SUB_CLIENT_VERSIONS * 2 - 1) - i]);
						if (cli_lastCount[2] + (MAX_SUB_CLIENT_VERSIONS * 2 - 1) - i == MAX_SUB_CLIENT_VERSIONS / 2)
							m_stattree.DeleteItem(cli_other[2]);
					}

				cli_lastCount[2] = verCount;
			} // End Clients -> Client Software -> eDonkey Section


			// CLIENTS -> CLIENT SOFTWARE -> AMULE SECTION
			if (forceUpdate || m_stattree.IsExpanded(clisoft[3]) || cli_lastCount[3] == 0) {
				uint32 verCount = 0;

				//--- find top 4 client versions ---
				uint32 currtopcnt = 0;
				uint32 currtopver = 0;
				uint32 totalOther = 0;
				for (uint32 i = 0; i < MAX_SUB_CLIENT_VERSIONS; ++i) {
					POSITION pos = clientVersionAMule.GetStartPosition();
					uint32 topver = 0;
					uint32 topcnt = 0;
					double topper = 0.0;
					while (pos) {
						uint32 ver;
						uint32 cnt;
						clientVersionAMule.GetNextAssoc(pos, ver, cnt);
						if (currtopcnt < cnt) {
							topper = (double)cnt / myStats[10];
							topver = ver;
							topcnt = cnt;
							currtopcnt = cnt;
							currtopver = ver;
						} else if (currtopcnt == cnt && currtopver < ver) {
							topver = ver;
							currtopver = ver;
						}
					}
					currtopcnt = 0;
					currtopver = 0;
					clientVersionAMule.RemoveKey(topver);

					if (!topcnt)
						continue;

					UINT verMaj = topver / (100 * 10 * 100);
					UINT verMin = (topver - (verMaj * 100 * 10 * 100)) / (100 * 10);
					UINT verUp = (topver - (verMaj * 100 * 10 * 100) - (verMin * 100 * 10)) / (100);
					if (topver >= MAKE_CLIENT_VERSION(0, 40, 0) || verUp != 0)
						cbuffer.Format(_T("v%u.%u.%u: %u (%1.1f%%)"), verMaj, verMin, verUp, topcnt, topper * 100);
					else
						cbuffer.Format(_T("v%u.%u: %u (%1.1f%%)"), verMaj, verMin, topcnt, topper * 100);

					if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
						totalOther += topcnt;

					if (i >= cli_lastCount[3]) {
						if (i == MAX_SUB_CLIENT_VERSIONS / 2)
							cli_other[3] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), clisoft[3]);
						if (i >= MAX_SUB_CLIENT_VERSIONS / 2)
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 3 + i] = m_stattree.InsertItem(cbuffer, cli_other[3]);
						else
							cli_versions[MAX_SUB_CLIENT_VERSIONS * 3 + i] = m_stattree.InsertItem(cbuffer, clisoft[3]);
					} else
						m_stattree.SetItemText(cli_versions[MAX_SUB_CLIENT_VERSIONS * 3 + i], cbuffer);

					++verCount;
				}
				if (verCount > MAX_SUB_CLIENT_VERSIONS / 2) {
					cbuffer.Format(_T("%s: %u (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_MINORCLIENTS), totalOther, 100.0 * totalOther / myStats[10]);
					m_stattree.SetItemText(cli_other[3], cbuffer);
				}
				if (verCount < cli_lastCount[3])
					for (uint32 i = 0; i < cli_lastCount[3] - verCount; ++i) {
						m_stattree.DeleteItem(cli_versions[cli_lastCount[3] + (MAX_SUB_CLIENT_VERSIONS * 3 - 1) - i]);
						if (cli_lastCount[3] + (MAX_SUB_CLIENT_VERSIONS * 3 - 1) - i == MAX_SUB_CLIENT_VERSIONS / 2)
							m_stattree.DeleteItem(cli_other[3]);
					}

				cli_lastCount[3] = verCount;
			} // End Clients -> Client Software -> aMule Section

		} // - End Clients -> Client Software Section
		// CLIENTS -> NETWORK SECTION
		if (forceUpdate || m_stattree.IsExpanded(hclinet)) {
			cbuffer.Format(_T("eD2K: %i (%.1f%%)"), myStats[15], perc*myStats[15]);
			m_stattree.SetItemText(clinet[0], cbuffer);
			cbuffer.Format(_T("Kad: %i (%.1f%%)"), myStats[16], perc*myStats[16]);
			m_stattree.SetItemText(clinet[1], cbuffer);
			cbuffer.Format(_T("eD2K/Kad: %i (%.1f%%)"), myStats[17], perc*myStats[17]);
			m_stattree.SetItemText(clinet[2], cbuffer);
			cbuffer.Format(_T("%s: %i (%.1f%%)"), (LPCTSTR)GetResString(IDS_UNKNOWN), myStats[18], perc*myStats[18]);
			m_stattree.SetItemText(clinet[3], cbuffer);
		}
		// End Clients -> Network Section

		// CLIENTS -> PORT SECTION
		if (forceUpdate || m_stattree.IsExpanded(hcliport)) {
			cbuffer.Format(_T("%s: %i (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTDEF), myStats[8], myStats[8] > 0 ? (100.0 * myStats[8] / (myStats[8] + myStats[9])) : 0);
			m_stattree.SetItemText(cliport[0], cbuffer);
			cbuffer.Format(_T("%s: %i (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PRTOTHER), myStats[9], myStats[9] > 0 ? (100.0 * myStats[9] / (myStats[8] + myStats[9])) : 0);
			m_stattree.SetItemText(cliport[1], cbuffer);
		} // - End Clients -> Port Section

		// CLIENTS -> FIREWALLED (KAD) SECTION
		if (forceUpdate || m_stattree.IsExpanded(hclifirewalled)) {
			if (!Kademlia::CKademlia::IsRunning() || Kademlia::CUDPFirewallTester::IsFirewalledUDP(true)) {
				cbuffer.Format(_T("UDP: %s"), (LPCTSTR)GetResString(IDS_KAD_UNKNOWN));
				m_stattree.SetItemText(clifirewalled[0], cbuffer);
				cbuffer.Format(_T("TCP: %s"), (LPCTSTR)GetResString(IDS_KAD_UNKNOWN));
				m_stattree.SetItemText(clifirewalled[1], cbuffer);
			} else {
				if (Kademlia::CKademlia::GetPrefs()->StatsGetFirewalledRatio(true) > 0)
					cbuffer.Format(_T("UDP: %1.1f%%"), Kademlia::CKademlia::GetPrefs()->StatsGetFirewalledRatio(true) * 100);
				else
					cbuffer.Format(_T("UDP: %s"), (LPCTSTR)GetResString(IDS_FSTAT_WAITING));
				m_stattree.SetItemText(clifirewalled[0], cbuffer);
				if (Kademlia::CKademlia::GetPrefs()->StatsGetFirewalledRatio(false) > 0)
					cbuffer.Format(_T("TCP: %1.1f%%"), Kademlia::CKademlia::GetPrefs()->StatsGetFirewalledRatio(false) * 100);
				else
					cbuffer.Format(_T("TCP: %s"), (LPCTSTR)GetResString(IDS_FSTAT_WAITING));
				m_stattree.SetItemText(clifirewalled[1], cbuffer);
			}
		} // - End Clients -> Firewalled (Kad) Section

		// General Client Statistics
		cbuffer.Format(_T("%s: %i (%1.1f%%)"), (LPCTSTR)GetResString(IDS_STATS_PROBLEMATIC), myStats[6], perc*myStats[6]);
		m_stattree.SetItemText(cligen[0], cbuffer);

		cbuffer.Format(_T("%s: %i"), (LPCTSTR)GetResString(IDS_BANNED), (int)theApp.clientlist->GetBannedCount());
		m_stattree.SetItemText(cligen[1], cbuffer);

		cbuffer.Format(GetResString(IDS_STATS_FILTEREDCLIENTS), theStats.filteredclients);
		m_stattree.SetItemText(cligen[2], cbuffer);
	} // - END CLIENTS SECTION


	// UPDATE RECORDS FOR SERVERS AND SHARED FILES
	thePrefs.SetRecordStructMembers();

	// SERVERS SECTION
	if (forceUpdate || m_stattree.IsExpanded(h_servers)) {
		// Get stat values
		uint32	servtotal, servfail, servuser, servfile, servlowiduser, servtuser, servtfile;
		float	servocc;
		theApp.serverlist->GetStatus(servtotal, servfail, servuser, servfile, servlowiduser, servtuser, servtfile, servocc);
		// Set working servers value
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_WORKING), (LPCTSTR)CastItoIShort(servtotal - servfail));
		m_stattree.SetItemText(srv[0], cbuffer);
		if (forceUpdate || m_stattree.IsExpanded(srv[0])) {
			// Set users on working servers value
			cbuffer.Format(_T("%s: %s; %s: %s (%.1f%%)"), (LPCTSTR)GetResString(IDS_SF_WUSER), (LPCTSTR)CastItoIShort(servuser), (LPCTSTR)GetResString(IDS_IDLOW), (LPCTSTR)CastItoIShort(servlowiduser), servuser ? (servlowiduser * 100.0 / servuser) : 0.0);
			m_stattree.SetItemText(srv_w[0], cbuffer);
			// Set files on working servers value
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_WFILE), (LPCTSTR)CastItoIShort(servfile));
			m_stattree.SetItemText(srv_w[1], cbuffer);
			// Set server occ value
			cbuffer.Format(GetResString(IDS_SF_SRVOCC), servocc);
			m_stattree.SetItemText(srv_w[2], cbuffer);
		}
		// Set failed servers value
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_FAIL), (LPCTSTR)CastItoIShort(servfail));
		m_stattree.SetItemText(srv[1], cbuffer);
		// Set deleted servers value
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_DELCOUNT), (LPCTSTR)CastItoIShort(theApp.serverlist->GetDeletedServerCount()));
		m_stattree.SetItemText(srv[2], cbuffer);
		// Set total servers value
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_TOTAL), (LPCTSTR)CastItoIShort(servtotal));
		m_stattree.SetItemText(srv[3], cbuffer);
		// Set total users value
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_USER), (LPCTSTR)CastItoIShort(servtuser));
		m_stattree.SetItemText(srv[4], cbuffer);
		// Set total files value
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_SF_FILE), (LPCTSTR)CastItoIShort(servtfile));
		m_stattree.SetItemText(srv[5], cbuffer);
		// SERVERS -> RECORDS SECTION
		if (forceUpdate || m_stattree.IsExpanded(hsrv_records)) {
			// Set most working servers
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_SVRECWORKING), (LPCTSTR)CastItoIShort(thePrefs.GetSrvrsMostWorkingServers()));
			m_stattree.SetItemText(srv_r[0], cbuffer);
			// Set most users online
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_SVRECUSERS), (LPCTSTR)CastItoIShort(thePrefs.GetSrvrsMostUsersOnline()));
			m_stattree.SetItemText(srv_r[1], cbuffer);
			// Set most files avail
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_SVRECFILES), (LPCTSTR)CastItoIShort(thePrefs.GetSrvrsMostFilesAvail()));
			m_stattree.SetItemText(srv_r[2], cbuffer);
		} // - End Servers -> Records Section
	} // - END SERVERS SECTION


	// SHARED FILES SECTION
	if (forceUpdate || m_stattree.IsExpanded(h_shared)) {
		// Set Number of Shared Files
		cbuffer.Format(GetResString(IDS_SHAREDFILESCOUNT), (int)theApp.sharedfiles->GetCount());
		// SLUGFILLER: SafeHash - extra statistics
		if (theApp.sharedfiles->GetHashingCount())
			cbuffer.AppendFormat(GetResString(IDS_HASHINGFILESCOUNT), theApp.sharedfiles->GetHashingCount());

		// SLUGFILLER: SafeHash
		m_stattree.SetItemText(shar[0], cbuffer);
		// Set Average File Size
		uint64 bytesLargestFile = 0;
		uint64 allsize = theApp.sharedfiles->GetDatasize(bytesLargestFile); // Func returns total share size and sets pointer uint64 to largest single file size
		CString cbuffer2;
		if (theApp.sharedfiles->GetCount() > 0)
			cbuffer2 = CastItoXBytes(allsize / (UINT_PTR)theApp.sharedfiles->GetCount());
		else
			cbuffer2 = CastItoXBytes((uint32)0);
		cbuffer.Format(GetResString(IDS_SF_AVERAGESIZE), (LPCTSTR)cbuffer2);
		m_stattree.SetItemText(shar[1], cbuffer);
		// Set Largest File Size
		cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_LARGESTFILE), (LPCTSTR)CastItoXBytes(bytesLargestFile));
		m_stattree.SetItemText(shar[2], cbuffer);
		// Set Total Share Size
		cbuffer.Format(GetResString(IDS_SF_SIZE), (LPCTSTR)CastItoXBytes(allsize));
		m_stattree.SetItemText(shar[3], cbuffer);

		// SHARED FILES -> RECORDS SECTION
		if (forceUpdate || m_stattree.IsExpanded(hshar_records)) {
			// Set Most Files Shared
			cbuffer.Format(_T("%s: %u"), (LPCTSTR)GetResString(IDS_STATS_SHRECNUM), thePrefs.GetSharedMostFilesShared());
			m_stattree.SetItemText(shar_r[0], cbuffer);
			// Set largest avg file size
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_SHRECASIZE), (LPCTSTR)CastItoXBytes(thePrefs.GetSharedLargestAvgFileSize()));
			m_stattree.SetItemText(shar_r[1], cbuffer);
			// Set largest file size
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_LARGESTFILE), (LPCTSTR)CastItoXBytes(thePrefs.GetSharedLargestFileSize()));
			m_stattree.SetItemText(shar_r[2], cbuffer);
			// Set largest share size
			cbuffer.Format(_T("%s: %s"), (LPCTSTR)GetResString(IDS_STATS_SHRECSIZE), (LPCTSTR)CastItoXBytes(thePrefs.GetSharedLargestShareSize()));
			m_stattree.SetItemText(shar_r[3], cbuffer);
		} // - End Shared Files -> Records Section
	} // - END SHARED FILES SECTION

	if (forceUpdate || m_stattree.IsExpanded(h_total_downloads)) {
		uint64 ui64TotalFileSize = 0;
		uint64 ui64TotalLeftToTransfer = 0;
		uint64 ui64TotalAdditionalNeededSpace = 0;
		int iActiveFiles = theApp.downloadqueue->GetDownloadFilesStats(ui64TotalFileSize, ui64TotalLeftToTransfer, ui64TotalAdditionalNeededSpace);

		cbuffer.Format(GetResString(IDS_DWTOT_NR), iActiveFiles);
		m_stattree.SetItemText(h_total_num_of_dls, cbuffer);

		cbuffer.Format(GetResString(IDS_DWTOT_TSD), (LPCTSTR)CastItoXBytes(ui64TotalFileSize));
		m_stattree.SetItemText(h_total_size_of_dls, cbuffer);

		uint64 ui64TotalTransferred = ui64TotalFileSize - ui64TotalLeftToTransfer;
		double fPercent = ui64TotalFileSize ? (ui64TotalTransferred * 100.0) / ui64TotalFileSize : 0;

		cbuffer.Format(GetResString(IDS_DWTOT_TCS), (LPCTSTR)CastItoXBytes(ui64TotalTransferred), fPercent);
		m_stattree.SetItemText(h_total_size_dld, cbuffer);

		cbuffer.Format(GetResString(IDS_DWTOT_TSL), (LPCTSTR)CastItoXBytes(ui64TotalLeftToTransfer));
		m_stattree.SetItemText(h_total_size_left_to_dl, cbuffer);

		cbuffer.Format(GetResString(IDS_DWTOT_TSN), (LPCTSTR)CastItoXBytes(ui64TotalAdditionalNeededSpace));
		m_stattree.SetItemText(h_total_size_needed, cbuffer);

		uint64 ui64TotalFreeSpace = GetFreeTempSpace(-1);
		CString buffer2;
		buffer2.Format(GetResString(IDS_DWTOT_FS), (LPCTSTR)CastItoXBytes(ui64TotalFreeSpace));

		if (ui64TotalAdditionalNeededSpace > ui64TotalFreeSpace)
			cbuffer.Format(GetResString(IDS_NEEDFREEDISKSPACE), (LPCTSTR)buffer2, (LPCTSTR)CastItoXBytes(ui64TotalAdditionalNeededSpace - ui64TotalFreeSpace));
		else
			cbuffer = buffer2;
		m_stattree.SetItemText(h_total_size_left_on_drive, cbuffer);
	}
	// - End Set Tree Values

#ifdef _DEBUG
	if (g_pfnPrevCrtAllocHook) {
		_CrtMemState memState;
		_CrtMemCheckpoint(&memState);

		cbuffer.Format(_T("%s: %u bytes in %u blocks"), _T("Free"), memState.lSizes[0], memState.lCounts[0]);
		m_stattree.SetItemText(debug1, cbuffer);
		cbuffer.Format(_T("%s: %u bytes in %u blocks"), _T("Normal"), memState.lSizes[1], memState.lCounts[1]);
		m_stattree.SetItemText(debug2, cbuffer);
		cbuffer.Format(_T("%s: %u bytes in %u blocks"), _T("CRT"), memState.lSizes[2], memState.lCounts[2]);
		m_stattree.SetItemText(debug3, cbuffer);
		cbuffer.Format(_T("%s: %u bytes in %u blocks"), _T("Ignore"), memState.lSizes[3], memState.lCounts[3]);
		m_stattree.SetItemText(debug4, cbuffer);
		cbuffer.Format(_T("%s: %u bytes in %u blocks"), _T("Client"), memState.lSizes[4], memState.lCounts[4]);
		m_stattree.SetItemText(debug5, cbuffer);

		extern CMap<const unsigned char*, const unsigned char*, UINT, UINT> g_allocations;

		for (POSITION pos = blockFiles.GetStartPosition(); pos != NULL;) {
			const unsigned char *pszFileName;
			HTREEITEM *pTag;
			blockFiles.GetNextAssoc(pos, pszFileName, pTag);
			m_stattree.SetItemText(*pTag, _T(""));
		}
		for (POSITION pos = g_allocations.GetStartPosition(); pos != NULL;) {
			const unsigned char *pszFileName;
			UINT count;
			g_allocations.GetNextAssoc(pos, pszFileName, count);
			HTREEITEM *pTag;
			if (blockFiles.Lookup(pszFileName, pTag) == 0) {
				pTag = new HTREEITEM;
				*pTag = m_stattree.InsertItem(_T("0"), debug2);
				m_stattree.SetItemData(*pTag, 1);
				blockFiles.SetAt(pszFileName, pTag);
			}
			cbuffer.Format(_T("%hs : %u blocks"), pszFileName, count);
			m_stattree.SetItemText(*pTag, cbuffer);
		}
	}
#endif

#ifdef USE_MEM_STATS
	if (forceUpdate || m_stattree.IsExpanded(h_allocs)) {
		ULONGLONG ullTotalAllocs = 0;
		for (int i = 0; i < ALLOC_SLOTS; ++i)
			ullTotalAllocs += g_aAllocStats[i];
		for (int i = 0; i < ALLOC_SLOTS; ++i) {
			unsigned uStart, uEnd;
			if (i <= 1)
				uStart = uEnd = i;
			else {
				uStart = 1 << (i - 1);
				uEnd = (i == ALLOC_SLOTS - 1) ? UINT_MAX : (uStart << 1) - 1;
			}
			CString strLabel;
			strLabel.Format(_T("Block size %08X-%08X: %s (%1.1f%%)"), uStart, uEnd, (LPCTSTR)CastItoIShort(g_aAllocStats[i], false, 2), ullTotalAllocs != 0 ? g_aAllocStats[i] * 100.0 / ullTotalAllocs : 0.0);
			m_stattree.SetItemText(h_allocSizes[i], strLabel);
		}
	}
#endif

	m_stattree.SetRedraw(true);

} // ShowStatistics(bool forceRedraw = false){}

void CStatisticsDlg::UpdateConnectionsGraph()
{
	// This updates the Y-Axis scale of the Connections Statistics graph...
	// And it updates the trend ratio for the active connections trend.

	m_Statistics.SetRanges(0, thePrefs.GetStatsMax());
	m_Statistics.SetTrendRatio(0, thePrefs.GetStatsConnectionsGraphRatio());
}

void CStatisticsDlg::OnShowWindow(BOOL /*bShow*/, UINT /*nStatus*/)
{
}

void CStatisticsDlg::OnSize(UINT nType, int cx, int cy)
{
	CResizableDialog::OnSize(nType, cx, cy);
	if (cx > 0 && cy > 0 && (cx != m_oldcx || cy != m_oldcy)) {
		m_oldcx = cx;
		m_oldcy = cy;
		ShowInterval();
	}
}

void CStatisticsDlg::ShowInterval()
{
	if (theApp.IsClosing())
		return;

	// Check if OScope already initialized
	if (m_DownloadOMeter.GetSafeHwnd() != NULL && m_UploadOMeter.GetSafeHwnd() != NULL) {
		// Retrieve the size (in pixel) of the OScope
		CRect plotRect;
		m_UploadOMeter.GetPlotRect(plotRect);

		// Dynamic update of time scale [Maella]
		int shownSecs = plotRect.Width() * thePrefs.GetTrafficOMeterInterval();

		// CB Mod ---> Make Setters
		m_Statistics.m_nXPartial = m_DownloadOMeter.m_nXPartial = m_UploadOMeter.m_nXPartial = shownSecs % HR2S(1);
		m_Statistics.m_nXGrids = m_DownloadOMeter.m_nXGrids = m_UploadOMeter.m_nXGrids = shownSecs / HR2S(1);

		if (shownSecs <= 0) {
			m_DownloadOMeter.SetXUnits(GetResString(IDS_STOPPED));
			m_UploadOMeter.SetXUnits(GetResString(IDS_STOPPED));
			m_Statistics.SetXUnits(GetResString(IDS_STOPPED));
		} else {
			const CString &buffer = CastSecondsToHM(shownSecs);
			m_UploadOMeter.SetXUnits(buffer);
			m_DownloadOMeter.SetXUnits(buffer);
			m_Statistics.SetXUnits(buffer);
		}
		UpdateData(FALSE);
	}
}

void CStatisticsDlg::SetARange(bool SetDownload, int maxValue)
{
	if (SetDownload)
		m_DownloadOMeter.SetRanges(0, maxValue);
	else
		m_UploadOMeter.SetRanges(0, maxValue);
}

// Various changes in Localize() and a new button event...
void CStatisticsDlg::Localize()
{
	RepaintMeters();

	CString myBuffer;
	myBuffer.Format(GetResString(IDS_STATS_LASTRESETSTATIC), (LPCTSTR)thePrefs.GetStatsLastResetStr(false));
	SetDlgItemText(IDC_STATIC_LASTRESET, myBuffer);
}
// End Localize

// Menu Button: Displays the menu of stat tree commands.
void CStatisticsDlg::OnMenuButtonClicked()
{
	CRect rectBn;
	GetDlgItem(IDC_BNMENU)->GetWindowRect(rectBn);
	m_stattree.DoMenu(rectBn.BottomRight());
}

void CStatisticsDlg::CreateMyTree()
{
	m_stattree.DeleteAllItems();

	// Setup Tree
	h_transfer = m_stattree.InsertItem(GetResString(IDS_FSTAT_TRANSFER), 1, 1);				// Transfers Section
	CString buffer;
	buffer.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_STATS_SRATIO), (LPCTSTR)GetResString(IDS_FSTAT_WAITING)); // Make It Pretty
	trans[0] = m_stattree.InsertItem(buffer, h_transfer);										// Session Ratio

	buffer.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_STATS_FRATIO), (LPCTSTR)GetResString(IDS_FSTAT_WAITING)); // Make It Pretty
	trans[1] = m_stattree.InsertItem(buffer, h_transfer);										// Friend Session Ratio

	buffer.Format(_T("%s %s"), (LPCTSTR)GetResString(IDS_STATS_CRATIO), (LPCTSTR)GetResString(IDS_FSTAT_WAITING)); // Make It Pretty
	trans[2] = m_stattree.InsertItem(buffer, h_transfer);										// Cumulative Ratio

	h_upload = m_stattree.InsertItem(GetResString(IDS_TW_UPLOADS), 6, 6, h_transfer);			// Uploads Section

	h_up_session = m_stattree.InsertItem(GetResString(IDS_STATS_SESSION), 8, 8, h_upload);	// Session Section (Uploads)
	for (unsigned i = 0; i < 6; ++i)
		up_S[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_up_session);		//MORPH - Added by Yun.SF3, ZZ Upload System
	hup_scb = m_stattree.InsertItem(GetResString(IDS_CLIENTS), up_S[0]);						// Clients Section
	for (unsigned i = 0; i < _countof(up_scb); ++i)
		up_scb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_scb);
	hup_spb = m_stattree.InsertItem(GetResString(IDS_PORT), up_S[0]);							// Ports Section
	for (unsigned i = 0; i < _countof(up_spb); ++i)
		up_spb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_spb);
	hup_ssb = m_stattree.InsertItem(GetResString(IDS_STATS_DATASOURCE), up_S[0]);				// Data Source Section
	for (unsigned i = 0; i < 2; ++i)
		up_ssb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_ssb);
	for (unsigned i = 0; i < 4; ++i)
		up_ssessions[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), up_S[5]);	//MORPH - Added by Yun.SF3, ZZ Upload System
	hup_soh = m_stattree.InsertItem(GetResString(IDS_STATS_OVRHD), h_up_session);				// Upline Overhead (Session)
	for (unsigned i = 0; i < _countof(up_soh); ++i)
		up_soh[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_soh);
	h_up_total = m_stattree.InsertItem(GetResString(IDS_STATS_CUMULATIVE), 9, 9, h_upload);	// Cumulative Section (Uploads)
	up_T[0] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_up_total);				// Uploaded Data (Total)
	hup_tcb = m_stattree.InsertItem(GetResString(IDS_CLIENTS), up_T[0]);						// Clients Section
	for (unsigned i = 0; i < _countof(up_tcb); ++i)
		up_tcb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_tcb);
	hup_tpb = m_stattree.InsertItem(GetResString(IDS_PORT), up_T[0]);							// Ports Section
	for (unsigned i = 0; i < _countof(up_tpb); ++i)
		up_tpb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_tpb);
	hup_tsb = m_stattree.InsertItem(GetResString(IDS_STATS_DATASOURCE), up_T[0]);				// Data Source Section
	for (unsigned i = 0; i < 2; ++i)
		up_tsb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_tsb);
	up_T[1] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_up_total);				// Upload Sessions (Total)
	for (unsigned i = 0; i < 4; ++i)
		up_tsessions[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), up_T[1]);
	hup_toh = m_stattree.InsertItem(GetResString(IDS_STATS_OVRHD), h_up_total);				// Upline Overhead (Total)
	for (unsigned i = 0; i < _countof(up_toh); ++i)
		up_toh[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hup_toh);
	h_download = m_stattree.InsertItem(GetResString(IDS_TW_DOWNLOADS), 7, 7, h_transfer);		// Downloads Section
	h_down_session = m_stattree.InsertItem(GetResString(IDS_STATS_SESSION), 8, 8, h_download); // Session Section (Downloads)
	for (unsigned i = 0; i < 8; ++i)
		down_S[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_down_session);
	hdown_scb = m_stattree.InsertItem(GetResString(IDS_CLIENTS), down_S[0]);					// Clients Section
	for (unsigned i = 0; i < _countof(down_scb); ++i)
		down_scb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hdown_scb);
	hdown_spb = m_stattree.InsertItem(GetResString(IDS_PORT), down_S[0]);						// Ports Section
	for (unsigned i = 0; i < _countof(down_spb); ++i)
		down_spb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hdown_spb);
	for (unsigned i = 0; i < _countof(down_sources); ++i)
		down_sources[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), down_S[3]);
	for (unsigned i = 0; i < 4; ++i)
		down_ssessions[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), down_S[4]);
	hdown_soh = m_stattree.InsertItem(GetResString(IDS_STATS_OVRHD), h_down_session);			// Downline Overhead (Session)
	for (unsigned i = 0; i < _countof(down_soh); ++i)
		down_soh[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hdown_soh);
	h_down_total = m_stattree.InsertItem(GetResString(IDS_STATS_CUMULATIVE), 9, 9, h_download);// Cumulative Section (Downloads)
	for (unsigned i = 0; i < 6; ++i)
		down_T[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_down_total);
	hdown_tcb = m_stattree.InsertItem(GetResString(IDS_CLIENTS), down_T[0]);					// Clients Section
	for (unsigned i = 0; i < _countof(down_tcb); ++i)
		down_tcb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hdown_tcb);
	hdown_tpb = m_stattree.InsertItem(GetResString(IDS_PORT), down_T[0]);						// Ports Section
	for (unsigned i = 0; i < _countof(down_tpb); ++i)
		down_tpb[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hdown_tpb);
	for (unsigned i = 0; i < 4; ++i)
		down_tsessions[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), down_T[2]);
	hdown_toh = m_stattree.InsertItem(GetResString(IDS_STATS_OVRHD), h_down_total);			// Downline Overhead (Total)
	for (unsigned i = 0; i < _countof(down_toh); ++i)
		down_toh[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hdown_toh);
	h_connection = m_stattree.InsertItem(GetResString(IDS_CONNECTIONS), 2, 2);			// Connection Section
	h_conn_session = m_stattree.InsertItem(GetResString(IDS_STATS_SESSION), 8, 8, h_connection); // Session Section (Connection)
	hconn_sg = m_stattree.InsertItem(GetResString(IDS_STATS_GENERAL), 11, 11, h_conn_session); // General Section (Session)
	for (unsigned i = 0; i < 5; ++i)
		conn_sg[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hconn_sg);
	hconn_su = m_stattree.InsertItem(GetResString(IDS_PW_CON_UPLBL), 6, 6, h_conn_session);	// Uploads Section (Session)
	for (unsigned i = 0; i < 4; ++i)
		conn_su[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hconn_su);
	hconn_sd = m_stattree.InsertItem(GetResString(IDS_PW_CON_DOWNLBL), 7, 7, h_conn_session);	// Downloads Section (Session)
	for (unsigned i = 0; i < 4; ++i)
		conn_sd[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hconn_sd);
	h_conn_total = m_stattree.InsertItem(GetResString(IDS_STATS_CUMULATIVE), 9, 9, h_connection);	// Cumulative Section (Connection)
	hconn_tg = m_stattree.InsertItem(GetResString(IDS_STATS_GENERAL), 11, 11, h_conn_total);	// General (Total)
	for (unsigned i = 0; i < 4; ++i)
		conn_tg[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hconn_tg);
	hconn_tu = m_stattree.InsertItem(GetResString(IDS_PW_CON_UPLBL), 6, 6, h_conn_total);		// Uploads (Total)
	for (unsigned i = 0; i < 3; ++i)
		conn_tu[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hconn_tu);
	hconn_td = m_stattree.InsertItem(GetResString(IDS_PW_CON_DOWNLBL), 7, 7, h_conn_total);	// Downloads (Total)
	for (unsigned i = 0; i < 3; ++i)
		conn_td[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hconn_td);
	h_time = m_stattree.InsertItem(GetResString(IDS_STATS_TIMESTATS), 12, 12);				// Time Statistics Section
	for (unsigned i = 0; i < 2; ++i)
		tvitime[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_time);
	htime_s = m_stattree.InsertItem(GetResString(IDS_STATS_SESSION), 8, 8, h_time);			// Session Section (Time)
	for (unsigned i = 0; i < 4; ++i)
		tvitime_s[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), htime_s);
	for (unsigned i = 0; i < 2; ++i)
		tvitime_st[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), tvitime_s[1]);
	htime_t = m_stattree.InsertItem(GetResString(IDS_STATS_CUMULATIVE), 9, 9, h_time);		// Cumulative Section (Time)
	for (unsigned i = 0; i < 3; ++i)
		tvitime_t[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), htime_t);
	for (unsigned i = 0; i < 2; ++i)
		tvitime_tt[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), tvitime_t[1]);
	htime_aap = m_stattree.InsertItem(GetResString(IDS_STATS_AVGANDPROJ), 13, 13, h_time);	// Projected Averages Section
	time_aaph[0] = m_stattree.InsertItem(GetResString(IDS_DAYLY), 14, 14, htime_aap);			// Daily Section
	time_aaph[1] = m_stattree.InsertItem(GetResString(IDS_STATS_MONTHLY), 15, 15, htime_aap);	// Monthly Section
	time_aaph[2] = m_stattree.InsertItem(GetResString(IDS_STATS_YEARLY), 16, 16, htime_aap);	// Yearly Section
	for (int x = 0; x < 3; ++x) {
		time_aap_hup[x] = m_stattree.InsertItem(GetResString(IDS_TW_UPLOADS), 6, 6, time_aaph[x]); // Upload Section
		for (unsigned i = 0; i < 3; ++i)
			time_aap_up[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_hup[x]);
		time_aap_up_hd[x][0] = m_stattree.InsertItem(GetResString(IDS_CLIENTS), time_aap_up[x][0]);							// Clients Section
		for (unsigned i = 0; i < 7; ++i)
			time_aap_up_dc[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_up_hd[x][0]);
		time_aap_up_hd[x][1] = m_stattree.InsertItem(GetResString(IDS_PORT), time_aap_up[x][0]);								// Ports Section
		for (unsigned i = 0; i < _countof(time_aap_up_dp[0]); ++i)
			time_aap_up_dp[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_up_hd[x][1]);
		time_aap_up_hd[x][2] = m_stattree.InsertItem(GetResString(IDS_STATS_DATASOURCE), time_aap_up[x][0]);					// Data Source Section
		for (unsigned i = 0; i < 2; ++i)
			time_aap_up_ds[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_up_hd[x][2]);
		for (unsigned i = 0; i < 2; ++i)
			time_aap_up_s[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_up[x][1]);
		for (unsigned i = 0; i < 4; ++i)
			time_aap_up_oh[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_up[x][2]);
		time_aap_hdown[x] = m_stattree.InsertItem(GetResString(IDS_TW_DOWNLOADS), 7, 7, time_aaph[x]); // Download Section
		for (unsigned i = 0; i < 7; ++i)
			time_aap_down[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_hdown[x]);
		time_aap_down_hd[x][0] = m_stattree.InsertItem(GetResString(IDS_CLIENTS), time_aap_down[x][0]);							// Clients Section
		for (unsigned i = 0; i < 8; ++i)
			time_aap_down_dc[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_down_hd[x][0]);
		time_aap_down_hd[x][1] = m_stattree.InsertItem(GetResString(IDS_PORT), time_aap_down[x][0]);								// Ports Section
		for (unsigned i = 0; i < _countof(time_aap_down_dp[0]); ++i)
			time_aap_down_dp[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_down_hd[x][1]);
		for (unsigned i = 0; i < 2; ++i)
			time_aap_down_s[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_down[x][2]);
		for (unsigned i = 0; i < 4; ++i)
			time_aap_down_oh[x][i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), time_aap_down[x][6]);
	}
	h_clients = m_stattree.InsertItem(GetResString(IDS_CLIENTS), 3, 3);						// Clients Section
	cligen[5] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_clients);
	hclisoft = m_stattree.InsertItem(GetResString(IDS_CD_CSOFT), h_clients);					// Client Software Section
	for (unsigned i = 0; i < 8; ++i)
		clisoft[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hclisoft);
	hclinet = m_stattree.InsertItem(GetResString(IDS_NETWORK), h_clients);					// Client Network Section
	for (unsigned i = 0; i < 4; ++i)
		clinet[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hclinet);
	hcliport = m_stattree.InsertItem(GetResString(IDS_PORT), h_clients);						// Client Port Section
	for (unsigned i = 0; i < 2; ++i)
		cliport[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hcliport);
	CString sFwKAD(GetResString(IDS_FIREWALLED));
	sFwKAD.AppendFormat(_T(" (%s)"), (LPCTSTR)GetResString(IDS_KADEMLIA));
	hclifirewalled = m_stattree.InsertItem(sFwKAD, h_clients);
	for (unsigned i = 0; i < 2; ++i)
		clifirewalled[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hclifirewalled);
	cligen[4] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_clients);
	cligen[3] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_clients);
	for (unsigned i = 0; i < 3; ++i)
		cligen[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_clients);
	h_servers = m_stattree.InsertItem(GetResString(IDS_FSTAT_SERVERS), 4, 4);					// Servers section
	for (unsigned i = 0; i < 6; ++i)
		srv[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_servers);			// Servers Items
	for (unsigned i = 0; i < 3; ++i)
		srv_w[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), srv[0]);			// Working Servers Items
	hsrv_records = m_stattree.InsertItem(GetResString(IDS_STATS_RECORDS), 10, 10, h_servers);	// Servers Records Section
	for (unsigned i = 0; i < 3; ++i)
		srv_r[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hsrv_records);		// Record Items
	h_shared = m_stattree.InsertItem(GetResString(IDS_SHAREDFILES), 5, 5);					// Shared Files Section
	for (unsigned i = 0; i < 4; ++i)
		shar[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), h_shared);
	hshar_records = m_stattree.InsertItem(GetResString(IDS_STATS_RECORDS), 10, 10, h_shared);	// Shared Records Section
	for (unsigned i = 0; i < 4; ++i)
		shar_r[i] = m_stattree.InsertItem(GetResString(IDS_FSTAT_WAITING), hshar_records);
	h_total_downloads = m_stattree.InsertItem(GetResString(IDS_DWTOT), 17, 17);
	h_total_num_of_dls = m_stattree.InsertItem(GetResString(IDS_DWTOT_NR), h_total_downloads);
	h_total_size_of_dls = m_stattree.InsertItem(GetResString(IDS_DWTOT_TSD), h_total_downloads);
	h_total_size_dld = m_stattree.InsertItem(GetResString(IDS_DWTOT_TCS), h_total_downloads);
	h_total_size_left_to_dl = m_stattree.InsertItem(GetResString(IDS_DWTOT_TSL), h_total_downloads);
	h_total_size_left_on_drive = m_stattree.InsertItem(GetResString(IDS_DWTOT_FS), h_total_downloads);
	h_total_size_needed = m_stattree.InsertItem(GetResString(IDS_DWTOT_TSN), h_total_downloads);

#ifdef _DEBUG
	if (g_pfnPrevCrtAllocHook) {
		h_debug = m_stattree.InsertItem(_T("Debug info"));m_stattree.SetItemData(h_debug, 0);
		h_blocks = m_stattree.InsertItem(_T("Blocks"), h_debug);m_stattree.SetItemData(h_blocks, 1);
		debug1 = m_stattree.InsertItem(_T("Free"), h_blocks);m_stattree.SetItemData(debug1, 1);
		debug2 = m_stattree.InsertItem(_T("Normal"), h_blocks);m_stattree.SetItemData(debug2, 1);
		debug3 = m_stattree.InsertItem(_T("CRT"), h_blocks);m_stattree.SetItemData(debug3, 1);
		debug4 = m_stattree.InsertItem(_T("Ignore"), h_blocks);m_stattree.SetItemData(debug4, 1);
		debug5 = m_stattree.InsertItem(_T("Client"), h_blocks);m_stattree.SetItemData(debug5, 1);
		m_stattree.Expand(h_debug, TVE_EXPAND);
		m_stattree.Expand(h_blocks, TVE_EXPAND);
	}
#endif

#ifdef USE_MEM_STATS
	h_allocs = m_stattree.InsertItem(_T("Allocations"));
	for (int i = 0; i < ALLOC_SLOTS; ++i) {
		unsigned uStart, uEnd;
		if (i <= 1)
			uStart = uEnd = i;
		else {
			uStart = 1 << (i - 1);
			uEnd = (i == ALLOC_SLOTS - 1) ? UINT_MAX : (uStart << 1) - 1;
		}
		CString strLabel;
		strLabel.Format(_T("Block size %08X-%08X: %s (%1.1f%%)"), uStart, uEnd, (LPCTSTR)CastItoIShort(g_aAllocStats[i], false, 2), 0.0);
		h_allocSizes[i] = m_stattree.InsertItem(strLabel, h_allocs);
	}
#endif

	// Make section headers bold in order to make the tree easier to view at a glance.
	m_stattree.SetItemState(h_transfer, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_connection, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_time, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(htime_s, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(htime_t, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(htime_aap, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_total_downloads, TVIS_BOLD, TVIS_BOLD);
	for (int i = 0; i < 3; ++i) {
		m_stattree.SetItemState(time_aaph[i], TVIS_BOLD, TVIS_BOLD);
		m_stattree.SetItemState(time_aap_hup[i], TVIS_BOLD, TVIS_BOLD);
		m_stattree.SetItemState(time_aap_hdown[i], TVIS_BOLD, TVIS_BOLD);
	}
	m_stattree.SetItemState(h_clients, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_servers, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_shared, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_upload, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_download, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_up_session, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_up_total, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_down_session, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_down_total, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_conn_session, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(h_conn_total, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hsrv_records, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hshar_records, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hconn_sg, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hconn_su, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hconn_sd, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hconn_tg, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hconn_tu, TVIS_BOLD, TVIS_BOLD);
	m_stattree.SetItemState(hconn_td, TVIS_BOLD, TVIS_BOLD);

	// Expand our purdy new tree...
	m_stattree.ApplyExpandedMask(thePrefs.GetExpandedTreeItems());

	// Select the top item so that the tree is not scrolled to the bottom when first viewed.
	m_stattree.SelectItem(h_transfer);
	m_stattree.Init();

	// Initialize our client version counts
	memset(cli_lastCount, 0, sizeof cli_lastCount);

	// End Tree Setup
}

void CStatisticsDlg::OnStnDblclickScopeD()
{
	theApp.emuledlg->ShowPreferences(IDD_PPG_STATS);
}

void CStatisticsDlg::OnStnDblclickScopeU()
{
	theApp.emuledlg->ShowPreferences(IDD_PPG_STATS);
}

void CStatisticsDlg::OnStnDblclickStatsscope()
{
	theApp.emuledlg->ShowPreferences(IDD_PPG_STATS);
}

LRESULT CStatisticsDlg::OnOscopePositionMsg(WPARAM, LPARAM lParam)
{
	LPCTSTR pszInfo = (LPCTSTR)lParam;
	m_TimeToolTips->UpdateTipText(pszInfo, GetDlgItem(IDC_SCOPE_D));
	m_TimeToolTips->UpdateTipText(pszInfo, GetDlgItem(IDC_SCOPE_U));
	m_TimeToolTips->UpdateTipText(pszInfo, GetDlgItem(IDC_STATSSCOPE));
	m_TimeToolTips->Update();
	return 0;
}

BOOL CStatisticsDlg::PreTranslateMessage(MSG *pMsg)
{
	m_TimeToolTips->RelayEvent(pMsg);

	if (pMsg->message == WM_KEYDOWN) {
		// Don't handle Ctrl+Tab in this window. It will be handled by main window.
		if (pMsg->wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0)
			return FALSE;
	}

	return CDialog::PreTranslateMessage(pMsg);
}

BOOL CStatisticsDlg::OnHelpInfo(HELPINFO*)
{
	theApp.ShowHelp(eMule_FAQ_GUI_Statistics);
	return TRUE;
}

HBRUSH CStatisticsDlg::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = theApp.emuledlg->GetCtlColor(pDC, pWnd, nCtlColor);
	return hbr ? hbr : __super::OnCtlColor(pDC, pWnd, nCtlColor);
}