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
#include "kademlia/io/ByteIO.h"
#include "kademlia/io/IOException.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

CByteIO::CByteIO(byte *pbyBuffer, uint32 uAvailable)
	: m_bReadOnly()
	, m_pbyBuffer(pbyBuffer)
	, m_uAvailable(uAvailable)
	, m_uUsed()
{
}

CByteIO::CByteIO(const byte *pbyBuffer, uint32 uAvailable)
	: m_bReadOnly(true)
	, m_pbyBuffer(const_cast<byte*>(pbyBuffer))
	, m_uAvailable(uAvailable)
	, m_uUsed()
{
}

UINT CByteIO::GetAvailable() const
{
	return m_uAvailable;
}

void CByteIO::ReadArray(LPVOID lpResult, uint32 uByteCount)
{
	if (m_uAvailable < uByteCount)
		throw new CIOException(ERR_BUFFER_TOO_SMALL);

	memcpy(lpResult, m_pbyBuffer, uByteCount);
	m_pbyBuffer += uByteCount;
	m_uUsed += uByteCount;
	m_uAvailable -= uByteCount;
}

void CByteIO::WriteArray(LPCVOID lpVal, uint32 uByteCount)
{
	if (m_bReadOnly)
		throw new CIOException(ERR_READ_ONLY);

	if (m_uAvailable < uByteCount)
		throw new CIOException(ERR_BUFFER_TOO_SMALL);

	memcpy(m_pbyBuffer, lpVal, uByteCount);
	m_pbyBuffer += uByteCount;
	m_uUsed += uByteCount;
	m_uAvailable -= uByteCount;
}

void CByteIO::Reset()
{
	m_uAvailable += m_uUsed;
	m_pbyBuffer -= m_uUsed;
	m_uUsed = 0;
}

void CByteIO::Seek(uint32 newpos)
{
	if (newpos > (m_uUsed + m_uAvailable))
		throw new CIOException(ERR_BUFFER_TOO_SMALL);

	// Position the buffer
	m_pbyBuffer -= m_uUsed;
	m_pbyBuffer += newpos;
	// Update available
	m_uAvailable = (m_uUsed + m_uAvailable) - newpos;
	// The used bytes are the new pos.
	m_uUsed = newpos;
}