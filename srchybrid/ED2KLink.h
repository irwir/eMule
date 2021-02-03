//this file is part of eMule
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
#include "shahashset.h"
#include "otherfunctions.h"

class CSafeMemFile;

class CED2KLink
{
public:
	static CED2KLink* CreateLinkFromUrl(LPCTSTR uri);
	virtual	~CED2KLink() = default;

	typedef enum
	{
		kServerList,
		kServer,
		kFile,
		kNodesList,
		kSearch,
		kInvalid
	} LinkType;

	virtual LinkType GetKind() const						{ return kInvalid; }
	virtual void GetLink(CString &lnk) const = 0;
	virtual class CED2KServerListLink* GetServerListLink()	{ return NULL; }
	virtual class CED2KServerLink* GetServerLink()			{ return NULL; }
	virtual class CED2KFileLink* GetFileLink()				{ return NULL; }
	virtual class CED2KNodesListLink* GetNodesListLink()	{ return NULL; }
	virtual class CED2KSearchLink* GetSearchLink()			{ return NULL; }
};


class CED2KServerLink : public CED2KLink
{
public:
	CED2KServerLink(LPCTSTR ip, LPCTSTR port);

	LinkType GetKind() const								{ return kServer; }
	void GetLink(CString &lnk) const;
	CED2KServerLink* GetServerLink()						{ return this; }

	const CString& GetAddress() const						{ return m_strAddress; }
	uint16 GetPort() const									{ return m_port; }
	void GetDefaultName(CString &defName) const				{ defName = m_defaultName; }

private:
	CED2KServerLink();
	CED2KServerLink(const CED2KServerLink&) = delete;
	CED2KServerLink& operator=(const CED2KServerLink&) = delete;

	CString m_strAddress;
	uint16 m_port;
	CString m_defaultName;
};


class CED2KFileLink : public CED2KLink
{
public:
	CED2KFileLink(LPCTSTR pszName, LPCTSTR pszSize, LPCTSTR pszHash, const CStringArray &astrParams, LPCTSTR pszSources);
	~CED2KFileLink();

	LinkType GetKind() const							{ return kFile; };
	void GetLink(CString &lnk) const;
	CED2KFileLink* GetFileLink()						{ return this; }

	LPCTSTR GetName() const								{ return (LPCTSTR)m_name; }
	const uchar* GetHashKey() const						{ return m_hash; }
	const CAICHHash &GetAICHHash() const				{ return m_AICHHash; }
	EMFileSize GetSize() const							{ return (EMFileSize)(uint64)(_tstoi64(m_size)); }
	bool HasValidSources() const						{ return SourcesList != NULL; }
	bool HasHostnameSources() const						{ return !m_HostnameSourcesList.IsEmpty(); }
	bool HasValidAICHHash() const						{ return m_bAICHHashValid; }

	CSafeMemFile *SourcesList;
	CSafeMemFile *m_hashset;
	CTypedPtrList<CPtrList, SUnresolvedHostname*> m_HostnameSourcesList;

private:
	CED2KFileLink();
	CED2KFileLink(const CED2KFileLink&) = delete;
	CED2KFileLink& operator=(const CED2KFileLink&) = delete;

	CAICHHash m_AICHHash;
	CString m_name;
	CString m_size;
	uchar	m_hash[16];
	bool	m_bAICHHashValid;
};


class CED2KServerListLink : public CED2KLink
{
public:
	explicit CED2KServerListLink(LPCTSTR address);

	LinkType GetKind() const							{ return kServerList; }
	void GetLink(CString &lnk) const;
	CED2KServerListLink* GetServerListLink()			{ return this; }

	LPCTSTR GetAddress() const							{ return m_address; }

private:
	CED2KServerListLink();
	CED2KServerListLink(const CED2KServerListLink&) = delete;
	CED2KServerListLink& operator=(const CED2KServerListLink&) = delete;

	CString m_address;
};


class CED2KNodesListLink : public CED2KLink
{
public:
	explicit CED2KNodesListLink(LPCTSTR address);

	LinkType GetKind() const							{ return kNodesList; }
	void GetLink(CString &lnk) const;
	CED2KNodesListLink* GetNodesListLink()				{ return this; }

	LPCTSTR GetAddress() const							{ return m_address; }

private:
	CED2KNodesListLink();
	CED2KNodesListLink(const CED2KNodesListLink&) = delete;
	CED2KNodesListLink& operator=(const CED2KNodesListLink&) = delete;

	CString m_address;
};

class CED2KSearchLink : public CED2KLink
{
public:
	explicit CED2KSearchLink(LPCTSTR pszSearchTerm);

	LinkType GetKind() const							{ return kSearch; }
	void GetLink(CString &lnk) const;
	CED2KSearchLink* GetSearchLink()					{ return this; }

	const CString& GetSearchTerm() const				{ return m_strSearchTerm; }

private:
	CED2KSearchLink();
	CED2KSearchLink(const CED2KSearchLink&) = delete;
	CED2KSearchLink& operator=(const CED2KSearchLink&) = delete;
	CString m_strSearchTerm;
};