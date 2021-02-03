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

#define ACTION_NONE			0
#define ACTION_SETUPL		1
#define ACTION_SETDOWNL		2
#define ACTION_SOURCESL		3
#define ACTION_CON5SEC		4
#define ACTION_CONS			5
#define ACTION_CATSTOP		6
#define ACTION_CATRESUME	7

#define DAY_DAYLY		0
#define DAY_MO			1
#define DAY_DI			2
#define DAY_MI			3
#define DAY_DO			4
#define DAY_FR			5
#define DAY_SA			6
#define DAY_SO			7
#define DAY_MO_FR		8
#define DAY_MO_SA		9
#define DAY_SA_SO		10

struct Schedule_Struct
{
	time_t		time;
	time_t		time2;
	CString		title;
	CString		values[16];
	UINT		day;
	int			actions[16];
	bool		enabled;
	void ResetActions()
	{
		for (int i = _countof(actions); --i >= 0;) {
			actions[i] = 0;
			values[i].Empty();
		}
	}
	~Schedule_Struct() = default;
};

class CScheduler
{
public:
	CScheduler();
	~CScheduler();

	INT_PTR	AddSchedule(Schedule_Struct *schedule);
	void	UpdateSchedule(INT_PTR index, Schedule_Struct *schedule) { if (index < GetCount())schedulelist[index] = schedule; }
	Schedule_Struct* GetSchedule(INT_PTR index)		{ return (index < GetCount()) ? schedulelist[index] : NULL; }
	void	RemoveSchedule(INT_PTR index);
	void	RemoveAll();
	int		LoadFromFile();
	void	SaveToFile();
	int		Check(bool forcecheck = false);
	INT_PTR	GetCount()								{ return schedulelist.GetCount(); }
	void	SaveOriginals();
	void	RestoreOriginals();
	void	ActivateSchedule(INT_PTR index, bool makedefault = false);

	uint32	original_upload;
	uint32	original_download;
	UINT	original_connections;
	UINT	original_cons5s;
	UINT	original_sources;

private:
	CArray<Schedule_Struct*, Schedule_Struct*> schedulelist;
	int		m_iLastCheckedMinute;
};