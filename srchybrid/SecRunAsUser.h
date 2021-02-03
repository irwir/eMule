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

#pragma once
#include <Iads.h>
#include <activeds.h>
#include <comdef.h>
#include <initguid.h>

#define LOGON_WITH_PROFILE			0x00000001
#define LOGON_NETCREDENTIALS_ONLY	0x00000002

enum eResult
{
	RES_OK_NEED_RESTART = 0,
	RES_OK,
	RES_FAILED
};

typedef BOOL(WINAPI *TCreateProcessWithLogonW)
(
	LPCWSTR lpUsername,                 // user's name
	LPCWSTR lpDomain,                   // user's domain
	LPCWSTR lpPassword,                 // user's password
	DWORD dwLogonFlags,                 // logon option
	LPCWSTR lpApplicationName,          // executable module name
	LPWSTR lpCommandLine,               // command-line string
	DWORD dwCreationFlags,              // creation flags
	LPVOID lpEnvironment,               // new environment block
	LPCWSTR lpCurrentDirectory,         // current directory name
	LPSTARTUPINFOW lpStartupInfo,       // startup information
	LPPROCESS_INFORMATION lpProcessInfo // process information
);

typedef DWORD(WINAPI *TGetNamedSecurityInfo)
(
	LPCTSTR pObjectName,                        // object name
	SE_OBJECT_TYPE ObjectType,                 // object type
	SECURITY_INFORMATION SecurityInfo,         // information type
	PSID *ppsidOwner,                          // owner SID
	PSID *ppsidGroup,                          // primary group SID
	PACL *ppDacl,                              // DACL
	PACL *ppSacl,                              // SACL
	PSECURITY_DESCRIPTOR *ppSecurityDescriptor // SD
);

typedef DWORD(WINAPI *TSetNamedSecurityInfo)
(
	LPTSTR pObjectName,                // object name
	SE_OBJECT_TYPE ObjectType,         // object type
	SECURITY_INFORMATION SecurityInfo, // type
	PSID psidOwner,                    // new owner SID
	PSID psidGroup,                    // new primary group SID
	PACL pDacl,                        // new DACL
	PACL pSacl                         // new SACL
);

typedef BOOL(WINAPI *TAddAccessAllowedAceEx)
(
	PACL pAcl,            // access control list
	DWORD dwAceRevision,  // ACL revision level
	DWORD AceFlags,       // ACE inheritance flags
	DWORD AccessMask,     // access mask for the new ACE
	PSID pSid             // trustee SID for new ACE
);

typedef BOOL(WINAPI *TLookupAccountName)
(
	LPCTSTR lpSystemName,   // system name
	LPCTSTR lpAccountName,  // account name
	PSID Sid,               // security identifier
	LPDWORD cbSid,          // size of security identifier
	LPTSTR DomainName,      // domain name
	LPDWORD cbDomainName,   // size of domain name
	PSID_NAME_USE peUse     // SID-type indicator
);

typedef BOOL(WINAPI *TGetAclInformation)
(
	PACL pAcl,                                   // access-control list
	LPVOID pAclInformation,                      // ACL information
	DWORD nAclInformationLength,                 // size of ACL information
	ACL_INFORMATION_CLASS dwAclInformationClass  // info class
	);

typedef BOOL(WINAPI *TInitializeAcl)
(
	PACL pAcl,			// ACL
	DWORD nAclLength,	// size of ACL
	DWORD dwAclRevision	// revision level of ACL
);

typedef BOOL(WINAPI *TGetAce)
(
	PACL	pAcl,		// access-control list
	DWORD	dwAceIndex,	// index of ACE to retrieve
	LPVOID	*pAce		// ACE
);

typedef BOOL(WINAPI *TAddAce)
(
	PACL pAcl,					// access-control list
	DWORD dwAceRevision,		// ACL revision level
	DWORD dwStartingAceIndex,	// index of ACE position in ACL
	LPVOID pAceList,			// one or more ACEs
	DWORD nAceListLength		// size of buffer for ACEs
);

typedef BOOL(WINAPI *TEqualSid)
(
	PSID pSid1,
	PSID pSid2
);

typedef DWORD(WINAPI *TGetLengthSid)
(
	PSID pSid   // SID to query
);

typedef HRESULT(WINAPI *TADsGetObject)
(
	LPWSTR	lpszPathName,
	REFIID	riid,
	VOID	**ppObject
);

typedef HRESULT(WINAPI *TADsBuildEnumerator)
(
	IADsContainer *pADsContainer,
	IEnumVARIANT **ppEnumVariant
);

typedef HRESULT(WINAPI *TADsEnumerateNext)
(
	IEnumVARIANT *pEnumVariant,
	ULONG cElements,
	VARIANT *pvar,
	ULONG *pcElementsFetched
);

typedef BOOL(WINAPI *TOpenProcessToken)
(
	HANDLE ProcessHandle,
	DWORD DesiredAccess,
	PHANDLE TokenHandle
);

typedef BOOL(WINAPI *TGetTokenInformation)
(
	HANDLE TokenHandle,
	TOKEN_INFORMATION_CLASS TokenInformationClass,
	LPVOID TokenInformation,
	DWORD TokenInformationLength,
	PDWORD ReturnLength
);

typedef BOOL(WINAPI *TCreateRestrictedToken)
(
	HANDLE ExistingTokenHandle,              // handle to existing token
	DWORD Flags,                             // privilege options
	DWORD DisableSidCount,                   // number of deny-only SIDs
	PSID_AND_ATTRIBUTES SidsToDisable,       // deny-only SIDs
	DWORD DeletePrivilegeCount,              // number of privileges
	PLUID_AND_ATTRIBUTES PrivilegesToDelete, // privileges
	DWORD RestrictedSidCount,                // number of restricting SIDs
	PSID_AND_ATTRIBUTES SidsToRestrict,      // list of restricting SIDs
	PHANDLE NewTokenHandle                   // handle to new token
);

typedef BOOL(WINAPI *TCreateProcessAsUser)(
	HANDLE hToken,                             // handle to user token
	LPCTSTR lpApplicationName,                 // name of executable module
	LPTSTR lpCommandLine,                      // command-line string
	LPSECURITY_ATTRIBUTES lpProcessAttributes, // SD
	LPSECURITY_ATTRIBUTES lpThreadAttributes,  // SD
	BOOL bInheritHandles,                      // inheritance option
	DWORD dwCreationFlags,                     // creation flags
	LPVOID lpEnvironment,                      // new environment block
	LPCTSTR lpCurrentDirectory,                // current directory name
	LPSTARTUPINFO lpStartupInfo,               // startup information
	LPPROCESS_INFORMATION lpProcessInformation // process information
);


typedef _com_ptr_t<_com_IIID<IADsContainer, &IID_IADsContainer> >  IADsContainerPtr;
typedef _com_ptr_t<_com_IIID<IADs, &IID_IADs> >  IADsPtr;
typedef _com_ptr_t<_com_IIID<IADsUser, &IID_IADsUser> >  IADsUserPtr;
typedef _com_ptr_t<_com_IIID<IADsAccessControlEntry, &IID_IADsAccessControlEntry> >  IIADsAccessControlEntryPtr;
typedef _com_ptr_t<_com_IIID<IADsSecurityDescriptor, &IID_IADsSecurityDescriptor> >  IADsSecurityDescriptorPtr;
typedef _com_ptr_t<_com_IIID<IADsWinNTSystemInfo, &IID_IADsWinNTSystemInfo> >  IADsWinNTSystemInfoPtr;
#define MHeapAlloc(x) (HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, x))
#define MHeapFree(x)  (HeapFree(GetProcessHeap(), 0, x))

#define EMULEACCOUNTW L"eMule_Secure"

class CSecRunAsUser
{
public:
	CSecRunAsUser();
	~CSecRunAsUser();

	eResult	RestartSecure();
	bool	IsRunningEmuleAccount() const	{ return m_bRunningAsEmule; }
	bool	IsRunningRestricted() const		{ return m_bRunningRestricted; }
	bool	IsRunningSecure() const			{ return m_bRunningRestricted || m_bRunningAsEmule; }
	CStringW	GetCurrentUserW() const;

protected:
	eResult	PrepareUser();
	eResult	RestartAsUser();
	eResult	RestartAsRestricted();

	bool	SetDirectoryPermissions();
	bool	CreateEmuleUser(IADsContainerPtr pUsers);
	static CStringW	CreateRandomPW();
	bool	SetObjectPermission(const CString &strDirFile, DWORD lGrantedAccess);
	bool	LoadAPI();
	void	FreeAPI();

private:
	CStringW ADSPath;
	CStringW m_strPassword;
	CStringW m_strDomain;
	CStringW m_strCurrentUser;
	HMODULE m_hADVAPI32_DLL;
	HMODULE m_hACTIVEDS_DLL;
	bool m_bRunningAsEmule;
	bool m_bRunningRestricted;

	TCreateProcessWithLogonW CreateProcessWithLogonW;
	TGetNamedSecurityInfo GetNamedSecurityInfo;
	TSetNamedSecurityInfo SetNamedSecurityInfo;
	TAddAccessAllowedAceEx AddAccessAllowedAceEx;
	TLookupAccountName LookupAccountName;
	TGetAclInformation GetAclInformation;
	TInitializeAcl InitializeAcl;
	TGetAce GetAce;
	TAddAce AddAce;
	TEqualSid EqualSid;
	TGetLengthSid GetLengthSid;
	TOpenProcessToken OpenProcessToken;
	TGetTokenInformation GetTokenInformation;
	TCreateRestrictedToken CreateRestrictedToken;
	TCreateProcessAsUser CreateProcessAsUser;

	TADsGetObject ADsGetObject;
	TADsBuildEnumerator ADsBuildEnumerator;
	TADsEnumerateNext ADsEnumerateNext;
};