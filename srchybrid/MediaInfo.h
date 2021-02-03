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
#include "RichEditStream.h"

/////////////////////////////////////////////////////////////////////////////
// CStringStream

class CStringStream
{
public:
	CStringStream() = default;

	CStringStream& operator<<(LPCTSTR psz);
	CStringStream& operator<<(char *psz);
	CStringStream& operator<<(UINT uVal);
	CStringStream& operator<<(int iVal);
	CStringStream& operator<<(double fVal);

	bool IsEmpty() const
	{
		return str.IsEmpty();
	}
	void AppendFormat(LPCTSTR pszFmt, ...)
	{
		va_list argp;
		va_start(argp, pszFmt);
		str.AppendFormatV(pszFmt, argp);
		va_end(argp);
	}
	const CString& GetText() const
	{
		return str;
	}

protected:
	CString str;
};

// DirectShow MediaDet
#ifdef HAVE_QEDIT_H
#include <strmif.h>
//#include <uuids.h>
#define	_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
			EXTERN_C const GUID DECLSPEC_SELECTANY name \
				= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
_DEFINE_GUID(MEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
_DEFINE_GUID(MEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
_DEFINE_GUID(FORMAT_VideoInfo,0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
_DEFINE_GUID(FORMAT_WaveFormatEx,0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
//#define MMNODRV		// mmsystem: Installable driver support
#define MMNOSOUND		// mmsystem: Sound support
//#define MMNOWAVE		// mmsystem: Waveform support
#define MMNOMIDI		// mmsystem: MIDI support
#define MMNOAUX			// mmsystem: Auxiliary audio support
#define MMNOMIXER		// mmsystem: Mixer support
#define MMNOTIMER		// mmsystem: Timer support
#define MMNOJOY			// mmsystem: Joystick support
#define MMNOMCI			// mmsystem: MCI support
//#define MMNOMMIO		// mmsystem: Multimedia file I/O support
#define MMNOMMSYSTEM	// mmsystem: General MMSYSTEM functions
// NOTE: If you get a compile error due to missing 'qedit.h', look at "emule_site_config.h" for further information.
#include <qedit.h>
#else//HAVE_QEDIT_H
#include <mmsystem.h>
typedef LONGLONG REFERENCE_TIME;
#endif//HAVE_QEDIT_H
typedef struct tagVIDEOINFOHEADER
{
	RECT			rcSource;		   // The bit we really want to use
	RECT			rcTarget;		   // Where the video should go
	DWORD			dwBitRate;		   // Approximate bit data rate
	DWORD			dwBitErrorRate;	   // Bit error rate for this stream
	REFERENCE_TIME	AvgTimePerFrame;   // Average time per frame (100ns units)
	BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER;

// Those defines are for 'mmreg.h' which is included by 'vfw.h'
#define NOMMIDS		 // Multimedia IDs are not defined
//#define NONEWWAVE	   // No new waveform types are defined except WAVEFORMATEX
#define NONEWRIFF	 // No new RIFF forms are defined
#define NOJPEGDIB	 // No JPEG DIB definitions
#define NONEWIC		 // No new Image Compressor types are defined
#define NOBITMAP	 // No extended bitmap info header definition
// Those defines are for 'vfw.h'
//#define NOCOMPMAN
//#define NODRAWDIB
#define NOVIDEO
//#define NOAVIFMT
//#define NOMMREG
//#define NOAVIFILE
#define NOMCIWND
#define NOAVICAP
#define NOMSACM
#include <windows.h>
#include <mmiscapi.h>
#include <mmeapi.h>
#define MMNOMIXERDEV
#include <mmddk.h>
#include <vfw.h>


/////////////////////////////////////////////////////////////////////////////
// SMediaInfo

struct SMediaInfo
{
	SMediaInfo()
		: ulFileSize()
		, fFileLengthSec()
		, bFileLengthEstimated()
		, iVideoStreams()
		, video()
		, fVideoLengthSec()
		, bVideoLengthEstimated()
		, fVideoFrameRate()
		, fVideoAspectRatio()
		, iAudioStreams()
		, audio()
		, fAudioLengthSec()
		, bAudioLengthEstimated()
		, bOutputFileName(true)
	{
	}

	SMediaInfo& operator=(const SMediaInfo &strm)
	{
		strFileFormat = strm.strFileFormat;
		strMimeType = strm.strMimeType;
		ulFileSize = strm.ulFileSize;
		fFileLengthSec = strm.fFileLengthSec;
		bFileLengthEstimated = strm.bFileLengthEstimated;
		strTitle = strm.strTitle;
		strAuthor = strm.strAuthor;
		strAlbum = strm.strAlbum;

		iVideoStreams = strm.iVideoStreams;
		strVideoFormat = strm.strVideoFormat;
		video = strm.video;
		fVideoLengthSec = strm.fVideoLengthSec;
		bVideoLengthEstimated = strm.bVideoLengthEstimated;
		fVideoFrameRate = strm.fVideoFrameRate;
		fVideoAspectRatio = strm.fVideoAspectRatio;

		iAudioStreams = strm.iAudioStreams;
		strAudioFormat = strm.strAudioFormat;
		audio = strm.audio;
		fAudioLengthSec = strm.fAudioLengthSec;
		bAudioLengthEstimated = strm.bAudioLengthEstimated;
		strAudioLanguage = strm.strAudioLanguage;
		strFileName = strm.strFileName;
		bOutputFileName = strm.bOutputFileName;
		return *this;
	}

	SMediaInfo(const SMediaInfo &strm)
	{
		*this = strm;
	}

	void OutputFileName()
	{
		if (bOutputFileName) {
			bOutputFileName = false;
			if (!strInfo.IsEmpty())
				strInfo << _T("\n");
			strInfo.SetSelectionCharFormat(strInfo.m_cfBold);
			strInfo << GetResString(IDS_FILE) << _T(": ") << strFileName << _T("\n");
		}
	}

	void InitFileLength()
	{
		if (fFileLengthSec == 0) {
			if (fVideoLengthSec > 0.0) {
				fFileLengthSec = fVideoLengthSec;
				bFileLengthEstimated = bVideoLengthEstimated;
			} else if (fAudioLengthSec > 0.0) {
				fFileLengthSec = fAudioLengthSec;
				bFileLengthEstimated = bAudioLengthEstimated;
			}
		}
	}

	CString			strFileName;
	CString			strFileFormat;
	CString			strMimeType;
	EMFileSize		ulFileSize;
	double			fFileLengthSec;
	bool			bFileLengthEstimated;
	CString			strTitle;
	CString			strAuthor;
	CString			strAlbum;

	int				iVideoStreams;
	CString			strVideoFormat;
	VIDEOINFOHEADER	video;
	double			fVideoLengthSec;
	bool			bVideoLengthEstimated;
	double			fVideoFrameRate;
	double			fVideoAspectRatio;

	int				iAudioStreams;
	CString			strAudioFormat;
	WAVEFORMAT		audio;
	double			fAudioLengthSec;
	bool			bAudioLengthEstimated;
	CString			strAudioLanguage;

	bool			bOutputFileName;
	CRichEditStream	strInfo;
};


bool GetMimeType(LPCTSTR pszFilePath, CString &rstrMimeType);
bool GetDRM(LPCTSTR pszFilePath);
bool GetRIFFHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsAVI, bool bFullInfo = false);
bool GetRMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsRM, bool bFullInfo = false);
#ifdef HAVE_WMSDK_H
bool GetWMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsWM, bool bFullInfo = false);
#endif//HAVE_WMSDK_H
CString GetAudioFormatName(WORD wFormatTag, CString &rstrComment);
CString GetAudioFormatName(WORD wFormatTag);
CString GetAudioFormatCodecId(WORD wFormatTag);
bool IsEqualFOURCC(FOURCC fccA, FOURCC fccB);
CString GetVideoFormatName(DWORD biCompression);
CString GetKnownAspectRatioDisplayString(float fAspectRatio);
CString GetCodecDisplayName(const CString &strCodecId);