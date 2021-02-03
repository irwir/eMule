//this file is part of eMule
//Copyright (C)2004-2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "StdAfx.h"
#include "emule.h"
#include "secrunasuser.h"
#include "Preferences.h"
#include "emuledlg.h"
#include "otherfunctions.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CSecRunAsUser::CSecRunAsUser()
	: m_hADVAPI32_DLL()
	, m_hACTIVEDS_DLL()
	, m_bRunningAsEmule()
	, m_bRunningRestricted()
{
}

CSecRunAsUser::~CSecRunAsUser()
{
	FreeAPI();
}

eResult CSecRunAsUser::PrepareUser()
{
	(void)CoInitialize(NULL);
	bool bResult = false;
	if (!LoadAPI())
		return RES_FAILED;

	try {
		IADsContainerPtr pUsers;
		try {
			IADsWinNTSystemInfoPtr pNTsys;
			if (CoCreateInstance(CLSID_WinNTSystemInfo, NULL, CLSCTX_INPROC_SERVER, IID_IADsWinNTSystemInfo, (void**)&pNTsys) != S_OK)
				throw CString(_T("Failed to create IADsWinNTSystemInfo"));
			// check if we are already running on our eMule Account
			// todo: check if the current account is an administrator

			CComBSTR bstrUserName;
			pNTsys->get_UserName(&bstrUserName);
			m_strCurrentUser = bstrUserName;

			if (m_strCurrentUser == EMULEACCOUNTW) {
				m_bRunningAsEmule = true;
				throw CString(_T("Already running as eMule_Secure Account (everything is fine)"));
			}
			CComBSTR bstrCompName;
			pNTsys->get_ComputerName(&bstrCompName);
			CStringW cscompName = bstrCompName;

			CComBSTR bstrDomainName;
			pNTsys->get_DomainName(&bstrDomainName);
			m_strDomain = bstrDomainName;

			ADSPath.Format(L"WinNT://%s,computer", (LPCTSTR)cscompName);
			if (!SUCCEEDED(ADsGetObject(ADSPath.AllocSysString(), IID_IADsContainer, (void **)&pUsers)))
				throw CString(_T("Failed ADsGetObject()"));

			IEnumVARIANTPtr pEnum;
			ADsBuildEnumerator(pUsers, &pEnum);

			IADsUserPtr pChild;
			_variant_t vChild;
			while (ADsEnumerateNext(pEnum, 1, &vChild, NULL) == S_OK) {
				if (vChild.pdispVal->QueryInterface(IID_IADsUser, (void **)&pChild) != S_OK)
					continue;
				//If the object in the container is user then get properties
				CComBSTR bstrName;
				pChild->get_Name(&bstrName);
				CStringW csName(bstrName);

				// find the emule user account if possible
				if (csName == EMULEACCOUNTW) {
					// account found, set new random password and save it
					m_strPassword = CreateRandomPW();
					if (!SUCCEEDED(pChild->SetPassword(m_strPassword.AllocSysString())))
						throw CString(_T("Failed to set password"));

					bResult = true;
					break;
				}
			}

		} catch (const CString &error) {
			// clean up and abort
			theApp.QueueDebugLogLine(false, _T("Run as unprivileged user: Exception while preparing user account: %s!"), (LPCTSTR)error);
			CoUninitialize();
			return m_bRunningAsEmule ? RES_OK : RES_FAILED;
		}
		if (bResult || CreateEmuleUser(pUsers))
			bResult = SetDirectoryPermissions();
	} catch (...) {
		// clean up and abort
		theApp.QueueDebugLogLine(false, _T("Run as unprivileged user: Unexpected fatal error while preparing user account!"));
		bResult = false;
	}

	CoUninitialize();
	FreeAPI();
	return bResult ? RES_OK_NEED_RESTART : RES_FAILED;
}

bool CSecRunAsUser::CreateEmuleUser(IADsContainerPtr pUsers)
{
	IDispatchPtr pDisp = NULL;
	if (!SUCCEEDED(pUsers->Create(CComBSTR(L"user"), CString(EMULEACCOUNTW).AllocSysString(), &pDisp)))
		return false;

	IADsUserPtr pUser;
	if (!SUCCEEDED(pDisp->QueryInterface(&pUser)))
		return false;

	VARIANT_BOOL bAccountDisabled = VARIANT_FALSE;
	VARIANT_BOOL bIsLocked = VARIANT_FALSE;
	VARIANT_BOOL bPwRequired = VARIANT_TRUE;
	pUser->put_AccountDisabled(bAccountDisabled);
	pUser->put_IsAccountLocked(bIsLocked);
	pUser->put_PasswordRequired(bPwRequired);
	pUser->put_Description(CString(_T("Account used to run eMule with additional security")).AllocSysString());
	pUser->SetInfo();
	m_strPassword = CreateRandomPW();
	return SUCCEEDED(pUser->SetPassword(m_strPassword.AllocSysString()));
}

CStringW CSecRunAsUser::CreateRandomPW()
{
	CStringW strResult;
	while (strResult.GetLength() < 10) {
		char chRnd = (char)('0' + (rand() % 'z')); //'0'..'z'
		if (chRnd >= 'a' && chRnd <= 'z' || chRnd >= 'A' && chRnd <= 'Z' || chRnd >= '0' && chRnd <= '9' || chRnd == '_')
			strResult += chRnd;
	}
	return strResult;
}

bool CSecRunAsUser::SetDirectoryPermissions()
{
#define FULLACCESS ADS_RIGHT_GENERIC_ALL
	// shared files list: read permission only
	// we odnt check for success here, for emule will also run if one dir fails for some reason
	// if there is a dir which is also an incoming dir, rights will be overwritten below
	for (POSITION pos = thePrefs.shareddir_list.GetHeadPosition(); pos != NULL;)
		VERIFY(SetObjectPermission(thePrefs.shareddir_list.GetNext(pos), (DWORD)ADS_RIGHT_GENERIC_READ));

	// set special permission for emule account on needed folders

	// verify permissions on the most important folders. Keep in mind that this is mainly for WinXP-PublicUser-Installs,
	// and is not really needed for MultiUser or Vista Setups
	bool bSucceeded = SetObjectPermission(thePrefs.GetMuleDirectory(EMULE_CONFIGBASEDIR), FULLACCESS);
	bSucceeded = bSucceeded && SetObjectPermission(thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR), FULLACCESS);
	bSucceeded = bSucceeded && SetObjectPermission(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR), FULLACCESS);
	for (int i = 0; i < thePrefs.GetTempDirCount(); ++i)
		bSucceeded = bSucceeded && SetObjectPermission(thePrefs.GetTempDir(i), FULLACCESS);

	INT_PTR cCats = thePrefs.GetCatCount();
	for (int i = 0; i < cCats; ++i)
		if (!thePrefs.GetCatPath(i).IsEmpty())
			bSucceeded = bSucceeded && SetObjectPermission(thePrefs.GetCatPath(i), FULLACCESS);

	if (!bSucceeded)
		theApp.QueueDebugLogLine(false, _T("Run as unprivileged user: Error: Failed to set directory permissions!"));

	return bSucceeded;
}

bool CSecRunAsUser::SetObjectPermission(const CString &strDirFile, DWORD lGrantedAccess)
{
	if (!m_hADVAPI32_DLL) {
		ASSERT(0);
		return false;
	}
	if (strDirFile.IsEmpty())
		return true;

	SID_NAME_USE   snuType;
	TCHAR *szDomain = NULL;
	LPVOID pUserSID = NULL;
	PACL pNewACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	BOOL fAPISuccess;

	try {
		// get user SID
		DWORD cbDomain = 0;
		DWORD cbUserSID = 0;
		fAPISuccess = LookupAccountName(NULL, EMULEACCOUNTW, pUserSID, &cbUserSID, szDomain, &cbDomain, &snuType);
		if ((!fAPISuccess) && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			throw CString(_T("Run as unprivileged user: Error: LookupAccountName() failed,"));

		pUserSID = MHeapAlloc(cbUserSID);
		if (!pUserSID)
			throw CString(_T("Run as unprivileged user: Error: Allocating memory failed,"));

		szDomain = (TCHAR*)MHeapAlloc(cbDomain * sizeof(TCHAR));
		if (!szDomain)
			throw CString(_T("Run as unprivileged user: Error: Allocating memory failed,"));

		fAPISuccess = LookupAccountName(NULL, EMULEACCOUNTW, pUserSID, &cbUserSID, szDomain, &cbDomain, &snuType);
		if (!fAPISuccess)
			throw CString(_T("Run as unprivileged user: Error: LookupAccountName()2 failed"));

		if (CStringW(szDomain) != m_strDomain)
			throw CString(_T("Run as unprivileged user: Logical Error: Domain name mismatch"));

		// get old ACL
		PACL pOldACL = NULL;
		fAPISuccess = GetNamedSecurityInfo(strDirFile, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldACL, NULL, &pSD);
		if (fAPISuccess != ERROR_SUCCESS)
			throw CString(_T("Run as unprivileged user: Error: GetNamedSecurityInfo() failed"));

		// calculate size for new ACL
		ACL_SIZE_INFORMATION AclInfo;
		AclInfo.AceCount = 0; // Assume NULL DACL.
		AclInfo.AclBytesFree = 0;
		AclInfo.AclBytesInUse = sizeof(ACL);

		if (pOldACL != NULL && !GetAclInformation(pOldACL, &AclInfo, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation))
			throw CString(_T("Run as unprivileged user: Error: GetAclInformation() failed"));

		// Create new ACL
		DWORD cbNewACL = AclInfo.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(pUserSID) - sizeof(DWORD);

		pNewACL = (PACL)MHeapAlloc(cbNewACL);
		if (!pNewACL)
			throw CString(_T("Run as unprivileged user: Error: Allocating memory failed,"));

		if (!InitializeAcl(pNewACL, cbNewACL, ACL_REVISION2))
			throw CString(_T("Run as unprivileged user: Error: Allocating memory failed,"));


		// copy the entries form the old acl into the new one and enter a new ace in the right order
		uint32 CurrentAceIndex = 0;
		if (AclInfo.AceCount) {
			for (CurrentAceIndex = 0; CurrentAceIndex < AclInfo.AceCount; ++CurrentAceIndex) {
				LPVOID pTempAce;
				if (!GetAce(pOldACL, CurrentAceIndex, &pTempAce))
					throw CString(_T("Run as unprivileged user: Error: GetAce() failed,"));

				if (((ACCESS_ALLOWED_ACE*)pTempAce)->Header.AceFlags & INHERITED_ACE)
					break;
				// no multiple entries
				if (EqualSid(pUserSID, &(((ACCESS_ALLOWED_ACE*)pTempAce)->SidStart)))
					continue;

				if (!AddAce(pNewACL, ACL_REVISION, MAXDWORD, pTempAce, ((PACE_HEADER)pTempAce)->AceSize))
					throw CString(_T("Run as unprivileged user: Error: AddAce()1 failed,"));
			}
		}
		// here we add the actually entry
		if (!AddAccessAllowedAceEx(pNewACL, ACL_REVISION2, CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE, lGrantedAccess, pUserSID))
			throw CString(_T("Run as unprivileged user: Error: AddAccessAllowedAceEx() failed,"));

		// copy the rest
		if (AclInfo.AceCount) {
			for (; CurrentAceIndex < AclInfo.AceCount; ++CurrentAceIndex) {
				LPVOID pTempAce;
				if (!GetAce(pOldACL, CurrentAceIndex, &pTempAce))
					throw CString(_T("Run as unprivileged user: Error: GetAce()2 failed,"));

				if (!AddAce(pNewACL, ACL_REVISION, MAXDWORD, pTempAce, ((PACE_HEADER)pTempAce)->AceSize))
					throw CString(_T("Run as unprivileged user: Error: AddAce()2 failed,"));
			}
		}

		fAPISuccess = SetNamedSecurityInfo((LPTSTR)(LPCTSTR)strDirFile, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pNewACL, NULL);
		if (fAPISuccess != ERROR_SUCCESS)
			throw CString(_T("Run as unprivileged user: Error: SetNamedSecurityInfo() failed,"));
		fAPISuccess = TRUE;
	} catch (const CString &error) {
		fAPISuccess = FALSE;
		theApp.QueueDebugLogLine(false, error);
	}
	// clean up
	if (pUserSID != NULL)
		MHeapFree(pUserSID);
	if (szDomain != NULL)
		MHeapFree(szDomain);
	if (pNewACL != NULL)
		MHeapFree(pNewACL);
	if (pSD != NULL)
		LocalFree(pSD);

	// finished
	return fAPISuccess != FALSE;
}

eResult CSecRunAsUser::RestartAsUser()
{
	if (m_bRunningRestricted || m_bRunningAsEmule)
		return RES_OK;
	if (!LoadAPI())
		return RES_FAILED;

	TCHAR szAppPath[MAX_PATH + 2];
	DWORD dwModPathLen = ::GetModuleFileName(NULL, &szAppPath[1], MAX_PATH);
	if (dwModPathLen == 0 || dwModPathLen == _countof(szAppPath))
		return RES_FAILED;
	szAppPath[0] = _T('"');
	szAppPath[++dwModPathLen] = _T('"');
	szAppPath[++dwModPathLen] = _T('\0');

	ASSERT(!m_strPassword.IsEmpty());
	BOOL bResult;
	try {
		PROCESS_INFORMATION ProcessInfo = {};

		STARTUPINFOW StartInf = {};
		StartInf.cb = (DWORD)sizeof StartInf;
		StartInf.dwFlags = STARTF_USESHOWWINDOW;
		StartInf.wShowWindow = SW_NORMAL;

		// remove the current mutex, so that the restart emule can create its own without problems
		// in the rare case CreateProcessWithLogonW fails, this will allow mult. instances, but if that function fails we have other problems anyway
		::CloseHandle(theApp.m_hMutexOneInstance);

		bResult = CreateProcessWithLogonW(EMULEACCOUNTW, m_strDomain, m_strPassword
			, LOGON_WITH_PROFILE, NULL, szAppPath, 0, NULL, NULL, &StartInf, &ProcessInfo);
		::CloseHandle(ProcessInfo.hProcess);
		::CloseHandle(ProcessInfo.hThread);
	} catch (...) {
		theApp.QueueDebugLogLine(false, _T("Run as unprivileged user: Error: Unexpected exception while loading advapi32.dll"));
		FreeAPI();
		return RES_FAILED;
	}
	FreeAPI();
	if (bResult)
		return RES_OK_NEED_RESTART;
	
	theApp.QueueDebugLogLine(false, _T("Run as unprivileged user: Error: Failed to restart eMule as different user! Error Code: %i"), ::GetLastError());
	return RES_FAILED;
}

CStringW CSecRunAsUser::GetCurrentUserW() const
{
	return m_strCurrentUser.IsEmpty() ? CString(_T("Unknown")) : m_strCurrentUser;
}

bool CSecRunAsUser::LoadAPI()
{
	if (m_hADVAPI32_DLL == 0) {
		m_hADVAPI32_DLL = LoadLibrary(_T("Advapi32.dll"));
		if (m_hADVAPI32_DLL == 0) {
			AddDebugLogLine(false, _T("Failed to load Advapi32.dll!"));
			return false;
		}
	}
	if (m_hACTIVEDS_DLL == 0) {
		m_hACTIVEDS_DLL = LoadLibrary(_T("ActiveDS"));
		if (m_hACTIVEDS_DLL == 0) {
			AddDebugLogLine(false, _T("Failed to load ActiveDS.dll!"));
			return false;
		}
	}

	bool bSucceeded = (CreateProcessWithLogonW = (TCreateProcessWithLogonW) GetProcAddress(m_hADVAPI32_DLL,"CreateProcessWithLogonW")) != NULL
			&& (GetNamedSecurityInfo = (TGetNamedSecurityInfo)GetProcAddress(m_hADVAPI32_DLL,_TWINAPI("GetNamedSecurityInfo"))) != NULL
			&& (SetNamedSecurityInfo = (TSetNamedSecurityInfo)GetProcAddress(m_hADVAPI32_DLL,_TWINAPI("SetNamedSecurityInfo"))) != NULL
			&& (AddAccessAllowedAceEx = (TAddAccessAllowedAceEx)GetProcAddress(m_hADVAPI32_DLL,"AddAccessAllowedAceEx")) != NULL
			// Probably these functions do not need to be loaded dynamically, but just to be sure
			&& (LookupAccountName = (TLookupAccountName)GetProcAddress(m_hADVAPI32_DLL,_TWINAPI("LookupAccountName"))) != NULL
			&& (GetAclInformation = (TGetAclInformation)GetProcAddress(m_hADVAPI32_DLL,"GetAclInformation")) != NULL
			&& (InitializeAcl = (TInitializeAcl)GetProcAddress(m_hADVAPI32_DLL,"InitializeAcl")) != NULL
			&& (GetAce = (TGetAce)GetProcAddress(m_hADVAPI32_DLL,"GetAce")) != NULL
			&& (AddAce = (TAddAce)GetProcAddress(m_hADVAPI32_DLL,"AddAce")) != NULL
			&& (EqualSid = (TEqualSid)GetProcAddress(m_hADVAPI32_DLL,"EqualSid")) != NULL
			&& (GetLengthSid = (TGetLengthSid)GetProcAddress(m_hADVAPI32_DLL,"GetLengthSid")) != NULL
			// for SecureShellExecute
			&& (OpenProcessToken = (TOpenProcessToken)GetProcAddress(m_hADVAPI32_DLL,"OpenProcessToken")) != NULL
			&& (GetTokenInformation = (TGetTokenInformation)GetProcAddress(m_hADVAPI32_DLL,"GetTokenInformation")) != NULL
			&& (CreateRestrictedToken = (TCreateRestrictedToken)GetProcAddress(m_hADVAPI32_DLL,"CreateRestrictedToken")) != NULL
			&& (CreateProcessAsUser = (TCreateProcessAsUser)GetProcAddress(m_hADVAPI32_DLL,_TWINAPI("CreateProcessAsUser"))) != NULL
			// activeDS.dll
			&& (ADsGetObject = (TADsGetObject)GetProcAddress(m_hACTIVEDS_DLL,"ADsGetObject")) != NULL
			&& (ADsBuildEnumerator = (TADsBuildEnumerator)GetProcAddress(m_hACTIVEDS_DLL,"ADsBuildEnumerator")) != NULL
			&& (ADsEnumerateNext = (TADsEnumerateNext)GetProcAddress(m_hACTIVEDS_DLL,"ADsEnumerateNext")) != NULL;

	if (!bSucceeded) {
		AddDebugLogLine(false, _T("Failed to load all functions from Advapi32.dll!"));
		FreeAPI();
	}
	return bSucceeded;
}

void CSecRunAsUser::FreeAPI()
{
	if (m_hADVAPI32_DLL != 0) {
		FreeLibrary(m_hADVAPI32_DLL);
		m_hADVAPI32_DLL = 0;
	}
	if (m_hACTIVEDS_DLL != 0) {
		FreeLibrary(m_hACTIVEDS_DLL);
		m_hACTIVEDS_DLL = 0;
	}
}

eResult CSecRunAsUser::RestartAsRestricted()
{
	if (m_bRunningRestricted || m_bRunningAsEmule)
		return RES_OK;
	if (!LoadAPI())
		return RES_FAILED;

	HANDLE hProcessToken = NULL;
	HANDLE hRestrictedToken = NULL;
	PTOKEN_USER pstructUserToken = NULL;

	try {
		// get our access token from the process
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_READ, &hProcessToken))
			throw(CString(_T("Failed to retrieve access token from process")));

		// there is no easy way to check if we have already restricted token when not using the restricted SID list
		// so just check if we set the SANDBOX_INERT flag and hope no one else did
		// (which is unlikely tho because AFAIK you would only set it when using CreateRestirctedToken) :)
		DWORD dwLen = 0;
		DWORD dwInertFlag;
		if (!GetTokenInformation(hProcessToken, TokenSandBoxInert, &dwInertFlag, sizeof(dwInertFlag), &dwLen))
			throw(CString(_T("Failed to Flag-Status from AccessToken")));

		if (dwInertFlag != 0) {
			m_bRunningRestricted = true;
			throw(CString(_T("Already using a restricted Token it seems (everything is fine!)")));
		}

		// get the user account SID to disable it in our new token
		dwLen = 0;
		while (!GetTokenInformation(hProcessToken, TokenUser, pstructUserToken, dwLen, &dwLen)) {
			if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || pstructUserToken != NULL)
				throw(CString(_T("Failed to retrieve UserSID from AccessToken")));
			pstructUserToken = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLen);
		}
		if (pstructUserToken == NULL)
			throw(CString(_T("Failed to retrieve UserSID from AccessToken")));

		// disabling our primary token would make sense from a Security POV, but this would cause file acces conflicts
		// in the default settings (since we cannot access files we created ourself if they don't have the write flag for the group "users")
		// so it stays enabled for now and we only reduce privileges

		// create the new token
		if (!CreateRestrictedToken(hProcessToken, DISABLE_MAX_PRIVILEGE | SANDBOX_INERT, 0 /*disabled*/, &pstructUserToken->User, 0, NULL, 0, NULL, &hRestrictedToken))
			throw(CString(_T("Failed to create Restricted Token")));

		// do the starting job
		TCHAR szAppPath[MAX_PATH + 2];
		DWORD dwModPathLen = ::GetModuleFileName(NULL, &szAppPath[1], MAX_PATH);
		if (dwModPathLen == 0 || dwModPathLen >= MAX_PATH)
			throw CString(_T("Failed to get module file path"));
		szAppPath[0] = _T('"');
		szAppPath[++dwModPathLen] = _T('"');
		szAppPath[++dwModPathLen] = _T('\0');
		//CString strAppName;
		//strAppName.Format(_T("\"%s\""), szAppPath);

		STARTUPINFO StartInf = {};
		StartInf.cb = sizeof StartInf;
		StartInf.dwFlags = STARTF_USESHOWWINDOW;
		StartInf.wShowWindow = SW_NORMAL;

		// remove the current mutex, so that the restart emule can create its own without problems
		// in the rare case CreateProcessWithLogonW fails, this will allow mult. instances, but if that function fails we have other problems anyway
		::CloseHandle(theApp.m_hMutexOneInstance);

		PROCESS_INFORMATION ProcessInfo = {};
		if (!CreateProcessAsUser(hRestrictedToken, NULL, szAppPath, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &StartInf, &ProcessInfo)) {
			CString e;
			GetErrorMessage(::GetLastError(), e, 0);
			throw (_T("CreateProcessAsUser failed: ") + e);
		}
		::CloseHandle(ProcessInfo.hProcess);
		::CloseHandle(ProcessInfo.hThread);

		// cleanup
		HeapFree(GetProcessHeap(), 0, (LPVOID)pstructUserToken);
		pstructUserToken = NULL;
		::CloseHandle(hRestrictedToken);
		::CloseHandle(hProcessToken);
	} catch (const CString &strError) {
		if (hProcessToken != NULL)
			::CloseHandle(hProcessToken);
		if (hRestrictedToken != NULL)
			::CloseHandle(hRestrictedToken);
		if (pstructUserToken != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pstructUserToken);

		theApp.QueueDebugLogLine(false, _T("SecureShellExecute exception: %s!"), (LPCTSTR)strError);
		return m_bRunningRestricted ? RES_OK : RES_FAILED;
	}
	return RES_OK_NEED_RESTART;
}

eResult CSecRunAsUser::RestartSecure()
{
	eResult res;

	if (!thePrefs.IsPreferingRestrictedOverUser()) {
		res = PrepareUser();
		if (res == RES_OK) {
			theApp.QueueLogLine(false, GetResString(IDS_RAU_RUNNING), EMULEACCOUNTW);
			return RES_OK;
		}
		if (res == RES_OK_NEED_RESTART) {
			res = RestartAsUser();
			if (res != RES_FAILED)
				return res;
		}
	}

	res = RestartAsRestricted();
	if (res == RES_OK)
		theApp.QueueLogLine(false, GetResString(IDS_RUNNINGRESTRICTED));

	return res;
}