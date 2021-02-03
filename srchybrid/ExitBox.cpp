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
#include "resource.h"
#include "eMule.h"
#include "ExitBox.h"
#include "Preferences.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNAMIC(ExitBox, CDialog)

BEGIN_MESSAGE_MAP(ExitBox, CDialog)
	ON_WM_CTLCOLOR()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

ExitBox::ExitBox(CWnd *pParent)
	: CDialog(ExitBox::IDD, pParent)
	, m_cancel(true)
{
	m_brush.CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
}

void ExitBox::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
}

void ExitBox::OnOK()
{
	CDialog::OnOK();
	m_cancel = false;
	if (IsDlgButtonChecked(IDC_DONOTASKAGAIN))
		thePrefs.SetConfirmExit(false);
}

BOOL ExitBox::OnInitDialog()
{
	CDialog::OnInitDialog();
	InitWindowStyles(this);
	CStatic *pic = static_cast<CStatic*>(GetDlgItem(IDC_STATIC));
	pic->SetIcon(::LoadIcon(NULL, IDI_QUESTION));

	SetWindowText(GetResString(IDS_CLOSEEMULE));
	SetDlgItemText(IDC_MAIN_EXIT, GetResString(IDS_MAIN_EXIT));
	SetDlgItemText(IDOK, GetResString(IDS_YES) );
	SetDlgItemText(IDCANCEL, GetResString(IDS_NO));
	SetDlgItemText(IDC_DONOTASKAGAIN, GetResString(IDS_DONOTASKAGAIN));

	PostMessage(WM_NEXTDLGCTL, (WPARAM)GetDlgItem(IDCANCEL)->GetSafeHwnd(), TRUE);
	return TRUE;
}

BOOL ExitBox::OnEraseBkgnd(CDC *pDC)
{
	CDialog::OnEraseBkgnd(pDC);
	// get clipping rectangle
	CRect rcClip;
	pDC->GetClipBox(rcClip);
	rcClip.bottom /= 2;
	// fill rectangle using a given brush
	pDC->FillRect(rcClip, &m_brush);
	return TRUE; // returns non-zero to prevent further erasing
}

HBRUSH ExitBox::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	int id = pWnd->GetDlgCtrlID();
	if (id != IDC_MAIN_EXIT && id != IDC_STATIC)
		return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
	return m_brush;
}