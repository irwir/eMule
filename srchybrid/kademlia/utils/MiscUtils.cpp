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

#include "stdafx.h"
#include "Log.h"
#include "kademlia/utils/MiscUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

//CString CMiscUtils::m_sAppDirectory;

void CMiscUtils::IPAddressToString(uint32 uIP, CString *pString)
{
	const BYTE *pucIP = (BYTE*)&uIP;
	pString->Format(_T("%u.%u.%u.%u"), pucIP[3], pucIP[2], pucIP[1], pucIP[0]);
}


#ifdef DEBUG
void CMiscUtils::DebugHexDump(const byte *pbyData, uint32 uLenData)
{
	try {
		int iLenLine = 16;

		for (UINT uPos = 0; uPos < uLenData; uPos += iLenLine) {
			CStringA sLine;
			CStringA sSingle;
			sLine.Format("%08X ", uPos);
			iLenLine = min((uLenData - uPos), 16);
			for (int i = 0; i < iLenLine; ++i) {
				sSingle.Format(" %02X", pbyData[uPos + i]);
				sLine += sSingle;
				if (i == 7)
					sLine += " ";
			}
			sLine += CStringA(' ', 60 - sLine.GetLength());
			for (int i = 0; i < iLenLine; ++i) {
				byte byC = pbyData[uPos + i];
				sSingle.Format("%c", (((byC > 31) && (byC < 127)) ? byC : '.'));
				sLine += sSingle;
			}
			AddDebugLogLine(false, _T("%hs"), (LPCSTR)sLine);
		}
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CMiscUtils::debugHexDump"));
	}
}
#endif