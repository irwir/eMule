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
#include "stdafx.h"
#include "emule.h"
#include "FrameGrabThread.h"
#include "CxImage/xImage.h"
#include "OtherFunctions.h"
#include "quantize.h"
#ifndef HAVE_QEDIT_H
// This is a remote feature and not optional, in order to keep to working properly for other clients who want to use it
// Check emule_site_config.h to fix it
#error Missing 'qedit.h', look at "emule_site_config.h" for further information.
#endif

// DirectShow MediaDet
#include <strmif.h>
#define _DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
		EXTERN_C const GUID DECLSPEC_SELECTANY name \
				= { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
_DEFINE_GUID(MEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
_DEFINE_GUID(MEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
_DEFINE_GUID(FORMAT_VideoInfo, 0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
_DEFINE_GUID(FORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
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
typedef struct tagVIDEOINFOHEADER
{
	RECT            rcSource;          // The bit we really want to use
	RECT            rcTarget;          // Where the video should go
	DWORD           dwBitRate;         // Approximate bit data rate
	DWORD           dwBitErrorRate;    // Bit error rate for this stream
	REFERENCE_TIME  AvgTimePerFrame;   // Average time per frame (100ns units)
	BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER;
#include "emuledlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CFrameGrabThread, CWinThread)

BEGIN_MESSAGE_MAP(CFrameGrabThread, CWinThread)
END_MESSAGE_MAP()

CFrameGrabThread::CFrameGrabThread()
	: imgResults()
	, pOwner()
	, pSender()
	, dStartTime()
	, nMaxWidth()
	, nFramesToGrab()
	, bReduceColor()
{
}

BOOL CFrameGrabThread::InitInstance()
{
	DbgSetThreadName("FrameGrabThread");
	InitThreadLocale();
	return TRUE;
}

BOOL CFrameGrabThread::Run()
{
	imgResults = new CxImage*[nFramesToGrab];
	FrameGrabResult_Struct *result = new FrameGrabResult_Struct;
	(void)CoInitialize(NULL);
	result->nImagesGrabbed = (uint8)GrabFrames();
	CoUninitialize();
	result->imgResults = imgResults;
	result->pSender = pSender;
	if (!theApp.emuledlg->PostMessage(TM_FRAMEGRABFINISHED, (WPARAM)pOwner, (LPARAM)result)) {
		for (int i = (int)result->nImagesGrabbed; --i >= 0;)
			delete result->imgResults[i];
		delete[] result->imgResults;
		delete result;
	}
	return 0;
}

UINT CFrameGrabThread::GrabFrames()
{
#define TIMEBETWEENFRAMES	50.0 // could be a param later, if needed
	for (int i = nFramesToGrab; --i >= 0;)
		imgResults[i] = NULL;
	try {
		HRESULT hr;
		CComPtr<IMediaDet> pDet;
		hr = pDet.CoCreateInstance(__uuidof(MediaDet));
		if (!SUCCEEDED(hr))
			return 0;

		// Convert the file name to a BSTR.
		CComBSTR bstrFilename(strFileName);
		pDet->put_Filename(bstrFilename);

		long lStreams;
		bool bFound = false;
		pDet->get_OutputStreams(&lStreams);
		for (long i = 0; i < lStreams; ++i) {
			GUID major_type;
			pDet->put_CurrentStream(i);
			pDet->get_StreamType(&major_type);
			if (major_type == MEDIATYPE_Video) {
				bFound = true;
				break;
			}
		}

		if (!bFound)
			return 0;

		double dLength = 0;
		pDet->get_StreamLength(&dLength);
		if (dStartTime > dLength)
			dStartTime = 0;

		long width = 0, height = 0;
		AM_MEDIA_TYPE mt;
		hr = pDet->get_StreamMediaType(&mt);
		if (mt.formattype != FORMAT_VideoInfo)
			return 0; // Should not happen, in theory.

		VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)(mt.pbFormat);
		width = pVih->bmiHeader.biWidth;
		height = pVih->bmiHeader.biHeight;

		// We want the absolute height, don't care about orientation.
		if (height < 0)
			height = -height;

		/*FreeMediaType(mt); = */
		if (mt.cbFormat != 0) {
			CoTaskMemFree((PVOID)mt.pbFormat);
			mt.cbFormat = 0;
			mt.pbFormat = NULL;
		}
		if (mt.pUnk != NULL) {
			mt.pUnk->Release();
			mt.pUnk = NULL;
		}
		/**/

		uint32 nFramesGrabbed;
		for (nFramesGrabbed = 0; nFramesGrabbed < nFramesToGrab; ++nFramesGrabbed) {
			long size;
			hr = pDet->GetBitmapBits(dStartTime + (nFramesGrabbed*TIMEBETWEENFRAMES), &size, NULL, width, height);
			if (SUCCEEDED(hr)) {
				// we could also directly create a Bitmap in memory, however this caused problems/failed with *some* movie files
				// when I tried it for the MMPreview, while this method works always - so I'll continue to use this one
				long nFullBufferLen = sizeof(BITMAPFILEHEADER) + size;
				char *buffer = new char[nFullBufferLen];

				BITMAPFILEHEADER bfh = {};
				bfh.bfType = 'MB';
				bfh.bfSize = nFullBufferLen;
				bfh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);
				memcpy(buffer, &bfh, sizeof bfh);

				try {
					hr = pDet->GetBitmapBits(dStartTime + (nFramesGrabbed*TIMEBETWEENFRAMES), NULL, buffer + sizeof bfh, width, height);
				} catch (...) {
					ASSERT(0);
					hr = E_FAIL;
				}
				if (SUCCEEDED(hr)) {
					// decode
					CxImage *imgResult = new CxImage();
					imgResult->Decode((BYTE*)buffer, nFullBufferLen, CXIMAGE_FORMAT_BMP);
					delete[] buffer;
					if (!imgResult->IsValid()) {
						delete imgResult;
						break;
					}

					// resize if needed
					if (nMaxWidth > 0 && nMaxWidth < width) {
						float scale = (float)nMaxWidth / imgResult->GetWidth();
						int nMaxHeigth = (int)(imgResult->GetHeight() * scale);
						imgResult->Resample(nMaxWidth, nMaxHeigth, 0);
					}

					// decrease bpp if needed
					if (bReduceColor) {
						RGBQUAD *ppal = (RGBQUAD*)malloc(256 * sizeof(RGBQUAD));
						if (ppal) {
							CQuantizer q(256, 8);
							q.ProcessImage(imgResult->GetDIB());
							q.SetColorTable(ppal);
							imgResult->DecreaseBpp(8, true, ppal);
							free(ppal);
						}
					}

					//CString TestName;
					//TestName.Format("G:\\testframe%i.png",nFramesGrabbed);
					//imgResult->Save(TestName,CXIMAGE_FORMAT_PNG);
					// done
					imgResults[nFramesGrabbed] = imgResult;
				} else {
					delete[] buffer;
					break;
				}
			}
		}
		return nFramesGrabbed;
	} catch (...) {
		ASSERT(0);
	}
	return 0;
}

void CFrameGrabThread::SetValues(const CKnownFile *in_pOwner, const CString &in_strFileName, uint8 in_nFramesToGrab, double in_dStartTime, bool in_bReduceColor, uint16 in_nMaxWidth, void *in_pSender)
{
	strFileName = in_strFileName;
	nFramesToGrab = in_nFramesToGrab;
	dStartTime = in_dStartTime;
	bReduceColor = in_bReduceColor;
	nMaxWidth = in_nMaxWidth;
	pOwner = in_pOwner;
	pSender = in_pSender;
}