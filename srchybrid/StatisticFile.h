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
class CKnownFile;

class CStatisticFile
{
public:
	CStatisticFile()
		: fileParent()
		, alltimetransferred()
		, transferred()
		, alltimerequested()
		, requested()
		, alltimeaccepted()
		, accepted()
	{
	}

	void	MergeFileStats(CStatisticFile *toMerge);
	void	AddRequest();
	void	AddAccepted();
	void	AddTransferred(uint64 bytes);

	UINT	GetRequests() const				{ return requested; }
	UINT	GetAccepts() const				{ return accepted; }
	uint64	GetTransferred() const			{ return transferred; }
	UINT	GetAllTimeRequests() const		{ return alltimerequested; }
	UINT	GetAllTimeAccepts() const		{ return alltimeaccepted; }
	uint64	GetAllTimeTransferred() const	{ return alltimetransferred; }
	void	SetAllTimeRequests(uint32 nVal);
	void	SetAllTimeAccepts(uint32 nVal);
	void	SetAllTimeTransferred(uint64 nVal);

	CKnownFile *fileParent;

private:
	uint64 alltimetransferred;
	uint64 transferred;
	uint32 alltimerequested;
	uint32 requested;
	uint32 alltimeaccepted;
	uint32 accepted;
};