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
#include "PPgWebServer.h"
#include "otherfunctions.h"
#include "WebServer.h"
#include "emuledlg.h"
#include "Preferences.h"
#include "ServerWnd.h"
#include "HelpIDs.h"
#include "ppgwebserver.h"
#include "UPnPImplWrapper.h"
#include "UPnPImpl.h"
#include "Log.h"
#include "TLSthreading.h"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/md.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

struct options
{
	LPCTSTR	issuer_key;		//filename of the issuer key file
	LPCTSTR cert_file;		//where to store the constructed certificate file
	LPCSTR	subject_name;	//subject name for certificate
	LPCSTR	issuer_name;	//issuer name for certificate
	LPCSTR	not_before;		//validity period not before
	LPCSTR	not_after;		//validity period not after
	LPCSTR	serial;			//serial number string
};

static int write_buffer(LPCTSTR output_file, const unsigned char *buffer)
{
	FILE *f = _tfopen(output_file, _T("wb"));
	if (f == NULL)
		return -1;
	size_t len = strlen((char*)buffer);
	if (fwrite((void*)buffer, 1, len, f) != len) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

static int write_private_key(mbedtls_pk_context *key, LPCTSTR output_file)
{
	unsigned char output_buf[16000];

	int ret = mbedtls_pk_write_key_pem(key, output_buf, sizeof(output_buf));
	return ret ? ret : write_buffer(output_file, output_buf);
}

int KeyCreate(mbedtls_pk_context *key, mbedtls_ctr_drbg_context *ctr_drbg, LPCTSTR output_file)
{
	mbedtls_mpi N, P, Q, D, E, DP, DQ, QP;
	LPCTSTR pmsg = NULL;
	int ret;

	mbedtls_mpi_init(&N);
	mbedtls_mpi_init(&P);
	mbedtls_mpi_init(&Q);
	mbedtls_mpi_init(&D);
	mbedtls_mpi_init(&E);
	mbedtls_mpi_init(&DP);
	mbedtls_mpi_init(&DQ);
	mbedtls_mpi_init(&QP);

	//create RSA 2048 key
	ret = mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
	if (ret) {
		pmsg = _T("mbedtls_pk_setup");
		goto exit;
	}
	ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, ctr_drbg, 2048u, 65537);
	if (ret) {
		pmsg = _T("mbedtls_rsa_gen_key");
		goto exit;
	}

	//write the key to a file
	ret = write_private_key(key, output_file);
	if (ret)
		DebugLogError(_T("Error: writing private key failed"));

exit:
	mbedtls_mpi_free(&N);
	mbedtls_mpi_free(&P);
	mbedtls_mpi_free(&Q);
	mbedtls_mpi_free(&D);
	mbedtls_mpi_free(&E);
	mbedtls_mpi_free(&DP);
	mbedtls_mpi_free(&DQ);
	mbedtls_mpi_free(&QP);

	if (pmsg)
		DebugLogError(_T("Error: %s returned -0x%04x - %s"), pmsg, -ret, (LPCTSTR)SSLerror(ret));
	return ret;
}

int write_certificate(mbedtls_x509write_cert *crt, LPCTSTR output_file, int(*f_rng)(void*, unsigned char*, size_t), void *p_rng)
{
	unsigned char output_buf[4096];

	int ret = mbedtls_x509write_crt_pem(crt, output_buf, 4096, f_rng, p_rng);
	return ret ? ret : write_buffer(output_file, output_buf);
}

int CertCreate(const struct options &opt)
{
	static const char pers[] = "cert create";
	mbedtls_pk_context issuer_key;
	mbedtls_x509write_cert crt;
	mbedtls_mpi serial;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	LPCTSTR pmsg = NULL;
	int ret;

	mbedtls_threading_set_alt(threading_mutex_init_alt, threading_mutex_free_alt, threading_mutex_lock_alt, threading_mutex_unlock_alt);
	mbedtls_x509write_crt_init(&crt);
	mbedtls_pk_init(&issuer_key);
	mbedtls_mpi_init(&serial);
	mbedtls_ctr_drbg_init(&ctr_drbg);

	//seed the PRNG
	mbedtls_entropy_init(&entropy);
	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (unsigned char*)pers, strlen(pers));
	if (ret) {
		DebugLogError(_T("Error: mbedtls_ctr_drbg_seed returned %d - %s"), ret, (LPCTSTR)SSLerror(ret));
		goto exit;
	}

	//generate the key
	ret = KeyCreate(&issuer_key, &ctr_drbg, opt.issuer_key);
	if (ret)
		goto exit;

	mbedtls_x509write_crt_set_subject_key(&crt, &issuer_key);
	mbedtls_x509write_crt_set_issuer_key(&crt, &issuer_key);

	// Parse serial to MPI
	ret = mbedtls_mpi_read_string(&serial, 10, opt.serial);
	if (ret) {
		pmsg = _T("mbedtls_mpi_read_string");
		goto exit;
	}

	//set parameters
	ret = mbedtls_x509write_crt_set_subject_name(&crt, opt.subject_name);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_subject_name");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_issuer_name(&crt, opt.issuer_name);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_issuer_name");
		goto exit;
	}

	mbedtls_x509write_crt_set_version(&crt, 2); //2 for v3 version
	mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

	ret = mbedtls_x509write_crt_set_serial(&crt, &serial);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_serial");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_validity(&crt, opt.not_before, opt.not_after);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_validity");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, 0);
	if (ret) {
		pmsg = _T("x509write_crt_set_basic_contraints");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_subject_key_identifier(&crt);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_subject_key_identifier");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_authority_key_identifier(&crt);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_authority_key_identifier");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_KEY_ENCIPHERMENT);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_key_usage");
		goto exit;
	}
	ret = mbedtls_x509write_crt_set_ns_cert_type(&crt, MBEDTLS_X509_NS_CERT_TYPE_SSL_SERVER);
	if (ret) {
		pmsg = _T("mbedtls_x509write_crt_set_ns_cert_type");
		goto exit;
	}

	//write the certificate to a file
	ret = write_certificate(&crt, opt.cert_file, mbedtls_ctr_drbg_random, &ctr_drbg);
	if (ret)
		pmsg = _T("write_certificate");

exit:
	mbedtls_x509write_crt_free(&crt);
	mbedtls_pk_free(&issuer_key);
	mbedtls_mpi_free(&serial);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);

	if (pmsg)
		DebugLogError(_T("Error: %s returned -0x%04x - %s"), pmsg, -ret, (LPCTSTR)SSLerror(ret));
	return ret;
}

IMPLEMENT_DYNAMIC(CPPgWebServer, CPropertyPage)

BEGIN_MESSAGE_MAP(CPPgWebServer, CPropertyPage)
	ON_EN_CHANGE(IDC_WSPASS, OnDataChange)
	ON_EN_CHANGE(IDC_WSPASSLOW, OnDataChange)
	ON_EN_CHANGE(IDC_WSPORT, OnDataChange)
	ON_EN_CHANGE(IDC_TMPLPATH, OnDataChange)
	ON_EN_CHANGE(IDC_CERTPATH, OnDataChange)
	ON_EN_CHANGE(IDC_KEYPATH, OnDataChange)
	ON_EN_CHANGE(IDC_WSTIMEOUT, OnDataChange)
	ON_BN_CLICKED(IDC_WSENABLED, OnEnChangeWSEnabled)
	ON_BN_CLICKED(IDC_WEB_HTTPS, OnChangeHTTPS)
	ON_BN_CLICKED(IDC_WEB_GENERATE, OnGenerateCertificate)
	ON_BN_CLICKED(IDC_WSENABLEDLOW, OnEnChangeWSEnabled)
	ON_BN_CLICKED(IDC_WSRELOADTMPL, OnReloadTemplates)
	ON_BN_CLICKED(IDC_TMPLBROWSE, OnBnClickedTmplbrowse)
	ON_BN_CLICKED(IDC_CERTBROWSE, OnBnClickedCertbrowse)
	ON_BN_CLICKED(IDC_KEYBROWSE, OnBnClickedKeybrowse)
	ON_BN_CLICKED(IDC_WS_GZIP, OnDataChange)
	ON_BN_CLICKED(IDC_WS_ALLOWHILEVFUNC, OnDataChange)
	ON_BN_CLICKED(IDC_WSUPNP, OnDataChange)
	ON_WM_HELPINFO()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CPPgWebServer::CPPgWebServer()
	: CPropertyPage(CPPgWebServer::IDD)
	, m_generating()
	, m_bNewCert()
	, m_bModified()
	, m_icoBrowse()
{
}

void CPPgWebServer::DoDataExchange(CDataExchange *pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}

BOOL CPPgWebServer::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	InitWindowStyles(this);

	AddBuddyButton(GetDlgItem(IDC_TMPLPATH)->m_hWnd, ::GetDlgItem(m_hWnd, IDC_TMPLBROWSE));
	InitAttachedBrowseButton(::GetDlgItem(m_hWnd, IDC_TMPLBROWSE), m_icoBrowse);

	AddBuddyButton(GetDlgItem(IDC_CERTPATH)->m_hWnd, ::GetDlgItem(m_hWnd, IDC_CERTBROWSE));
	InitAttachedBrowseButton(::GetDlgItem(m_hWnd, IDC_CERTBROWSE), m_icoBrowse);

	AddBuddyButton(GetDlgItem(IDC_KEYPATH)->m_hWnd, ::GetDlgItem(m_hWnd, IDC_KEYBROWSE));
	InitAttachedBrowseButton(::GetDlgItem(m_hWnd, IDC_KEYBROWSE), m_icoBrowse);

	static_cast<CEdit*>(GetDlgItem(IDC_WSPASS))->SetLimitText(32);
	static_cast<CEdit*>(GetDlgItem(IDC_WSPASSLOW))->SetLimitText(32);
	static_cast<CEdit*>(GetDlgItem(IDC_WSPORT))->SetLimitText(5);

	LoadSettings();
	Localize();

	OnEnChangeWSEnabled();

	return TRUE;
}

void CPPgWebServer::LoadSettings()
{
	CheckDlgButton(IDC_WSENABLED, static_cast<UINT>(thePrefs.GetWSIsEnabled()));
	CheckDlgButton(IDC_WS_GZIP, static_cast<UINT>(thePrefs.GetWebUseGzip()));

	CheckDlgButton(IDC_WSUPNP, static_cast<UINT>(thePrefs.m_bWebUseUPnP));
	GetDlgItem(IDC_WSUPNP)->EnableWindow(thePrefs.IsUPnPEnabled() && thePrefs.GetWSIsEnabled());
	SetDlgItemInt(IDC_WSPORT, thePrefs.GetWSPort());

	SetDlgItemText(IDC_TMPLPATH, thePrefs.GetTemplate());
	SetDlgItemInt(IDC_WSTIMEOUT, thePrefs.GetWebTimeoutMins());

	CheckDlgButton(IDC_WEB_HTTPS, static_cast<UINT>(thePrefs.GetWebUseHttps()));
	SetDlgItemText(IDC_CERTPATH, thePrefs.GetWebCertPath());
	SetDlgItemText(IDC_KEYPATH, thePrefs.GetWebKeyPath());

	SetDlgItemText(IDC_WSPASS, sHiddenPassword);
	CheckDlgButton(IDC_WS_ALLOWHILEVFUNC, static_cast<UINT>(thePrefs.GetWebAdminAllowedHiLevFunc()));
	CheckDlgButton(IDC_WSENABLEDLOW, static_cast<UINT>(thePrefs.GetWSIsLowUserEnabled()));
	SetDlgItemText(IDC_WSPASSLOW, sHiddenPassword);

	SetModified(FALSE);	// FoRcHa
}

void CPPgWebServer::OnDataChange()
{
	SetModified();
	SetTmplButtonState();
}

BOOL CPPgWebServer::OnApply()
{
	if (m_bModified) {
		CString sBuf;
		bool bUPnP = thePrefs.GetWSUseUPnP();
		bool bWSIsEnabled = IsDlgButtonChecked(IDC_WSENABLED) != 0;
		// get and check template file existence...
		GetDlgItemText(IDC_TMPLPATH, sBuf);
		if (bWSIsEnabled && !PathFileExists(sBuf)) {
			CString buffer;
			buffer.Format(GetResString(IDS_WEB_ERR_CANTLOAD), (LPCTSTR)sBuf);
			AfxMessageBox(buffer, MB_OK);
			return FALSE;
		}
		thePrefs.SetTemplate(sBuf);
		if (!theApp.webserver->ReloadTemplates()) {
			GetDlgItem(IDC_TMPLPATH)->SetFocus();
			return FALSE;
		}

		bool bHTTPS = IsDlgButtonChecked(IDC_WEB_HTTPS) != 0;
		GetDlgItemText(IDC_CERTPATH, sBuf);
		if (bWSIsEnabled && bHTTPS) {
			if (!PathFileExists(sBuf)) {
				AfxMessageBox(GetResString(IDS_CERT_NOT_FOUND), MB_OK);
				return FALSE;
			}
			if (!m_bNewCert)
				m_bNewCert = !thePrefs.GetWebCertPath().CompareNoCase(sBuf);
		}
		thePrefs.SetWebCertPath(sBuf);

		GetDlgItemText(IDC_KEYPATH, sBuf);
		if (bWSIsEnabled && bHTTPS) {
			if (!PathFileExists(sBuf)) {
				AfxMessageBox(GetResString(IDS_KEY_NOT_FOUND), MB_OK);
				return FALSE;
			}
			if (!m_bNewCert)
				m_bNewCert = !thePrefs.GetWebKeyPath().CompareNoCase(sBuf);
		}
		thePrefs.SetWebKeyPath(sBuf);

		GetDlgItemText(IDC_WSPASS, sBuf);
		if (sBuf != sHiddenPassword) {
			thePrefs.SetWSPass(sBuf);
			SetDlgItemText(IDC_WSPASS, sHiddenPassword);
		}

		GetDlgItemText(IDC_WSPASSLOW, sBuf);
		if (sBuf != sHiddenPassword) {
			thePrefs.SetWSLowPass(sBuf);
			SetDlgItemText(IDC_WSPASSLOW, sHiddenPassword);
		}

		thePrefs.m_iWebTimeoutMins = (int)GetDlgItemInt(IDC_WSTIMEOUT, NULL, FALSE);

		uint16 u = (uint16)GetDlgItemInt(IDC_WSPORT, NULL, FALSE);
		if (u > 0 && u != thePrefs.GetWSPort()) {
			thePrefs.SetWSPort(u);
			theApp.webserver->RestartSockets();
		}

		if (thePrefs.GetWebUseHttps() != bHTTPS || (bHTTPS && m_bNewCert))
			theApp.webserver->StopServer();
		m_bNewCert = false;

		thePrefs.SetWSIsEnabled(bWSIsEnabled);
		thePrefs.SetWebUseGzip(IsDlgButtonChecked(IDC_WS_GZIP) != 0);
		thePrefs.SetWebUseHttps(bHTTPS);
		thePrefs.SetWSIsLowUserEnabled(IsDlgButtonChecked(IDC_WSENABLEDLOW) != 0);
		theApp.webserver->StartServer();
		thePrefs.m_bAllowAdminHiLevFunc = IsDlgButtonChecked(IDC_WS_ALLOWHILEVFUNC) != 0;

		thePrefs.m_bWebUseUPnP = IsDlgButtonChecked(IDC_WSUPNP) != 0;
		//add the port to existing mapping without having eMule restarting (if all conditions are met)
		if (bUPnP != (thePrefs.m_bWebUseUPnP && bWSIsEnabled) && thePrefs.IsUPnPEnabled() && theApp.m_pUPnPFinder != NULL)
			theApp.m_pUPnPFinder->GetImplementation()->LateEnableWebServerPort(bUPnP ? 0 : thePrefs.GetWSPort());

		theApp.emuledlg->serverwnd->UpdateMyInfo();
		SetModified(FALSE);
		SetTmplButtonState();
	}

	return CPropertyPage::OnApply();
}

void CPPgWebServer::Localize()
{
	if (m_hWnd) {
		SetWindowText(GetResString(IDS_PW_WS));

		SetDlgItemText(IDC_WSENABLED, GetResString(IDS_ENABLED));
		SetDlgItemText(IDC_WS_GZIP, GetResString(IDS_WEB_GZIP_COMPRESSION));
		SetDlgItemText(IDC_WSUPNP, GetResString(IDS_WEBUPNPINCLUDE));
		SetDlgItemText(IDC_WSPORT_LBL, GetResString(IDS_PORT) + _T(':'));

		SetDlgItemText(IDC_TEMPLATE, GetResString(IDS_WS_RELOAD_TMPL) + _T(':'));
		SetDlgItemText(IDC_WSRELOADTMPL, GetResString(IDS_SF_RELOAD));

		SetDlgItemText(IDC_STATIC_GENERAL, GetResString(IDS_PW_GENERAL));

		SetDlgItemText(IDC_WSTIMEOUTLABEL, GetResString(IDS_WEB_SESSIONTIMEOUT) + _T(':'));
		SetDlgItemText(IDC_MINS, GetResString(IDS_LONGMINS).MakeLower());

		SetDlgItemText(IDC_WEB_HTTPS, GetResString(IDS_WEB_HTTPS));
		SetDlgItemText(IDC_WEB_GENERATE, GetResString(IDS_WEB_GENERATE));
		SetDlgItemText(IDC_WEB_CERT, GetResString(IDS_CERTIFICATE) + _T(':'));
		SetDlgItemText(IDC_WEB_KEY, GetResString(IDS_KEY) + _T(':'));

		SetDlgItemText(IDC_STATIC_ADMIN, GetResString(IDS_ADMIN));
		SetDlgItemText(IDC_WSPASS_LBL, GetResString(IDS_WS_PASS) + _T(':'));
		SetDlgItemText(IDC_WS_ALLOWHILEVFUNC, GetResString(IDS_WEB_ALLOWHILEVFUNC));

		SetDlgItemText(IDC_STATIC_LOWUSER, GetResString(IDS_WEB_LOWUSER));
		SetDlgItemText(IDC_WSENABLEDLOW, GetResString(IDS_ENABLED));
		SetDlgItemText(IDC_WSPASS_LBL2, GetResString(IDS_WS_PASS) + _T(':'));
	}
}

void CPPgWebServer::SetUPnPState()
{
	GetDlgItem(IDC_WSUPNP)->EnableWindow(thePrefs.IsUPnPEnabled() && IsDlgButtonChecked(IDC_WSENABLED));
}

void CPPgWebServer::OnChangeHTTPS()
{
	BOOL bEnable = IsDlgButtonChecked(IDC_WSENABLED) && IsDlgButtonChecked(IDC_WEB_HTTPS);
	GetDlgItem(IDC_WEB_GENERATE)->EnableWindow(bEnable && !m_generating);
	GetDlgItem(IDC_CERTPATH)->EnableWindow(bEnable);
	GetDlgItem(IDC_CERTBROWSE)->EnableWindow(bEnable);
	GetDlgItem(IDC_KEYPATH)->EnableWindow(bEnable);
	GetDlgItem(IDC_KEYBROWSE)->EnableWindow(bEnable);
	SetModified();
}

void CPPgWebServer::OnEnChangeWSEnabled()
{
	BOOL bIsWIEnabled = (BOOL)IsDlgButtonChecked(IDC_WSENABLED);
	GetDlgItem(IDC_WS_GZIP)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WSPORT)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_TMPLPATH)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_TMPLBROWSE)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WSTIMEOUT)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WEB_HTTPS)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WSPASS)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WS_ALLOWHILEVFUNC)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WSENABLEDLOW)->EnableWindow(bIsWIEnabled);
	GetDlgItem(IDC_WSPASSLOW)->EnableWindow(bIsWIEnabled && IsDlgButtonChecked(IDC_WSENABLEDLOW));
	SetUPnPState();
	SetTmplButtonState();
	OnChangeHTTPS();

	SetModified();
}

void CPPgWebServer::OnReloadTemplates()
{
	theApp.webserver->ReloadTemplates();
}

void CPPgWebServer::OnBnClickedTmplbrowse()
{
	CString strTempl;
	GetDlgItemText(IDC_TMPLPATH, strTempl);
	CString buffer(GetResString(IDS_WS_RELOAD_TMPL) + _T(" (*.tmpl)|*.tmpl||"));
	if (DialogBrowseFile(buffer, buffer, strTempl)) {
		SetDlgItemText(IDC_TMPLPATH, buffer);
		SetModified();
	}
	SetTmplButtonState();
}

//create cert.key and cert.crt in config directory
void CPPgWebServer::OnGenerateCertificate()
{
	if (InterlockedExchange(&m_generating, 1))
		return;
	CWaitCursor curWaiting;

	const CString &confdir(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR));
	const CString &fkey(confdir + _T("cert.key"));
	const CString &fcrt(confdir + _T("cert.crt"));
	CStringA not_after, not_before, serial;
	SYSTEMTIME st;
	GetSystemTime(&st);
	not_before.Format("%4hu%02hu01000000", st.wYear, st.wMonth);
	not_after.Format("%4hu%02hu01235959", st.wYear + 1, st.wMonth);
	int nserial = rand() & 0xfff; //avoid repeated serials (kind of)
	serial.Format("%d", nserial);

	struct options opt;
	opt.issuer_key = (LPCTSTR)fkey;
	opt.cert_file = (LPCTSTR)fcrt;
	opt.subject_name = "CN=Web Interface,O=emule-project.net,OU=eMule";
	opt.issuer_name = "CN=eMule,O=emule-project.net";
	opt.not_before = (LPCSTR)not_before;
	opt.not_after = (LPCSTR)not_after;
	opt.serial = (LPCSTR)serial;
	m_bNewCert = !CertCreate(opt);
	if (m_bNewCert) {
		AddLogLine(false, _T("New certificate created; serial %d"), nserial);
		SetDlgItemText(IDC_KEYPATH, fkey);
		SetDlgItemText(IDC_CERTPATH, fcrt);
		GetDlgItem(IDC_WEB_GENERATE)->EnableWindow(FALSE);
		SetModified();
	} else {
		LogError(_T("Certificate creation failed"));
		AfxMessageBox(GetResString(IDS_CERT_ERR_CREATE));
		InterlockedExchange(&m_generating, 0); //re-enable only if failed
	}
}

void CPPgWebServer::OnBnClickedCertbrowse()
{
	CString strCert;
	GetDlgItemText(IDC_CERTPATH, strCert);
	CString buffer(GetResString(IDS_CERTIFICATE) + _T(" (*.crt)|*.crt|All Files (*.*)|*.*||"));
	if (DialogBrowseFile(buffer, buffer, strCert))
		SetDlgItemText(IDC_CERTPATH, buffer);
	if (buffer.CompareNoCase(strCert) != 0)
		SetModified();
}

void CPPgWebServer::OnBnClickedKeybrowse()
{
	CString strKey;
	GetDlgItemText(IDC_KEYPATH, strKey);
	CString buffer(GetResString(IDS_KEY) + _T(" (*.key)|*.key|All Files (*.*)|*.*||"));
	if (DialogBrowseFile(buffer, buffer, strKey))
		SetDlgItemText(IDC_KEYPATH, buffer);
	if (buffer.CompareNoCase(strKey) != 0)
		SetModified();
}

void CPPgWebServer::SetTmplButtonState()
{
	CString buffer;
	GetDlgItemText(IDC_TMPLPATH, buffer);

	GetDlgItem(IDC_WSRELOADTMPL)->EnableWindow(IsDlgButtonChecked(IDC_WSENABLED) && (buffer.CompareNoCase(thePrefs.GetTemplate()) == 0));
}

void CPPgWebServer::OnHelp()
{
	theApp.ShowHelp(eMule_FAQ_Preferences_WebInterface);
}

BOOL CPPgWebServer::OnCommand(WPARAM wParam, LPARAM lParam)
{
	return (wParam == ID_HELP) ? OnHelpInfo(NULL) : __super::OnCommand(wParam, lParam);
}

BOOL CPPgWebServer::OnHelpInfo(HELPINFO*)
{
	OnHelp();
	return TRUE;
}

void CPPgWebServer::OnDestroy()
{
	CPropertyPage::OnDestroy();
	if (m_icoBrowse) {
		VERIFY(::DestroyIcon(m_icoBrowse));
		m_icoBrowse = NULL;
	}
}