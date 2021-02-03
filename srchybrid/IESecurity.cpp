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
#include "IESecurity.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


///////////////////////////////////////////////////////////////////////////////
// CMuleBrowserControlSite

BEGIN_INTERFACE_MAP(CMuleBrowserControlSite, CBrowserControlSite)
	INTERFACE_PART(CMuleBrowserControlSite, IID_IInternetSecurityManager, InternetSecurityManager)
	INTERFACE_PART(CMuleBrowserControlSite, IID_IServiceProvider, ServiceProvider)
END_INTERFACE_MAP()

CMuleBrowserControlSite::CMuleBrowserControlSite(COleControlContainer *pCtrlCont, CDHtmlDialog *pHandler)
	: CBrowserControlSite(pCtrlCont, pHandler)
	, m_eUrlZone(URLZONE_UNTRUSTED)
{
	// Compiler bug?, MFC bug?, eMule bug (compiler settings)?
	//
	// When this class is compiled with _AFXDLL and /Zp4, the offset for 'CMuleBrowserControlSite::m_eUrlZone'
	// and 'CBrowserControlSite::m_pHandler' are *EQUAL* !!
	//
	// Also, the offset for 'CBrowserControlSite::m_pHandler' is not the same in static MFC and shared MFC,
	// though this might have a different reason.
	//
	// When compiled with:
	//	_AFXDLL, /Zp8 (default packing)		OK
	//	_AFXDLL, /Zp4						*ERROR*
	//
	// Note also, MFC's internally used packing is 4 (_AFX_PACKING), and though it creates wrong offsets
	// when we compile with /Zp4.
	struct S1
	{
		char c;
		__int64 ll;
	};
	ASSERT( offsetof(S1, ll) == 8 );

	InitInternetSecurityZone();
}

void CMuleBrowserControlSite::InitInternetSecurityZone()
{
	const CString &strZone = AfxGetApp()->GetProfileString(_T("eMule"), _T("InternetSecurityZone"), _T("Untrusted"));
	if (strZone.CompareNoCase(_T("LocalMachine")) == 0)
		m_eUrlZone = URLZONE_LOCAL_MACHINE;
	else if (strZone.CompareNoCase(_T("Intranet")) == 0)
		m_eUrlZone = URLZONE_INTRANET;
	else if (strZone.CompareNoCase(_T("Trusted")) == 0)
		m_eUrlZone = URLZONE_TRUSTED;
	else if (strZone.CompareNoCase(_T("Internet")) == 0)
		m_eUrlZone = URLZONE_INTERNET;
	else {
		ASSERT(strZone.CompareNoCase(_T("Untrusted")) == 0);
		m_eUrlZone = URLZONE_UNTRUSTED;
	}
}


#ifdef _DEBUG
#define DUMPIID(iid, name) DumpIID(iid, name)
#else
#define DUMPIID(iid, name) /**/
#endif

#ifdef _DEBUG
void DumpIID(REFIID iid, LPCTSTR pszClassName)
{
	CRegKey key;
	TCHAR szName[100];
	DWORD dwType;
	DWORD dw = sizeof(szName);

	LPOLESTR pszGUID = NULL;
	if (FAILED(StringFromCLSID(iid, &pszGUID)))
		return;

	OutputDebugString(pszClassName);
	OutputDebugString(_T(" - "));

	bool bFound = false;
	// Attempt to find it in the interfaces section
	if (key.Open(HKEY_CLASSES_ROOT, _T("Interface"), KEY_READ) == ERROR_SUCCESS) {
		if (key.Open(key, pszGUID, KEY_READ) == ERROR_SUCCESS) {
			*szName = 0;
			if (RegQueryValueEx(key.m_hKey, (LPTSTR)NULL, NULL, &dwType, (LPBYTE)szName, &dw) == ERROR_SUCCESS) {
				OutputDebugString(szName);
				bFound = true;
			}
		}
	}
	// Attempt to find it in the clsid section
	else if (key.Open(HKEY_CLASSES_ROOT, _T("CLSID"), KEY_READ) == ERROR_SUCCESS) {
		if (key.Open(key, pszGUID, KEY_READ) == ERROR_SUCCESS) {
			*szName = 0;
			if (RegQueryValueEx(key.m_hKey, (LPTSTR)NULL, NULL, &dwType, (LPBYTE)szName, &dw) == ERROR_SUCCESS) {
				OutputDebugString(_T("(CLSID\?\?\?) "));
				OutputDebugString(szName);
				bFound = true;
			}
		}
	}

	if (!bFound)
		OutputDebugString(pszGUID);
	OutputDebugString(_T("\n"));
	CoTaskMemFree(pszGUID);
}
#endif


///////////////////////////////////////////////////////////////////////////////
// InternetSecurityManager
//
#pragma warning(push)
#pragma warning(disable:4555) // expression has no effect; expected expression with side-effect (because of the 'METHOD_PROLOGUE' macro)

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::QueryInterface(REFIID riid, void **ppvObj) noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	return (HRESULT)pThis->ExternalQueryInterface(&riid, ppvObj);
}

STDMETHODIMP_(ULONG) CMuleBrowserControlSite::XInternetSecurityManager::AddRef() noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	return pThis->ExternalAddRef();
}

STDMETHODIMP_(ULONG) CMuleBrowserControlSite::XInternetSecurityManager::Release() noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	return pThis->ExternalRelease();
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::SetSecuritySite(IInternetSecurityMgrSite* /*pSite*/) noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs\n"), "SetSecuritySite");
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::GetSecuritySite(IInternetSecurityMgrSite** /*ppSite*/) noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs\n"), "GetSecuritySite");
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::MapUrlToZone(
										LPCWSTR pwszUrl,
										DWORD *pdwZone,
										DWORD dwFlags) noexcept
{
	UNREFERENCED_PARAMETER(pwszUrl);
	UNREFERENCED_PARAMETER(dwFlags);
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs: URL=%ls, Zone=%d, Flags=0x%x\n"), "MapUrlToZone", pwszUrl, pdwZone ? *pdwZone : 0, dwFlags);
	if (pdwZone != NULL) {
		*pdwZone = (DWORD)pThis->m_eUrlZone;
		return S_OK;
	}
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::GetSecurityId(
										LPCWSTR pwszUrl,
										BYTE* /*pbSecurityId*/, DWORD* /*pcbSecurityId*/,
										DWORD_PTR dwReserved) noexcept
{
	UNREFERENCED_PARAMETER(pwszUrl);
	UNREFERENCED_PARAMETER(dwReserved);
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs: URL=%ls, Reserved=%u\n"), "GetSecurityId", pwszUrl, dwReserved);
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::ProcessUrlAction(
										LPCWSTR pwszUrl,
										DWORD dwAction,
										BYTE* /*pPolicy*/, DWORD /*cbPolicy*/,
										BYTE* /*pContext*/, DWORD /*cbContext*/,
										DWORD dwFlags, DWORD dwReserved) noexcept
{
	UNREFERENCED_PARAMETER(pwszUrl);
	UNREFERENCED_PARAMETER(dwAction);
	UNREFERENCED_PARAMETER(dwFlags);
	UNREFERENCED_PARAMETER(dwReserved);
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs: URL=%ls, Action=%u, Flags=0x%x, Reserved=%u\n"), "ProcessUrlAction", pwszUrl, dwAction, dwFlags, dwReserved);

#if 0
	DWORD dwPolicy = URLPOLICY_DISALLOW;
	if (cbPolicy >= sizeof(DWORD)) {
		*(DWORD*)pPolicy = dwPolicy;
		return S_OK;
	}
	return S_FALSE;
#else
	// Use the policy for the zone which was specified with 'MapUrlToZone'
	// If that particular policy setting is specified as 'Ask User', the control *WILL OPEN* a message box!
	return INET_E_DEFAULT_ACTION;
#endif
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::QueryCustomPolicy(
										LPCWSTR pwszUrl,
										REFGUID /*guidKey*/,
										BYTE** /*ppPolicy*/, DWORD* /*pcbPolicy*/,
										BYTE* /*pContext*/, DWORD /*cbContext*/,
										DWORD /*dwReserved*/) noexcept
{
	UNREFERENCED_PARAMETER(pwszUrl);
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs: URL=%ls\n"), "QueryCustomPolicy", pwszUrl);
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::SetZoneMapping(
										DWORD dwZone,
										LPCWSTR lpszPattern,
										DWORD dwFlags) noexcept
{
	UNREFERENCED_PARAMETER(dwZone);
	UNREFERENCED_PARAMETER(lpszPattern);
	UNREFERENCED_PARAMETER(dwFlags);
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs: Zone=%d, Pattern=%ls, Flags=0x%x\n"), "SetZoneMapping", dwZone, lpszPattern, dwFlags);
	return INET_E_DEFAULT_ACTION;
}

STDMETHODIMP CMuleBrowserControlSite::XInternetSecurityManager::GetZoneMappings(
										DWORD dwZone,
										IEnumString** /*ppenumString*/,
										DWORD dwFlags) noexcept
{
	UNREFERENCED_PARAMETER(dwZone);
	UNREFERENCED_PARAMETER(dwFlags);
	METHOD_PROLOGUE(CMuleBrowserControlSite, InternetSecurityManager);
	TRACE(_T("%hs: Zone=%d, Flags=0x%s\n"), "GetZoneMappings", dwZone, dwFlags);
	return INET_E_DEFAULT_ACTION;
}


///////////////////////////////////////////////////////////////////////////////
// IServiceProvider
//

STDMETHODIMP_(ULONG) CMuleBrowserControlSite::XServiceProvider::AddRef() noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, ServiceProvider);
	return pThis->ExternalAddRef();
}

STDMETHODIMP_(ULONG) CMuleBrowserControlSite::XServiceProvider::Release() noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, ServiceProvider);
	return pThis->ExternalRelease();
}

STDMETHODIMP CMuleBrowserControlSite::XServiceProvider::QueryInterface(REFIID riid, void **ppvObj) noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, ServiceProvider);
	return (HRESULT)pThis->ExternalQueryInterface(&riid, ppvObj);
}

STDMETHODIMP CMuleBrowserControlSite::XServiceProvider::QueryService(REFGUID guidService, REFIID riid, void **ppvObject) noexcept
{
	METHOD_PROLOGUE(CMuleBrowserControlSite, ServiceProvider);
	//DUMPIID(guidService, _T("guidService"));
	//DUMPIID(riid, _T("riid"));
	if (guidService == SID_SInternetSecurityManager && riid == IID_IInternetSecurityManager) {
		TRACE(_T("%hs\n"), "QueryService");
		return (HRESULT)pThis->ExternalQueryInterface(&riid, ppvObject);
	}
	*ppvObject = NULL;
	return E_NOINTERFACE;
}

#pragma warning(pop)