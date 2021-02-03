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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once
#include "SafeFile.h"

namespace Kademlia
{
	class CKadUDPKey
	{
	public:
		explicit CKadUDPKey(uint32 uZero = 0)			{ ASSERT(uZero == 0); m_dwKey = uZero; m_dwIP = 0; }
		CKadUDPKey(uint32 dwKey, uint32 dwIP)			{ m_dwKey = dwKey; m_dwIP = dwIP; }
		CKadUDPKey(const CKadUDPKey &k1)				{ *this = k1; }
		explicit CKadUDPKey(CFileDataIO &file)			{ ReadFromFile(file); }
		CKadUDPKey& operator=(const CKadUDPKey &k1)		{ m_dwKey = k1.m_dwKey; m_dwIP = k1.m_dwIP; return *this; }
		CKadUDPKey& operator=(const uint32 uZero)		{ ASSERT(uZero == 0); m_dwKey = uZero; m_dwIP = 0; return *this; }
		friend bool operator==(const CKadUDPKey &k1,const CKadUDPKey &k2)	{ return k1.GetKeyValue(k1.m_dwIP) == k2.GetKeyValue(k2.m_dwIP); }

		uint32	GetKeyValue(uint32 dwMyIP) const		{ return (dwMyIP == m_dwIP) ? m_dwKey : 0; }
		bool	IsEmpty() const							{ return (m_dwKey == 0) || (m_dwIP == 0); }
		void	StoreToFile(CFileDataIO &file) const	{ file.WriteUInt32(m_dwKey); file.WriteUInt32(m_dwIP); }
		void	ReadFromFile(CFileDataIO &file)			{ m_dwKey = file.ReadUInt32(); m_dwIP = file.ReadUInt32(); }
	private:
		uint32	m_dwKey;
		uint32	m_dwIP;
	};
}