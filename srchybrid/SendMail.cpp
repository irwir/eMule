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
#include "TaskbarNotifier.h"
#include "OtherFunctions.h"
#include "StringConversion.h"
#include "Log.h"
#include "Preferences.h"
#include "TLSthreading.h"
#include <atlenc.h>
#include <wincrypt.h>

#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void LogCertificate(PCCERT_CONTEXT pCertContext)
{
	if (!pCertContext)
		return;

	DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: Found certificate"));

	TCHAR szString[512];
	PCERT_INFO pCertInfo = pCertContext->pCertInfo;
	if (pCertInfo) {
		if (CertNameToStr(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, &pCertInfo->Subject, CERT_X500_NAME_STR, szString, _countof(szString)))
			DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: Subject: %s"), szString);

		if (pCertInfo->SerialNumber.cbData && pCertInfo->SerialNumber.pbData)
			DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: Serial nr.: %s"), (LPCTSTR)GetCertInteger(pCertInfo->SerialNumber.pbData, pCertInfo->SerialNumber.cbData));

		if (CertNameToStr(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, &pCertInfo->Issuer, CERT_X500_NAME_STR, szString, _countof(szString)))
			DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: Issuer: %s"), szString);
	} else {
		if (CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_DISABLE_IE4_UTF8_FLAG, szOID_COMMON_NAME, szString, _countof(szString)))
			DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: Name: %s"), szString);

		BYTE md5[16];
		DWORD cb = (DWORD)sizeof md5;
		if (CertGetCertificateContextProperty(pCertContext, CERT_MD5_HASH_PROP_ID, md5, &cb) && cb == sizeof md5)
			DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: MD5 hash: %s"), (LPCTSTR)GetCertHash(md5, cb));

		BYTE sha1[20];
		cb = (DWORD)sizeof sha1;
		if (CertGetCertificateContextProperty(pCertContext, CERT_SHA1_HASH_PROP_ID, sha1, &cb) && cb == sizeof sha1)
			DebugLog(LOG_DONTNOTIFY, _T("Email Encryption: SHA1 hash: %s"), (LPCTSTR)GetCertHash(sha1, cb));
	}
}

bool Encrypt(const CStringA &rstrContentA, CByteArray &raEncrypted, LPCWSTR pwszCertSubject)
{
	LPCTSTR pszContainer = AfxGetAppName();
	HCRYPTPROV hCryptProv;
	if (!CryptAcquireContext(&hCryptProv, pszContainer, NULL, PROV_RSA_FULL, NULL)) {
		DWORD dwError = ::GetLastError();
		if (dwError != (DWORD)NTE_BAD_KEYSET) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to acquire certificate context container '%s' - %s"), pszContainer, (LPCTSTR)GetErrorMessage(dwError, 1));
			return false;
		}
		if (!CryptAcquireContext(&hCryptProv, pszContainer, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to create certificate context container '%s' - %s"), pszContainer, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
			return false;
		}
	}

	static LPCTSTR const pszCertStore = _T("AddressBook");
	HCERTSTORE hStoreHandle = CertOpenSystemStore(hCryptProv, pszCertStore);
	if (hStoreHandle) {
		PCCERT_CONTEXT pRecipientCert = CertFindCertificateInStore(hStoreHandle, PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, pwszCertSubject, NULL);
		if (pRecipientCert) {
			if (thePrefs.GetVerbose())
				LogCertificate(pRecipientCert);
			//PCCERT_CONTEXT RecipientCertArray[1] = { pRecipientCert };

			CRYPT_ALGORITHM_IDENTIFIER EncryptAlgorithm = {};
			EncryptAlgorithm.pszObjId = szOID_RSA_DES_EDE3_CBC;

			CRYPT_ENCRYPT_MESSAGE_PARA EncryptParams = {};
			EncryptParams.cbSize = (DWORD)sizeof EncryptParams;
			EncryptParams.dwMsgEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
			EncryptParams.hCryptProv = hCryptProv;
			EncryptParams.ContentEncryptionAlgorithm = EncryptAlgorithm;

			DWORD cbEncryptedBlob = 0;
			if (CryptEncryptMessage(&EncryptParams, 1, &pRecipientCert, (BYTE*)(LPCSTR)rstrContentA, rstrContentA.GetLength(), NULL, &cbEncryptedBlob)) {
				try {
					raEncrypted.SetSize(cbEncryptedBlob);
					if (!CryptEncryptMessage(&EncryptParams, 1, &pRecipientCert, (BYTE*)(LPCSTR)rstrContentA, rstrContentA.GetLength(), raEncrypted.GetData(), &cbEncryptedBlob)) {
						DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to encrypt message - %s"), (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
						raEncrypted.SetSize(0);
					}
				} catch (CMemoryException *ex) {
					ex->Delete();
					DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to encrypt message - %s"), _tcserror(ENOMEM));
				}
			} else
				DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to get length of encrypted message - %s"), (LPCTSTR)GetErrorMessage(::GetLastError(), 1));

			VERIFY(CertFreeCertificateContext(pRecipientCert));
		} else
			DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to find certificate with subject '%ls' - %s"), pwszCertSubject, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));

		VERIFY(CertCloseStore(hStoreHandle, 0));
	} else
		DebugLogWarning(LOG_DONTNOTIFY, _T("Email Encryption: Failed to open certificate store '%s' - %s"), pszCertStore, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));

	VERIFY(CryptReleaseContext(hCryptProv, 0));

	return !raEncrypted.IsEmpty();
}

bool encoded_word(const CString &src, CStringA &dst)
{
	if (!NeedUTF8String(src)) {
		dst = src;
		return true;
	}
	CStringA srcA(wc2utf8(src));
	int iLength = Base64EncodeGetRequiredLength(srcA.GetLength(), ATL_BASE64_FLAG_NOCRLF);

	if (!Base64Encode(reinterpret_cast<const BYTE*>((LPCSTR)srcA), srcA.GetLength()
		, dst.GetBuffer(iLength), &iLength, ATL_BASE64_FLAG_NOCRLF))
	{
		dst.ReleaseBuffer(0);
		DebugLogWarning(LOG_DONTNOTIFY, _T("'%s' to base64 failed"), (LPCTSTR)src);
		return false;
	}
	dst.ReleaseBuffer(iLength);
	dst = "=?utf-8?b?" + dst + "?=";
	return true;
}

static int do_handshake(mbedtls_ssl_context *ssl)
{
	for (int ret; (ret = mbedtls_ssl_handshake(ssl)) != 0;)
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("SSL/TLS handshake failed: %d"), ret);
			return -1;
		}

	uint32_t flags = mbedtls_ssl_get_verify_result(ssl);
	if (flags != 0)
		DebugLogWarning(LOG_DONTNOTIFY, _T("Mail server certificate has issues: %u"), flags);
	return 0;
}

static int write_ssl(mbedtls_ssl_context *ssl, const char *buf, size_t len)
{
	if (len > 0) {
		for (int ret; (ret = mbedtls_ssl_write(ssl, (unsigned char*)buf, len)) <= 0;) {
			if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				DebugLog(LOG_DONTNOTIFY, _T("mbedtls_ssl_write returned %d"), ret);
				return -1;
			}
		}
	}

	for (;;) {
		unsigned char data[256];

		int ret = mbedtls_ssl_read(ssl, data, sizeof(data) - 1);
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			if (ret <= 0) {
				if (ret != MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
					DebugLog(LOG_DONTNOTIFY, _T("mbedtls_ssl_read returned %d"), ret);
				return -1;
			}
			data[min(3, ret)] = '\0';
			return atoi((char*)data);
		}
	}
}

static int write_txt(mbedtls_net_context *sock_fd, const char *buf, size_t len)
{
	if (len > 0) {
		int ret = mbedtls_net_send(sock_fd, (unsigned char*)buf, len);
		if (ret <= 0) {
			DebugLog(LOG_DONTNOTIFY, _T("mbedtls_net_send returned %d"), ret);
			return -1;
		}
	}

	unsigned char data[256];
	int ret = mbedtls_net_recv(sock_fd, data, sizeof data - 1);
	if (ret <= 0) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("mbedtls_net_recv returned %d"), ret);
		return -1;
	}

	data[min(3, ret)] = '\0';
	return atoi((char*)data);
}


///////////////////////////////////////////////////////////////////////////////
// CNotifierMailThread

class CNotifierMailThread : public CWinThread
{
	DECLARE_DYNCREATE(CNotifierMailThread)

	void sendmail();
	int write_data(mbedtls_ssl_context *ssl, const char *buf);
	int write_data(mbedtls_ssl_context *ssl, const char *buf, size_t len);

	EmailSettings m_mail;

	CString m_strSubject;
	CString m_strBody;

protected:
	CNotifierMailThread() = default;	// protected constructor used by dynamic creation
	virtual	~CNotifierMailThread() = default;
	static CCriticalSection sm_critSect;

public:
	virtual	BOOL InitInstance();
};

CCriticalSection CNotifierMailThread::sm_critSect;

IMPLEMENT_DYNCREATE(CNotifierMailThread, CWinThread)

BOOL CNotifierMailThread::InitInstance()
{
	DbgSetThreadName("NotifierMailThread");
	if (!theApp.IsClosing() && sm_critSect.Lock()) {
		InitThreadLocale();
		sendmail();

		sm_critSect.Unlock();
	}
	return FALSE;
}

void CNotifierMailThread::sendmail()
{
	static const unsigned char pers[] = "eMule_mail";
	int ret;
	CStringA sBodyA, sReceiverA, sSenderA, sServerA, sTmpA, sBufA;

	mbedtls_net_context server_fd;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	mbedtls_pk_context pkey;

	mbedtls_threading_set_alt(threading_mutex_init_alt, threading_mutex_free_alt, threading_mutex_lock_alt, threading_mutex_unlock_alt);
	mbedtls_net_init(&server_fd);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
	mbedtls_pk_init(&pkey);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);

	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, pers, sizeof pers -1);
	if (ret != 0) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Seeding the random number generator failed: %d"), ret);
		goto exit;
	}

	sServerA = CStringA(m_mail.sServer);
	sTmpA.Format("%d", m_mail.uPort);
	ret = mbedtls_net_connect(&server_fd, sServerA, sTmpA, MBEDTLS_NET_PROTO_TCP);
	if (ret != 0) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Connect to %s:%hu failed: %d"), (LPCTSTR)m_mail.sServer, m_mail.uPort, ret);
		goto exit;
	}

	ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret != 0) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Seting SSL/TLS defaults failed: %d"), ret);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

	ret = mbedtls_ssl_setup(&ssl, &conf);
	if (ret != 0) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("SSL/TLS setup failed: %d"), ret);
		goto exit;
	}

	ret = mbedtls_ssl_set_hostname(&ssl, sServerA);
	if (ret != 0) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Set hostname failed: %d"), ret);
		goto exit;
	}

	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

	char hostname[256];
	gethostname(hostname, sizeof hostname);

	switch (m_mail.uTLS) {
	case MODE_SSL_TLS:
		if (do_handshake(&ssl) != 0)
			goto exit;

		ret = write_ssl(&ssl, NULL, 0);
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Get header from server failed: %d"), ret);
			goto failed;
		}

		sBufA.Format("EHLO %s\r\n", hostname);
		ret = write_ssl(&ssl, sBufA, sBufA.GetLength()); //250
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Write EHLO failed: %d"), ret);
			goto failed;
		}
		break;
	case MODE_STARTTLS:
	default: //MODE_NONE
		ret = write_txt(&server_fd, NULL, 0); //220
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Get greetings from server failed: %d"), ret);
			goto exit;
		}

		sBufA.Format("EHLO %s\r\n", hostname);
		ret = write_txt(&server_fd, sBufA, sBufA.GetLength()); //250
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Send EHLO failed: %d"), ret);
			goto exit;
		}
		if (m_mail.uTLS == MODE_NONE)
			break;

		ret = write_txt(&server_fd, "STARTTLS\r\n", sizeof("STARTTLS\r\n")); //220
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Send STARTTLS failed: %d"), ret);
			goto exit;
		}
		if (do_handshake(&ssl) != 0)
			goto exit;
	}

	size_t n;
	unsigned char base[1024];
	switch (m_mail.uAuth) {
	case AUTH_PLAIN:
		sTmpA.Format("%c%s%c%s", '\0', (LPCSTR)((CStringA)m_mail.sUser), '\0', (LPCSTR)(CStringA(m_mail.sPass)));
		ret = mbedtls_base64_encode(base, sizeof base, &n, (unsigned char*)(LPCSTR)sTmpA, sTmpA.GetLength());
		if (ret != 0) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Plain ID to base64 failed: %d"), ret);
			goto failed;
		}

		sTmpA.Format("AUTH PLAIN %s\r\n", base);
		ret = write_data(&ssl, sTmpA); //235
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("AUTH PLAIN failed: %d"), ret);
			goto failed;
		}
		break;
	case AUTH_LOGIN:
		ret = write_data(&ssl, "AUTH LOGIN\r\n"); //334
		if (ret > 0 && ret < 200 || ret > 399) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("AUTH LOGIN failed: %d"), ret);
			goto failed;
		}

		sTmpA = CStringA(m_mail.sUser);
		ret = mbedtls_base64_encode(base, sizeof base, &n, (unsigned char*)(LPCSTR)sTmpA, sTmpA.GetLength());
		if (ret != 0) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Login to base64 failed: %d"), ret);
			goto failed;
		}
		sBufA.Format("%s\r\n", base);
		ret = write_data(&ssl, sBufA); //334
		if (ret > 0 && ret < 300 || ret > 399) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Send login failed: %d"), ret);
			goto failed;
		}

		sTmpA = CStringA(m_mail.sPass);
		ret = mbedtls_base64_encode(base, sizeof base, &n, (unsigned char*)(LPCSTR)sTmpA, sTmpA.GetLength());
		if (ret != 0) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Password to base64 failed: %d"), ret);
			goto failed;
		}
		sBufA.Format("%s\r\n", base);
		ret = write_data(&ssl, sBufA); //235
		if (ret > 0 && ret < 200 || ret > 299) {
			DebugLogWarning(LOG_DONTNOTIFY, _T("Send password failed: %d"), ret);
			goto failed;
		}
	}

	sSenderA = CStringA(m_mail.sFrom);
	sBufA.Format("MAIL FROM:<%s>\r\n", (LPCSTR)sSenderA);
	ret = write_data(&ssl, sBufA); //250
	if (ret > 0 && ret < 200 || ret > 299) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Send MAIL FROM failed: %d"), ret);
		goto failed;
	}

	sReceiverA = CStringA(m_mail.sTo);
	sBufA.Format("RCPT TO:<%s>\r\n", (LPCSTR)sReceiverA);
	ret = write_data(&ssl, sBufA); //250 251
	if (ret > 0 && ret < 200 || ret > 299) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Send RCPT TO failed: %d"), ret);
		goto failed;
	}

	ret = write_data(&ssl, "DATA\r\n"); //354 250
	if (ret > 0 && ret < 200 || ret > 399) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Write DATA failed: %d"), ret);
		goto failed;
	}

	sBodyA = "Content-Type: text/plain;\r\n"
			"\tformat=flowed;\r\n"
			"\tcharset=\"utf-8\"\r\n"
			"Content-Transfer-Encoding: 8bit\r\n\r\n"
			+ (NeedUTF8String(m_strBody) ? wc2utf8(m_strBody) : CStringA(m_strBody));

	bool bEncrypt = !m_mail.sEncryptCertName.IsEmpty();
	if (bEncrypt) {
		CByteArray aEncrypted;
		if (!Encrypt(sBodyA, aEncrypted, m_mail.sEncryptCertName))
			goto failed;

		int iLength = Base64EncodeGetRequiredLength((int)aEncrypted.GetSize());
		if (!Base64Encode(aEncrypted.GetData(), (int)aEncrypted.GetSize(), sBodyA.GetBuffer(iLength), &iLength)) {
			sBodyA.ReleaseBuffer(0);
			DebugLogWarning(LOG_DONTNOTIFY, _T("Encrypted body to base64 failed"));
			goto failed;
		}
		sBodyA.ReleaseBuffer(iLength);
		sBodyA = "Content-Type: application/x-pkcs7-mime;\r\n"
			"\tformat=flowed;\r\n"
			"\tsmime-type=enveloped-data;\r\n"
			"\tname=\"smime.p7m\"\r\n"
			"Content-Transfer-Encoding: base64\r\n"
			"Content-Disposition: attachment;\r\n"
			"\tfilename=\"smime.p7m\"\r\n\r\n" + sBodyA;
	}

	if (!encoded_word(m_strSubject, sTmpA)) //subject
		goto failed;

	sBufA.Format(
		"From: eMule <%s>\r\n"
		"Subject: %s\r\n"
		"To: <%s>\r\n"
		"MIME-Version: 1.0\r\n"
		"%s\r\n"
		"\r\n.\r\n"
		, (LPCSTR)sSenderA, (LPCSTR)sTmpA, (LPCSTR)sReceiverA, (LPCSTR)sBodyA);

	ret = write_data(&ssl, sBufA);
	if (ret > 0 && ret < 200 || ret > 299) {
		DebugLogWarning(LOG_DONTNOTIFY, _T("Write content failed: %d"), ret);
		goto failed;
	}

	ret = write_data(&ssl, "QUIT\r\n"); //221
	if (ret > 0 && ret < 200 || ret > 299)
		DebugLogWarning(LOG_DONTNOTIFY, _T("Send QUIT failed: %d"), ret);

failed:
	mbedtls_ssl_close_notify(&ssl);

exit:
	mbedtls_net_free(&server_fd);
	mbedtls_pk_free(&pkey);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
}

int CNotifierMailThread::write_data(mbedtls_ssl_context *ssl, const char *buf)
{
	return write_data(ssl, buf, buf == NULL ? 0 : strlen(buf));
}

int CNotifierMailThread::write_data(mbedtls_ssl_context *ssl, const char *buf, size_t len)
{
	return m_mail.uTLS == MODE_NONE
		? write_txt((mbedtls_net_context*)ssl->p_bio, buf, len)
		: write_ssl(ssl, buf, len);
}

void CemuleDlg::SendNotificationMail(TbnMsg nMsgType, LPCTSTR pszText)
{
	if (!thePrefs.IsNotifierSendMailEnabled())
		return;

	EmailSettings mail(thePrefs.GetEmailSettings());
	if (mail.sServer.Trim().IsEmpty() || mail.sTo.Trim().IsEmpty() || mail.sFrom.Trim().IsEmpty())
		return;

	CNotifierMailThread *pThread = static_cast<CNotifierMailThread*>(AfxBeginThread(RUNTIME_CLASS(CNotifierMailThread), THREAD_PRIORITY_LOWEST, 0, CREATE_SUSPENDED));
	if (pThread) {
		mail.sEncryptCertName = theApp.GetProfileString(_T("eMule"), _T("NotifierMailEncryptCertName")).Trim();
		pThread->m_mail = mail;
		pThread->m_strSubject = GetResString(IDS_EMULENOTIFICATION);
		UINT uid;
		switch (nMsgType) {
		case TBN_CHAT:
			uid = IDS_PW_TBN_POP_ALWAYS;
			break;
		case TBN_DOWNLOADFINISHED:
			uid = IDS_PW_TBN_ONDOWNLOAD;
			break;
		case TBN_DOWNLOADADDED:
			uid = IDS_TBN_ONNEWDOWNLOAD;
			break;
		case TBN_LOG:
			uid = IDS_PW_TBN_ONLOG;
			break;
		case TBN_IMPORTANTEVENT:
			uid = IDS_ERROR;
			break;
		case TBN_NEWVERSION:
			uid = IDS_CB_TBN_ONNEWVERSION;
			break;
		default:
			uid = 0;
			ASSERT(0);
		}
		if (uid)
			pThread->m_strSubject.AppendFormat(_T(": %s"), (LPCTSTR)GetResString(uid));
		pThread->m_strBody = pszText;
		pThread->ResumeThread();
	}
}