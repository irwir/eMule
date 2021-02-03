/*
Copyright (C)2003 Barry Dunne (http://www.emule-project.net)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Note To Mods //
/*
Please do not change anything here and release it.
There is going to be a new forum created just for the Kademlia side of the client.
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it. If it is a real improvement,
it will be added to the official client. Changing something without knowing
what all it does can cause great harm to the network if released in mass form.
Any mod that changes anything within the Kademlia side will not be allowed to advertise
their client on the eMule forum.
*/

#pragma once
#include "kademlia/routing/Maps.h"

namespace Kademlia
{
	class CDataIO
	{
	public:
		byte ReadByte();
		uint8 ReadUInt8();
		uint16 ReadUInt16();
		uint32 ReadUInt32();
		uint64 ReadUInt64();
		void ReadUInt128(CUInt128 *puValue);
		void ReadHash(BYTE *pbyValue);
		BYTE* ReadBsob(uint8 *puSize);
		float ReadFloat();
		CStringW ReadStringUTF8(bool bOptACP = false);
		void	 WriteString(const CStringW &strVal);
		CKadTag* ReadTag(bool bOptACP = false);
		void ReadTagList(TagList *pTaglist, bool bOptACP = false);
		void WriteByte(byte byVal);
		void WriteUInt8(uint8 uVal);
		void WriteUInt16(uint16 uVal);
		void WriteUInt32(uint32 uVal);
		void WriteUInt64(uint64 uVal);
		void WriteUInt128(const CUInt128 &uVal);
		void WriteHash(const BYTE *pbyValue);
		void WriteBsob(const BYTE *pbyValue, uint8 uSize);
		void WriteFloat(float fVal);
		void WriteTag(const CKadTag *pTag);
		void WriteTag(LPCSTR szName, uint8 uValue);
		void WriteTag(LPCSTR szName, uint16 uValue);
		void WriteTag(LPCSTR szName, uint32 uValue);
		void WriteTag(LPCSTR szName, uint64 uValue);
		void WriteTag(LPCSTR szName, float fValue);
		void WriteTagList(const TagList &tagList);
		virtual void ReadArray(LPVOID lpResult, uint32 uByteCount) = 0;
		virtual void WriteArray(LPCVOID lpVal, uint32 uByteCount) = 0;
		virtual UINT GetAvailable() const = 0;
	};
}