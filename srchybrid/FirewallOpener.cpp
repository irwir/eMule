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

// class to configure the ICS-Firewall of Windows XP - will not work with WinXP-SP2 yet

#include "StdAfx.h"
#include "firewallopener.h"
#include "emule.h"
#include "preferences.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define RETURN_ON_FAIL(x)	if (!SUCCEEDED(x)) return false;

CFirewallOpener::CFirewallOpener()
	: m_pINetSM()
	, m_bInited()
{
}

CFirewallOpener::~CFirewallOpener()
{
	UnInit();
}

bool CFirewallOpener::Init(bool bPreInit)
{
	if (!m_bInited) {
		ASSERT(m_liAddedRules.IsEmpty());
		if (thePrefs.GetWindowsVersion() != _WINVER_XP_ || !SUCCEEDED(CoInitialize(NULL)))
			return false;
		HRESULT hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
		if (!SUCCEEDED(hr) || !SUCCEEDED(CoCreateInstance(__uuidof(NetSharingManager), NULL, CLSCTX_ALL, __uuidof(INetSharingManager), (void**)&m_pINetSM))) {
			CoUninitialize();
			return false;
		}
	}
	m_bInited = true;
	if (bPreInit) {
		// will return here in order to not create an instance when not really needed
		// preinit is only used to call CoInitializeSecurity before its too late for that (aka something else called it)
		// will have to look deeper into this issue in order to find a nicer way if possible
		return true;
	}

	if (m_pINetSM == NULL) {
		if (!SUCCEEDED(CoCreateInstance(__uuidof(NetSharingManager), NULL, CLSCTX_ALL, __uuidof(INetSharingManager), (void**)&m_pINetSM))) {
			UnInit();
			return false;
		}
	}
	return true;
}

void CFirewallOpener::UnInit()
{
	if (!m_bInited)
		return;

	for (INT_PTR i = m_liAddedRules.GetCount(); --i >= 0;)
		if (m_liAddedRules[i].m_bRemoveOnExit)
			RemoveRule(m_liAddedRules[i]);

	m_liAddedRules.RemoveAll();

	m_bInited = false;
	if (m_pINetSM != NULL) {
		m_pINetSM->Release();
		m_pINetSM = NULL;
	} else
		ASSERT(0);
	CoUninitialize();
}

bool CFirewallOpener::DoAction(const EFOCAction eAction, const CICSRuleInfo &riPortRule)
{
	if (!Init())
		return false;
	//TODO lets see if we can find a reliable method to find out the internet standard connection set by the user

	bool bSuccess = true;
	bool bPartialSucceeded = false;
	bool bFoundAtLeastOneConn = false;

	INetSharingEveryConnectionCollectionPtr NSECCP;
	IEnumVARIANTPtr varEnum;
	IUnknownPtr pUnk;
	RETURN_ON_FAIL(m_pINetSM->get_EnumEveryConnection(&NSECCP));
	RETURN_ON_FAIL(NSECCP->get__NewEnum(&pUnk));
	RETURN_ON_FAIL(pUnk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&varEnum));

	_variant_t var;
	while (S_OK == varEnum->Next(1, &var, NULL)) {
		INetConnectionPtr NCP;
		if (V_VT(&var) == VT_UNKNOWN
			&& SUCCEEDED(V_UNKNOWN(&var)->QueryInterface(__uuidof(INetConnection), (void**)&NCP)))
		{
			INetConnectionPropsPtr pNCP;
			if (!SUCCEEDED(m_pINetSM->get_NetConnectionProps(NCP, &pNCP)))
				continue;
			DWORD dwCharacteristics = 0;
			pNCP->get_Characteristics(&dwCharacteristics);
			if (dwCharacteristics & (NCCF_FIREWALLED)) {
				NETCON_MEDIATYPE MediaType = NCM_NONE;
				pNCP->get_MediaType(&MediaType);
				if ((MediaType != NCM_SHAREDACCESSHOST_LAN) && (MediaType != NCM_SHAREDACCESSHOST_RAS)) {
					INetSharingConfigurationPtr pNSC;
					if (!SUCCEEDED(m_pINetSM->get_INetSharingConfigurationForINetConnection(NCP, &pNSC)))
						continue;
					VARIANT_BOOL varbool = VARIANT_FALSE;
					pNSC->get_InternetFirewallEnabled(&varbool);
					if (varbool == VARIANT_FALSE)
						continue;
					bFoundAtLeastOneConn = true;
					switch (eAction) {
					case FOC_ADDRULE:
						{
							bool bResult;
							// we do not want to overwrite an existing rule
							if (FindRule(FOC_FINDRULEBYPORT, riPortRule, pNSC, NULL))
								bResult = true;
							else
								bResult = AddRule(riPortRule, pNSC, pNCP);
							bSuccess = bSuccess && bResult;
							if (bResult && !bPartialSucceeded)
								m_liAddedRules.Add(riPortRule); // keep track of added rules
							bPartialSucceeded = bPartialSucceeded || bResult;
							break;
						}
					case FOC_FWCONNECTIONEXISTS:
						return true;
					case FOC_DELETERULEBYNAME:
					case FOC_DELETERULEEXCACT:
						bSuccess = bSuccess && FindRule(eAction, riPortRule, pNSC, NULL);
						break;
					case FOC_FINDRULEBYNAME:
						if (FindRule(FOC_FINDRULEBYNAME, riPortRule, pNSC, NULL))
							return true;
						else
							bSuccess = false;
						break;
					case FOC_FINDRULEBYPORT:
						if (FindRule(FOC_FINDRULEBYPORT, riPortRule, pNSC, NULL))
							return true;
						else
							bSuccess = false;
						break;
					default:
						ASSERT(0);
					}
				}
			}
		}
		var.Clear();
	}
	return bSuccess && bFoundAtLeastOneConn;
}

bool CFirewallOpener::AddRule(const CICSRuleInfo &riPortRule, const INetSharingConfigurationPtr &pNSC, const INetConnectionPropsPtr &pNCP)
{
	INetSharingPortMappingPtr pNSPM;
	HRESULT hr = pNSC->AddPortMapping(riPortRule.m_strRuleName.AllocSysString()
		, riPortRule.m_byProtocol
		, riPortRule.m_nPortNumber
		, riPortRule.m_nPortNumber
		, 0
		, CComBSTR(L"127.0.0.1")
		, ICSTT_IPADDRESS
		, &pNSPM);
	CComBSTR bstrName;
	pNCP->get_Name(&bstrName);
	if (SUCCEEDED(hr) && SUCCEEDED(pNSPM->Enable())) {
		theApp.QueueDebugLogLine(false, _T("Succeeded to add Rule '%s' for Port '%u' on Connection '%s'"), (LPCTSTR)riPortRule.m_strRuleName, riPortRule.m_nPortNumber, (LPCTSTR)CString(bstrName));
		return true;
	}
	theApp.QueueDebugLogLine(false, _T("Failed to add Rule '%s' for Port '%u' on Connection '%s'"), (LPCTSTR)riPortRule.m_strRuleName, riPortRule.m_nPortNumber, (LPCTSTR)CString(bstrName));
	return false;
}

bool CFirewallOpener::FindRule(const EFOCAction eAction, const CICSRuleInfo &riPortRule, const INetSharingConfigurationPtr &pNSC, INetSharingPortMappingPropsPtr *outNSPMP)
{
	INetSharingPortMappingCollectionPtr pNSPMC;
	RETURN_ON_FAIL(pNSC->get_EnumPortMappings(ICSSC_DEFAULT, &pNSPMC));

	INetSharingPortMappingPtr pNSPM;
	IEnumVARIANTPtr varEnum;
	IUnknownPtr pUnk;
	RETURN_ON_FAIL(pNSPMC->get__NewEnum(&pUnk));
	RETURN_ON_FAIL(pUnk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&varEnum));
	_variant_t var;
	while (S_OK == varEnum->Next(1, &var, NULL)) {
		INetSharingPortMappingPropsPtr pNSPMP;
		if (V_VT(&var) == VT_DISPATCH
			&& SUCCEEDED(V_DISPATCH(&var)->QueryInterface(__uuidof(INetSharingPortMapping), (void**)&pNSPM))
			&& SUCCEEDED(pNSPM->get_Properties(&pNSPMP)))
		{
			UCHAR ucProt = 0;
			long uExternal = 0;
			CComBSTR bstrName;
			pNSPMP->get_IPProtocol(&ucProt);
			pNSPMP->get_ExternalPort(&uExternal);
			pNSPMP->get_Name(&bstrName);
			switch (eAction) {
			case FOC_FINDRULEBYPORT:
				if (riPortRule.m_nPortNumber == uExternal && riPortRule.m_byProtocol == ucProt) {
					if (outNSPMP != NULL)
						*outNSPMP = pNSPM;
					return true;
				}
				break;
			case FOC_FINDRULEBYNAME:
				if (riPortRule.m_strRuleName == CString(bstrName)) {
					if (outNSPMP != NULL)
						*outNSPMP = pNSPM;
					return true;
				}
				break;
			case FOC_DELETERULEEXCACT:
				if (riPortRule.m_strRuleName == CString(bstrName)
					&& riPortRule.m_nPortNumber == uExternal && riPortRule.m_byProtocol == ucProt)
				{
					RETURN_ON_FAIL(pNSC->RemovePortMapping(pNSPM));
					theApp.QueueDebugLogLine(false, _T("Rule removed"));
				}
				break;
			case FOC_DELETERULEBYNAME:
				if (riPortRule.m_strRuleName == CString(bstrName)) {
					RETURN_ON_FAIL(pNSC->RemovePortMapping(pNSPM));
					theApp.QueueDebugLogLine(false, _T("Rule removed"));
				}
				break;
			default:
				ASSERT(0);
			}
		}
		var.Clear();
	}

	return (eAction == FOC_DELETERULEBYNAME || eAction == FOC_DELETERULEEXCACT);
}

bool CFirewallOpener::RemoveRule(const CString &strName)
{
	return DoAction(FOC_DELETERULEBYNAME, CICSRuleInfo(0, 0, strName));
}

bool CFirewallOpener::RemoveRule(const CICSRuleInfo &riPortRule)
{
	return DoAction(FOC_DELETERULEEXCACT, riPortRule);
}

bool CFirewallOpener::DoesRuleExist(const CString &strName)
{
	return DoAction(FOC_FINDRULEBYNAME, CICSRuleInfo(0, 0, strName));
}

bool CFirewallOpener::DoesRuleExist(const uint16 nPortNumber, const uint8 byProtocol)
{
	return DoAction(FOC_FINDRULEBYPORT, CICSRuleInfo(nPortNumber, byProtocol, _T("")));
}

bool CFirewallOpener::OpenPort(const uint16 nPortNumber, const uint8 byProtocol, const CString &strRuleName, const bool bRemoveOnExit)
{
	return DoAction(FOC_ADDRULE, CICSRuleInfo(nPortNumber, byProtocol, strRuleName, bRemoveOnExit));
}

bool CFirewallOpener::OpenPort(const CICSRuleInfo &riPortRule)
{
	return DoAction(FOC_ADDRULE, riPortRule);
}

bool CFirewallOpener::DoesFWConnectionExist()
{
	return DoAction(FOC_FWCONNECTIONEXISTS, CICSRuleInfo());
}