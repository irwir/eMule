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
#include "opcodes.h"
#include "SafeFile.h"

///////////////////////////////////////////////////////////////////////////////
// Packet

#pragma pack(push, 1)
struct Header_Struct
{
	uint8	eDonkeyID;
	uint32	packetlength;
	uint8	command;
};

struct UDP_Header_Struct
{
	uint8	eDonkeyID;
	uint8	command;
};
#pragma pack(pop)

#define PACKET_HEADER_SIZE sizeof(Header_Struct)

class Packet
{
	void	init();
public:
	explicit Packet(uint8 protocol = OP_EDONKEYPROT);
	explicit Packet(char *header); // only used for receiving packets
	explicit Packet(CMemFile *datafile, uint8 protocol = OP_EDONKEYPROT, uint8 ucOpcode = 0x00);
	Packet(const CStringA &str, uint8 protocol, uint8 ucOpcode);
	Packet(uint8 in_opcode, uint32 in_size, uint8 protocol = OP_EDONKEYPROT, bool bFromPartFile = true);
	Packet(char *pPacketPart, uint32 nSize, bool bLast, bool bFromPartFile = true); // only used for split packets!
	Packet(const Packet &old);
	virtual	~Packet();
	Packet& operator=(const Packet&) = delete;

	virtual char* GetHeader();
	virtual char* GetUDPHeader();
	virtual char* GetPacket();
	virtual char* DetachPacket();
	virtual uint32 GetRealPacketSize() const	{ return size + PACKET_HEADER_SIZE; }
	//bool	IsSplitted() const					{ return m_bSplitted; }
	//bool	IsLastSplitted() const				{ return m_bLastSplitted; }
	bool	IsFromPF() const					{ return m_bFromPF; }
	void	PackPacket();
	bool	UnPackPacket(UINT uMaxDecompressedSize = 50000u);

	char	*pBuffer;
	uint32	size;
	uint32	uStatsPayLoad; // only for statistics and co., not used within the class itself
	uint8	opcode;
	uint8	prot;

protected:
	char	*completebuffer;
	char	*tempbuffer;
	char	head[PACKET_HEADER_SIZE];
	bool	m_bSplitted;
	bool	m_bLastSplitted;
	bool	m_bPacked;
	bool	m_bFromPF;
};


///////////////////////////////////////////////////////////////////////////////
// CRawPacket

class CRawPacket : public Packet
{
public:
	explicit CRawPacket(const CStringA &rstr);
	CRawPacket(const char *pcData, UINT uSize, bool bFromPartFile = false);
	virtual	~CRawPacket();

	virtual char*	GetHeader();
	virtual char*	GetUDPHeader();
	virtual char*	GetPacket()					{ return pBuffer; }
	virtual void	AttachPacket(char *pPacketData, UINT uPacketSize, bool bFromPartFile = false);
	virtual char*	DetachPacket();
	virtual uint32	GetRealPacketSize() const	{ return size; }
};


///////////////////////////////////////////////////////////////////////////////
// CTag

class CTag
#ifdef _DEBUG
	: public CObject
#endif
{
public:
	CTag(LPCSTR pszName, uint64 uVal, bool bInt64 = false);
	CTag(uint8 uName, uint64 uVal, bool bInt64 = false);
	CTag(LPCSTR pszName, LPCTSTR pszVal);
	CTag(uint8 uName, LPCTSTR pszVal);
	CTag(LPCSTR pszName, const CString &rstrVal);
	CTag(uint8 uName, const CString &rstrVal);
	CTag(uint8 uName, const BYTE *pucHash);
	CTag(uint8 uName, size_t nSize, const BYTE *pucData); // data gets copied
	CTag(uint8 uName, BYTE *pucAttachData, uint32 nSize); // data gets attached (and deleted later on)
	CTag(const CTag &rTag);
	CTag(CFileDataIO *data, bool bOptUTF8);
	~CTag();
	CTag &operator=(const CTag &rTag);

	UINT GetType() const						{ return m_uType; }
	UINT GetNameID() const						{ return m_uName; }
	LPCSTR GetName() const						{ return (LPCSTR)m_szName; }

	bool IsStr() const							{ return m_uType == TAGTYPE_STRING; }
	bool IsInt() const							{ return m_uType == TAGTYPE_UINT32; }
	bool IsFloat() const						{ return m_uType == TAGTYPE_FLOAT32; }
	bool IsHash() const							{ return m_uType == TAGTYPE_HASH; }
	bool IsBlob() const							{ return m_uType == TAGTYPE_BLOB; }
	bool IsInt64(bool bOrInt32 = true) const	{ return m_uType == TAGTYPE_UINT64 || (bOrInt32 && IsInt()); }

	UINT	GetInt() const						{ ASSERT(IsInt());		return (UINT)m_uVal; }
	uint64	GetInt64() const					{ ASSERT(IsInt64(true));return m_uVal; }
	const	CString& GetStr() const				{ ASSERT(IsStr());		return const_cast<CString&>(*m_pstrVal); }
	float	GetFloat() const					{ ASSERT(IsFloat());	return m_fVal; }
	const	BYTE* GetHash() const				{ ASSERT(IsHash());		return m_pData; }
	uint32	GetBlobSize() const					{ ASSERT(IsBlob());		return m_nBlobSize; }
	const	BYTE* GetBlob() const				{ ASSERT(IsBlob());		return m_pData; }

	void SetInt(uint32 uVal);
	void SetInt64(uint64 uVal);
	void SetStr(LPCTSTR pszVal);

	//CTag *CloneTag()							{ return new CTag(*this); }

	bool WriteTagToFile(CFileDataIO *file, EUTF8str eStrEncode = UTF8strNone) const;	// old eD2K tags
	bool WriteNewEd2kTag(CFileDataIO *data, EUTF8str eStrEncode = UTF8strNone) const;	// new eD2K tags

	CString GetFullInfo(CString(*pfnDbgGetFileMetaTagName)(UINT uMetaTagID) = NULL) const;

	#ifdef _DEBUG
		// Diagnostic Support
		virtual void AssertValid() const;
		virtual void Dump(CDumpContext &dc) const;
	#endif

protected:
	union
	{
		CString	*m_pstrVal;
		uint64	m_uVal;
		float	m_fVal;
		BYTE	*m_pData;
	};
	CStringA	m_szName;
	uint32		m_nBlobSize;
	uint8		m_uType;
	uint8		m_uName;
private:
	void cleanup();
};


///////////////////////////////////////////////////////////////////////////////
// CTag and tag string helpers

inline int CmpED2KTagName(LPCSTR pszTagName1, LPCSTR pszTagName2)
{
	// string compare is independent from any codepage and/or LC_CTYPE setting.
	return __ascii_stricmp(pszTagName1, pszTagName2);
}
void ConvertED2KTag(CTag *&pTag);