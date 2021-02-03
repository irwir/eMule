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
#include "emule.h"
#include "ArchiveRecovery.h"
#include "Log.h"
#include "PartFile.h"
#include "zlib/zlib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#pragma pack(push, 1)
typedef struct
{
	BYTE	type;
	WORD	flags;
	WORD	size;
} RARMAINHDR;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	BYTE	type;
	WORD	flags;
	WORD	size;
	DWORD	packetSize;
	DWORD	unpacketSize;
	BYTE	hostOS;
	DWORD	fileCRC;
	DWORD	fileTime;
	BYTE	unpVer;
	BYTE	method;
	WORD	nameSize;
	DWORD	fileAttr;
} RARFILEHDR;
#pragma pack(pop)


void CArchiveRecovery::recover(CPartFile *partFile, bool preview, bool bCreatePartFileCopy)
{
	if (partFile->m_bPreviewing || partFile->m_bRecoveringArchive)
		return;
	partFile->m_bRecoveringArchive = true;

	AddLogLine(true, _T("%s \"%s\""), (LPCTSTR)GetResString(IDS_ATTEMPTING_RECOVERY), (LPCTSTR)partFile->GetFileName());

	// Get the current filled list for this file
	CTypedPtrList<CPtrList, Gap_Struct*> *filled = new CTypedPtrList<CPtrList, Gap_Struct*>;
	partFile->GetFilledList(filled);
#ifdef _DEBUG
	{
		int i = 0;
		TRACE("%s: filled\n", __FUNCTION__);
		for (POSITION pos = filled->GetHeadPosition(); pos != NULL;) {
			const Gap_Struct *gap = filled->GetNext(pos);
			TRACE("%3u: %10u  %10u  (%u)\n", i++, gap->start, gap->end, gap->end - gap->start + 1);
		}
	}
#endif

	// The rest of the work safely can be done in a new thread
	ThreadParam *tp = new ThreadParam;
	tp->partFile = partFile;
	tp->filled = filled;
	tp->preview = preview;
	tp->bCreatePartFileCopy = bCreatePartFileCopy;
	// - do NOT use Windows API 'CreateThread' to create a thread which uses MFC/CRT -> lot of mem leaks!
	if (!AfxBeginThread(run, (LPVOID)tp, THREAD_PRIORITY_IDLE)) {
		partFile->m_bRecoveringArchive = false;
		LogError(LOG_STATUSBAR, _T("%s \"%s\""), (LPCTSTR)GetResString(IDS_RECOVERY_FAILED), (LPCTSTR)partFile->GetFileName());
		// Need to delete the memory here as it wouldn't be done in the thread
		DeleteMemory(tp);
	}
}

UINT AFX_CDECL CArchiveRecovery::run(LPVOID lpParam)
{
	ThreadParam *tp = static_cast<ThreadParam*>(lpParam);
	DbgSetThreadName("ArchiveRecovery");
	InitThreadLocale();

	if (!performRecovery(tp->partFile, tp->filled, tp->preview, tp->bCreatePartFileCopy))
		theApp.QueueLogLine(true, _T("%s \"%s\""), (LPCTSTR)GetResString(IDS_RECOVERY_FAILED), (LPCTSTR)tp->partFile->GetFileName());

	tp->partFile->m_bRecoveringArchive = false;

	// Delete memory used by copied gap list
	DeleteMemory(tp);

	return 0;
}

bool CArchiveRecovery::performRecovery(CPartFile *partFile, CTypedPtrList<CPtrList, Gap_Struct*> *filled
	, bool preview, bool bCreatePartFileCopy)
{
	if (filled->IsEmpty())
		return false;

	bool success = false;
	try {
		CFile temp;
		CString tempFileName;
		if (bCreatePartFileCopy) {
			// Copy the file
			tempFileName.Format(_T("%s%s-rec.tmp"), (LPCTSTR)partFile->GetTempPath(), (LPCTSTR)partFile->GetFileName().Left(5));
			if (!CopyPartFile(partFile, filled, tempFileName))
				return false;

			// Open temp file for reading
			if (!temp.Open(tempFileName, CFile::modeRead | CFile::shareDenyWrite))
				return false;
		} else if (!temp.Open(partFile->GetFilePath(), CFile::modeRead | CFile::shareDenyNone))
			return false;

		// Open the output file
		EFileType myAtype = GetFileTypeEx(partFile);
		LPCTSTR ext;
		switch (myAtype) {
		case ARCHIVE_ZIP:
			ext = _T(".zip");
			break;
		case ARCHIVE_RAR:
			ext = _T(".rar");
			break;
		case ARCHIVE_ACE:
			ext = _T(".ace");
			break;
		case ARCHIVE_7Z:
			ext = _T(".7z");
			break;
		default:
			ext = _T("");
		}

		CString outputFileName;
		outputFileName.Format(_T("%s%s-rec%s"), (LPCTSTR)partFile->GetTempPath(), (LPCTSTR)partFile->GetFileName().Left(5), ext);
		CFile output;
		ULONGLONG ulTempFileSize = 0;
		if (output.Open(outputFileName, CFile::modeWrite | CFile::shareDenyWrite | CFile::modeCreate)) {
			// Process the output file
			switch (myAtype) {
			case ARCHIVE_ZIP:
				success = recoverZip(&temp, &output, NULL, filled, (temp.GetLength() == partFile->GetFileSize()));
				break;
			case ARCHIVE_RAR:
				success = recoverRar(&temp, &output, NULL, filled);
				break;
			case ARCHIVE_ACE:
				success = recoverAce(&temp, &output, NULL, filled);
			}
			ulTempFileSize = output.GetLength();
			// Close output
			output.Close();
		}
		// Close temp file
		temp.Close();

		// Remove temp file
		if (!tempFileName.IsEmpty())
			CFile::Remove(tempFileName);

		// Report success
		if (success) {
			theApp.QueueLogLine(true, _T("%s \"%s\""), (LPCTSTR)GetResString(IDS_RECOVERY_SUCCESSFUL), (LPCTSTR)partFile->GetFileName());
			theApp.QueueDebugLogLine(false, _T("Archive recovery: Part file size: %s, temp. archive file size: %s (%.1f%%)"), (LPCTSTR)CastItoXBytes(partFile->GetFileSize()), (LPCTSTR)CastItoXBytes(ulTempFileSize), partFile->GetFileSize() > 0ull ? (ulTempFileSize * 100.0 / (uint64)partFile->GetFileSize()) : 0.0);

			// Preview file if required
			if (preview) {
				SHELLEXECUTEINFO SE = {};
				SE.fMask = SEE_MASK_NOCLOSEPROCESS;
				SE.lpVerb = _T("open");
				SE.lpFile = outputFileName;
				SE.nShow = SW_SHOW;
				SE.cbSize = (DWORD)sizeof SE;
				ShellExecuteEx(&SE);
				if (SE.hProcess) {
					::WaitForSingleObject(SE.hProcess, INFINITE);
					::CloseHandle(SE.hProcess);
				}
				CFile::Remove(outputFileName);
			}
		} else
			CFile::Remove(outputFileName);
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	return success;
}

bool CArchiveRecovery::recoverZip(CFile *zipInput, CFile *zipOutput, archiveScannerThreadParams_s *aitp
	, CTypedPtrList<CPtrList, Gap_Struct*> *filled, bool fullSize)
{
	bool retVal = false;
	INT_PTR fileCount = 0;
	CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *pDir = NULL;
	try {
		CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *centralDirectoryEntries;
		if (aitp == NULL)
			pDir = centralDirectoryEntries = new CTypedPtrList<CPtrList, ZIP_CentralDirectory*>;
		else
			centralDirectoryEntries = aitp->ai->centralDirectoryEntries;

		// If the central directory is intact this is simple
		if (fullSize && readZipCentralDirectory(zipInput, centralDirectoryEntries, filled)) {
			if (centralDirectoryEntries->IsEmpty())
				goto done;

			// if only read directory, return now
			if (zipOutput == NULL) {
				if (aitp) {
					aitp->ai->bZipCentralDir = true;
					retVal = true;
				} else
					ASSERT(0); // FIXME
				goto done;
			}

			for (POSITION pos = centralDirectoryEntries->GetHeadPosition(); pos != NULL;) {
				bool deleteCD = false;
				POSITION del = pos;
				ZIP_CentralDirectory *cdEntry = centralDirectoryEntries->GetNext(pos);
				uint32 lenEntry = sizeof(ZIP_Entry) + cdEntry->lenFilename + cdEntry->lenExtraField + cdEntry->lenCompressed;
				if (IsFilled(cdEntry->relativeOffsetOfLocalHeader, (uint64)cdEntry->relativeOffsetOfLocalHeader + lenEntry, filled)) {
					zipInput->Seek(cdEntry->relativeOffsetOfLocalHeader, CFile::begin);
					// Update offset
					cdEntry->relativeOffsetOfLocalHeader = (uint32)zipOutput->GetPosition();
					if (!processZipEntry(zipInput, zipOutput, lenEntry, NULL))
						deleteCD = true;
				} else
					deleteCD = true;

				if (deleteCD) {
					delete[] cdEntry->filename;
					delete[] cdEntry->extraField;
					delete[] cdEntry->comment;
					delete cdEntry;
					centralDirectoryEntries->RemoveAt(del);
				}
			}
		} else {
			// Have to scan the file the hard way
			zipInput->SeekToBegin();
			// Loop through filled areas of the file looking for entries
			for (POSITION pos = filled->GetHeadPosition(); pos != NULL;) {
				const Gap_Struct *fill = filled->GetNext(pos);
				const ULONGLONG filePos = zipInput->GetPosition();
				// The file may have been positioned to the next entry in ScanForMarker() or processZipEntry()
				if (filePos > fill->end)
					continue;
				if (filePos < fill->start)
					zipInput->Seek(fill->start, CFile::begin);

				// If there is any problem, then don't bother checking the rest of this part
				do {
					if (aitp && !aitp->m_bIsValid)
						return 0;
					// Scan for entry marker within this filled area
				} while (scanForZipMarker(zipInput, aitp, ZIP_LOCAL_HEADER_MAGIC, fill->end - zipInput->GetPosition() + 1)
						&& zipInput->GetPosition() <= fill->end
						&& processZipEntry(zipInput, zipOutput, (uint32)(fill->end - zipInput->GetPosition() + 1), centralDirectoryEntries));
			}
			if (!zipOutput) {
				retVal = !centralDirectoryEntries->IsEmpty();
				goto done;
			}
		}

		// Remember offset before CD entries
		ULONGLONG startOffset = zipOutput->GetPosition();

		// Write all central directory entries
		fileCount = centralDirectoryEntries->GetCount();
		if (fileCount > 0) {
			while (!centralDirectoryEntries->IsEmpty()) {
				const ZIP_CentralDirectory *cdEntry = centralDirectoryEntries->RemoveHead();

				writeUInt32(zipOutput, ZIP_CD_MAGIC);
				writeUInt16(zipOutput, cdEntry->versionMadeBy);
				writeUInt16(zipOutput, cdEntry->versionToExtract);
				writeUInt16(zipOutput, cdEntry->generalPurposeFlag);
				writeUInt16(zipOutput, cdEntry->compressionMethod);
				writeUInt16(zipOutput, cdEntry->lastModFileTime);
				writeUInt16(zipOutput, cdEntry->lastModFileDate);
				writeUInt32(zipOutput, cdEntry->crc32);
				writeUInt32(zipOutput, cdEntry->lenCompressed);
				writeUInt32(zipOutput, cdEntry->lenUncompressed);
				writeUInt16(zipOutput, cdEntry->lenFilename);
				writeUInt16(zipOutput, cdEntry->lenExtraField);
				writeUInt16(zipOutput, cdEntry->lenComment);
				writeUInt16(zipOutput, 0); // Disk number where file starts
				writeUInt16(zipOutput, cdEntry->internalFileAttributes);
				writeUInt32(zipOutput, cdEntry->externalFileAttributes);
				writeUInt32(zipOutput, cdEntry->relativeOffsetOfLocalHeader);
				zipOutput->Write(cdEntry->filename, cdEntry->lenFilename);
				if (cdEntry->lenExtraField > 0)
					zipOutput->Write(cdEntry->extraField, cdEntry->lenExtraField);
				if (cdEntry->lenComment > 0)
					zipOutput->Write(cdEntry->comment, cdEntry->lenComment);

				delete[] cdEntry->filename;
				delete[] cdEntry->extraField;
				delete[] cdEntry->comment;
				delete cdEntry;
			}

			// Remember offset before CD entries
			ULONGLONG endOffset = zipOutput->GetPosition();

			// Write end of central directory
			writeUInt32(zipOutput, ZIP_END_CD_MAGIC);
			writeUInt16(zipOutput, 0); // Number of this disk
			writeUInt16(zipOutput, 0); // Disk number where the central directory starts
			writeUInt16(zipOutput, (uint16)fileCount); //Number of central directory records on this disk 
			writeUInt16(zipOutput, (uint16)fileCount); //Total number of central directory records 
			writeUInt32(zipOutput, (uint32)(endOffset - startOffset));
			writeUInt32(zipOutput, (uint32)startOffset);
			writeUInt16(zipOutput, (uint16)strlen(ZIP_COMMENT));
			zipOutput->Write(ZIP_COMMENT, (UINT)strlen(ZIP_COMMENT));

			centralDirectoryEntries->RemoveAll();
		}
		retVal = true;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
done:
	delete pDir;
	if (!retVal || zipOutput || fileCount) //fileCount==0 if central directory exists
		LogRecovered(fileCount);
	return retVal;
}

bool CArchiveRecovery::readZipCentralDirectory(CFile *zipInput, CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *centralDirectoryEntries, CTypedPtrList<CPtrList, Gap_Struct*> *filled)
{
	try {
		// Ideally this zip file will not have a comment and the End-CD will be easy to find
		zipInput->Seek(-22, CFile::end);
		if (readUInt32(zipInput) != ZIP_END_CD_MAGIC) {
			// Have to look for it, comment could be up to 65535 chars but only try with less than 1k
			zipInput->Seek(-1046, CFile::end);
			if (!scanForZipMarker(zipInput, NULL, ZIP_END_CD_MAGIC, 1046ull))
				return false;
			// Skip it again
			readUInt32(zipInput);
		}

		// Found End-CD
		// Only interested in offset of first CD
		zipInput->Seek(12, CFile::current);
		uint64 startOffset = readUInt32(zipInput);
		if (!IsFilled(startOffset, zipInput->GetLength(), filled))
			return false;

		// Goto first CD and start reading
		zipInput->Seek(startOffset, CFile::begin);
		while (readUInt32(zipInput) == ZIP_CD_MAGIC) {
			ZIP_CentralDirectory *cdEntry = new ZIP_CentralDirectory;
			cdEntry->versionMadeBy = readUInt16(zipInput);
			cdEntry->versionToExtract = readUInt16(zipInput);
			cdEntry->generalPurposeFlag = readUInt16(zipInput);
			cdEntry->compressionMethod = readUInt16(zipInput);
			cdEntry->lastModFileTime = readUInt16(zipInput);
			cdEntry->lastModFileDate = readUInt16(zipInput);
			cdEntry->crc32 = readUInt32(zipInput);
			cdEntry->lenCompressed = readUInt32(zipInput);
			cdEntry->lenUncompressed = readUInt32(zipInput);
			cdEntry->lenFilename = readUInt16(zipInput);
			cdEntry->lenExtraField = readUInt16(zipInput);
			cdEntry->lenComment = readUInt16(zipInput);
			cdEntry->diskNumberStart = readUInt16(zipInput);
			cdEntry->internalFileAttributes = readUInt16(zipInput);
			cdEntry->externalFileAttributes = readUInt32(zipInput);
			cdEntry->relativeOffsetOfLocalHeader = readUInt32(zipInput);

			if (cdEntry->lenFilename > 0) {
				cdEntry->filename = new BYTE[cdEntry->lenFilename];
				zipInput->Read(cdEntry->filename, cdEntry->lenFilename);
			}
			if (cdEntry->lenExtraField > 0) {
				cdEntry->extraField = new BYTE[cdEntry->lenExtraField];
				zipInput->Read(cdEntry->extraField, cdEntry->lenExtraField);
			}
			if (cdEntry->lenComment > 0) {
				cdEntry->comment = new BYTE[cdEntry->lenComment];
				zipInput->Read(cdEntry->comment, cdEntry->lenComment);
			}

			centralDirectoryEntries->AddTail(cdEntry);
		}

		return true;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	return false;
}

bool CArchiveRecovery::processZipEntry(CFile *zipInput, CFile *zipOutput, uint32 available, CTypedPtrList<CPtrList, ZIP_CentralDirectory*> *centralDirectoryEntries)
{
	if (available < 26)
		return false;

	try {
		// Need to know where it started
		ULONGLONG startOffset = (zipOutput ? zipOutput : zipInput)->GetPosition();

		// Entry format :
		//  4      2 bytes  Version needed to extract
		//  6      2 bytes  General purpose bit flag
		//  8      2 bytes  Compression method
		// 10      2 bytes  Last mod file time
		// 12      2 bytes  Last mod file date
		// 14      4 bytes  CRC-32
		// 18      4 bytes  Compressed size (n)
		// 22      4 bytes  Uncompressed size
		// 26      2 bytes  Filename length (f)
		// 28      2 bytes  Extra field length (e)
		//        (f)bytes  Filename
		//        (e)bytes  Extra field
		//        (n)bytes  Compressed data

		// Read header
		if (readUInt32(zipInput) != ZIP_LOCAL_HEADER_MAGIC)
			return false;

		ZIP_Entry entry = {};
		entry.versionToExtract = readUInt16(zipInput);
		entry.generalPurposeFlag = readUInt16(zipInput);
		entry.compressionMethod = readUInt16(zipInput);
		entry.lastModFileTime = readUInt16(zipInput);
		entry.lastModFileDate = readUInt16(zipInput);
		entry.crc32 = readUInt32(zipInput);
		entry.lenCompressed = readUInt32(zipInput);
		entry.lenUncompressed = readUInt32(zipInput);
		entry.lenFilename = readUInt16(zipInput);
		entry.lenExtraField = readUInt16(zipInput);

		// Do some quick checks at this stage that data is looking OK
//		if ((entry.crc32 == 0) && (entry.lenCompressed == 0) && (entry.lenUncompressed == 0) && (entry.lenFilename > 0))
//			; // this is a directory entry
//		else
		if ((entry.crc32 == 0) || (entry.lenCompressed == 0) || (entry.lenUncompressed == 0) || (entry.lenFilename == 0))
			return false;

		// Is this entry complete
		if (entry.lenFilename + entry.lenExtraField + (zipOutput ? entry.lenCompressed : 0) > available - 26) {
			// Move the file pointer to the start of the next entry
			zipInput->Seek((LONGLONG)entry.lenFilename + entry.lenExtraField + entry.lenCompressed, CFile::current);
			return false;
		}

		// Filename
		if (entry.lenFilename > MAX_PATH)
			return false; // Possibly corrupt, don't allocate lots of memory
		entry.filename = new BYTE[entry.lenFilename];
		if (zipInput->Read(entry.filename, entry.lenFilename) != entry.lenFilename) {
			delete[] entry.filename;
			return false;
		}

		// Extra data
		if (entry.lenExtraField > 0) {
			entry.extraField = new BYTE[entry.lenExtraField];
			zipInput->Read(entry.extraField, entry.lenExtraField);
		}

		if (zipOutput) {
			// Output
			writeUInt32(zipOutput, ZIP_LOCAL_HEADER_MAGIC);
			writeUInt16(zipOutput, entry.versionToExtract);
			writeUInt16(zipOutput, entry.generalPurposeFlag);
			writeUInt16(zipOutput, entry.compressionMethod);
			writeUInt16(zipOutput, entry.lastModFileTime);
			writeUInt16(zipOutput, entry.lastModFileDate);
			writeUInt32(zipOutput, entry.crc32);
			writeUInt32(zipOutput, entry.lenCompressed);
			writeUInt32(zipOutput, entry.lenUncompressed);
			writeUInt16(zipOutput, entry.lenFilename);
			writeUInt16(zipOutput, entry.lenExtraField);
			zipOutput->Write(entry.filename, entry.lenFilename);
			if (entry.lenExtraField > 0)
				zipOutput->Write(entry.extraField, entry.lenExtraField);

			// Read and write compressed data to avoid reading all into memory
			BYTE buf[4096];
			for (uint32 written = 0; written < entry.lenCompressed;) {
				uint32 lenChunk = min(entry.lenCompressed - written, sizeof buf);
				lenChunk = zipInput->Read(buf, lenChunk);
				if (lenChunk == 0)
					break;
				zipOutput->Write(buf, lenChunk);
				written += lenChunk;
			}
			zipOutput->Flush();
		}

		//Central directory:
		if (centralDirectoryEntries != NULL) {
			ZIP_CentralDirectory *cdEntry = new ZIP_CentralDirectory;
			cdEntry->header = ZIP_CD_MAGIC;
			cdEntry->versionMadeBy = entry.versionToExtract;
			cdEntry->versionToExtract = entry.versionToExtract;
			cdEntry->generalPurposeFlag = entry.generalPurposeFlag;
			cdEntry->compressionMethod = entry.compressionMethod;
			cdEntry->lastModFileTime = entry.lastModFileTime;
			cdEntry->lastModFileDate = entry.lastModFileDate;
			cdEntry->crc32 = entry.crc32;
			cdEntry->lenCompressed = entry.lenCompressed;
			cdEntry->lenUncompressed = entry.lenUncompressed;
			cdEntry->lenFilename = entry.lenFilename;
			cdEntry->lenExtraField = entry.lenExtraField;
			cdEntry->diskNumberStart = 0;
			cdEntry->internalFileAttributes = 1;
			cdEntry->externalFileAttributes = 0x81B60020;
			cdEntry->relativeOffsetOfLocalHeader = (uint32)startOffset;
			cdEntry->filename = entry.filename;

			if (entry.lenExtraField > 0)
				cdEntry->extraField = entry.extraField;

			cdEntry->lenComment = 0;
			if (zipOutput != NULL) {
				cdEntry->lenComment = static_cast<uint16>(strlen(ZIP_COMMENT));
				cdEntry->comment = new BYTE[cdEntry->lenComment];
				memcpy(cdEntry->comment, ZIP_COMMENT, cdEntry->lenComment);
			}

			centralDirectoryEntries->AddTail(cdEntry);
		} else {
			delete[] entry.filename;
			if (entry.lenExtraField > 0)
				delete[] entry.extraField;
		}

		// skip data when reading directory only
		if (zipOutput == NULL) {

			// out of available data?
			if ((entry.lenFilename + entry.lenExtraField + entry.lenCompressed) > (available - 26))
				return false;

			zipInput->Seek(entry.lenCompressed, CFile::current);
		}
		return true;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	return false;
}

void CArchiveRecovery::DeleteMemory(ThreadParam *tp)
{
	while (!tp->filled->IsEmpty())
		delete tp->filled->RemoveHead();
	delete tp->filled;
	delete tp;
}

bool CArchiveRecovery::CopyPartFile(CPartFile *partFile, CTypedPtrList<CPtrList, Gap_Struct*> *filled, const CString &tempFileName)
{
	try {
		CFile srcFile;
		if (!srcFile.Open(partFile->GetFilePath(), CFile::modeRead | CFile::shareDenyNone))
			return false;

		// Open destination file and set length to the last filled end position
		CFile destFile;
		destFile.Open(tempFileName, CFile::modeWrite | CFile::shareDenyWrite | CFile::modeCreate);
		Gap_Struct *fill = filled->GetTail();
		destFile.SetLength(fill->end);

		// Loop through the filled areas and copy data
		partFile->m_bPreviewing = true;
		for (POSITION pos = filled->GetHeadPosition(); pos != NULL;) {
			fill = filled->GetNext(pos);
			uint32 copied = 0;
			srcFile.Seek(fill->start, CFile::begin);
			destFile.Seek(fill->start, CFile::begin);
			uint32 read;
			BYTE buffer[4096];
			while ((read = srcFile.Read(buffer, sizeof buffer)) > 0) {
				destFile.Write(buffer, read);
				copied += read;
				// Stop when finished filling (don't worry about extra)
				if (fill->start + copied >= fill->end)
					break;
			}
		}
		destFile.Close();
		srcFile.Close();
		partFile->m_bPreviewing = false;

		return true;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}

	return false;
}

bool CArchiveRecovery::recoverRar(CFile *rarInput, CFile *rarOutput, archiveScannerThreadParams_s *aitp
	, CTypedPtrList<CPtrList, Gap_Struct*> *filled)
{
	bool retVal = false;
	INT_PTR fileCount = 0;
	RAR_BlockFile *block = NULL;
	try {
		// Try to get file header and main header
		//
		bool bValidFileHeader = false;
		bool bValidMainHeader = false;
		BYTE fileHeader[7];
		RARMAINHDR mainHeader = {};
		static const BYTE start[] = {
			// RAR file header
			0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00, //"Rar!\x1a\7\0"

			// main header
			0x08, 0x1A,			// CRC
			0x73,				// type
			0x02, 0x00,			// flags
			0x3B, 0x00,			// size
			0x00, 0x00,			// AV
			0x00, 0x00,			// AV
			0x00, 0x00,			// AV

			// main comment
			0xCA, 0x44,			// CRC
			0x75,				// type
			0x00, 0x00,			// flags
			0x2E, 0x00,			// size

			0x12, 0x00, 0x14, 0x34, 0x2B,
			0x4A, 0x08, 0x15, 0x48, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0A, 0x2B, 0xF9, 0x0E, 0xE2, 0xC1,
			0x32, 0xFB, 0x9E, 0x04, 0x10, 0x50, 0xD7, 0xFE, 0xCD, 0x75, 0x87, 0x9C, 0x28, 0x85, 0xDF, 0xA3,
			0x97, 0xE0 };

		if (rarInput->Read(fileHeader, sizeof fileHeader) == sizeof fileHeader) {
			bool bOldFormat = false;
			if (fileHeader[0] == 0x52) {
				if (fileHeader[1] == 0x45 && fileHeader[2] == 0x7e && fileHeader[3] == 0x5e) { //"RE~^"
					bOldFormat = true;
					bValidFileHeader = true;
				} else if (memcmp(&start[1], &fileHeader[1], 6) == 0)
					bValidFileHeader = true;
			}

			if (bValidFileHeader && !bOldFormat) {
				WORD checkCRC;
				if (rarInput->Read(&checkCRC, sizeof checkCRC) == sizeof checkCRC) {
					if (rarInput->Read(&mainHeader, sizeof mainHeader) == sizeof mainHeader) {
						if (mainHeader.type == 0x73) {
							DWORD crc = crc32(0, (Bytef*)&mainHeader, sizeof mainHeader);
							for (UINT i = 0; i < sizeof(WORD) + sizeof(DWORD); ++i) {
								BYTE ch;
								if (rarInput->Read(&ch, sizeof ch) != sizeof ch)
									break;
								crc = crc32(crc, &ch, 1);
							}
							if (checkCRC == (WORD)crc)
								bValidMainHeader = true;
						}
					}
				}
			}
			rarInput->SeekToBegin();
		}

		// If this is a 'solid' archive the chance to successfully decompress any entries gets higher,
		// when we pass the 'solid' main header bit to the temp. archive.
		BYTE start1[sizeof start];
		memcpy(start1, start, sizeof start);
		if (bValidFileHeader && bValidMainHeader && (mainHeader.flags & 8/*MHD_SOLID*/)) {
			start1[10] |= 8; /*MHD_SOLID*/
			*((short*)&start1[7]) = (short)crc32(0, &start1[9], 11);
		}
		if (aitp)
			aitp->ai->rarFlags = mainHeader.flags;
		if (rarOutput)
			rarOutput->Write(start1, sizeof(start1));

		// loop through the filled blocks
		for (POSITION pos = filled->GetHeadPosition(); pos != NULL;) {
			const Gap_Struct *fill = filled->GetNext(pos);

			rarInput->Seek(fill->start, CFile::begin);

			while ((block = scanForRarFileHeader(rarInput, aitp, (fill->end - rarInput->GetPosition()))) != NULL) {
				if (aitp) {
					aitp->ai->RARdir->AddTail(block);
					if (!aitp->m_bIsValid)
						return false;
				}

				if (rarOutput != NULL && IsFilled(block->offsetData, block->offsetData + block->dataLength, filled)) {
					// Don't include directories in file count
					if ((block->HEAD_FLAGS & 0xE0) != 0xE0)
						++fileCount;
					writeRarBlock(rarInput, rarOutput, block);
				} else
					rarInput->Seek(block->offsetData + block->dataLength, CFile::begin);

				if (aitp == NULL) {
					delete[] block->FILE_NAME;
					block->FILE_NAME = NULL;
					delete block;
					block = NULL;
				}
				if (rarInput->GetPosition() >= fill->end)
					break;
			}
		}
		retVal = true;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}

	if (aitp == NULL) {
		if (block) {
			delete[] block->FILE_NAME;
			delete block;
		}
		LogRecovered(fileCount);
	}
	return retVal;
}

bool CArchiveRecovery::IsFilled(uint64 start, uint64 end, CTypedPtrList<CPtrList, Gap_Struct*> *filled)
{
	for (POSITION pos = filled->GetHeadPosition(); pos != NULL;) {
		const Gap_Struct *fill = filled->GetNext(pos);
		if (fill->start > start)
			break;
		if (fill->end >= end)
			return true;
	}
	return false;
}

#define TESTCHUNKSIZE 51200	 // 50k buffer
// This will find the marker in the file and leave it positioned at the position to read the marker again
bool CArchiveRecovery::scanForZipMarker(CFile *input, archiveScannerThreadParams_s *aitp, uint32 marker, ULONGLONG available)
{
	try {
		//ULONGLONG originalOffset = input->GetPosition();
		BYTE chunk[TESTCHUNKSIZE];
		int lenChunk = (int)sizeof(chunk);
		BYTE *foundPos = NULL;
		int pos = 0;

		while ((available > 0) && ((lenChunk = input->Read(chunk, lenChunk)) > 0)) {
			available -= lenChunk;
			foundPos = chunk;
			// Move back one, will be incremented in loop
			--foundPos;
			while (foundPos != NULL) {
				if (aitp && !aitp->m_bIsValid)
					return false;

				// Find first matching byte
				foundPos = (BYTE*)memchr(foundPos + 1, (marker & 0xFF), (lenChunk - (foundPos + 1 - (&chunk[0]))));
				if (foundPos == NULL)
					break;

				// Test for end of buffer
				pos = (int)(foundPos - chunk);
				if ((pos + 3) > lenChunk) {
					// Re-read buffer starting from found first byte position
					input->Seek((LONGLONG)pos - lenChunk, CFile::current);
					available += lenChunk - pos;
					break;
				}

				if (aitp)
					ProcessProgress(aitp, input->GetPosition());

				// Check for other bytes
				if (*(uint32*)&chunk[pos] == marker) {
					// Found it
					input->Seek((LONGLONG)pos - lenChunk, CFile::current);
					return true;
				}
			}
		}
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	return false;
}

// This will find a file block in the file and leave it positioned at the end of the filename
RAR_BlockFile* CArchiveRecovery::scanForRarFileHeader(CFile *input, archiveScannerThreadParams_s *aitp, ULONGLONG available)
{
	RAR_BlockFile *retVal = NULL;
	BYTE *fileName = NULL;
	try {
		BYTE chunk[TESTCHUNKSIZE];
		BYTE checkCRC[sizeof(RARFILEHDR) + 8 + sizeof(DWORD) * 2 + 512];

		while (available > 0) {
			ULONGLONG chunkstart = input->GetPosition();
			UINT lenChunk = input->Read(chunk, (UINT)(min(available, TESTCHUNKSIZE)));
			if (!lenChunk)
				break;

			available -= lenChunk;
			BYTE *foundPos = chunk;

			// Move back one, will be incremented in loop
			--foundPos;
			while (foundPos != NULL) {
				if (aitp && !aitp->m_bIsValid)
					return 0;

				// Find rar head block marker
				foundPos = (BYTE*)memchr(foundPos + 1, RAR_HEAD_FILE, (lenChunk - (foundPos + 1 - (&chunk[0]))));
				if (foundPos == NULL)
					break;

				ULONGLONG chunkOffset = foundPos - (&chunk[0]);
				ULONGLONG foundOffset = chunkstart + chunkOffset;

				if (aitp)
					ProcessProgress(aitp, foundOffset);

				// Move back 2 bytes to get crc and read block
				input->Seek(foundOffset - 2, CFile::begin);

				// CRC of fields from HEAD_TYPE to ATTR + filename + ext. stuff
				uint16 headCRC = readUInt16(input);

				RARFILEHDR *hdr = (RARFILEHDR*)checkCRC;
				input->Read(checkCRC, sizeof *hdr);

				// this bit always is set
				if (!(hdr->flags & 0x8000))
					continue;

				unsigned checkCRCsize = sizeof *hdr;

				// get high parts of 64-bit file size fields
				if (hdr->flags & 0x0100/*LHD_LARGE*/) {
					input->Read(&checkCRC[checkCRCsize], sizeof(DWORD) * 2);
					checkCRCsize += sizeof(DWORD) * 2;
				}

				// get filename
				uint16 lenFileName = hdr->nameSize;
				fileName = new BYTE[lenFileName];
				input->Read(fileName, lenFileName);

				// get encryption params
				unsigned saltPos = 0;
				if (hdr->flags & 0x0400/*LHD_SALT*/) {
					saltPos = checkCRCsize;
					input->Read(&checkCRC[checkCRCsize], 8);
					checkCRCsize += 8;
				}

				// get ext. file date/time
				unsigned extTimePos = 0;
				unsigned extTimeSize = 0;
				if (hdr->flags & 0x1000/*LHD_EXTTIME*/) {
					try {
						extTimePos = checkCRCsize;
						ASSERT(checkCRCsize + sizeof(WORD) <= sizeof checkCRC);
						//if (checkCRCsize + sizeof(WORD) > sizeof checkCRC)
						//	throw -1;
						input->Read(&checkCRC[checkCRCsize], sizeof(WORD));
						unsigned short Flags = *((WORD*)&checkCRC[checkCRCsize]);
						checkCRCsize += sizeof(WORD);
						for (int i = 0; i < 4; ++i) {
							unsigned rmode = Flags >> ((3 - i) * 4);
							if ((rmode & 8) == 0)
								continue;
							if (i != 0) {
								if (checkCRCsize + sizeof(DWORD) > sizeof checkCRC)
									throw -1;
								input->Read(&checkCRC[checkCRCsize], sizeof(DWORD));
								checkCRCsize += sizeof(DWORD);
							}
							int count = rmode & 3;
							for (int j = 0; j < count; ++j) {
								if (checkCRCsize + sizeof(BYTE) > sizeof checkCRC)
									throw - 1;
								input->Read(&checkCRC[checkCRCsize], sizeof(BYTE));
								checkCRCsize += sizeof(BYTE);
							}
						}
						extTimeSize = checkCRCsize - extTimePos;
					} catch (int ex) {
						(void)ex;
						extTimePos = 0;
						extTimeSize = 0;
					}
				}

				uLong crc = crc32(0, checkCRC, sizeof *hdr);
				crc = crc32(crc, fileName, lenFileName);
				if (checkCRCsize > sizeof *hdr)
					crc = crc32(crc, &checkCRC[sizeof *hdr], (checkCRCsize - sizeof *hdr));
				if ((uint16)crc == headCRC) {
					// Found valid crc, build block and return
					// Note that it may still be invalid data, so more checks should be performed
					retVal = new RAR_BlockFile;
					retVal->HEAD_CRC = headCRC;
					retVal->HEAD_TYPE = 0x74;
					retVal->HEAD_FLAGS = calcUInt16(&checkCRC[1]);
					retVal->HEAD_SIZE = calcUInt16(&checkCRC[3]);
					retVal->PACK_SIZE = calcUInt32(&checkCRC[5]);
					retVal->UNP_SIZE = calcUInt32(&checkCRC[9]);
					retVal->HOST_OS = checkCRC[13];
					retVal->FILE_CRC = calcUInt32(&checkCRC[14]);
					retVal->FTIME = calcUInt32(&checkCRC[18]);
					retVal->UNP_VER = checkCRC[22];
					retVal->METHOD = checkCRC[23];
					retVal->NAME_SIZE = lenFileName;
					retVal->ATTR = calcUInt32(&checkCRC[26]);
					// Optional values, present only if bit 0x100 in HEAD_FLAGS is set.
					if (retVal->HEAD_FLAGS & 0x100) {
						retVal->HIGH_PACK_SIZE = calcUInt32(&checkCRC[30]);
						retVal->HIGH_UNP_SIZE = calcUInt32(&checkCRC[34]);
					} else {
						retVal->HIGH_PACK_SIZE = 0;
						retVal->HIGH_UNP_SIZE = 0;
					}
					retVal->FILE_NAME = fileName;
					if (saltPos != 0)
						memcpy(retVal->SALT, &checkCRC[saltPos], sizeof retVal->SALT);
					if (extTimePos != 0 && extTimeSize != 0) {
						retVal->EXT_DATE = new BYTE[extTimeSize];
						memcpy(retVal->EXT_DATE, &checkCRC[extTimePos], extTimeSize);
						retVal->EXT_DATE_SIZE = extTimeSize;
					}

					// Run some quick checks
					if (validateRarFileBlock(retVal)) {
						// Set some useful markers in the block
						retVal->offsetData = input->GetPosition();
						uint32 dataLength = retVal->PACK_SIZE;
						// If comment present find length
						if ((retVal->HEAD_FLAGS & 0x08) == 0x08) {
							// Skip start of comment block
							input->Seek(5, CFile::current);
							// Read comment length
							dataLength += readUInt16(input);
						}
						retVal->dataLength = dataLength;

						return retVal;
					}
				}
				// If not valid, continue searching where we left of
				delete[] fileName;
				fileName = NULL;
				delete retVal;
				retVal = NULL;
			}
		}
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	delete[] fileName;
	return NULL;
}

// This assumes that head crc has already been checked
bool CArchiveRecovery::validateRarFileBlock(RAR_BlockFile *block)
{
	if (block->HEAD_TYPE != 0x74)
		return false;

	// The following condition basically makes sense, but it though triggers false errors
	// in some cases (e.g. when there are very small files compressed it's not that unlikely
	// that the compressed size is a little larger than the uncompressed size due to RAR
	// overhead.
	//if ((block->HEAD_FLAGS & 0x0400/*LHD_SALT*/) == 0 && block->UNP_SIZE < block->PACK_SIZE)
	//	return false;

	if (block->HOST_OS > 5)
		return false;

	switch (block->METHOD) {
	case 0x30: // storing
	case 0x31: // fastest compression
	case 0x32: // fast compression
	case 0x33: // normal compression
	case 0x34: // good compression
	case 0x35: // best compression
		break;
	default:
		return false;
	}

	if (block->HEAD_FLAGS & 0x0200) {
		// ANSI+Unicode name
		if (block->NAME_SIZE > MAX_PATH + MAX_PATH * 2)
			return false;
	} else {
		// ANSI+Unicode name
		if (block->NAME_SIZE > MAX_PATH)
			return false;
	}

	// Check directory entry has no size
	if (((block->HEAD_FLAGS & 0xE0) == 0xE0) && ((block->PACK_SIZE + block->UNP_SIZE + block->FILE_CRC) > 0))
		return false;

	return true;
}

void CArchiveRecovery::writeRarBlock(CFile *input, CFile *output, RAR_BlockFile *block)
{
	ULONGLONG offsetStart = output->GetPosition();
	try {
		writeUInt16(output, block->HEAD_CRC);
		output->Write(&block->HEAD_TYPE, 1);
		writeUInt16(output, block->HEAD_FLAGS);
		writeUInt16(output, block->HEAD_SIZE);
		writeUInt32(output, block->PACK_SIZE);
		writeUInt32(output, block->UNP_SIZE);
		output->Write(&block->HOST_OS, 1);
		writeUInt32(output, block->FILE_CRC);
		writeUInt32(output, block->FTIME);
		output->Write(&block->UNP_VER, 1);
		output->Write(&block->METHOD, 1);
		writeUInt16(output, block->NAME_SIZE);
		writeUInt32(output, block->ATTR);
		// Optional values, present only if bit 0x100 in HEAD_FLAGS is set.
		if (block->HEAD_FLAGS & 0x100) {
			writeUInt32(output, block->HIGH_PACK_SIZE);
			writeUInt32(output, block->HIGH_UNP_SIZE);
		}
		output->Write(block->FILE_NAME, block->NAME_SIZE);
		if (block->HEAD_FLAGS & 0x0400/*LHD_SALT*/)
			output->Write(block->SALT, sizeof block->SALT);
		output->Write(block->EXT_DATE, block->EXT_DATE_SIZE);

		// Now copy compressed data from input file
		uint32 lenToCopy = block->dataLength;
		if (lenToCopy > 0) {
			input->Seek(block->offsetData, CFile::begin);
			BYTE chunk[4096];
			while (lenToCopy > 0) {
				uint32 lenChunk = min(lenToCopy, sizeof chunk);
				lenChunk = input->Read(chunk, lenChunk);
				if (lenChunk == 0)
					break;
				output->Write(chunk, lenChunk);
				lenToCopy -= lenChunk;
			}
		}
		output->Flush();
		return;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	try {
		output->SetLength(offsetStart);
	} catch (...) {
		ASSERT(0);
	}
}


//################################   ACE   ####################################
#define CRC_MASK 0xFFFFFFFFL
#define CRCPOLY  0xEDB88320L
static ULONG crctable[256];
void make_crctable()   // initializes CRC table
{
	for (ULONG i = 0; i <= 255; ++i) {
		ULONG r = i;
		for (ULONG j = 8; j; --j)
			r = (r & 1) ? (r >> 1) ^ CRCPOLY : (r >> 1);
		crctable[i] = r;
	}
}

// Updates crc from addr till addr+len-1
//
ULONG getcrc(ULONG crc, UCHAR *addr, INT len)
{
	while (len--)
		crc = crctable[(unsigned char)crc ^ (*addr++)] ^ (crc >> 8);
	return crc;
}

bool CArchiveRecovery::recoverAce(CFile *aceInput, CFile *aceOutput, archiveScannerThreadParams_s *aitp
		, CTypedPtrList<CPtrList, Gap_Struct*> *filled)
{
	make_crctable();
	ACE_ARCHIVEHEADER *acehdr = NULL;
	ACE_BlockFile *block = NULL;

	try {
		UINT64 filesearchstart = 0;
		// Try to get file header and main header
		if (IsFilled(0, 32, filled)) {
			static const char ACE_ID[] = {0x2A, 0x2A, 0x41, 0x43, 0x45, 0x2A, 0x2A};
			acehdr = new ACE_ARCHIVEHEADER;

			UINT hdrread = aceInput->Read((void*)acehdr, sizeof(ACE_ARCHIVEHEADER) - (3 * sizeof(char*)) - sizeof(uint16));

			if (memcmp(acehdr->HEAD_SIGN, ACE_ID, sizeof ACE_ID) != 0 || acehdr->HEAD_TYPE != 0
				|| !IsFilled(0, acehdr->HEAD_SIZE, filled))
			{
				delete acehdr;
				acehdr = NULL;
				if (aceOutput)
					return false;
			} else {
				hdrread -= 2 * sizeof(uint16);		// care for the size that is specified with HEADER_SIZE
				LONG headcrc = getcrc(CRC_MASK, (UCHAR*)&acehdr->HEAD_TYPE, hdrread);

				if (acehdr->AVSIZE) {
					acehdr->AV = (char*)malloc((size_t)acehdr->AVSIZE + 1);
					hdrread += aceInput->Read(acehdr->AV, acehdr->AVSIZE);
					headcrc = getcrc(headcrc, (UCHAR*)acehdr->AV, acehdr->AVSIZE);
				}

				if (hdrread < acehdr->HEAD_SIZE) {

					hdrread += aceInput->Read(&acehdr->COMMENT_SIZE, sizeof(acehdr->COMMENT_SIZE));
					headcrc = getcrc(headcrc, (UCHAR*)(&acehdr->COMMENT_SIZE), sizeof(acehdr->COMMENT_SIZE));
					if (acehdr->COMMENT_SIZE) {
						acehdr->COMMENT = (char*)malloc((size_t)acehdr->COMMENT_SIZE + 1);
						hdrread += aceInput->Read(acehdr->COMMENT, acehdr->COMMENT_SIZE);
						headcrc = getcrc(headcrc, (UCHAR*)acehdr->COMMENT, acehdr->COMMENT_SIZE);
					}
				}

				// some unknown data left to read
				if (hdrread < acehdr->HEAD_SIZE) {
					UINT dumpsize = acehdr->HEAD_SIZE - hdrread;
					acehdr->DUMP = (char*)malloc(dumpsize);
					hdrread += aceInput->Read(acehdr->DUMP, dumpsize);
					headcrc = getcrc(headcrc, (UCHAR*)acehdr->DUMP, dumpsize);
				}

				if (acehdr->HEAD_CRC == (uint16)headcrc) {
					if (aitp)
						aitp->ai->ACEhdr = acehdr;
					filesearchstart = acehdr->HEAD_SIZE + 2 * sizeof(uint16);
					if (aceOutput)
						writeAceHeader(aceOutput, acehdr);

				} else {
					aceInput->SeekToBegin();
					filesearchstart = 0;
					delete acehdr;
					acehdr = NULL;
				}
			}
		}

		if (aceOutput && !acehdr)
			return false;
		if (aitp == NULL && acehdr != NULL) {
			delete acehdr;
			acehdr = NULL;
		}

		// loop filled blocks
		for (POSITION pos = filled->GetHeadPosition(); pos != NULL;) {
			const Gap_Struct *fill = filled->GetNext(pos);

			filesearchstart = (filesearchstart < fill->start) ? fill->start : filesearchstart;
			aceInput->Seek(filesearchstart, CFile::begin);

			while ((block = scanForAceFileHeader(aceInput, aitp, (fill->end - filesearchstart))) != NULL) {
				if (aitp) {
					if (!aitp->m_bIsValid)
						return false;

					aitp->ai->ACEdir->AddTail(block);
				}

				if (aceOutput != NULL && IsFilled(block->data_offset, block->data_offset + block->PACK_SIZE, filled)) {
					// Don't include directories in file count
					//if ((block->HEAD_FLAGS & 0xE0) != 0xE0)
					//	++fileCount;
					writeAceBlock(aceInput, aceOutput, block);
				} else
					aceInput->Seek(block->PACK_SIZE, CFile::current);

				if (aitp == NULL) {
					delete block;
					block = NULL;
				}

				if (aceInput->GetPosition() >= fill->end)
					break;
			}
		}
		return true;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	delete acehdr;
	if (aitp == NULL)
		delete block;
	return false;
}

#define MAXACEHEADERSIZE 10240
ACE_BlockFile* CArchiveRecovery::scanForAceFileHeader(CFile *input, archiveScannerThreadParams_s *aitp, ULONGLONG available)
{
	static char blockmem[MAXACEHEADERSIZE];

	try {
		BYTE chunk[TESTCHUNKSIZE];
		while (available > 0) {
			ULONGLONG chunkstart = input->GetPosition();
			UINT lenChunk = input->Read(chunk, (UINT)(min(available, TESTCHUNKSIZE)));
			if (lenChunk == 0)
				break;

			available -= lenChunk;
			BYTE *foundPos = &chunk[0];

			// Move back one, will be incremented in loop
			--foundPos;
			while (foundPos != NULL) {
				if (aitp && !aitp->m_bIsValid)
					return NULL;

				// Find rar head block marker
				foundPos = (BYTE*)memchr(foundPos + 1, 0x01, (lenChunk - (foundPos + 1 - (&chunk[0]))));
				if (foundPos == NULL)
					break;

				ULONGLONG chunkOffset = foundPos - (&chunk[0]);
				ULONGLONG foundOffset = chunkstart + chunkOffset;

				if (chunkOffset < 4)
					continue;

				if (aitp)
					ProcessProgress(aitp, foundOffset);


				// Move back 4 bytes to get crc,size and read block
				input->Seek(foundOffset - 4, CFile::begin);

				uint16 headCRC, headSize;
				input->Read(&headCRC, 2);
				input->Read(&headSize, 2);
				if (headSize > MAXACEHEADERSIZE // header limit
					|| (lenChunk - chunkOffset) + available < headSize)	// header too big for my filled part
				{
					continue;
				}
				input->Read(blockmem, headSize);
				uint32 crc = getcrc(CRC_MASK, (UCHAR*)blockmem, headSize);

				if ((uint16)crc != headCRC)
					continue;

				char *mempos = blockmem;

				ACE_BlockFile *newblock = new ACE_BlockFile;

				newblock->HEAD_CRC = headCRC;
				newblock->HEAD_SIZE = headSize;

				memcpy(&newblock->HEAD_TYPE, mempos, (char*)&newblock->COMM_SIZE - (char*)&newblock->HEAD_TYPE);
//				sizeof(ACE_BlockFile) - 4 - (2*sizeof(newblock->COMMENT)) - sizeof(newblock->COMM_SIZE) - sizeof(newblock->data_offset) );

				mempos += (char*)&newblock->COMM_SIZE - (char*)&newblock->HEAD_TYPE;
//				mempos += sizeof(ACE_BlockFile)-sizeof(newblock->HEAD_CRC)-sizeof(newblock->HEAD_SIZE) - (2*sizeof(newblock->COMMENT)) - sizeof(newblock->COMM_SIZE) - sizeof(newblock->data_offset);

				// basic checks
				if (newblock->HEAD_SIZE < newblock->FNAME_SIZE) {
					delete newblock;
					continue;
				}

				if (newblock->FNAME_SIZE > 0) {
					newblock->FNAME = (char*)malloc(newblock->FNAME_SIZE + (size_t)1);
					if (newblock->FNAME == NULL) {
						delete newblock;
						continue;
					}
					memcpy(newblock->FNAME, mempos, newblock->FNAME_SIZE);
					mempos += newblock->FNAME_SIZE;
				}

				//int blockleft = newblock->HEAD_SIZE - newblock->FNAME_SIZE - (sizeof(ACE_BlockFile)- (3*sizeof(uint16)) - sizeof(newblock->data_offset) - 2*sizeof(char*));

				if (mempos < blockmem + newblock->HEAD_SIZE) {
					memcpy(&newblock->COMM_SIZE, mempos, sizeof newblock->COMM_SIZE);
					mempos += sizeof(newblock->COMM_SIZE);

					if (newblock->COMM_SIZE) {
						newblock->COMMENT = (char*)malloc(newblock->COMM_SIZE);
						if (newblock->COMMENT == NULL) {
							delete newblock;
							continue;
						}
						memcpy(newblock->COMMENT, mempos, newblock->COMM_SIZE);
						mempos += newblock->COMM_SIZE;
					}
				}

				//if (mempos - blockmem[4] > 0)
				//	input->Seek(blockleft, CFile::current);


				newblock->data_offset = input->GetPosition();
				return newblock;

			} // while foundpos
		} // while available>0
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	return NULL;
}

void CArchiveRecovery::writeAceHeader(CFile *output, ACE_ARCHIVEHEADER *hdr)
{

	writeUInt16(output, hdr->HEAD_CRC);
	writeUInt16(output, hdr->HEAD_SIZE);
	output->Write(&hdr->HEAD_TYPE, 1);
	writeUInt16(output, hdr->HEAD_FLAGS);
	output->Write(hdr->HEAD_SIGN, sizeof(hdr->HEAD_SIGN));
	output->Write(&hdr->VER_EXTRACT, 1);
	output->Write(&hdr->VER_CREATED, 1);
	output->Write(&hdr->HOST_CREATED, 1);
	output->Write(&hdr->VOLUME_NUM, 1);
	writeUInt32(output, hdr->FTIME);
	output->Write(hdr->RESERVED, sizeof(hdr->RESERVED));
	output->Write(&hdr->AVSIZE, 1);

	int rest = hdr->HEAD_SIZE - (sizeof(ACE_ARCHIVEHEADER) - (3 * sizeof(uint16)) - (3 * sizeof(char*)));

	if (hdr->AVSIZE > 0) {
		output->Write(hdr->AV, hdr->AVSIZE);
		rest -= hdr->AVSIZE;
	}

	if (hdr->COMMENT_SIZE) {
		writeUInt16(output, hdr->COMMENT_SIZE);
		rest -= sizeof(hdr->COMMENT_SIZE);

		output->Write(hdr->COMMENT, hdr->COMMENT_SIZE);
		rest -= hdr->COMMENT_SIZE;
	}

	if (rest && hdr->DUMP) {
		output->Write(hdr->DUMP, rest);
		rest = 0;
	}
	ASSERT(rest == 0);
}

void CArchiveRecovery::writeAceBlock(CFile *input, CFile *output, ACE_BlockFile *block)
{
	ULONGLONG offsetStart = output->GetPosition();
	try {
//		block->data_offset = offsetStart;

		writeUInt16(output, block->HEAD_CRC);
		writeUInt16(output, block->HEAD_SIZE);
		output->Write(&block->HEAD_TYPE, 1);
		writeUInt16(output, block->HEAD_FLAGS);
		writeUInt32(output, block->PACK_SIZE);
		writeUInt32(output, block->ORIG_SIZE);
		writeUInt32(output, block->FTIME);
		writeUInt32(output, block->FILE_ATTRIBS);
		writeUInt32(output, block->CRC32);
		writeUInt32(output, block->TECHINFO);
		writeUInt16(output, block->RESERVED);
		writeUInt16(output, block->FNAME_SIZE);
		output->Write(block->FNAME, block->FNAME_SIZE);

		if (block->COMM_SIZE) {
			writeUInt16(output, block->COMM_SIZE);
			output->Write(block->COMMENT, block->COMM_SIZE);
		}

		// skip unknown data between header and compressed data - if ever existed...

		// Now copy compressed data from input file
		uint32 lenToCopy = block->PACK_SIZE;
		if (lenToCopy > 0) {
			input->Seek(block->data_offset, CFile::begin);
			BYTE chunk[4096];
			for (uint32 written = 0; written < lenToCopy;) {
				uint32 lenChunk = min(lenToCopy - written, sizeof chunk);
				lenChunk = input->Read(chunk, lenChunk);
				if (lenChunk == 0)
					break;
				output->Write(chunk, lenChunk);
				written += lenChunk;
			}
		}
		output->Flush();
		return;
	} catch (CFileException *error) {
		error->Delete();
	} catch (...) {
		ASSERT(0);
	}
	try {
		output->SetLength(offsetStart);
	} catch (...) {
		ASSERT(0);
	}
}

// ############### ISO handling #############
// ISO, reads a directory entries of a directory at the given sector (startSec)

void CArchiveRecovery::ISOReadDirectory(archiveScannerThreadParams_s *aitp, UINT32 startSec, CFile *isoInput, const CString &currentDirName)
{
	if (!aitp || !aitp->ai)
		return;
	const LONGLONG secSize = aitp->ai->isoInfos.secSize;
	if (!IsFilled(startSec * secSize, startSec * secSize + secSize, aitp->filled))
		return;
	// read directory entries
	int iSecsOfDirectoy = -1;
	isoInput->Seek(startSec * secSize, FILE_BEGIN);
	while (aitp->m_bIsValid) {
		ISO_FileFolderEntry *file = new ISO_FileFolderEntry;

		UINT32 blocksize = isoInput->Read(file, (sizeof(ISO_FileFolderEntry) - sizeof file->name));

		if (file->lenRecord == 0) {
			delete file;

			// do we continue at next sector?
			if (iSecsOfDirectoy-- > 1) {
				++startSec;
				if (!IsFilled(startSec * secSize, startSec * secSize + secSize, aitp->filled))
					break;

				isoInput->Seek(startSec * secSize, FILE_BEGIN);
				continue;
			}
			break; // folder end
		}

		file->name = (TCHAR*)calloc(file->nameLen + (size_t)2, sizeof(TCHAR));
		if (!file->name) {
			delete file;
			return;
		}
		blocksize += isoInput->Read(file->name, file->nameLen);

		if (!(file->nameLen & 1))
			++blocksize;

		UINT32 skip = LODWORD(file->lenRecord) - blocksize;
		UINT64 pos2 = isoInput->Seek(skip, FILE_CURRENT);		// skip padding
		if (pos2 & 1) {
			isoInput->Seek(1, FILE_CURRENT);					// skip padding
			++pos2;
		}

		// set progressbar
		ProcessProgress(aitp, pos2);

		// selfdir, parentdir ( "." && ".." ) handling
		if ((file->fileFlags & ISO_DIRECTORY) && file->nameLen == 1 && (unsigned)file->name[0] <= 1) {
			// get size of directory and calculate how many sectors are spanned
			if (file->name[0] == 0x00)
				iSecsOfDirectoy = (int)(file->dataSize / secSize);
			delete file;
			continue;
		}

		if (aitp->ai->isoInfos.iJolietUnicode) //convert UTF-16BE to UTF-16LE
			for (unsigned int i = 0; i < file->nameLen / sizeof(uint16); ++i)
				file->name[i] = _byteswap_ushort(file->name[i]);

		// make filename CString from ASCII or Unicode
		const CString &filename(aitp->ai->isoInfos.iJolietUnicode ? CString(file->name) : CString((char*)file->name));
		free(file->name);

		if (file->fileFlags & ISO_DIRECTORY) {
			// store dir entry
			CString pathNew;
			pathNew.Format(_T("%s%s\\"), (LPCTSTR)currentDirName, (LPCTSTR)filename);
			file->name = _tcsdup(pathNew);
			aitp->ai->ISOdir->AddTail(file);

			// read subdirectory recursively
			LONGLONG curpos = isoInput->GetPosition();
			ISOReadDirectory(aitp, LODWORD(file->sector1OfExtension), isoInput, pathNew);
			isoInput->Seek(curpos, FILE_BEGIN);
		} else {
			// store file entry
			const CString &fullpath(currentDirName + filename);
			file->name = _tcsdup(fullpath);
			aitp->ai->ISOdir->AddTail(file);
		}
	}
}

bool CArchiveRecovery::recoverISO(CFile *isoInput, CFile *isoOutput, archiveScannerThreadParams_s *aitp
		, CTypedPtrList<CPtrList, Gap_Struct*> *filled)
{
	if (isoOutput)
		return false;

#define SECSIZE sizeof(ISO_PVD_s)
	aitp->ai->isoInfos.secSize = (DWORD)SECSIZE;
	uint64 nextstart = 16 * SECSIZE;

	// do we have the primary volume descriptor?
	if (!IsFilled(nextstart, nextstart + SECSIZE, filled))
		return false;

	ISO_PVD_s pvd, svd, tempSec;

	// skip to PVD
	isoInput->Seek(nextstart, FILE_BEGIN);

	pvd.descr_type = 0xff;
	svd.descr_type = 0xff;
	aitp->ai->isoInfos.type = ISOtype_unknown;

	int iUdfDetectState = 0;
	// read PVD
	do {
		isoInput->Read(&tempSec, aitp->ai->isoInfos.secSize);
		nextstart += SECSIZE;

		if (tempSec.descr_type == 0xff || (tempSec.descr_type == 0 && tempSec.descr_ver == 0)) // Volume Descriptor Set Terminator (VDST)
			break;

		if (tempSec.descr_type == 0x01 && pvd.descr_type == 0xff) {
			memcpy(&pvd, &tempSec, SECSIZE);
			aitp->ai->isoInfos.type |= ISOtype_9660;
		}

		if (tempSec.descr_type == 0x02) {
			memcpy(&svd, &tempSec, SECSIZE);
			if (svd.escSeq[0] == 0x25 && svd.escSeq[1] == 0x2f) {
				aitp->ai->isoInfos.type |= ISOtype_joliet;

				if (svd.escSeq[2] == 0x40)
					aitp->ai->isoInfos.iJolietUnicode = 1;
				else if (svd.escSeq[2] == 0x43)
					aitp->ai->isoInfos.iJolietUnicode = 2;
				else if (svd.escSeq[2] == 0x45)
					aitp->ai->isoInfos.iJolietUnicode = 3;
			}
		}

		if (tempSec.descr_type == 0x00) {
			BootDescr *bDesc = (BootDescr*)&tempSec;
			if (memcmp(bDesc->sysid, sElToritoID, _countof(sElToritoID) - 1) == 0)
				aitp->ai->isoInfos.bBootable = true;	// anything else?
		}

		// check for udf
		if (tempSec.descr_type == 0x00 && tempSec.descr_ver == 0x01) {

			if (memcmp(tempSec.magic, sig_udf_bea, 5) == 0 && iUdfDetectState == 0)
				iUdfDetectState = 1;// detected Beginning Extended Area Descriptor (BEA)

			else if (memcmp(tempSec.magic, sig_udf_nsr2, 5) == 0 && iUdfDetectState == 1) // Volume Sequence Descriptor (VSD) 2
				iUdfDetectState = 2;

			else if (memcmp(tempSec.magic, sig_udf_nsr3, 5) == 0 && iUdfDetectState == 1) // Volume Sequence Descriptor (VSD) 3
				iUdfDetectState = 3;

			else if (memcmp(tempSec.magic, sig_tea, 5) == 0 && (iUdfDetectState == 2 || iUdfDetectState == 3))
				iUdfDetectState += 2;	// remember Terminating Extended Area Descriptor (TEA) received
		}

	} while (IsFilled(nextstart, nextstart + SECSIZE, filled));

	if (iUdfDetectState == 4)
		aitp->ai->isoInfos.type |= ISOtype_UDF_nsr02;
	else if (iUdfDetectState == 5)
		aitp->ai->isoInfos.type |= ISOtype_UDF_nsr03;

	if (aitp->ai->isoInfos.type == 0)
		return false;

	if (iUdfDetectState == 4 || iUdfDetectState == 5) {
		// TODO: UDF handling  (http://www.osta.org/specs/)
		return false;	// no known udf version
	}

	// ISO9660/Joliet handling

	// read root directory of iso and recursive
	ISO_FileFolderEntry rootdir;
	memcpy(&rootdir.lenRecord, svd.descr_type != 0xff ? svd.rootdir : pvd.rootdir, 33); //33 includes 'nameLen'

	ISOReadDirectory(aitp, LODWORD(rootdir.sector1OfExtension), isoInput, _T(""));

	return true;
}

// ########## end of ISO handling #############


uint16 CArchiveRecovery::readUInt16(CFile *input)
{
	uint16 retVal;
	if (input->Read(&retVal, sizeof retVal) != sizeof retVal)
		return 0;
	return retVal;
}

uint32 CArchiveRecovery::readUInt32(CFile *input)
{
	uint32 retVal;
	if (input->Read(&retVal, sizeof retVal) != sizeof retVal)
		return 0;
	return retVal;
}

uint16 CArchiveRecovery::calcUInt16(BYTE *input)
{
	return *(uint16*)input;
}

uint32 CArchiveRecovery::calcUInt32(BYTE *input)
{
	return *(uint32*)input;
}

void CArchiveRecovery::writeUInt16(CFile *output, uint16 val)
{
	output->Write(&val, sizeof val);
}

void CArchiveRecovery::writeUInt32(CFile *output, uint32 val)
{
	output->Write(&val, sizeof val);
}

void CArchiveRecovery::ProcessProgress(archiveScannerThreadParams_s *aitp, UINT64 pos)
{
	if (aitp->m_bIsValid && (uint64)aitp->file->GetFileSize() > 0) {
		unsigned nNewProgress = (unsigned)((pos * 1000) / (uint64)aitp->file->GetFileSize());
		if (nNewProgress > aitp->curProgress + 1) {
			aitp->curProgress = nNewProgress;
			SendMessage(aitp->progressHwnd, PBM_SETPOS, nNewProgress, 0);
		}
	}
}

void CArchiveRecovery::LogRecovered(INT_PTR fileCount)
{
	// Tell the user how many files were recovered
	CString msg;
	if (fileCount == 1)
		msg = GetResString(IDS_RECOVER_SINGLE);
	else
		msg.Format(GetResString(IDS_RECOVER_MULTIPLE), (unsigned)fileCount);
	theApp.QueueLogLine(true, _T("%s"), (LPCTSTR)msg);
}
