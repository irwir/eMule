//
// UPnPFinder.h
//
// Copyright (c) Shareaza Development Team, 2002-2005.
// This file is part of SHAREAZA (www.shareaza.com)
//
// this file is part of eMule
// Copyright (C)2007 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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

#pragma once

#include "UPnPImpl.h"
#include "opcodes.h"
#include <upnp.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <winsvc.h>

#include <vector>
#include <exception>
#include <functional>


typedef _com_ptr_t<_com_IIID<IUPnPDeviceFinder, &IID_IUPnPDeviceFinder> > FinderPointer;
typedef _com_ptr_t<_com_IIID<IUPnPDevice, &IID_IUPnPDevice> > DevicePointer;
typedef _com_ptr_t<_com_IIID<IUPnPService, &IID_IUPnPService> > ServicePointer;
typedef _com_ptr_t<_com_IIID<IUPnPDeviceFinderCallback, &IID_IUPnPDeviceFinderCallback> > DeviceFinderCallback;
typedef _com_ptr_t<_com_IIID<IUPnPServiceCallback, &IID_IUPnPServiceCallback> > ServiceCallback;
typedef _com_ptr_t<_com_IIID<IEnumUnknown, &IID_IEnumUnknown> > EnumUnknownPtr;
typedef _com_ptr_t<_com_IIID<IUnknown, &IID_IUnknown> > UnknownPtr;

typedef DWORD(WINAPI *TGetBestInterface)(
	IPAddr dwDestAddr,
	PDWORD pdwBestIfIndex
	);

typedef DWORD(WINAPI *TGetIpAddrTable)(
	PMIB_IPADDRTABLE pIpAddrTable,
	PULONG pdwSize,
	BOOL bOrder
	);

typedef DWORD(WINAPI *TGetIfEntry)(
	PMIB_IFROW pIfRow
	);

CString translateUPnPResult(HRESULT hr);
HRESULT UPnPMessage(HRESULT hr);

class CUPnPImplWinServ : public CUPnPImpl
{
	friend class CDeviceFinderCallback;
	friend class CServiceCallback;
// Construction
public:
	virtual	~CUPnPImplWinServ();
	CUPnPImplWinServ();

	virtual void StartDiscovery(uint16 nTCPPort, uint16 nUDPPort, uint16 nTCPWebPort)
	{
		StartDiscovery(nTCPPort, nUDPPort, nTCPWebPort, false);
	}
	virtual void StopAsyncFind();
	virtual void DeletePorts();
	virtual bool IsReady();
	virtual int GetImplementationID()
	{
		return UPNP_IMPL_WINDOWSERVICE;
	}

	// No Support for Refreshing for this (fallback) implementation yet - in many cases where it would be needed (router reset etc)
	// the windows side of the implementation tends to get bugged until reboot anyway. Still might get added later
	virtual bool CheckAndRefresh()
	{
		return false;
	}

protected:
	void StartDiscovery(uint16 nTCPPort, uint16 nUDPPort, uint16 nTCPWebPort, bool bSecondTry);
	void AddDevice(DevicePointer device, bool bAddChildren, int nLevel = 0);
	void RemoveDevice(CComBSTR bsUDN);
	bool OnSearchComplete();
	void Init();

	inline bool IsAsyncFindRunning()
	{
		if (m_pDeviceFinder != NULL && m_bAsyncFindRunning && ::GetTickCount() >= m_tLastEvent + SEC2MS(10)) {
			m_pDeviceFinder->CancelAsyncFind(m_nAsyncFindHandle);
			m_bAsyncFindRunning = false;
		}
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return m_bAsyncFindRunning;
	}
	TRISTATE m_bUPnPDeviceConnected;

// Implementation
	// API functions
	SC_HANDLE(WINAPI *m_pfnOpenSCManager)(LPCTSTR, LPCTSTR, DWORD);
	SC_HANDLE(WINAPI *m_pfnOpenService)(SC_HANDLE, LPCTSTR, DWORD);
	BOOL(WINAPI *m_pfnQueryServiceStatusEx)(SC_HANDLE, SC_STATUS_TYPE, LPBYTE, DWORD, LPDWORD);
	BOOL(WINAPI *m_pfnCloseServiceHandle)(SC_HANDLE);
	BOOL(WINAPI *m_pfnStartService)(SC_HANDLE, DWORD, LPCTSTR*);
	BOOL(WINAPI *m_pfnControlService)(SC_HANDLE, DWORD, LPSERVICE_STATUS);

	TGetBestInterface m_pfGetBestInterface;
	TGetIpAddrTable m_pfGetIpAddrTable;
	TGetIfEntry m_pfGetIfEntry;

	static FinderPointer CreateFinderInstance();
	struct FindDevice : private std::unary_function<DevicePointer, bool>
	{
		explicit FindDevice(const CComBSTR &udn)
			: m_udn(udn)
		{
		}

		result_type operator()(argument_type device) const
		{
			CComBSTR deviceName;
			HRESULT hr = device->get_UniqueDeviceName(&deviceName);

			if (FAILED(hr)) {
				UPnPMessage(hr);
				return false;
			}
			return wcscmp(deviceName.m_str, m_udn) == 0;
		}

		CComBSTR m_udn;
	};

	void ProcessAsyncFind(CComBSTR bsSearchType);
	HRESULT GetDeviceServices(DevicePointer pDevice);
	void StartPortMapping();
	HRESULT MapPort(const ServicePointer &service);
	void DeleteExistingPortMappings(ServicePointer pService);
	void CreatePortMappings(ServicePointer pService);
	HRESULT SaveServices(EnumUnknownPtr pEU, const LONG nTotalItems);
	HRESULT InvokeAction(ServicePointer pService, CComBSTR action
						, LPCTSTR pszInArgString, CString &strResult);
	void StopUPnPService();

	// Utility functions
	static HRESULT CreateSafeArray(const VARTYPE vt, const ULONG nArgs, SAFEARRAY **ppsa);
	static INT_PTR CreateVarFromString(const CString &strArgs, VARIANT ***pppVars);
	static LONG GetStringFromOutArgs(const VARIANT *pvaOutArgs, CString &strArgs);
	static void DestroyVars(const INT_PTR nCount, VARIANT ***pppVars);
	static HRESULT GetSafeArrayBounds(SAFEARRAY *psa, LONG *pLBound, LONG *pUBound);
	static HRESULT GetVariantElement(SAFEARRAY *psa, LONG pos, VARIANT *pvar);
	CString GetLocalRoutableIP(ServicePointer pService);

// Private members
private:
	std::vector<DevicePointer> m_pDevices;
	std::vector<ServicePointer> m_pServices;
	FinderPointer m_pDeviceFinder;
	DeviceFinderCallback m_pDeviceFinderCallback;
	ServiceCallback m_pServiceCallback;

	CString m_sLocalIP;
	CString m_sExternalIP;
	HMODULE m_hADVAPI32_DLL;
	HMODULE m_hIPHLPAPI_DLL;
	DWORD m_tLastEvent;	// When the last event was received?
	LONG m_nAsyncFindHandle;
	bool m_bCOM;
	bool m_bPortIsFree;
	bool m_bADSL;	// Is the device ADSL?
	bool m_ADSLFailed;	// Did port mapping failed for the ADSL device?
	bool m_bInited;
	bool m_bAsyncFindRunning;
	bool m_bSecondTry;
	bool m_bServiceStartedByEmule;
	bool m_bDisableWANIPSetup;
	bool m_bDisableWANPPPSetup;
};

// DeviceFinder Callback
class CDeviceFinderCallback
	: public IUPnPDeviceFinderCallback
{
public:
	explicit CDeviceFinderCallback(CUPnPImplWinServ &instance)
		: m_instance(instance)
		, m_lRefCount()
	{
	}

	STDMETHODIMP QueryInterface(REFIID iid, LPVOID *ppvObject);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

// implementation
private:
	HRESULT __stdcall DeviceAdded(LONG nFindData, IUPnPDevice *pDevice);
	HRESULT __stdcall DeviceRemoved(LONG nFindData, BSTR bsUDN);
	HRESULT __stdcall SearchComplete(LONG nFindData);

private:
	CUPnPImplWinServ &m_instance;
	LONG m_lRefCount;
};

// Service Callback
class CServiceCallback : public IUPnPServiceCallback
{
public:
	explicit CServiceCallback(CUPnPImplWinServ &instance)
		: m_instance(instance)
		, m_lRefCount()
	{
	}

	STDMETHODIMP QueryInterface(REFIID iid, LPVOID *ppvObject);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

// implementation
private:
	HRESULT __stdcall StateVariableChanged(IUPnPService *pService, LPCWSTR pszStateVarName, VARIANT varValue);
	HRESULT __stdcall ServiceInstanceDied(IUPnPService *pService);

private:
	CUPnPImplWinServ &m_instance;
	LONG m_lRefCount;
};