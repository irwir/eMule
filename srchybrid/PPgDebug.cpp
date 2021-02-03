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
#include "PPgDebug.h"
#include "Preferences.h"
#include "OtherFunctions.h"
#include "UserMsgs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


///////////////////////////////////////////////////////////////////////////////
// CPPgDebug dialog

IMPLEMENT_DYNAMIC(CPPgDebug, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgDebug, CPropertyPage)
	ON_WM_DESTROY()
	ON_MESSAGE(UM_TREEOPTSCTRL_NOTIFY, OnTreeOptsCtrlNotify)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

CPPgDebug::CPPgDebug()
	: CPropertyPage(CPPgDebug::IDD)
	, m_ctrlTreeOptions(theApp.m_iDfltImageListColorFlags)
{
	ClearAllMembers();
}

void CPPgDebug::ClearAllMembers()
{
	m_bInitializedTreeOpts = false;
	m_htiServer = NULL;
	m_htiClient = NULL;
	memset(m_cb, 0, sizeof m_cb);
	memset(m_lv, 0, sizeof m_lv);
	memset(m_checks, 0, sizeof m_checks);
	memset(m_levels, 0, sizeof m_levels);
	memset(m_htiInteger, 0, sizeof m_htiInteger);
	memset(m_iValInteger, 0, sizeof m_iValInteger);
}

void CPPgDebug::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DEBUG_OPTS, m_ctrlTreeOptions);
	if (!m_bInitializedTreeOpts) {
		CImageList *piml = m_ctrlTreeOptions.GetImageList(TVSIL_NORMAL);
		if (piml) {
			HICON hIcon = theApp.LoadIcon(_T("Server"));
			if (hIcon)
				VERIFY(::DestroyIcon(hIcon));

			hIcon = theApp.LoadIcon(_T("StatsClients"));
			if (hIcon)
				VERIFY(::DestroyIcon(hIcon));
		}

#define	ADD_DETAIL_ITEM(idx, label, group) \
		m_cb[idx] = m_ctrlTreeOptions.InsertCheckBox(label, group); \
		m_lv[idx] = m_ctrlTreeOptions.InsertItem(_T("Level"), TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, m_cb[idx]); \
		m_ctrlTreeOptions.AddEditBox(m_lv[idx], RUNTIME_CLASS(CNumTreeOptionsEdit))

#define	ADD_INTEGER_ITEM(idx, label, group) \
		m_htiInteger[idx] = m_ctrlTreeOptions.InsertItem(label, TREEOPTSCTRLIMG_EDIT, TREEOPTSCTRLIMG_EDIT, group); \
		m_ctrlTreeOptions.AddEditBox(m_htiInteger[idx], RUNTIME_CLASS(CNumTreeOptionsEdit))

		m_htiServer = m_ctrlTreeOptions.InsertCheckBox(_T("Server"), TVI_ROOT, FALSE);
		ADD_DETAIL_ITEM(0, _T("TCP"), m_htiServer);
		ADD_DETAIL_ITEM(1, _T("UDP"), m_htiServer);
		ADD_DETAIL_ITEM(2, _T("Sources"), m_htiServer);
		ADD_DETAIL_ITEM(3, _T("Searches"), m_htiServer);

		m_htiClient = m_ctrlTreeOptions.InsertCheckBox(_T("Client"), TVI_ROOT, FALSE);
		ADD_DETAIL_ITEM(4, _T("TCP"), m_htiClient);
		ADD_DETAIL_ITEM(5, _T("UDP (eD2K)"), m_htiClient);
		ADD_DETAIL_ITEM(6, _T("UDP (Kad)"), m_htiClient);

		ADD_INTEGER_ITEM(0, _T("Memory corruption check level"), TVI_ROOT);

#undef ADD_DETAIL_ITEM
#undef ADD_INTEGER_ITEM

		m_ctrlTreeOptions.Expand(m_htiServer, TVE_EXPAND);
		m_ctrlTreeOptions.Expand(m_htiClient, TVE_EXPAND);
		m_ctrlTreeOptions.SendMessage(WM_VSCROLL, SB_TOP);
		m_bInitializedTreeOpts = true;
	}

	for (unsigned i = 0; i < _countof(m_cb); ++i)
		DDX_TreeCheck(pDX, IDC_DEBUG_OPTS, m_cb[i], m_checks[i]);
	m_ctrlTreeOptions.UpdateCheckBoxGroup(m_htiServer);
	m_ctrlTreeOptions.UpdateCheckBoxGroup(m_htiClient);

	for (unsigned i = 0; i < _countof(m_lv); ++i)
		DDX_TreeEdit(pDX, IDC_DEBUG_OPTS, m_lv[i], m_levels[i]);

	for (unsigned i = 0; i < _countof(m_htiInteger); ++i)
		DDX_TreeEdit(pDX, IDC_DEBUG_OPTS, m_htiInteger[i], m_iValInteger[i]);
}

BOOL CPPgDebug::OnInitDialog()
{
#define	SET_DETAIL_OPT(idx, var) \
	m_checks[idx] = ((var) > 0); \
	m_levels[idx] = ((var) > 0) ? (var) : -(var)

#define	SET_INTEGER_OPT(idx, var) \
	m_iValInteger[idx] = var

	SET_DETAIL_OPT(0, thePrefs.m_iDebugServerTCPLevel);
	SET_DETAIL_OPT(1, thePrefs.m_iDebugServerUDPLevel);
	SET_DETAIL_OPT(2, thePrefs.m_iDebugServerSourcesLevel);
	SET_DETAIL_OPT(3, thePrefs.m_iDebugServerSearchesLevel);
	SET_DETAIL_OPT(4, thePrefs.m_iDebugClientTCPLevel);
	SET_DETAIL_OPT(5, thePrefs.m_iDebugClientUDPLevel);
	SET_DETAIL_OPT(6, thePrefs.m_iDebugClientKadUDPLevel);

	SET_INTEGER_OPT(0, thePrefs.m_iDbgHeap);

#undef SET_OPT
#undef SET_INTEGER_OPT

	m_ctrlTreeOptions.SetImageListColorFlags(theApp.m_iDfltImageListColorFlags);
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	return TRUE;  // return TRUE unless you set the focus to the control
				  // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPgDebug::OnKillActive()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit
	// any possibly pending data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();
	return CPropertyPage::OnKillActive();
}

BOOL CPPgDebug::OnApply()
{
	// if prop page is closed by pressing ENTER we have to explicitly commit
	// any possibly pending data from an open edit control
	m_ctrlTreeOptions.HandleChildControlLosingFocus();

	if (!UpdateData())
		return FALSE;

#define	GET_DETAIL_OPT(idx, opt) \
	if (m_checks[idx]) \
		opt = (m_levels[idx] > 0) ? m_levels[idx] : 1; \
	else \
		opt = -m_levels[idx]

#define	GET_INTEGER_OPT(idx, opt) \
	opt = m_iValInteger[idx]

	GET_DETAIL_OPT(0, thePrefs.m_iDebugServerTCPLevel);
	GET_DETAIL_OPT(1, thePrefs.m_iDebugServerUDPLevel);
	GET_DETAIL_OPT(2, thePrefs.m_iDebugServerSourcesLevel);
	GET_DETAIL_OPT(3, thePrefs.m_iDebugServerSearchesLevel);
	GET_DETAIL_OPT(4, thePrefs.m_iDebugClientTCPLevel);
	GET_DETAIL_OPT(5, thePrefs.m_iDebugClientUDPLevel);
	GET_DETAIL_OPT(6, thePrefs.m_iDebugClientKadUDPLevel);

	GET_INTEGER_OPT(0, thePrefs.m_iDbgHeap);

#undef GET_DETAIL_OPT
#undef GET_INTEGER_OPT

	SetModified(FALSE);
	return CPropertyPage::OnApply();
}

void CPPgDebug::OnDestroy()
{
	m_ctrlTreeOptions.DeleteAllItems();
	m_ctrlTreeOptions.DestroyWindow();
	ClearAllMembers();
	CPropertyPage::OnDestroy();
}

LRESULT CPPgDebug::OnTreeOptsCtrlNotify(WPARAM wParam, LPARAM)
{
	if (wParam == IDC_DEBUG_OPTS) {
		//TREEOPTSCTRLNOTIFY *pton = (TREEOPTSCTRLNOTIFY*)lParam;
		SetModified();
	}
	return 0;
}

void CPPgDebug::OnHelp()
{
	//theApp.ShowHelp(0);
}

BOOL CPPgDebug::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgDebug::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}