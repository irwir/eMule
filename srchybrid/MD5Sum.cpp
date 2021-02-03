#include "stdafx.h"
#include "MD5Sum.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

MD5Sum::MD5Sum()
	: m_hash()
{
}

MD5Sum::MD5Sum(const CString &sSource)
{
	Calculate(sSource);
}

MD5Sum::MD5Sum(const byte *pachSource, size_t nLength)
{
	Calculate(pachSource, nLength);
}

void MD5Sum::Calculate(const CString &sSource)
{
	Calculate((byte*)(LPCTSTR)sSource, sSource.GetLength() * sizeof(TCHAR));
}

void MD5Sum::Calculate(const byte *pachSource, size_t nLength)
{
	CryptoPP::Weak::MD5 m_md5;
	m_md5.Restart();
	m_md5.Update(const_cast<byte*>(pachSource), nLength);
	m_md5.Final(m_hash.b);
}

CString MD5Sum::GetHashString() const
{
	return md4str(m_hash.b);
}