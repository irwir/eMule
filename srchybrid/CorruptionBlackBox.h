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

class CUpDownClient;

enum EBBRStatus
{
	BBR_NONE = 0,
	BBR_VERIFIED,
	BBR_CORRUPTED
};


class CCBBRecord
{
public:
	CCBBRecord(uint64 nStartPos = 0, uint64 nEndPos = 0, uint32 dwIP = 0, EBBRStatus BBRStatus = BBR_NONE);
	CCBBRecord(const CCBBRecord &cv)			{ *this = cv; }
	CCBBRecord& operator=(const CCBBRecord &cv);

	bool	Merge(uint64 nStartPos, uint64 nEndPos, uint32 dwIP, EBBRStatus BBRStatus = BBR_NONE);
	bool	CanMerge(uint64 nStartPos, uint64 nEndPos, uint32 dwIP, EBBRStatus BBRStatus = BBR_NONE) const;

	uint64	m_nStartPos;
	uint64	m_nEndPos;
	uint32	m_dwIP;
	EBBRStatus	m_BBRStatus;
};

typedef CArray<CCBBRecord> CRecordArray;


class CCorruptionBlackBox
{
public:
	CCorruptionBlackBox() = default;
	~CCorruptionBlackBox() = default;
	void	Init(EMFileSize nFileSize);
	void	Free();
	void	TransferredData(uint64 nStartPos, uint64 nEndPos, const CUpDownClient *pSender);
	void	VerifiedData(uint64 nStartPos, uint64 nEndPos);
	void	CorruptedData(uint64 nStartPos, uint64 nEndPos);
	void	EvaluateData(uint16 nPart);

private:
	CArray<CRecordArray> m_aaRecords;
};