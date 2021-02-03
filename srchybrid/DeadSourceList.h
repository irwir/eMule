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

#pragma once
#include "otherfunctions.h"

///////////////////////////////////////////////////////////////////////////////////////
//// CDeadSource

class CDeadSource : public CObject
{
public:
	CDeadSource(const CDeadSource &ds)			{ *this = ds; }
	CDeadSource(uint32 dwID = 0, uint16 nPort = 0, uint32 dwServerIP = 0, uint16 nKadPort = 0);
	explicit CDeadSource(const uchar *paucHash);

	CDeadSource& operator=(const CDeadSource &ds);
	friend bool operator==(const CDeadSource &ds1, const CDeadSource &ds2);

	uint32			m_dwID;
	uint32			m_dwServerIP;
	uint16			m_nPort;
	uint16			m_nKadPort;
	uchar			m_aucHash[16];
};

template<> inline UINT AFXAPI HashKey(const CDeadSource &ds)
{
	uint32 hash = ds.m_dwID;
	if (hash != 0) {
		if (::IsLowID(hash))
			hash ^= ds.m_dwServerIP;
	} else {
		ASSERT(!isnulmd4(ds.m_aucHash));
		hash = 1;
		for (unsigned i = 0; i < 16; ++i)
			hash += (ds.m_aucHash[i] + 1) * ((i * i) + 1);
	}
	return hash;
};

///////////////////////////////////////////////////////////////////////////////////////
//// CDeadSourceList
class CUpDownClient;
class CDeadSourceList
{
public:
	CDeadSourceList();
	~CDeadSourceList() = default;
	void		AddDeadSource(const CUpDownClient *pToAdd);
	void		RemoveDeadSource(const CUpDownClient *client);
	bool		IsDeadSource(const CUpDownClient *pToCheck) const;
	INT_PTR		GetDeadSourcesCount() const		{ return m_mapDeadSources.GetCount(); }
	void		Init(bool bGlobalList);

protected:
	void		CleanUp();

private:
	CMap<CDeadSource, const CDeadSource&, uint32, uint32> m_mapDeadSources;
	uint32	m_dwLastCleanUp;
	bool	m_bGlobalList;
};