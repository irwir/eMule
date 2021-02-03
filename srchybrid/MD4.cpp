//
// Free implementation of the MD4 hash algorithm
// MD4C.C - RSA Data Security, Inc., MD4 message-digest algorithm
//

/*
	Copyright (C) 1990-2, RSA Data Security, Inc. All rights reserved.

	License to copy and use this software is granted provided that it
	is identified as the "RSA Data Security, Inc. MD4 Message-Digest
	Algorithm" in all material mentioning or referencing this software
	or this function.

	License is also granted to make and use derivative works provided
	that such works are identified as "derived from the RSA Data
	Security, Inc. MD4 Message-Digest Algorithm" in all material
	mentioning or referencing the derived work.

	RSA Data Security, Inc. makes no representations concerning either
	the merchantability of this software or the suitability of this
	software for any particular purpose. It is provided "as is"
	without express or implied warranty of any kind.

	These notices must be retained in any copies of any part of this
	documentation and/or software.
*/
#include "StdAfx.h"
#include "MD4.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

///////////////////////////////////////////////////////////////////////////////
// CMD4
//

CMD4::CMD4()
{
	Reset();
}

// MD4 initialization. Begins an MD4 operation, writing a new context
void CMD4::Reset()
{
	m_hash = MD4();
	m_md4.Restart();
}

// MD4 block update operation. Continues an MD4 message-digest
//     operation, processing another message block, and updating the
//     context
void CMD4::Add(LPCVOID pData, size_t nLength)
{
	m_md4.Update((byte*)pData, nLength);
}

// MD4 finalization. Ends an MD4 message-digest operation, writing the
//     the message digest and zeroing the context.
void CMD4::Finish()
{
	m_md4.Final(m_hash.b);
}

const byte* CMD4::GetHash() const
{
	return (byte*)m_hash.b;
}