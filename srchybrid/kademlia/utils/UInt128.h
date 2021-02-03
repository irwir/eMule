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

namespace Kademlia
{
	class CUInt128
	{
	public:
		CUInt128();
		explicit CUInt128(bool bFill);
		explicit CUInt128(ULONG uValue);
		explicit CUInt128(const byte *pbyValueBE);
		//Generates a new number, copying the most significant 'numBits' bits from 'value'.
		//The remaining bits are randomly generated.
		CUInt128(const CUInt128 &uValue, UINT uNumBits = 128);

		const byte* GetData() const;
		byte*	GetDataPtr() const;
		/** Bit at position 0 being the most significant. */
		UINT	GetBitNumber(UINT uBit) const;
		int		CompareTo(const CUInt128 &uOther) const;
		int		CompareTo(ULONG uValue) const;
		void	ToHexString(CString *pstr) const;
		CString	ToHexString() const;
		void	ToBinaryString(CString *pstr, bool bTrim = false) const;
		void	ToByteArray(byte *pby) const;
		ULONG	Get32BitChunk(int iVal) const;
		CUInt128& SetValue(const CUInt128 &uValue);
		CUInt128& SetValue(ULONG uValue);
		CUInt128& SetValueBE(const byte *pbyValueBE);
		CUInt128& SetValueRandom();
		CUInt128& SetValueGUID();
		CUInt128& SetBitNumber(UINT uBit, UINT uValue);
		CUInt128& ShiftLeft(UINT uBits);
		CUInt128& Add(const CUInt128 &uValue);
		CUInt128& Add(ULONG uValue);
		//CUInt128& Div(ULONG uValue);
		CUInt128& Subtract(const CUInt128 &uValue);
		CUInt128& Subtract(ULONG uValue);
		CUInt128& Xor(const CUInt128 &uValue);
		CUInt128& XorBE(const byte *pbyValueBE);
		CUInt128& operator+ (const CUInt128 &uValue);
		CUInt128& operator- (const CUInt128 &uValue);
		CUInt128& operator= (const CUInt128 &uValue);
		bool operator<  (const CUInt128 &uValue) const;
		bool operator>  (const CUInt128 &uValue) const;
		bool operator<= (const CUInt128 &uValue) const;
		bool operator>= (const CUInt128 &uValue) const;
		bool operator== (const CUInt128 &uValue) const;
		bool operator!= (const CUInt128 &uValue) const;
		CUInt128& operator+ (ULONG uValue);
		CUInt128& operator- (ULONG uValue);
		CUInt128& operator= (ULONG uValue);
		bool operator<  (ULONG uValue) const;
		bool operator>  (ULONG uValue) const;
		bool operator<= (ULONG uValue) const;
		bool operator>= (ULONG uValue) const;
		bool operator== (ULONG uValue) const;
		bool operator!= (ULONG uValue) const;
	private:
		union
		{
			ULONG	m_uData[4];
			uint64	m_u64Data[2];
		};
	};
}