//this file is part of eMule
//Copyright (C)2003 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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

class CxImage;
class CKnownFile;

struct FrameGrabResult_Struct
{
	CxImage	**imgResults;
	uint8	nImagesGrabbed;
	void	*pSender;
};

// CPreview

class CFrameGrabThread : public CWinThread
{
	DECLARE_DYNCREATE(CFrameGrabThread)

protected:
	CFrameGrabThread();           // protected constructor used by dynamic creation
	virtual	~CFrameGrabThread() = default;
	DECLARE_MESSAGE_MAP()
	UINT	GrabFrames();
public:
	virtual	BOOL InitInstance();
	virtual int	 Run();
	void	SetValues(const CKnownFile *in_pOwner, const CString &in_strFileName, uint8 in_nFramesToGrab, double in_dStartTime, bool in_bReduceColor, uint16 in_nMaxWidth, void *pSender);

private:
	CString	strFileName;
	CxImage	**imgResults;
	const CKnownFile *pOwner;
	void	*pSender;
	double	dStartTime;
	uint16	nMaxWidth;
	uint8	nFramesToGrab;
	bool	bReduceColor;
};