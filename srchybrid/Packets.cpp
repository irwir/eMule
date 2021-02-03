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
#include "stdafx.h"
#include "Packets.h"
#include "OtherFunctions.h"
#include "SafeFile.h"
#include "StringConversion.h"
#include "zlib/zlib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


void Packet::init()
{
	m_bPacked = false;
	tempbuffer = NULL;
	uStatsPayLoad = 0;
	memset(head, 0, sizeof head);
}

Packet::Packet(uint8 protocol)
	: pBuffer()
	, size()
	, opcode()
	, prot(protocol)
	, completebuffer()
	, m_bSplitted()
	, m_bLastSplitted()
	, m_bFromPF()
{
	init();
}

Packet::Packet(char *header)
	: pBuffer()
	, size(reinterpret_cast<Header_Struct*>(header)->packetlength - 1)
	, opcode(reinterpret_cast<Header_Struct*>(header)->command)
	, prot(reinterpret_cast<Header_Struct*>(header)->eDonkeyID)
	, completebuffer()
	, m_bSplitted()
	, m_bLastSplitted()
	, m_bFromPF()
{
	init();
}

Packet::Packet(char *pPacketPart, uint32 nSize, bool bLast, bool bFromPartFile) // only used for split packets!
	: pBuffer()
	, size(nSize - PACKET_HEADER_SIZE)
	, opcode()
	, prot()
	, completebuffer(pPacketPart)
	, m_bSplitted(true)
	, m_bLastSplitted(bLast)
	, m_bFromPF(bFromPartFile)
{
	init();
}

Packet::Packet(uint8 in_opcode, uint32 in_size, uint8 protocol, bool bFromPartFile)
	: size(in_size)
	, opcode(in_opcode)
	, prot(protocol)
	, m_bSplitted()
	, m_bLastSplitted()
	, m_bFromPF(bFromPartFile)
{
	init();
	if (in_size) {
		completebuffer = new char[in_size + 10]();
		pBuffer = completebuffer + PACKET_HEADER_SIZE;
	} else {
		completebuffer = NULL;
		pBuffer = NULL;
	}
}

Packet::Packet(CMemFile *datafile, uint8 protocol, uint8 ucOpcode)
	: size(static_cast<uint32>(datafile->GetLength()))
	, opcode(ucOpcode)
	, prot(protocol)
	, m_bSplitted()
	, m_bLastSplitted()
	, m_bFromPF()
{
	init();
	completebuffer = new char[size + 10];
	pBuffer = completebuffer + PACKET_HEADER_SIZE;
	BYTE *tmp = datafile->Detach();
	memcpy(pBuffer, tmp, size);
	free(tmp);
}

Packet::Packet(const CStringA &str, uint8 protocol, uint8 ucOpcode)
	: size(str.GetLength())
	, opcode(ucOpcode)
	, prot(protocol)
	, completebuffer()
	, m_bSplitted()
	, m_bLastSplitted()
	, m_bFromPF()
{
	init();
	completebuffer = new char[size + 10];
	pBuffer = completebuffer + PACKET_HEADER_SIZE;
	memcpy(pBuffer, (LPCSTR)str, size);
}

Packet::Packet(const Packet &old)
	: size(old.size)
	, opcode(old.opcode)
	, prot(old.prot)
	, tempbuffer()
	, m_bSplitted()
	, m_bLastSplitted()
	, m_bPacked(old.m_bPacked)
	, m_bFromPF(old.m_bFromPF)
{
	ASSERT(!old.m_bSplitted); //for non-split packets only
	memcpy(head, old.head, PACKET_HEADER_SIZE);
	completebuffer = new char[size + 10];
	pBuffer = completebuffer + PACKET_HEADER_SIZE;
	memcpy(completebuffer, old.completebuffer, size + PACKET_HEADER_SIZE);
}

Packet::~Packet()
{
	if (completebuffer)
		delete[] completebuffer;
	else
		delete[] pBuffer;
	delete[] tempbuffer;
}

char* Packet::GetPacket()
{
	if (completebuffer) {
		if (!m_bSplitted)
			memcpy(completebuffer, GetHeader(), PACKET_HEADER_SIZE);
		return completebuffer;
	}
	delete[] tempbuffer;
	tempbuffer = NULL; // 'new' may throw an exception
	tempbuffer = new char[size + 10];
	memcpy(tempbuffer, GetHeader(), PACKET_HEADER_SIZE);
	memcpy(tempbuffer + PACKET_HEADER_SIZE, pBuffer, size);
	return tempbuffer;
}

char* Packet::DetachPacket()
{
	if (completebuffer) {
		if (!m_bSplitted)
			memcpy(completebuffer, GetHeader(), PACKET_HEADER_SIZE);
		char *result = completebuffer;
		completebuffer = NULL;
		pBuffer = NULL;
		return result;
	}
	delete[] tempbuffer;
	tempbuffer = NULL;
	char *result = new char[size + 10]; // 'new' may throw an exception
	memcpy(result, GetHeader(), PACKET_HEADER_SIZE);
	memcpy(result + PACKET_HEADER_SIZE, pBuffer, size);
	return result;
}

char* Packet::GetHeader()
{
	ASSERT(!m_bSplitted);
	*reinterpret_cast<Header_Struct*>(head) = Header_Struct{prot, size + 1, opcode};
	return head;
}

char* Packet::GetUDPHeader()
{
	ASSERT(!m_bSplitted);
	*reinterpret_cast<UDP_Header_Struct*>(head) = UDP_Header_Struct{prot, opcode};
	return head;
}

void Packet::PackPacket()
{
	ASSERT(!m_bSplitted);
	uLongf newsize = size + 300;
	Bytef *output = new BYTE[newsize];
	int result = compress2(output, &newsize, (Bytef*)pBuffer, size, Z_BEST_COMPRESSION);
	if (result == Z_OK && newsize < size) {
		prot = (prot == OP_KADEMLIAHEADER) ? OP_KADEMLIAPACKEDPROT : OP_PACKEDPROT;
		memcpy(pBuffer, output, newsize);
		size = newsize;
		m_bPacked = true;
	}
	delete[] output;
}

bool Packet::UnPackPacket(UINT uMaxDecompressedSize)
{
	ASSERT(prot == OP_PACKEDPROT || prot == OP_KADEMLIAPACKEDPROT);
	UINT nNewSize = size * 10 + 300;
	if (nNewSize > uMaxDecompressedSize) {
		//ASSERT(0);
		nNewSize = uMaxDecompressedSize;
	}
	Bytef *unpack = NULL;
	uLongf unpackedsize = 0;
	int result = Z_OK;
	do {
		delete[] unpack;
		unpack = new BYTE[nNewSize];
		unpackedsize = nNewSize;
		result = uncompress(unpack, &unpackedsize, (Bytef*)pBuffer, size);
		nNewSize *= 2; // size for the next try if needed
	} while (result == Z_BUF_ERROR && nNewSize < uMaxDecompressedSize);

	if (result == Z_OK) {
		ASSERT(completebuffer == NULL);
		ASSERT(pBuffer != NULL);
		size = unpackedsize;
		delete[] pBuffer;
		pBuffer = (char*)unpack;
		prot = (prot == OP_KADEMLIAPACKEDPROT) ? OP_KADEMLIAHEADER : OP_EMULEPROT;
		return true;
	}
	delete[] unpack;
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// CRawPacket

CRawPacket::CRawPacket(const CStringA &rstr)
{
	ASSERT(opcode == 0);
	ASSERT(!m_bSplitted);
	ASSERT(!m_bLastSplitted);
	ASSERT(!m_bPacked);
	ASSERT(!m_bFromPF);
	ASSERT(completebuffer == NULL);
	ASSERT(tempbuffer == NULL);

	prot = 0x00;
	size = rstr.GetLength();
	pBuffer = new char[size];
	memcpy(pBuffer, (LPCSTR)rstr, size);
}

CRawPacket::CRawPacket(const char *pcData, UINT uSize, bool bFromPartFile)
{
	ASSERT(opcode == 0);
	ASSERT(!m_bSplitted);
	ASSERT(!m_bLastSplitted);
	ASSERT(!m_bPacked);
	ASSERT(!m_bFromPF);
	ASSERT(completebuffer == NULL);
	ASSERT(tempbuffer == NULL);

	prot = 0x00;
	size = uSize;
	pBuffer = new char[size];
	memcpy(pBuffer, pcData, size);
	m_bFromPF = bFromPartFile;
}

CRawPacket::~CRawPacket()
{
	ASSERT(completebuffer == NULL);
}

char* CRawPacket::GetHeader()
{
	ASSERT(0);
	return NULL;
}

char* CRawPacket::GetUDPHeader()
{
	ASSERT(0);
	return NULL;
}

void CRawPacket::AttachPacket(char *pPacketData, UINT uPacketSize, bool bFromPartFile)
{
	ASSERT(pBuffer == NULL);
	pBuffer = pPacketData;
	size = uPacketSize;
	m_bFromPF = bFromPartFile;
}

char* CRawPacket::DetachPacket()
{
	char *pBuf = pBuffer;
	pBuffer = NULL;
	return pBuf;
}


///////////////////////////////////////////////////////////////////////////////
// CTag

CTag::CTag(LPCSTR pszName, uint64 uVal, bool bInt64)
	: m_szName(pszName)
	, m_nBlobSize()
	, m_uType(bInt64 ? TAGTYPE_UINT64 : TAGTYPE_UINT32)
	, m_uName()
{
	ASSERT(uVal <= _UI32_MAX || bInt64);
	m_uVal = uVal;
	ASSERT_VALID(this);
}

CTag::CTag(uint8 uName, uint64 uVal, bool bInt64)
	: m_nBlobSize()
	, m_uType(bInt64 ? TAGTYPE_UINT64 : TAGTYPE_UINT32)
	, m_uName(uName)
{
	ASSERT(uVal <= _UI32_MAX || bInt64);
	m_uVal = uVal;
	ASSERT_VALID(this);
}

CTag::CTag(LPCSTR pszName, LPCTSTR pszVal)
	: m_szName(pszName)
	, m_nBlobSize()
	, m_uType(TAGTYPE_STRING)
	, m_uName()
{
	m_pstrVal = new CString(pszVal);
	ASSERT_VALID(this);
}

CTag::CTag(uint8 uName, LPCTSTR pszVal)
	: m_nBlobSize()
	, m_uType(TAGTYPE_STRING)
	, m_uName(uName)
{
	m_pstrVal = new CString(pszVal);
	ASSERT_VALID(this);
}

CTag::CTag(LPCSTR pszName, const CString &rstrVal)
	: m_szName(pszName)
	, m_nBlobSize()
	, m_uType(TAGTYPE_STRING)
	, m_uName()
{
	m_pstrVal = new CString(rstrVal);
	ASSERT_VALID(this);
}

CTag::CTag(uint8 uName, const CString &rstrVal)
	: m_nBlobSize()
	, m_uType(TAGTYPE_STRING)
	, m_uName(uName)
{
	m_pstrVal = new CString(rstrVal);
	ASSERT_VALID(this);
}

CTag::CTag(uint8 uName, const BYTE *pucHash)
	: m_nBlobSize()
	, m_uType(TAGTYPE_HASH)
	, m_uName(uName)
{
	m_pData = new BYTE[MDX_DIGEST_SIZE];
	md4cpy(m_pData, pucHash);
	ASSERT_VALID(this);
}

CTag::CTag(uint8 uName, size_t nSize, const BYTE *pucData)
	: m_nBlobSize((uint32)nSize)
	, m_uType(TAGTYPE_BLOB)
	, m_uName(uName)
{
	m_pData = new BYTE[nSize];
	memcpy(m_pData, pucData, nSize);
	ASSERT_VALID(this);
}

CTag::CTag(uint8 uName, BYTE *pucAttachData, uint32 nSize)
	: m_pData(pucAttachData)
	, m_nBlobSize(nSize)
	, m_uType(TAGTYPE_BLOB)
	, m_uName(uName)
{
	ASSERT_VALID(this);
}

CTag::CTag(const CTag &rTag)
	: m_szName(rTag.m_szName)
	, m_nBlobSize()
	, m_uType(rTag.m_uType)
	, m_uName(rTag.m_uName)
{
	if (rTag.IsStr())
		m_pstrVal = new CString(rTag.GetStr());
	else if (rTag.IsInt64())
		m_uVal = rTag.GetInt64();
	else if (rTag.IsFloat())
		m_fVal = rTag.GetFloat();
	else if (rTag.IsHash()) {
		m_pData = new BYTE[MDX_DIGEST_SIZE];
		md4cpy(m_pData, rTag.GetHash());
	} else if (rTag.IsBlob()) {
		m_nBlobSize = rTag.GetBlobSize();
		m_pData = new BYTE[m_nBlobSize];
		memcpy(m_pData, rTag.GetBlob(), m_nBlobSize);
	} else {
		ASSERT(0);
		m_uVal = 0;
	}
	ASSERT_VALID(this);
}

CTag::CTag(CFileDataIO *data, bool bOptUTF8)
{
	m_uType = data->ReadUInt8();
	if (m_uType & 0x80) {
		m_uType &= 0x7F;
		m_uName = data->ReadUInt8();
	} else {
		UINT length = data->ReadUInt16();
		if (length == 1)
			m_uName = data->ReadUInt8();
		else {
			m_uName = 0;
			LPSTR p = m_szName.GetBuffer(length);
			try {
				data->Read(p, length);
			} catch (...) {
				throw;
			}
			m_szName.ReleaseBuffer(length);
		}
	}

	m_nBlobSize = 0;

	// NOTE: It's very important that we read the *entire* packet data, even if we do
	// not use each tag. Otherwise we will get troubles when the packets are returned in
	// a list - like the search results from a server.
	if (m_uType == TAGTYPE_STRING)
		m_pstrVal = new CString(data->ReadString(bOptUTF8));
	else if (m_uType == TAGTYPE_UINT32)
		m_uVal = data->ReadUInt32();
	else if (m_uType == TAGTYPE_UINT64)
		m_uVal = data->ReadUInt64();
	else if (m_uType == TAGTYPE_UINT16) {
		m_uVal = data->ReadUInt16();
		m_uType = TAGTYPE_UINT32;
	} else if (m_uType == TAGTYPE_UINT8) {
		m_uVal = data->ReadUInt8();
		m_uType = TAGTYPE_UINT32;
	} else if (m_uType == TAGTYPE_FLOAT32)
		data->Read(&m_fVal, 4);
	else if (m_uType >= TAGTYPE_STR1 && m_uType <= TAGTYPE_STR16) {
		UINT length = m_uType - TAGTYPE_STR1 + 1;
		m_pstrVal = new CString(data->ReadString(bOptUTF8, length));
		m_uType = TAGTYPE_STRING;
	} else if (m_uType == TAGTYPE_HASH) {
		BYTE bHash[MDX_DIGEST_SIZE];
		data->Read(bHash, MDX_DIGEST_SIZE);
		m_pData = new BYTE[MDX_DIGEST_SIZE];
		md4cpy(m_pData, bHash);
	} else if (m_uType == TAGTYPE_BOOL) {
		TRACE("***NOTE: %s; Reading BOOL tag\n", __FUNCTION__);
		data->Seek(1, CFile::current);
	} else if (m_uType == TAGTYPE_BOOLARRAY) {
		TRACE("***NOTE: %s; Reading BOOL Array tag\n", __FUNCTION__);
		uint16 len;
		data->Read(&len, 2);
		// 07-Apr-2004: eMule versions prior to 0.42e.29 used the formula "(len+7)/8"!
		data->Seek((len / 8) + 1ll, CFile::current);
	} else if (m_uType == TAGTYPE_BLOB) {
		// 07-Apr-2004: eMule versions prior to 0.42e.29 handled the "len" as int16!
		m_nBlobSize = data->ReadUInt32();
		if (m_nBlobSize <= data->GetLength() - data->GetPosition()) {
			m_pData = new BYTE[m_nBlobSize];
			data->Read(m_pData, m_nBlobSize);
		} else {
			ASSERT(0);
			m_nBlobSize = 0;
			m_pData = NULL;
		}
	} else {
		if (m_uName != 0)
			TRACE("%s; Unknown tag: type=0x%02X  specialtag=%u\n", __FUNCTION__, m_uType, m_uName);
		else
			TRACE("%s; Unknown tag: type=0x%02X  name=\"%s\"\n", __FUNCTION__, m_uType, (LPCSTR)m_szName);
		m_uVal = 0;
	}
	ASSERT_VALID(this);
}

CTag::~CTag()
{
	cleanup();
}

CTag& CTag::operator=(const CTag &rTag)
{
	if (this != &rTag) {
		cleanup();
		m_nBlobSize = 0;
		m_uType = rTag.m_uType;
		m_uName = rTag.m_uName;
		m_szName = rTag.m_szName;
		if (rTag.IsStr())
			m_pstrVal = new CString(rTag.GetStr());
		else if (rTag.IsInt())
			m_uVal = rTag.GetInt();
		else if (rTag.IsInt64(false))
			m_uVal = rTag.GetInt64();
		else if (rTag.IsFloat())
			m_fVal = rTag.GetFloat();
		else if (rTag.IsHash()) {
			m_pData = new BYTE[MDX_DIGEST_SIZE];
			md4cpy(m_pData, rTag.GetHash());
		} else if (rTag.IsBlob()) {
			m_nBlobSize = rTag.GetBlobSize();
			m_pData = new BYTE[m_nBlobSize];
			memcpy(m_pData, rTag.GetBlob(), m_nBlobSize);
		} else {
			ASSERT(0);
			m_uVal = 0;
		}
	}
	return *this;
}

bool CTag::WriteNewEd2kTag(CFileDataIO *data, EUTF8str eStrEncode) const
{
	ASSERT_VALID(this);

	// Write tag type
	uint8 uType;
	UINT uStrValLen = 0;
	CStringA strValA;
	if (IsInt64()) {
		if (m_uVal <= _UI8_MAX)
			uType = TAGTYPE_UINT8;
		else if (m_uVal <= _UI16_MAX)
			uType = TAGTYPE_UINT16;
		else if (m_uVal <= _UI32_MAX)
			uType = TAGTYPE_UINT32;
		else
			uType = TAGTYPE_UINT64;
	} else if (IsStr()) {
		if (eStrEncode == UTF8strRaw) {
			CUnicodeToUTF8 utf8(*m_pstrVal);
			strValA = CStringA((LPCSTR)utf8, utf8.GetLength());
		} else if (eStrEncode == UTF8strOptBOM) {
			if (NeedUTF8String(*m_pstrVal)) {
				CUnicodeToBOMUTF8 bomutf8(*m_pstrVal);
				strValA = CStringA((LPCSTR)bomutf8, bomutf8.GetLength());
			} else {
				CUnicodeToMultiByte mb(*m_pstrVal);
				strValA = CStringA((LPCSTR)mb, mb.GetLength());
			}
		} else {
			CUnicodeToMultiByte mb(*m_pstrVal);
			strValA = CStringA((LPCSTR)mb, mb.GetLength());
		}
		uStrValLen = strValA.GetLength();
		if (uStrValLen >= 1 && uStrValLen <= 16)
			uType = (uint8)(TAGTYPE_STR1 + uStrValLen - 1);
		else
			uType = TAGTYPE_STRING;
	} else
		uType = m_uType;

	// Write tag name
	if (m_szName.IsEmpty()) {
		ASSERT(m_uName != 0);
		data->WriteUInt8(uType | 0x80);
		data->WriteUInt8(m_uName);
	} else {
		data->WriteUInt8(uType);
		UINT uTagNameLen = (UINT)m_szName.GetLength();
		data->WriteUInt16((uint16)uTagNameLen);
		data->Write(m_szName, uTagNameLen);
	}

	// Write tag data
	switch (uType) {
	case TAGTYPE_STRING:
		data->WriteUInt16((uint16)uStrValLen);
		data->Write((void*)(LPCSTR)strValA, uStrValLen);
		break;
	case TAGTYPE_UINT64:
		data->WriteUInt64(m_uVal);
		break;
	case TAGTYPE_UINT32:
		data->WriteUInt32((uint32)m_uVal);
		break;
	case TAGTYPE_UINT16:
		data->WriteUInt16((uint16)m_uVal);
		break;
	case TAGTYPE_UINT8:
		data->WriteUInt8((uint8)m_uVal);
		break;
	case TAGTYPE_FLOAT32:
		data->Write(&m_fVal, 4);
		break;
	case TAGTYPE_HASH:
		data->WriteHash16(m_pData);
		break;
	case TAGTYPE_BLOB:
		data->WriteUInt32(m_nBlobSize);
		data->Write(m_pData, m_nBlobSize);
		break;
	default:
		if (uType < TAGTYPE_STR1 || uType > TAGTYPE_STR16) {
			TRACE("%s; Unknown tag: type=0x%02X\n", __FUNCTION__, uType);
			ASSERT(0);
			return false;
		}
		data->Write((void*)(LPCSTR)strValA, uStrValLen);
	}

	return true;
}

bool CTag::WriteTagToFile(CFileDataIO *file, EUTF8str eStrEncode) const
{
	ASSERT_VALID(this);

	// don't write tags of unknown types, we wouldn't be able to read in them,
	// and the met file would be corrupted
	if (IsStr() || IsInt64() || IsFloat() || IsBlob()) {
		file->WriteUInt8(m_uType);

		if (m_szName.IsEmpty()) {
			file->WriteUInt16(1);
			file->WriteUInt8(m_uName);
		} else {
			UINT taglen = (UINT)m_szName.GetLength();
			file->WriteUInt16((uint16)taglen);
			file->Write(m_szName, taglen);
		}

		if (IsStr())
			file->WriteString(GetStr(), eStrEncode);
		else if (IsInt())
			file->WriteUInt32((uint32)m_uVal);
		else if (IsInt64(false))
			file->WriteUInt64(m_uVal);
		else if (IsFloat())
			file->Write(&m_fVal, 4);
		else if (IsBlob()) {
			// NOTE: This will break backward compatibility with met files for eMule versions prior to 0.44a
			file->WriteUInt32(m_nBlobSize);
			file->Write(m_pData, m_nBlobSize);
		} else {
			//TODO: Support more tag types
			TRACE("%s; Unknown tag: type=0x%02X\n", __FUNCTION__, m_uType);
			ASSERT(0);
			return false;
		}
		return true;
	}
	TRACE("%s; Ignored tag with unknown type=0x%02X\n", __FUNCTION__, m_uType);
	ASSERT(0);
	return false;
}

void CTag::SetInt(uint32 uVal)
{
	ASSERT(IsInt());
	if (IsInt())
		m_uVal = uVal;
}

void CTag::SetInt64(uint64 uVal)
{
	ASSERT(IsInt64(true));
	if (IsInt64(true)) {
		m_uVal = uVal;
		m_uType = TAGTYPE_UINT64;
	}
}

void CTag::SetStr(LPCTSTR pszVal)
{
	ASSERT(IsStr());
	if (IsStr()) {
		delete m_pstrVal;
		m_pstrVal = new CString(pszVal);
	}
}

CString CTag::GetFullInfo(CString(*pfnDbgGetFileMetaTagName)(UINT uMetaTagID)) const
{
	CString strTag;
	if (!m_szName.IsEmpty())
		strTag.Format(_T("\"%hs\"="), (LPCSTR)m_szName);
	else if (pfnDbgGetFileMetaTagName)
		strTag.Format(_T("\"%s\"="), (LPCTSTR)(*pfnDbgGetFileMetaTagName)(m_uName));
	else
		strTag.Format(_T("Tag0x%02X="), m_uName);

	if (m_uType == TAGTYPE_STRING)
		strTag.AppendFormat(_T("\"%s\""), (LPCTSTR)*m_pstrVal);
	else if (m_uType >= TAGTYPE_STR1 && m_uType <= TAGTYPE_STR16)
		strTag.AppendFormat(_T("(Str%u)\"%s\""), m_uType - TAGTYPE_STR1 + 1u, (LPCTSTR)*m_pstrVal);
	else if (m_uType == TAGTYPE_UINT32)
		strTag.AppendFormat(_T("(Int32)%u"), (uint32)m_uVal);
	else if (m_uType == TAGTYPE_UINT64)
		strTag.AppendFormat(_T("(Int64)%I64u"), m_uVal);
	else if (m_uType == TAGTYPE_UINT16)
		strTag.AppendFormat(_T("(Int16)%u"), (uint16)m_uVal);
	else if (m_uType == TAGTYPE_UINT8)
		strTag.AppendFormat(_T("(Int8)%u"), (uint8)m_uVal);
	else if (m_uType == TAGTYPE_FLOAT32)
		strTag.AppendFormat(_T("(Float32)%f"), m_fVal);
	else if (m_uType == TAGTYPE_BLOB)
		strTag.AppendFormat(_T("(Blob)%u"), m_nBlobSize);
	else
		strTag.AppendFormat(_T("Type=%u"), m_uType);

	return strTag;
}


void CTag::cleanup()
{
	if (IsStr())
		delete m_pstrVal;
	else if (IsHash() || IsBlob())
		delete[] m_pData;
}

#ifdef _DEBUG
void CTag::AssertValid() const
{
	CObject::AssertValid();

	ASSERT((m_uType & ~0x80) != 0);
	ASSERT(m_uName != 0 && m_szName.IsEmpty() || m_uName == 0 && !m_szName.IsEmpty());
	ASSERT(m_szName.IsEmpty() || AfxIsValidString(m_szName));
	if (IsStr())
		ASSERT(m_pstrVal != NULL && AfxIsValidString(*m_pstrVal));
	else if (IsHash())
		ASSERT(m_pData != NULL && AfxIsValidAddress(m_pData, 16));
	else if (IsBlob())
		ASSERT(m_pData != NULL && AfxIsValidAddress(m_pData, m_nBlobSize));
}

void CTag::Dump(CDumpContext &dc) const
{
	CObject::Dump(dc);
}
#endif