//this file is part of eMule
//Copyright (C)2002-2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "opcodes.h"

enum E_DebugFSAtion
{
	DFSA_ADD = 0,
	DFSA_SUB,
	DFSA_MUL,
	DFSA_DIV
};

class CEMFileSize
{
public:
	CEMFileSize()																{ m_nSize = _UI64_MAX; }
	CEMFileSize(uint64 nSize)													{ m_nSize = nSize; Check(); }
	CEMFileSize(const CEMFileSize &k1)											{ m_nSize = k1.m_nSize; Check(); }
	__declspec(deprecated) CEMFileSize(uint32 nSize)							{ m_nSize = nSize; Check(); }

	CEMFileSize& operator=(const CEMFileSize &k1)								{ m_nSize = k1.m_nSize; Check(); return *this; }
	CEMFileSize& operator=(uint64 k1)											{ m_nSize = k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator=(sint64 k1)					{ m_nSize = k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator=(uint32 k1)					{ m_nSize = k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator=(sint32 k1)					{ m_nSize = k1; Check(); return *this; }

	CEMFileSize& operator-=(const CEMFileSize &k1)								{ Check(); m_nSize -= k1.m_nSize; Check(); return *this; }
	CEMFileSize& operator-=(uint64 k1)											{ Check(); m_nSize -= k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator-=(sint64 k1)					{ Check(); m_nSize -= k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator-=(uint32 k1)					{ Check(); m_nSize -= k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator-=(sint32 k1)					{ Check(); m_nSize -= k1; Check(); return *this; }

	CEMFileSize& operator+=(const CEMFileSize &k1)								{ Check(); m_nSize += k1.m_nSize; Check(); return *this; }
	CEMFileSize& operator+=(uint64 k1)											{ Check(); m_nSize += k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator+=(sint64 k1)					{ Check(); m_nSize += k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator+=(uint32 k1)					{ Check(); m_nSize += k1; Check(); return *this; }
	__declspec(deprecated) CEMFileSize& operator+=(sint32 k1)					{ Check(); m_nSize += k1; Check(); return *this; }


	operator uint64() const														{ return m_nSize; }
	operator double() const														{ return (double)m_nSize; }
	/*__declspec(deprecated)*/ operator float() const							{ return (float)m_nSize; }
	/*__declspec(deprecated)*/ operator sint64() const							{ return (sint64)m_nSize; }
	__declspec(deprecated) operator uint32() const								{ ASSERT(m_nSize < _UI32_MAX); return (uint32)m_nSize; }
	__declspec(deprecated) operator sint32() const								{ ASSERT(m_nSize < _I32_MAX); return (sint32)m_nSize; }

	friend bool operator==(const CEMFileSize &k1, const CEMFileSize &k2)		{ return k1.m_nSize == k2.m_nSize; }
	friend bool operator==(const CEMFileSize &k1, uint64 k2)					{ return k1.m_nSize == k2; }
	friend bool operator==(uint64 k1, const CEMFileSize &k2)					{ return k1 == k2.m_nSize; }
	__declspec(deprecated) friend bool operator==(uint32 k1, const CEMFileSize &k2)	{ return k1 == k2.m_nSize; }
	__declspec(deprecated) friend bool operator==(const CEMFileSize &k1, uint32 k2)	{ return k1.m_nSize == k2; }

	friend bool operator!=(const CEMFileSize &k1, const CEMFileSize &k2)		{ return k1.m_nSize != k2.m_nSize; }
	friend bool operator!=(const CEMFileSize &k1, uint64 k2)					{ return k1.m_nSize != k2; }
	friend bool operator!=(uint64 k1, const CEMFileSize &k2)					{ return k1 != k2.m_nSize; }
	__declspec(deprecated) friend bool operator!=(uint32 k1, const CEMFileSize &k2)	{ return k1 != k2.m_nSize; }
	__declspec(deprecated) friend bool operator!=(const CEMFileSize &k1, uint32 k2)	{ return k1.m_nSize != k2; }

	friend bool operator>(const CEMFileSize &k1, const CEMFileSize &k2)			{ return k1.m_nSize > k2.m_nSize; }
	friend bool operator>(const CEMFileSize &k1, uint64 k2)						{ return k1.m_nSize > k2; }
	friend bool operator>(uint64 k1, const CEMFileSize &k2)						{ return k1 > k2.m_nSize; }
	__declspec(deprecated) friend bool operator>(uint32 k1, const CEMFileSize &k2)	{ return k1 > k2.m_nSize; }
	__declspec(deprecated) friend bool operator>(const CEMFileSize &k1, uint32 k2)	{ return k1.m_nSize > k2; }

	friend bool operator<(const CEMFileSize &k1, const CEMFileSize &k2)			{ return k1.m_nSize < k2.m_nSize; }
	friend bool operator<(const CEMFileSize &k1, uint64 k2)						{ return k1.m_nSize < k2; }
	friend bool operator<(uint64 k1, const CEMFileSize &k2)						{ return k1 < k2.m_nSize; }
	__declspec(deprecated) friend bool operator<(uint32 k1, const CEMFileSize &k2)	{ return k1 < k2.m_nSize; }
	__declspec(deprecated) friend bool operator<(const CEMFileSize &k1, uint32 k2)	{ return k1.m_nSize < k2; }

	friend bool operator>=(const CEMFileSize &k1, const CEMFileSize &k2)			{ return k1.m_nSize >= k2.m_nSize; }
	friend bool operator>=(const CEMFileSize &k1, uint64 k2)						{ return k1.m_nSize >= k2; }
	friend bool operator>=(uint64 k1, const CEMFileSize &k2)						{ return k1 >= k2.m_nSize; }
	__declspec(deprecated) friend bool operator>=(uint32 k1,const CEMFileSize &k2)	{ return k1 >= k2.m_nSize; }
	__declspec(deprecated) friend bool operator>=(const CEMFileSize &k1,uint32 k2)	{ return k1.m_nSize >= k2; }

	friend bool operator<=(const CEMFileSize &k1, const CEMFileSize &k2)			{ return k1.m_nSize <= k2.m_nSize; }
	friend bool operator<=(const CEMFileSize &k1, uint64 k2)						{ return k1.m_nSize <= k2; }
	friend bool operator<=(uint64 k1, const CEMFileSize &k2)						{ return k1 <= k2.m_nSize; }
	__declspec(deprecated) friend bool operator<=(uint32 k1, const CEMFileSize &k2)	{ return k1 <= k2.m_nSize; }
	__declspec(deprecated) friend bool operator<=(const CEMFileSize &k1, uint32 k2)	{ return k1.m_nSize <= k2; }

	friend CEMFileSize operator+(const CEMFileSize &k1, const CEMFileSize &k2)	{ return CEMFileSize(k1.m_nSize, k2.m_nSize, DFSA_ADD); }
	friend CEMFileSize operator+(const CEMFileSize &k1, uint64 k2)				{ return CEMFileSize(k1.m_nSize, k2, DFSA_ADD); }
	friend CEMFileSize operator+(uint64 k1, const CEMFileSize &k2)				{ return CEMFileSize(k1, k2.m_nSize, DFSA_ADD); }

	friend CEMFileSize operator-(const CEMFileSize &k1, const CEMFileSize &k2)	{ return CEMFileSize(k1.m_nSize, k2.m_nSize, DFSA_SUB); }
	friend CEMFileSize operator-(const CEMFileSize &k1, uint64 k2)				{ return CEMFileSize(k1.m_nSize, k2, DFSA_SUB); }
	friend CEMFileSize operator-(uint64 k1, const CEMFileSize &k2)				{ return CEMFileSize(k1, k2.m_nSize, DFSA_SUB); }
	__declspec(deprecated) friend CEMFileSize operator-(uint32 k1, const CEMFileSize &k2)	{ return CEMFileSize(k1, k2.m_nSize, DFSA_SUB); }

	friend CEMFileSize operator*(const CEMFileSize &k1, const CEMFileSize &k2)	{ return CEMFileSize(k1.m_nSize, k2.m_nSize, DFSA_MUL); }
	friend CEMFileSize operator*(const CEMFileSize &k1, uint64 k2)				{ return CEMFileSize(k1.m_nSize, k2, DFSA_MUL); }
	friend CEMFileSize operator*(uint64 k1, const CEMFileSize &k2)				{ return CEMFileSize(k1, k2.m_nSize, DFSA_MUL); }

	friend CEMFileSize operator/(const CEMFileSize &k1, const CEMFileSize &k2)	{ return CEMFileSize(k1.m_nSize, k2.m_nSize, DFSA_DIV); }
	friend CEMFileSize operator/(const CEMFileSize &k1, uint64 k2)				{ return CEMFileSize(k1.m_nSize, k2, DFSA_DIV); }
	friend CEMFileSize operator/(uint64 k1, const CEMFileSize &k2)				{ return CEMFileSize(k1, k2.m_nSize, DFSA_DIV); }

private:
	CEMFileSize(uint64 nSize1, uint64 nSize2, E_DebugFSAtion edfsAction)
	{
		switch (edfsAction) {
		case DFSA_ADD:
			m_nSize = nSize1 + nSize2;
			ASSERT(m_nSize >= nSize1 && m_nSize >= nSize2 && m_nSize <= MAX_EMULE_FILE_SIZE);
			break;
		case DFSA_SUB:
			m_nSize = nSize1 - nSize2;
			ASSERT(m_nSize <= nSize1 && m_nSize <= MAX_EMULE_FILE_SIZE);
			break;
		case DFSA_DIV:
			if (nSize2 != 0)
				m_nSize = nSize1 / nSize2;
			else
				ASSERT(0);
			break;
		case DFSA_MUL:
			m_nSize = nSize1 * nSize2;
			ASSERT(m_nSize >= nSize1 && m_nSize >= nSize2 && m_nSize <= MAX_EMULE_FILE_SIZE);
		}
	}
	void	Check()	const
	{
		ASSERT( m_nSize != _UI64_MAX && m_nSize <= MAX_EMULE_FILE_SIZE);
	}
	uint64	m_nSize;
};