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
#include "PreferencesDlg.h"
#include "SMTPdialog.h"
#include "emuledlg.h"
#include "StatisticsDlg.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CSMTPserverDlg dialog

IMPLEMENT_DYNAMIC(CSMTPserverDlg, CDialog)

BEGIN_MESSAGE_MAP(CSMTPserverDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
	ON_CBN_SELCHANGE(IDC_SMTPAUTH, OnCbnSelChangeAuthMethod)
	ON_CBN_SELCHANGE(IDC_SMTPSEC, OnCbnSelChangeSecurity)
END_MESSAGE_MAP()

CSMTPserverDlg::CSMTPserverDlg(CWnd *pParent /*=NULL*/)
	: CDialog(CSMTPserverDlg::IDD, pParent)
	, m_icoWnd()
	, m_mail(thePrefs.GetEmailSettings())
{
}

CSMTPserverDlg::~CSMTPserverDlg()
{
	if (m_icoWnd)
		VERIFY(::DestroyIcon(m_icoWnd));
}

void CSMTPserverDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CSMTPserverDlg::OnBnClickedOk()
{
	m_mail.uTLS = (TLSmode)static_cast<CComboBox*>(GetDlgItem(IDC_SMTPSEC))->GetCurSel();
	CString str;
	if (GetDlgItemText(IDC_TXT_SMTPSERVER, str)) {
		int iColon = str.Find(':');
		if (iColon >= 0) {
			SetDlgItemText(IDC_SMTPPORT, str.Mid(iColon + 1));
			str.Truncate(iColon);
		}
	}
	m_mail.sServer = str;
	m_mail.uPort = (uint16)GetDlgItemInt(IDC_SMTPPORT, NULL, FALSE);
	m_mail.uAuth = (SMTPauth)static_cast<CComboBox*>(GetDlgItem(IDC_SMTPAUTH))->GetCurSel();
	if (GetDlgItemText(IDC_SMTPUSER, str))
		m_mail.sUser = str;
	else
		m_mail.sUser.Empty();
	if (GetDlgItemText(IDC_SMTPPASS, str)) {
		if (str != sHiddenPassword)
			m_mail.sPass = str;
	} else
		m_mail.sPass.Empty();

	thePrefs.SetEmailSettings(m_mail);
	CDialog::OnOK();
}

void CSMTPserverDlg::OnBnClickedCancel()
{
	CDialog::OnCancel();
}

void CSMTPserverDlg::OnCbnSelChangeAuthMethod()
{
	BOOL bEnable = static_cast<CComboBox*>(GetDlgItem(IDC_SMTPAUTH))->GetCurSel() > 0;
	GetDlgItem(IDC_SMTPUSER)->EnableWindow(bEnable);
	GetDlgItem(IDC_SMTPPASS)->EnableWindow(bEnable);
}

void CSMTPserverDlg::OnCbnSelChangeSecurity()
{
	UINT port;
	switch ((TLSmode)static_cast<CComboBox*>(GetDlgItem(IDC_SMTPSEC))->GetCurSel()) {
	case MODE_NONE:
		port = 25; //deprecated
		break;
	case MODE_SSL_TLS:
		port = 465;
		break;
	//case MODE_STARTTLS:
	default:
		port = 587;
	}
	SetDlgItemInt(IDC_SMTPPORT, port, FALSE);
}

BOOL CSMTPserverDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	InitWindowStyles(this);

	SetIcon(m_icoWnd = theApp.LoadIcon(_T("Email")), FALSE);

	static_cast<CComboBox*>(GetDlgItem(IDC_SMTPSEC))->SetCurSel(m_mail.uTLS);
	SetDlgItemText(IDC_TXT_SMTPSERVER, m_mail.sServer);
	if (m_mail.uPort)
		SetDlgItemInt(IDC_SMTPPORT, m_mail.uPort);
	else
		OnCbnSelChangeSecurity();
	SetDlgItemText(IDC_SMTPUSER, m_mail.sUser);
	SetDlgItemText(IDC_SMTPPASS, sHiddenPassword);
	static_cast<CComboBox*>(GetDlgItem(IDC_SMTPAUTH))->SetCurSel(m_mail.uAuth);
	OnCbnSelChangeAuthMethod();
	static_cast<CEdit*>(GetDlgItem(IDC_SMTPUSER))->SetLimitText(254);
	static_cast<CEdit*>(GetDlgItem(IDC_SMTPPASS))->SetLimitText(32);
	static_cast<CEdit*>(GetDlgItem(IDC_SMTPPORT))->SetLimitText(5);

	Localize();

	return TRUE;
}

void CSMTPserverDlg::Localize()
{
	SetWindowText(GetResString(IDS_SMTPSERVER));

	SetDlgItemText(IDC_SMTPCONN_GROUP, GetResString(IDS_CONNECTION));
	SetDlgItemText(IDC_SMTPSEC_LBL, GetResString(IDS_SECURITY) + _T(":"));
	SetDlgItemText(IDC_SMTPSERVER_LBL, GetResString(IDS_SERVER) + _T(":"));
	SetDlgItemText(IDC_SMTPPORT_LBL, GetResString(IDS_PORT) + _T(":"));

	SetDlgItemText(IDC_SMTPAUTH_GROUP, GetResString(IDS_AUTH));
	SetDlgItemText(IDC_SMTPAUTH_LBL, GetResString(IDS_AUTHMETHOD) + _T(":"));
	SetDlgItemText(IDC_SMTPUSER_LBL, GetResString(IDS_QL_USERNAME) + _T(":"));
	SetDlgItemText(IDC_SMTPPASS_LBL, GetResString(IDS_WS_PASS) + _T(":"));
}