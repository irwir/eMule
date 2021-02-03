#pragma once

#include "SafeFile.h"

///////////////////////////////////////////////////////////////////////////////
// ESearchType

//NOTE: The numbers are *equal* to items position in the combo box -> TODO: use item data
enum ESearchType : uint8
{
	SearchTypeAutomatic = 0,
	SearchTypeEd2kServer,
	SearchTypeEd2kGlobal,
	SearchTypeKademlia,
	SearchTypeContentDB
};


#define	MAX_SEARCH_EXPRESSION_LEN	512

///////////////////////////////////////////////////////////////////////////////
// SSearchParams

struct SSearchParams
{
	SSearchParams()
		: dwSearchID(_UI32_MAX)
		, bClientSharedFiles()
		, eType(SearchTypeEd2kServer)
		, ullMinSize()
		, ullMaxSize()
		, uAvailability()
		, uComplete()
		, ulMinBitrate()
		, ulMinLength()
		, bMatchKeywords()
	{
	}

	explicit SSearchParams(CFileDataIO &rFile)
		: ullMinSize()
		, ullMaxSize()
		, uAvailability()
		, uComplete()
		, ulMinBitrate()
		, ulMinLength()
		, bMatchKeywords()
	{
		dwSearchID = rFile.ReadUInt32();
		eType = (ESearchType)rFile.ReadUInt8();
		bClientSharedFiles = rFile.ReadUInt8() > 0;
		strSpecialTitle = rFile.ReadString(true);
		strExpression = rFile.ReadString(true);
		strFileType = rFile.ReadString(true);
	}

	void StorePartially(CFileDataIO &rFile) const
	{
		rFile.WriteUInt32(dwSearchID);
		rFile.WriteUInt8(static_cast<uint8>(eType));
		rFile.WriteUInt8(static_cast<uint8>(bClientSharedFiles));
		rFile.WriteString(strSpecialTitle, UTF8strRaw);
		rFile.WriteString(strExpression, UTF8strRaw);
		rFile.WriteString(strFileType, UTF8strRaw);
	}

	uint32 dwSearchID;
	bool bClientSharedFiles;
	CString strSearchTitle;
	CString strExpression;
	CStringW strKeyword;
	CString strBooleanExpr;
	ESearchType eType;
	CString strFileType;
	CString strMinSize;
	uint64 ullMinSize;
	CString strMaxSize;
	uint64 ullMaxSize;
	UINT uAvailability;
	CString strExtension;
	UINT uComplete;
	CString strCodec;
	ULONG ulMinBitrate;
	ULONG ulMinLength;
	CString strTitle;
	CString strAlbum;
	CString strArtist;
	CString strSpecialTitle;
	bool bMatchKeywords;
};

bool GetSearchPacket(CSafeMemFile *pData, SSearchParams *pParams, bool bTargetSupports64Bit, bool *pbPacketUsing64Bit);