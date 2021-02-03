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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

class CPartFile;
class CShareableFile;
struct Gap_Struct;

#define ZIP_LOCAL_HEADER_MAGIC		0x04034b50u
#define ZIP_LOCAL_HEADER_EXT_MAGIC	0x08074b50u
#define ZIP_CD_MAGIC				0x02014b50u
#define ZIP_END_CD_MAGIC			0x06054b50u
#define ZIP_COMMENT					"Recovered by eMule"

#define RAR_HEAD_FILE 0x74

#pragma pack(push, 1)
struct ZIP_Entry
{
	uint32	header;
	uint16	versionToExtract;
	uint16	generalPurposeFlag;
	uint16	compressionMethod;
	uint16	lastModFileTime;
	uint16	lastModFileDate;
	uint32	crc32;
	uint32	lenCompressed;
	uint32	lenUncompressed;
	uint16	lenFilename;
	uint16	lenExtraField;
	BYTE	*filename;
	BYTE	*extraField;
	BYTE	*compressedData;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ZIP_CentralDirectory
{
	ZIP_CentralDirectory()
	{
		memset(this, 0, sizeof(ZIP_CentralDirectory));
	}
	uint32	header;
	uint16	versionMadeBy;
	uint16	versionToExtract;
	uint16	generalPurposeFlag;
	uint16	compressionMethod;
	uint16	lastModFileTime;
	uint16	lastModFileDate;
	uint32	crc32;
	uint32	lenCompressed;
	uint32	lenUncompressed;
	uint16	lenFilename;
	uint16	lenExtraField;
	uint16	lenComment;
	uint16	diskNumberStart;
	uint16	internalFileAttributes;
	uint32	externalFileAttributes;
	uint32	relativeOffsetOfLocalHeader;
	BYTE	*filename;
	BYTE	*extraField;
	BYTE	*comment;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RAR_BlockFile
{
	RAR_BlockFile()
	{
		memset(this, 0, sizeof(RAR_BlockFile));
	}
	~RAR_BlockFile()
	{
		delete[] EXT_DATE;
	}

	// This indicates the position in the input file just after the filename
	ULONGLONG offsetData;
	// This indicates how much of the block is after this offset
	uint32 dataLength;

	uint16	HEAD_CRC;
	BYTE	HEAD_TYPE;
	uint16	HEAD_FLAGS;
	uint16	HEAD_SIZE;
	uint32	PACK_SIZE;
	uint32	UNP_SIZE;
	BYTE	HOST_OS;
	uint32	FILE_CRC;
	uint32	FTIME;
	BYTE	UNP_VER;
	BYTE	METHOD;
	uint16	NAME_SIZE;
	uint32	ATTR;
	uint32	HIGH_PACK_SIZE;
	uint32	HIGH_UNP_SIZE;
	BYTE	*FILE_NAME;
	BYTE	*EXT_DATE;
	uint32	EXT_DATE_SIZE;
	BYTE	SALT[8];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ACE_ARCHIVEHEADER
{
	uint16	HEAD_CRC;
	uint16	HEAD_SIZE;
	BYTE	HEAD_TYPE;
	uint16	HEAD_FLAGS;
	BYTE	HEAD_SIGN[7];
	BYTE	VER_EXTRACT;
	BYTE	VER_CREATED;
	BYTE	HOST_CREATED;
	BYTE	VOLUME_NUM;
	uint32	FTIME;
	BYTE	RESERVED[8];
	BYTE	AVSIZE;
	//**AV
	uint16	COMMENT_SIZE;

	char	*AV;
	char	*COMMENT;
	char	*DUMP;

	ACE_ARCHIVEHEADER()
	{
		memset(this, 0, sizeof(ACE_ARCHIVEHEADER));
	}
	~ACE_ARCHIVEHEADER()
	{
		free(AV);
		free(COMMENT);
		free(DUMP);
	}
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ACE_BlockFile
{
	uint16	HEAD_CRC;
	uint16	HEAD_SIZE;
	BYTE	HEAD_TYPE;
	uint16	HEAD_FLAGS;
	uint32  PACK_SIZE;
	uint32  ORIG_SIZE;
	uint32  FTIME;
	uint32  FILE_ATTRIBS;
	uint32  CRC32;
	uint32  TECHINFO;
	uint16  RESERVED;
	uint16  FNAME_SIZE;
	uint16  COMM_SIZE;
	char	*FNAME;
	char	*COMMENT;
	uint64  data_offset;

	ACE_BlockFile()
		: FNAME_SIZE()
		, COMM_SIZE()
		, FNAME()
		, COMMENT()
	{
	}
	~ACE_BlockFile()
	{
		free(FNAME);
		free(COMMENT);
	}
};
#pragma pack(pop)

// ################################
// ISO related
static unsigned char sig_udf_bea[5]	 = {0x42, 0x45, 0x41, 0x30, 0x31};	// "BEA01"
static unsigned char sig_udf_nsr2[5] = {0x4e, 0x53, 0x52, 0x30, 0x32};	// "NSR02"
static unsigned char sig_udf_nsr3[5] = {0x4e, 0x53, 0x52, 0x30, 0x33};	// "NSR03"
static unsigned char sig_tea[5]		 = {0x54, 0x45, 0x41, 0x30, 0x31};	// "TEA01"
static const unsigned char sElToritoID[] = "EL TORITO SPECIFICATION";

enum ISO_ImageType
{
	ISOtype_unknown = 0,
	ISOtype_9660 = 1,
	ISOtype_joliet = 2,
	ISOtype_UDF_nsr02 = 4,
	ISOtype_UDF_nsr03 = 8,
};
enum ISO_FileFlags
{
	ISO_HIDDEN = 1,
	ISO_DIRECTORY = 2,
	ISO_FILE = 4,
	ISO_RECORD = 8,
	ISO_READONLY = 16
};
#pragma pack(push, 1)
struct ISO_DateTimePVD_s
{
	unsigned char year[4];
	unsigned char month[2];
	unsigned char day[2];
	unsigned char hour[2];
	unsigned char minute[2];
	unsigned char second[2];
	unsigned char sechundr[2];
	unsigned char offsetGreenwich;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ISO_DateTimeFileFolder_s
{
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char offsetGreenwich;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ISO_PVD_s //PVD = Primary Volume Descriptor
{
	unsigned char descr_type;
	unsigned char magic[5];
	unsigned char descr_ver;
	unsigned char unused;
	unsigned char sysid[32];
	unsigned char volid[32];
	unsigned char zeros1[8];
	unsigned char seknum[8];
	unsigned char escSeq[32];

	UINT32 volsetsize;
	UINT32 volseqnum;
	UINT32 seksize;
	UINT64 pathtablen;

	UINT32 firstSek_LEpathTab1_LE;
	UINT32 firstsek_LEpathtab2_LE;
	UINT32 firstsek_BEpathtab1_BE;
	UINT32 firstsek_BEpathtab2_BE;

	unsigned char rootdir[34];
	unsigned char volsetid[128];
	unsigned char pubid[128];
	unsigned char dataprepid[128];
	unsigned char appid[128];
	unsigned char copyr[37];
	unsigned char abstractfileid[37];
	unsigned char bibliofileid[37];

	ISO_DateTimePVD_s creationdate;
	ISO_DateTimePVD_s modifydate;
	ISO_DateTimePVD_s expiredate;

	unsigned char effective[17];
	unsigned char filestruc_ver;
	unsigned char zero;
	unsigned char app_use[512];
	unsigned char res[653];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BootDescr
{
	unsigned char descr_type;
	unsigned char magic[5];
	unsigned char descr_ver;
	unsigned char sysid[32];
	unsigned char bootid[32];
	unsigned char system_use[1977];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ISO_BootDescr_s
{
	unsigned char descr_type;
	unsigned char magic[5];
	unsigned char descr_ver;
	unsigned char sysid[32];
	unsigned char bootid[32];
	unsigned char system_use[1977];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ISO_PathtableEntry
{
	BYTE	len;
	BYTE	lenExt;
	unsigned int	sectorOfExtension;
	UINT16	sectorOfParent;
	char	*name;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ISO_FileFolderEntry
{
	BYTE	lenRecord;
	BYTE	nrOfSecInExt;
	UINT64	sector1OfExtension;
	UINT64	dataSize;
	ISO_DateTimeFileFolder_s dateTime;
	BYTE	fileFlags;
	BYTE	fileUnitSize;
	BYTE	interleaveGapSize;
	unsigned int	volSeqNr;
	BYTE	nameLen;
	TCHAR	*name;
	ISO_FileFolderEntry()	{ name = NULL; };
	~ISO_FileFolderEntry()	{ free(name); };
};
#pragma pack(pop)

struct ISOInfos_s
{
	bool	bBootable;
	DWORD	secSize;
	bool	bUDF;
	int		iJolietUnicode;
	DWORD	type;
};

struct ThreadParam
{
	CPartFile *partFile;
	CTypedPtrList<CPtrList, Gap_Struct*> *filled;
	bool preview;
	bool bCreatePartFileCopy;
};


struct archiveinfo_s
{
	CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *centralDirectoryEntries;
	CTypedPtrList<CPtrList, RAR_BlockFile*> *RARdir;
	CTypedPtrList<CPtrList, ACE_BlockFile*> *ACEdir;
	CTypedPtrList<CPtrList, ISO_FileFolderEntry*> *ISOdir;

	bool bZipCentralDir;
	WORD rarFlags;
	ISOInfos_s isoInfos;
	ACE_ARCHIVEHEADER *ACEhdr;
	archiveinfo_s()
		: centralDirectoryEntries()
		, RARdir()
		, ACEdir()
		, ISOdir()
		, bZipCentralDir()
		, rarFlags()
		, ACEhdr()
	{
		isoInfos.bBootable = false;
		isoInfos.secSize = 0;
		isoInfos.iJolietUnicode = 0;
	}
};

struct archiveScannerThreadParams_s
{
	CShareableFile	*file;
	archiveinfo_s	*ai;
	CTypedPtrList<CPtrList, Gap_Struct*> *filled;
	int				type;
	HWND			ownerHwnd;
	HWND			progressHwnd;
	unsigned		curProgress;
	volatile bool	m_bIsValid;
};

class CArchiveRecovery
{
public:
	static void recover(CPartFile *partFile, bool preview = false, bool bCreatePartFileCopy = true);
	static bool recoverZip(CFile *zipInput, CFile *zipOutput, archiveScannerThreadParams_s *aitp, CTypedPtrList<CPtrList, Gap_Struct*> *filled, bool fullSize);
	static bool recoverRar(CFile *rarInput, CFile *rarOutput, archiveScannerThreadParams_s *aitp, CTypedPtrList<CPtrList, Gap_Struct*> *filled);
	static bool recoverAce(CFile *aceInput, CFile *aceOutput, archiveScannerThreadParams_s *aitp, CTypedPtrList<CPtrList, Gap_Struct*> *filled);
	static bool recoverISO(CFile *isoInput, CFile *isoOutput, archiveScannerThreadParams_s *aitp, CTypedPtrList<CPtrList, Gap_Struct*> *filled);

private:
	CArchiveRecovery(); // Just use static recover method

	static UINT AFX_CDECL run(LPVOID lpParam);
	static bool performRecovery(CPartFile *partFile, CTypedPtrList<CPtrList, Gap_Struct*> *filled, bool preview, bool bCreatePartFileCopy = true);

	static bool scanForZipMarker(CFile *input, archiveScannerThreadParams_s *aitp, uint32 marker, ULONGLONG available);
	static bool processZipEntry(CFile *zipInput, CFile *zipOutput, uint32 available, CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *centralDirectoryEntries);
	static bool readZipCentralDirectory(CFile *zipInput, CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *centralDirectoryEntries, CTypedPtrList<CPtrList, Gap_Struct*> *filled);

	static RAR_BlockFile* scanForRarFileHeader(CFile *input, archiveScannerThreadParams_s *aitp, ULONGLONG available);
	static bool validateRarFileBlock(RAR_BlockFile *block);
	static void writeRarBlock(CFile *input, CFile *output, RAR_BlockFile *block);

	static ACE_BlockFile* scanForAceFileHeader(CFile *input, archiveScannerThreadParams_s *aitp, ULONGLONG available);
	static void writeAceBlock(CFile *input, CFile *output, ACE_BlockFile *block);
	static void writeAceHeader(CFile *output, ACE_ARCHIVEHEADER *hdr);

	static bool CopyPartFile(CPartFile *partFile, CTypedPtrList<CPtrList, Gap_Struct*> *filled, const CString &tempFileName);
	static void DeleteMemory(ThreadParam *tp);
	static bool IsFilled(uint64 start, uint64 end, CTypedPtrList<CPtrList, Gap_Struct*> *filled);

	static void ISOReadDirectory(archiveScannerThreadParams_s *aitp, UINT32 startSec, CFile *isoInput, const CString &currentDirName);

	static void ProcessProgress(archiveScannerThreadParams_s *aitp, UINT64 pos);
	static void LogRecovered(INT_PTR fileCount);

	static uint16 readUInt16(CFile *input);
	static uint32 readUInt32(CFile *input);
	static uint16 calcUInt16(BYTE *input);
	static uint32 calcUInt32(BYTE *input);
	static void writeUInt16(CFile *output, uint16 val);
	static void writeUInt32(CFile *output, uint32 val);

};	 // class