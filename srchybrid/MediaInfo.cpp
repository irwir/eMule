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
#include "stdafx.h"
#include "resource.h"
#include "OtherFunctions.h"
#include "MediaInfo.h"
#include "SafeFile.h"
#include <io.h>
#include <fcntl.h>
#ifdef HAVE_WMSDK_H
#include <wmsdk.h>
#if !defined(HAVE_VISTA_SDK)
static const WCHAR g_wszWMPeakBitrate[] = L"WM/PeakBitrate";
static const WCHAR g_wszWMStreamTypeInfo[] = L"WM/StreamTypeInfo";
typedef struct
{
	GUID	guidMajorType;
	DWORD	cbFormat;
} WM_STREAM_TYPE_INFO;
#endif
#endif//HAVE_WMSDK_H

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CStringStream& CStringStream::operator<<(LPCTSTR psz)
{
	str += psz;
	return *this;
}

CStringStream& CStringStream::operator<<(char *psz)
{
	str += psz;
	return *this;
}

CStringStream& CStringStream::operator<<(UINT uVal)
{
	str.AppendFormat(_T("%u"), uVal);
	return *this;
}

CStringStream& CStringStream::operator<<(int iVal)
{
	str.AppendFormat(_T("%d"), iVal);
	return *this;
}

CStringStream& CStringStream::operator<<(double fVal)
{
	str.AppendFormat(_T("%.3f"), fVal);
	return *this;
}

static const struct
{
	UINT	uFmtTag;
	LPCTSTR	pszDefine;
	LPCTSTR	pszComment;
} s_WavFmtTag[] =
{
// START: Codecs from Windows SDK "mmreg.h" file
	{ 0x0000, _T(""),						_T("Unknown") },
	{ 0x0001, _T("PCM"),					_T("Uncompressed") },
	{ 0x0002, _T("ADPCM"),					_T("") },
	{ 0x0003, _T("IEEE_FLOAT"),				_T("") },
	{ 0x0004, _T("VSELP"),					_T("Compaq Computer Corp.") },
	{ 0x0005, _T("IBM_CVSD"),				_T("") },
	{ 0x0006, _T("ALAW"),					_T("") },
	{ 0x0007, _T("MULAW"),					_T("") },
	{ 0x0008, _T("DTS"),					_T("Digital Theater Systems") },
	{ 0x0009, _T("DRM"),					_T("") },
	{ 0x000A, _T("WMAVOICE9"),				_T("") },
	{ 0x000B, _T("WMAVOICE10"),				_T("") },
	{ 0x0010, _T("OKI_ADPCM"),				_T("") },
	{ 0x0011, _T("DVI_ADPCM"),				_T("Intel Corporation") },
	{ 0x0012, _T("MEDIASPACE_ADPCM"),		_T("Videologic") },
	{ 0x0013, _T("SIERRA_ADPCM"),			_T("") },
	{ 0x0014, _T("G723_ADPCM"),				_T("Antex Electronics Corporation") },
	{ 0x0015, _T("DIGISTD"),				_T("DSP Solutions, Inc.") },
	{ 0x0016, _T("DIGIFIX"),				_T("DSP Solutions, Inc.") },
	{ 0x0017, _T("DIALOGIC_OKI_ADPCM"),		_T("") },
	{ 0x0018, _T("MEDIAVISION_ADPCM"),		_T("") },
	{ 0x0019, _T("CU_CODEC"),				_T("Hewlett-Packard Company") },
	{ 0x0020, _T("YAMAHA_ADPCM"),			_T("") },
	{ 0x0021, _T("SONARC"),					_T("Speech Compression") },
	{ 0x0022, _T("DSPGROUP_TRUESPEECH"),	_T("") },
	{ 0x0023, _T("ECHOSC1"),				_T("Echo Speech Corporation") },
	{ 0x0024, _T("AUDIOFILE_AF36"),			_T("Virtual Music, Inc.") },
	{ 0x0025, _T("APTX"),					_T("Audio Processing Technology") },
	{ 0x0026, _T("AUDIOFILE_AF10"),			_T("Virtual Music, Inc.") },
	{ 0x0027, _T("PROSODY_1612"),			_T("Aculab plc") },
	{ 0x0028, _T("LRC"),					_T("Merging Technologies S.A.") },
	{ 0x0030, _T("DOLBY_AC2"),				_T("") },
	{ 0x0031, _T("GSM610"),					_T("") },
	{ 0x0032, _T("MSNAUDIO"),				_T("") },
	{ 0x0033, _T("ANTEX_ADPCME"),			_T("") },
	{ 0x0034, _T("CONTROL_RES_VQLPC"),		_T("") },
	{ 0x0035, _T("DIGIREAL"),				_T("DSP Solutions, Inc.") },
	{ 0x0036, _T("DIGIADPCM"),				_T("DSP Solutions, Inc.") },
	{ 0x0037, _T("CONTROL_RES_CR10"),		_T("") },
	{ 0x0038, _T("NMS_VBXADPCM"),			_T("Natural MicroSystems") },
	{ 0x0039, _T("CS_IMAADPCM"),			_T("Crystal Semiconductor IMA ADPCM") },
	{ 0x003A, _T("ECHOSC3"),				_T("Echo Speech Corporation") },
	{ 0x003B, _T("ROCKWELL_ADPCM"),			_T("") },
	{ 0x003C, _T("ROCKWELL_DIGITALK"),		_T("") },
	{ 0x003D, _T("XEBEC"),					_T("") },
	{ 0x0040, _T("G721_ADPCM"),				_T("Antex Electronics Corporation") },
	{ 0x0041, _T("G728_CELP"),				_T("Antex Electronics Corporation") },
	{ 0x0042, _T("MSG723"),					_T("") },
	{ 0x0050, _T("MP1"),					_T("MPEG-1, Layer 1") },
	{ 0x0051, _T("MP2"),					_T("MPEG-1, Layer 2") },
	{ 0x0052, _T("RT24"),					_T("InSoft, Inc.") },
	{ 0x0053, _T("PAC"),					_T("InSoft, Inc.") },
	{ 0x0055, _T("MP3"),					_T("MPEG-1, Layer 3") },
	{ 0x0059, _T("LUCENT_G723"),			_T("") },
	{ 0x0060, _T("CIRRUS"),					_T("") },
	{ 0x0061, _T("ESPCM"),					_T("ESS Technology") },
	{ 0x0062, _T("VOXWARE"),				_T("") },
	{ 0x0063, _T("CANOPUS_ATRAC"),			_T("") },
	{ 0x0064, _T("G726_ADPCM"),				_T("APICOM") },
	{ 0x0065, _T("G722_ADPCM"),				_T("APICOM") },
	{ 0x0067, _T("DSAT_DISPLAY"),			_T("") },
	{ 0x0069, _T("VOXWARE_BYTE_ALIGNED"),	_T("") },
	{ 0x0070, _T("VOXWARE_AC8"),			_T("") },
	{ 0x0071, _T("VOXWARE_AC10"),			_T("") },
	{ 0x0072, _T("VOXWARE_AC16"),			_T("") },
	{ 0x0073, _T("VOXWARE_AC20"),			_T("") },
	{ 0x0074, _T("VOXWARE_RT24"),			_T("") },
	{ 0x0075, _T("VOXWARE_RT29"),			_T("") },
	{ 0x0076, _T("VOXWARE_RT29HW"),			_T("") },
	{ 0x0077, _T("VOXWARE_VR12"),			_T("") },
	{ 0x0078, _T("VOXWARE_VR18"),			_T("") },
	{ 0x0079, _T("VOXWARE_TQ40"),			_T("") },
	{ 0x0080, _T("SOFTSOUND"),				_T("") },
	{ 0x0081, _T("VOXWARE_TQ60"),			_T("") },
	{ 0x0082, _T("MSRT24"),					_T("") },
	{ 0x0083, _T("G729A"),					_T("AT&T Labs, Inc.") },
	{ 0x0084, _T("MVI_MVI2"),				_T("Motion Pixels") },
	{ 0x0085, _T("DF_G726"),				_T("DataFusion Systems (Pty) (Ltd)") },
	{ 0x0086, _T("DF_GSM610"),				_T("DataFusion Systems (Pty) (Ltd)") },
	{ 0x0088, _T("ISIAUDIO"),				_T("Iterated Systems, Inc.") },
	{ 0x0089, _T("ONLIVE"),					_T("") },
	{ 0x0091, _T("SBC24"),					_T("Siemens Business Communications Sys") },
	{ 0x0092, _T("DOLBY_AC3_SPDIF"),		_T("Sonic Foundry") },
	{ 0x0093, _T("MEDIASONIC_G723"),		_T("") },
	{ 0x0094, _T("PROSODY_8KBPS"),			_T("Aculab plc") },
	{ 0x0097, _T("ZYXEL_ADPCM"),			_T("") },
	{ 0x0098, _T("PHILIPS_LPCBB"),			_T("") },
	{ 0x0099, _T("PACKED"),					_T("Studer Professional Audio AG") },
	{ 0x00A0, _T("MALDEN_PHONYTALK"),		_T("") },
	{ 0x0100, _T("RHETOREX_ADPCM"),			_T("") },
	{ 0x0101, _T("IRAT"),					_T("BeCubed Software Inc.") },
	{ 0x0111, _T("VIVO_G723"),				_T("") },
	{ 0x0112, _T("VIVO_SIREN"),				_T("") },
	{ 0x0123, _T("DIGITAL_G723"),			_T("Digital Equipment Corporation") },
	{ 0x0125, _T("SANYO_LD_ADPCM"),			_T("") },
	{ 0x0130, _T("SIPROLAB_ACEPLNET"),		_T("") },
	{ 0x0130, _T("SIPR"),					_T("Real Audio 4 (Sipro)") },
	{ 0x0131, _T("SIPROLAB_ACELP4800"),		_T("") },
	{ 0x0132, _T("SIPROLAB_ACELP8V3"),		_T("") },
	{ 0x0133, _T("SIPROLAB_G729"),			_T("") },
	{ 0x0134, _T("SIPROLAB_G729A"),			_T("") },
	{ 0x0135, _T("SIPROLAB_KELVIN"),		_T("") },
	{ 0x0140, _T("G726ADPCM"),				_T("Dictaphone Corporation") },
	{ 0x0150, _T("QUALCOMM_PUREVOICE"),		_T("") },
	{ 0x0151, _T("QUALCOMM_HALFRATE"),		_T("") },
	{ 0x0155, _T("TUBGSM"),					_T("Ring Zero Systems, Inc.") },
	{ 0x0160, _T("MSAUDIO1"),				_T("Microsoft Audio") },
	{ 0x0161, _T("WMAUDIO2"),				_T("Windows Media Audio") },
	{ 0x0162, _T("WMAUDIO3"),				_T("Windows Media Audio 9 Pro") },
	{ 0x0163, _T("WMAUDIO_LOSSLESS"),		_T("Windows Media Audio 9 Lossless") },
	{ 0x0164, _T("WMASPDIF"),				_T("Windows Media Audio Pro-over-S/PDIF") },
	{ 0x0170, _T("UNISYS_NAP_ADPCM"),		_T("") },
	{ 0x0171, _T("UNISYS_NAP_ULAW"),		_T("") },
	{ 0x0172, _T("UNISYS_NAP_ALAW"),		_T("") },
	{ 0x0173, _T("UNISYS_NAP_16K"),			_T("") },
	{ 0x0200, _T("CREATIVE_ADPCM"),			_T("") },
	{ 0x0202, _T("CREATIVE_FASTSPEECH8"),	_T("") },
	{ 0x0203, _T("CREATIVE_FASTSPEECH10"),	_T("") },
	{ 0x0210, _T("UHER_ADPCM"),				_T("") },
	{ 0x0220, _T("QUARTERDECK"),			_T("") },
	{ 0x0230, _T("ILINK_VC"),				_T("I-link Worldwide") },
	{ 0x0240, _T("RAW_SPORT"),				_T("Aureal Semiconductor") },
	{ 0x0241, _T("ESST_AC3"),				_T("ESS Technology, Inc.") },
	{ 0x0250, _T("IPI_HSX"),				_T("Interactive Products, Inc.") },
	{ 0x0251, _T("IPI_RPELP"),				_T("Interactive Products, Inc.") },
	{ 0x0260, _T("CS2"),					_T("Consistent Software") },
	{ 0x0270, _T("SONY_SCX"),				_T("") },
	{ 0x0300, _T("FM_TOWNS_SND"),			_T("Fujitsu Corp.") },
	{ 0x0400, _T("BTV_DIGITAL"),			_T("Brooktree Corporation") },
	{ 0x0401, _T("IMC"),					_T("Intel Music Coder for MSACM") },
	{ 0x0450, _T("QDESIGN_MUSIC"),			_T("") },
	{ 0x0680, _T("VME_VMPCM"),				_T("AT&T Labs, Inc.") },
	{ 0x0681, _T("TPC"),					_T("AT&T Labs, Inc.") },
	{ 0x1000, _T("OLIGSM"),					_T("Olivetti") },
	{ 0x1001, _T("OLIADPCM"),				_T("Olivetti") },
	{ 0x1002, _T("OLICELP"),				_T("Olivetti") },
	{ 0x1003, _T("OLISBC"),					_T("Olivetti") },
	{ 0x1004, _T("OLIOPR"),					_T("Olivetti") },
	{ 0x1100, _T("LH_CODEC"),				_T("Lernout & Hauspie") },
	{ 0x1400, _T("NORRIS"),					_T("") },
	{ 0x1500, _T("SOUNDSPACE_MUSICOMPRESS"),_T("AT&T Labs, Inc.") },
	{ 0x1600, _T("MPEG_ADTS_AAC"),			_T("") },
	{ 0x1601, _T("MPEG_RAW_AAC"),			_T("") },
	{ 0x1608, _T("NOKIA_MPEG_ADTS_AAC"),	_T("") },
	{ 0x1609, _T("NOKIA_MPEG_RAW_AAC"),		_T("") },
	{ 0x160A, _T("VODAFONE_MPEG_ADTS_AAC"),	_T("") },
	{ 0x160B, _T("VODAFONE_MPEG_RAW_AAC"),	_T("") },
	{ 0x2000, _T("AC3"),					_T("Dolby AC3") },
// END: Codecs from Windows SDK "mmreg.h" file

	{ 0x2001, _T("DTS"),					_T("Digital Theater Systems") },

// Real Audio (Baked) codecs
	{ 0x2002, _T("RA14"),					_T("RealAudio 1/2 14.4") },
	{ 0x2003, _T("RA28"),					_T("RealAudio 1/2 28.8") },
	{ 0x2004, _T("COOK"),					_T("RealAudio G2/8 Cook (Low Bitrate)") },
	{ 0x2005, _T("DNET"),					_T("RealAudio 3/4/5 Music (DNET)") },
	{ 0x2006, _T("RAAC"),					_T("RealAudio 10 AAC (RAAC)") },
	{ 0x2007, _T("RACP"),					_T("RealAudio 10 AAC+ (RACP)") }
};

CString GetAudioFormatName(WORD wFormatTag, CString &rstrComment)
{
	for (unsigned i = 0; i < _countof(s_WavFmtTag); ++i) {
		if (s_WavFmtTag[i].uFmtTag == wFormatTag) {
			rstrComment = s_WavFmtTag[i].pszComment;
			return CString(s_WavFmtTag[i].pszDefine);
		}
	}

	CString strCompression;
	strCompression.Format(_T("0x%04x (Unknown)"), wFormatTag);
	return strCompression;
}

CString GetAudioFormatName(WORD wFormatTag)
{
	CString strComment;
	CString strFormat = GetAudioFormatName(wFormatTag, strComment);
	if (!strComment.IsEmpty())
		strFormat.AppendFormat(_T(" (%s)"), (LPCTSTR)strComment);
	return strFormat;
}

CString GetAudioFormatCodecId(WORD wFormatTag)
{
	for (unsigned i = 0; i < _countof(s_WavFmtTag); ++i)
		if (s_WavFmtTag[i].uFmtTag == wFormatTag)
			return s_WavFmtTag[i].pszDefine;

	return CString();
}

CString GetAudioFormatDisplayName(const CString &strCodecId)
{
	CString sFmt;
	for (unsigned i = 0; i < _countof(s_WavFmtTag); ++i)
		if (_tcsicmp(s_WavFmtTag[i].pszDefine, strCodecId) == 0) {
			if (s_WavFmtTag[i].uFmtTag && *s_WavFmtTag[i].pszDefine && *s_WavFmtTag[i].pszComment)
				sFmt.Format(_T("%s (%s)"), s_WavFmtTag[i].pszDefine, s_WavFmtTag[i].pszComment);
			break;
		}

	return sFmt;
}

bool IsEqualFOURCC(FOURCC fccA, FOURCC fccB)
{
	for (int i = 0; i < 4; ++i) {
		if (tolower((unsigned char)fccA) != tolower((unsigned char)fccB))
			return false;
		fccA >>= 8;
		fccB >>= 8;
	}
	return true;
}

CString GetVideoFormatDisplayName(DWORD biCompression)
{
	LPCSTR pFormat;
	switch (biCompression) {
	case BI_RGB:
		pFormat = "RGB (Uncompressed)";
		break;
	case BI_RLE8:
		pFormat = "RLE8 (Run Length Encoded 8-bit)";
		break;
	case BI_RLE4:
		pFormat = "RLE4 (Run Length Encoded 4-bit)";
		break;
	case BI_BITFIELDS:
		pFormat = "Bitfields";
		break;
	case BI_JPEG:
		pFormat = "JPEG";
		break;
	case BI_PNG:
		pFormat = "PNG";
		break;
	default:
		{
			unsigned char *cp = (unsigned char*)&biCompression;
			DWORD fcc;
			for (int i = 0; i < sizeof(DWORD); ++i)
				((byte*)&fcc)[i] = (byte)toupper(*cp++);

			switch (fcc) {
			case MAKEFOURCC('D', 'I', 'V', '3'):
				pFormat = " (DivX ;-) MPEG-4 v3 Low)";
				break;
			case MAKEFOURCC('D', 'I', 'V', '4'):
				pFormat = " (DivX ;-) MPEG-4 v3 Fast)";
				break;
			case MAKEFOURCC('D', 'I', 'V', 'X'):
				pFormat = " (DivX 4)";
				break;
			case MAKEFOURCC('D', 'X', '5', '0'):
				pFormat = " (DivX 5)";
				break;
			case MAKEFOURCC('M', 'P', 'G', '4'):
				pFormat = " (Microsoft MPEG-4 v1)";
				break;
			case MAKEFOURCC('M', 'P', '4', '2'):
				pFormat = " (Microsoft MPEG-4 v2)";
				break;
			case MAKEFOURCC('M', 'P', '4', '3'):
				pFormat = " (Microsoft MPEG-4 v3)";
				break;
			case MAKEFOURCC('D', 'X', 'S', 'B'):
				pFormat = " (Subtitle)";
				break;
			case MAKEFOURCC('W', 'M', 'V', '1'):
				pFormat = " (Windows Media Video 7)";
				break;
			case MAKEFOURCC('W', 'M', 'V', '2'):
				pFormat = " (Windows Media Video 8)";
				break;
			case MAKEFOURCC('W', 'M', 'V', '3'):
				pFormat = " (Windows Media Video 9)";
				break;
			case MAKEFOURCC('W', 'M', 'V', 'A'):
				pFormat = " (Windows Media Video 9 Advanced Profile)";
				break;
			case MAKEFOURCC('R', 'V', '1', '0'):
			case MAKEFOURCC('R', 'V', '1', '3'):
				pFormat = " (Real Video 5)";
				break;
			case MAKEFOURCC('R', 'V', '2', '0'):
				pFormat = " (Real Video G2)";
				break;
			case MAKEFOURCC('R', 'V', '3', '0'):
				pFormat = " (Real Video 8)";
				break;
			case MAKEFOURCC('R', 'V', '4', '0'):
				pFormat = " (Real Video 9)";
				break;
			case MAKEFOURCC('H', '2', '6', '4'):
				pFormat = " (MPEG-4 AVC)";
				break;
			case MAKEFOURCC('X', '2', '6', '4'):
				pFormat = " (x264 MPEG-4 AVC)";
				break;
			case MAKEFOURCC('X', 'V', 'I', 'D'):
				pFormat = " (Xvid MPEG-4)";
				break;
			case MAKEFOURCC('T', 'S', 'C', 'C'):
				pFormat = " (TechSmith Screen Capture)";
				break;
			case MAKEFOURCC('M', 'J', 'P', 'G'):
				pFormat = " (M-JPEG)";
				break;
			case MAKEFOURCC('I', 'V', '3', '2'):
				pFormat = " (Intel Indeo Video 3.2)";
				break;
			case MAKEFOURCC('I', 'V', '4', '0'):
				pFormat = " (Intel Indeo Video 4.0)";
				break;
			case MAKEFOURCC('I', 'V', '5', '0'):
				pFormat = " (Intel Indeo Video 5.0)";
				break;
			case MAKEFOURCC('F', 'M', 'P', '4'):
				pFormat = " (MPEG-4)";
				break;
			case MAKEFOURCC('C', 'V', 'I', 'D'):
				pFormat = " (Cinepack)";
				break;
			case MAKEFOURCC('C', 'R', 'A', 'M'):
				pFormat = " (Microsoft Video 1)";
				break;
			default:
				return CString();
			}
			return CString(CStringA((LPCSTR)&biCompression, 4) + pFormat);
		}
	}
	return CString(pFormat);
}

CString GetVideoFormatName(DWORD biCompression)
{
	CString strFormat(GetVideoFormatDisplayName(biCompression));
	if (strFormat.IsEmpty()) {
		char szFourcc[5];
		*(LPDWORD)szFourcc = biCompression;
		szFourcc[4] = '\0';
		strFormat = szFourcc;
		strFormat.MakeUpper();
	}
	return strFormat;
}

CString GetKnownAspectRatioDisplayString(float fAspectRatio)
{
	LPCTSTR pAspectRatio;
	if (fAspectRatio >= 1.00F && fAspectRatio < 1.50F)
		pAspectRatio = _T("4/3");
	else if (fAspectRatio >= 1.50F && fAspectRatio < 2.00F)
		pAspectRatio = _T("16/9");
	else if (fAspectRatio >= 2.00F && fAspectRatio < 2.22F)
		pAspectRatio = _T("2.2");
	else if (fAspectRatio >= 2.22F && fAspectRatio < 2.30F)
		pAspectRatio = _T("2.25");
	else if (fAspectRatio >= 2.30F && fAspectRatio < 2.50F)
		pAspectRatio = _T("2.35");
	else
		pAspectRatio = _T("");
	return CString(pAspectRatio);
}

CString GetCodecDisplayName(const CString &strCodecId)
{
	static CMapStringToString s_mapCodecDisplayName;
	CString strCodecDisplayName;
	if (s_mapCodecDisplayName.Lookup(strCodecId, strCodecDisplayName))
		return strCodecDisplayName;

	if (strCodecId.GetLength() == 3 || strCodecId.GetLength() == 4) {
		bool bHaveFourCC = true;
		FOURCC fcc;
		if (strCodecId == _T("rgb"))
			fcc = BI_RGB;
		else if (strCodecId == _T("rle8"))
			fcc = BI_RLE8;
		else if (strCodecId == _T("rle4"))
			fcc = BI_RLE4;
		else if (strCodecId == _T("jpeg"))
			fcc = BI_JPEG;
		else if (strCodecId == _T("png"))
			fcc = BI_PNG;
		else {
			fcc = MAKEFOURCC(' ', ' ', ' ', ' ');
			LPSTR pcFourCC = (LPSTR)&fcc;
			for (int i = 0; i < strCodecId.GetLength(); ++i) {
				WCHAR wch = strCodecId[i];
				if (wch >= 0x100 || (!__iscsym((unsigned char)wch) && wch != L'.' && wch != L' ')) {
					bHaveFourCC = false;
					break;
				}
				pcFourCC[i] = (CHAR)toupper((unsigned char)wch);
			}
		}
		if (bHaveFourCC)
			strCodecDisplayName = GetVideoFormatDisplayName(fcc);
	}

	if (strCodecDisplayName.IsEmpty())
		strCodecDisplayName = GetAudioFormatDisplayName(strCodecId);
	if (strCodecDisplayName.IsEmpty()) {
		strCodecDisplayName = strCodecId;
		strCodecDisplayName.MakeUpper();
	}
	s_mapCodecDisplayName.SetAt(strCodecId, strCodecDisplayName);
	return strCodecDisplayName;
}

typedef struct
{
	SHORT	left;
	SHORT	top;
	SHORT	right;
	SHORT	bottom;
} RECT16;

typedef struct
{
	FOURCC		fccType;
	FOURCC		fccHandler;
	DWORD		dwFlags;
	WORD		wPriority;
	WORD		wLanguage;
	DWORD		dwInitialFrames;
	DWORD		dwScale;
	DWORD		dwRate;
	DWORD		dwStart;
	DWORD		dwLength;
	DWORD		dwSuggestedBufferSize;
	DWORD		dwQuality;
	DWORD		dwSampleSize;
	RECT16		rcFrame;
} AVIStreamHeader_fixed;

#ifndef AVIFILEINFO_NOPADDING
#define AVIFILEINFO_NOPADDING	0x0400 // from the SDK tool "RIFFWALK.EXE"
#endif

#ifndef AVIFILEINFO_TRUSTCKTYPE
#define AVIFILEINFO_TRUSTCKTYPE	0x0800 // from DirectX SDK "Types of DV AVI Files"
#endif

typedef struct
{
	AVIStreamHeader_fixed	hdr;
	DWORD					dwFormatLen;
	union
	{
		BITMAPINFOHEADER	*bmi;
		PCMWAVEFORMAT		*wav;
		LPBYTE				dat;
	} fmt;
	char					*nam;
} STREAMHEADER;

static bool ReadChunkHeader(int fd, FOURCC *pfccType, DWORD *pdwLength)
{
	return _read(fd, pfccType, sizeof * pfccType) == sizeof *pfccType
		&& _read(fd, pdwLength, sizeof * pdwLength) == sizeof *pdwLength;
}

static bool ParseStreamHeader(int hAviFile, DWORD dwLengthLeft, STREAMHEADER *pStrmHdr)
{
	FOURCC fccType;
	DWORD dwLength;
	while (dwLengthLeft >= sizeof(DWORD) * 2) {
		if (!ReadChunkHeader(hAviFile, &fccType, &dwLength))
			return false;

		dwLengthLeft -= sizeof(DWORD) * 2;
		if (dwLength > dwLengthLeft) {
			errno = 0;
			return false;
		}
		dwLengthLeft -= dwLength + (dwLength & 1);

		switch (fccType) {
		case ckidSTREAMHEADER:
			if (dwLength < sizeof pStrmHdr->hdr) {
				memset(&pStrmHdr->hdr, 0x00, sizeof pStrmHdr->hdr);
				if (_read(hAviFile, &pStrmHdr->hdr, dwLength) != (int)dwLength)
					return false;
				if (dwLength & 1 && _lseek(hAviFile, 1, SEEK_CUR) == -1)
					return false;
			} else {
				if (_read(hAviFile, &pStrmHdr->hdr, sizeof pStrmHdr->hdr) != sizeof pStrmHdr->hdr)
					return false;
				if (_lseek(hAviFile, dwLength + (dwLength & 1) - sizeof pStrmHdr->hdr, SEEK_CUR) == -1)
					return false;
			}
			dwLength = 0;
			break;

		case ckidSTREAMFORMAT:
			if (dwLength > 4096) // expect corrupt data
				return false;
			try {
				pStrmHdr->dwFormatLen = dwLength;
				pStrmHdr->fmt.dat = new BYTE[dwLength];
			} catch (...) {
				errno = ENOMEM;
				return false;
			}
			if (_read(hAviFile, pStrmHdr->fmt.dat, dwLength) != (int)dwLength)
				return false;
			if (dwLength & 1) {
				if (_lseek(hAviFile, 1, SEEK_CUR) == -1)
					return false;
			}
			dwLength = 0;
			break;
		case ckidSTREAMNAME:
			if (dwLength > 512) // expect corrupt data
				return false;
			try {
				pStrmHdr->nam = new char[dwLength + 1];
			} catch (...) {
				errno = ENOMEM;
				return false;
			}
			if (_read(hAviFile, pStrmHdr->nam, dwLength) != (int)dwLength)
				return false;
			pStrmHdr->nam[dwLength] = '\0';
			if (dwLength & 1 && _lseek(hAviFile, 1, SEEK_CUR) == -1)
				return false;

			dwLength = 0;
		}

		if (dwLength && _lseek(hAviFile, dwLength + (dwLength & 1), SEEK_CUR) == -1)
			return false;
	}

	return !(dwLengthLeft && _lseek(hAviFile, dwLengthLeft, SEEK_CUR) == -1);
}

bool GetRIFFHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsAVI, bool bFullInfo)
{
	ASSERT(!bFullInfo || mi->strInfo.m_hWnd != NULL);

	// Open AVI file
	int hAviFile = _topen(pszFileName, O_RDONLY | O_BINARY);
	if (hAviFile == -1)
		return false;

	DWORD dwLengthLeft;
	FOURCC fccType;
	DWORD dwLength;
	bool bSizeInvalid = false;
	int iStream = 0;
	DWORD dwMovieChunkSize = 0;
	DWORD uVideoFrames = 0;
	int	iNonAVStreams = 0;
	DWORD dwAllNonVideoAvgBytesPerSec = 0;

	bool bResult = false;

	//
	// Read 'RIFF' header
	//
	if (!ReadChunkHeader(hAviFile, &fccType, &dwLength))
		goto cleanup;
	if (fccType != FOURCC_RIFF)
		goto cleanup;
	if (dwLength < sizeof(DWORD)) {
		dwLength = 0xFFFFFFF0;
		bSizeInvalid = true;
	}
	dwLengthLeft = dwLength -= sizeof(DWORD);

	//
	// Read 'AVI ' or 'WAVE' header
	//
	FOURCC fccMain;
	if (_read(hAviFile, &fccMain, sizeof fccMain) != sizeof fccMain)
		goto cleanup;
	rbIsAVI = (fccMain == formtypeAVI);
	if (!rbIsAVI && fccMain != mmioFOURCC('W', 'A', 'V', 'E'))
		goto cleanup;

	// We need to read almost all streams (regardless of 'bFullInfo' mode) because we need to get the 'dwMovieChunkSize'
	bool bHaveReadAllStreams = false;
	while (!bHaveReadAllStreams && dwLengthLeft >= sizeof(DWORD) * 2) {
		if (!ReadChunkHeader(hAviFile, &fccType, &dwLength))
			goto inv_format_errno;
		if (fccType == 0 && dwLength == 0) {
			// We jumped right into a gap which is (still) filled with 0-bytes. If we
			// continue reading this until EOF we throw an error although we may have
			// already read valid data.
			if (mi->iVideoStreams > 0 || mi->iAudioStreams > 0)
				break; // already have valid data
			goto cleanup;
		}

		bool bInvalidLength = false;
		if (!bSizeInvalid) {
			dwLengthLeft -= sizeof(DWORD) * 2;
			if (dwLength > dwLengthLeft) {
				if (fccType != FOURCC_LIST)
					goto cleanup;
				bInvalidLength = true;
			}
			dwLengthLeft -= (dwLength + (dwLength & 1));
			if (dwLengthLeft == _UI32_MAX)
				dwLengthLeft = 0;
		}

		switch (fccType) {
		case FOURCC_LIST:
			if (_read(hAviFile, &fccType, sizeof fccType) != sizeof fccType)
				goto inv_format_errno;
			if (fccType != listtypeAVIHEADER && bInvalidLength)
				goto inv_format;

			// Some Premiere plugin is writing AVI files with an invalid size field in the LIST/hdrl chunk.
			if (dwLength < sizeof(DWORD) && fccType != listtypeAVIHEADER && (fccType != listtypeAVIMOVIE || !bSizeInvalid))
				goto inv_format;
			dwLength -= sizeof(DWORD);

			switch (fccType) {
			case listtypeAVIHEADER:
				dwLengthLeft += (dwLength + (dwLength & 1)) + 4;
				dwLength = 0;	// silently enter the header block
				break;
			case listtypeSTREAMHEADER:
				{
					bool bStreamRes;
					STREAMHEADER strmhdr = {};
					if ((bStreamRes = ParseStreamHeader(hAviFile, dwLength, &strmhdr)) != false) {
						double fSamplesSec = (strmhdr.hdr.dwScale != 0) ? strmhdr.hdr.dwRate / (double)strmhdr.hdr.dwScale : 0.0;
						double fLength = (fSamplesSec != 0.0) ? strmhdr.hdr.dwLength / (double)fSamplesSec : 0.0;
						if (strmhdr.hdr.fccType == streamtypeAUDIO) {
							mi->iAudioStreams++;
							if (mi->iAudioStreams == 1) {
								mi->fAudioLengthSec = fLength;
								if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav) {
									*(PCMWAVEFORMAT*)&mi->audio = *strmhdr.fmt.wav;
									mi->strAudioFormat = GetAudioFormatName(strmhdr.fmt.wav->wf.wFormatTag);
								}
							} else {
								// this works only for CBR
								//
								// TODO: Determine VBR audio...
								if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav)
									dwAllNonVideoAvgBytesPerSec += strmhdr.fmt.wav->wf.nAvgBytesPerSec;

								if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams;
									if (strmhdr.nam && strmhdr.nam[0] != '\0')
										mi->strInfo << _T(": \"") << strmhdr.nam << _T("\"");
									mi->strInfo << _T("\n");
									if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav) {
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetAudioFormatName(strmhdr.fmt.wav->wf.wFormatTag) << _T("\n");

										if (strmhdr.fmt.wav->wf.nAvgBytesPerSec) {
											CString strBitrate;
											if (strmhdr.fmt.wav->wf.nAvgBytesPerSec == _UI32_MAX)
												strBitrate = _T("Variable");
											else
												strBitrate.Format(_T("%u %s"), (UINT)((strmhdr.fmt.wav->wf.nAvgBytesPerSec * 8 + 500) / 1000), (LPCTSTR)GetResString(IDS_KBITSSEC));
											mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << strBitrate << _T("\n");
										}

										if (strmhdr.fmt.wav->wf.nChannels) {
											mi->strInfo << _T("   ") << GetResString(IDS_CHANNELS) << _T(":\t");
											if (strmhdr.fmt.wav->wf.nChannels == 1)
												mi->strInfo << _T("1 (Mono)");
											else if (strmhdr.fmt.wav->wf.nChannels == 2)
												mi->strInfo << _T("2 (Stereo)");
											else if (strmhdr.fmt.wav->wf.nChannels == 5)
												mi->strInfo << _T("5.1 (Surround)");
											else
												mi->strInfo << strmhdr.fmt.wav->wf.nChannels;
											mi->strInfo << _T("\n");
										}

										if (strmhdr.fmt.wav->wf.nSamplesPerSec)
											mi->strInfo << _T("   ") << GetResString(IDS_SAMPLERATE) << _T(":\t") << strmhdr.fmt.wav->wf.nSamplesPerSec / 1000.0 << _T(" kHz\n");

										if (strmhdr.fmt.wav->wBitsPerSample)
											mi->strInfo << _T("   Bit/sample:\t") << strmhdr.fmt.wav->wBitsPerSample << _T(" Bit\n");
									}
									if (fLength) {
										CString strLength;
										SecToTimeLength((ULONG)fLength, strLength);
										mi->strInfo << _T("   ") << GetResString(IDS_LENGTH) << _T(":\t") << strLength << _T("\n");
									}
								}
							}
						} else if (strmhdr.hdr.fccType == streamtypeVIDEO) {
							mi->iVideoStreams++;
							if (mi->iVideoStreams == 1) {
								uVideoFrames = strmhdr.hdr.dwLength;
								mi->fVideoLengthSec = fLength;
								mi->fVideoFrameRate = fSamplesSec;
								if (strmhdr.fmt.bmi && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.bmi) {
									mi->video.bmiHeader = *strmhdr.fmt.bmi;
									mi->strVideoFormat = GetVideoFormatName(strmhdr.fmt.bmi->biCompression);
									if (strmhdr.fmt.bmi->biWidth && strmhdr.fmt.bmi->biHeight)
										mi->fVideoAspectRatio = abs(strmhdr.fmt.bmi->biWidth / (double)strmhdr.fmt.bmi->biHeight);
								}
							} else {
								if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams;
									if (strmhdr.nam && strmhdr.nam[0] != '\0')
										mi->strInfo << _T(": \"") << strmhdr.nam << _T("\"");
									mi->strInfo << _T("\n");
									if (strmhdr.fmt.bmi && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.bmi) {
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetVideoFormatName(strmhdr.fmt.bmi->biCompression) << _T("\n");
										if (strmhdr.fmt.bmi->biWidth && strmhdr.fmt.bmi->biHeight) {
											mi->strInfo << _T("   ") << GetResString(IDS_WIDTH) << _T(" x ") << GetResString(IDS_HEIGHT) << _T(":\t") << abs(strmhdr.fmt.bmi->biWidth) << _T(" x ") << abs(strmhdr.fmt.bmi->biHeight) << _T("\n");
											float fAspectRatio = abs(strmhdr.fmt.bmi->biWidth / (float)strmhdr.fmt.bmi->biHeight);
											mi->strInfo << _T("   ") << GetResString(IDS_ASPECTRATIO) << _T(":\t") << fAspectRatio << _T("  (") << GetKnownAspectRatioDisplayString(fAspectRatio) << _T(")\n");
										}
									}
									if (fLength) {
										CString strLength;
										SecToTimeLength((ULONG)fLength, strLength);
										mi->strInfo << _T("   ") << GetResString(IDS_LENGTH) << _T(":\t") << strLength << _T("\n");
									}
								}
							}
						} else {
							++iNonAVStreams;
							if (bFullInfo && mi->strInfo.m_hWnd) {
								if (!mi->strInfo.IsEmpty())
									mi->strInfo << _T("\n");
								mi->OutputFileName();
								mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
								mi->strInfo << _T("Unknown Stream #") << iStream;
								if (strmhdr.nam && strmhdr.nam[0] != '\0')
									mi->strInfo << _T(": \"") << strmhdr.nam << _T("\"");
								mi->strInfo << _T("\n");
								if (fLength) {
									CString strLength;
									SecToTimeLength((ULONG)fLength, strLength);
									mi->strInfo << _T("   ") << GetResString(IDS_LENGTH) << _T(":\t") << strLength << _T("\n");
								}
							}
						}
					}
					delete[] strmhdr.fmt.dat;
					delete[] strmhdr.nam;
					if (!bStreamRes)
						goto inv_format_errno;
					++iStream;

					dwLength = 0;
				}
				break;
			case listtypeAVIMOVIE:
				dwMovieChunkSize = dwLength;
				if (!bFullInfo)
					bHaveReadAllStreams = true;
				break;
			case mmioFOURCC('I', 'N', 'F', 'O'):
				{
					if (dwLength < 0x10000) {
						bool bError = false;
						BYTE *pChunk = new BYTE[dwLength];
						if ((DWORD)_read(hAviFile, pChunk, dwLength) == dwLength) {
							CSafeMemFile ck(pChunk, dwLength);
							try {
								if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");
								}

								CString strValue;
								while (ck.GetPosition() < ck.GetLength()) {
									FOURCC ckId = ck.ReadUInt32();
									DWORD ckLen = ck.ReadUInt32();
									if (ckLen < 512) {
										CStringA strValueA;
										ck.Read(strValueA.GetBuffer(ckLen), ckLen);
										strValueA.ReleaseBuffer(ckLen);
										strValue = strValueA;
										strValue.Trim();
									} else {
										ck.Seek(ckLen, CFile::current);
										strValue.Empty();
									}
									strValue.Replace(_T('\r'), _T(' '));
									strValue.Replace(_T('\n'), _T(' '));
									switch (ckId) {
									case mmioFOURCC('I', 'N', 'A', 'M'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_TITLE) << _T(":\t") << strValue << _T("\n");
										mi->strTitle = strValue;
										break;
									case mmioFOURCC('I', 'S', 'B', 'J'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << _T("Subject") << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'A', 'R', 'T'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_AUTHOR) << _T(":\t") << strValue << _T("\n");
										mi->strAuthor = strValue;
										break;
									case mmioFOURCC('I', 'C', 'O', 'P'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << _T("Copyright") << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'C', 'M', 'T'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_COMMENT) << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'C', 'R', 'D'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_DATE) << _T(":\t") << strValue << _T("\n");
										break;
									case mmioFOURCC('I', 'S', 'F', 'T'):
										if (bFullInfo && mi->strInfo.m_hWnd && !strValue.IsEmpty())
											mi->strInfo << _T("   ") << _T("Software") << _T(":\t") << strValue << _T("\n");
										break;
									}

									if (ckLen & 1)
										ck.Seek(1, CFile::current);
								}
							} catch (CException *ex) {
								ex->Delete();
								bError = true;
							}
						} else
							bError = true;

						delete[] pChunk;

						if (bError)
							bHaveReadAllStreams = true;
						else {
							if (dwLength & 1) {
								if (_lseek(hAviFile, 1, SEEK_CUR) == -1)
									bHaveReadAllStreams = true;
							}
							dwLength = 0;
						}
					}
				}
			}
			break;
		case ckidAVIMAINHDR:
			if (dwLength == sizeof(MainAVIHeader)) {
				MainAVIHeader avihdr;
				if (_read(hAviFile, &avihdr, sizeof avihdr) != sizeof avihdr)
					goto inv_format_errno;
				if (dwLength & 1)
					if (_lseek(hAviFile, 1, SEEK_CUR) == -1)
						goto inv_format_errno;

				dwLength = 0;
			}
			break;
		case ckidAVINEWINDEX:	// idx1
			if (!bFullInfo)
				bHaveReadAllStreams = true;
			break;
		case mmioFOURCC('f', 'm', 't', ' '):
			if (fccMain == mmioFOURCC('W', 'A', 'V', 'E')) {
				STREAMHEADER strmhdr = {};
				if (dwLength > 4096) // expect corrupt data
					goto inv_format;
				try {
					strmhdr.fmt.dat = new BYTE[strmhdr.dwFormatLen = dwLength];
				} catch (...) {
					errno = ENOMEM;
					goto inv_format_errno;
				}
				if (_read(hAviFile, strmhdr.fmt.dat, dwLength) != (int)dwLength)
					goto inv_format_errno;
				if (dwLength & 1) {
					if (_lseek(hAviFile, 1, SEEK_CUR) == -1)
						goto inv_format_errno;
				}
				dwLength = 0;

				strmhdr.hdr.fccType = streamtypeAUDIO;
				if (strmhdr.dwFormatLen) {
					mi->iAudioStreams++;
					if (mi->iAudioStreams == 1) {
						if (strmhdr.fmt.wav && strmhdr.dwFormatLen >= sizeof *strmhdr.fmt.wav) {
							*(PCMWAVEFORMAT*)&mi->audio = *strmhdr.fmt.wav;
							mi->strAudioFormat = GetAudioFormatName(strmhdr.fmt.wav->wf.wFormatTag);
						}
					}
				}
				delete[] strmhdr.fmt.dat;
				delete[] strmhdr.nam;
				++iStream;
			}
			break;
		case mmioFOURCC('d', 'a', 't', 'a'):
			if (fccMain == mmioFOURCC('W', 'A', 'V', 'E')) {
				if (mi->iAudioStreams == 1 && mi->iVideoStreams == 0 && mi->audio.nAvgBytesPerSec != 0 && mi->audio.nAvgBytesPerSec != _UI32_MAX) {
					mi->fAudioLengthSec = (double)dwLength / mi->audio.nAvgBytesPerSec;
					if (mi->fAudioLengthSec < 1.0)
						mi->fAudioLengthSec = 1.0; // compensate for very small files
				}
			}
		}

		if (bHaveReadAllStreams)
			break;
		if (dwLength) {
			if (_lseek(hAviFile, dwLength + (dwLength & 1), SEEK_CUR) == -1)
				goto inv_format_errno;
		}
	}

	if (fccMain == formtypeAVI) {
		mi->strFileFormat = _T("AVI");

		// NOTE: This video bit rate is published to ed2k servers and Kad, so, do everything to determine it right!
		if (mi->iVideoStreams == 1 /*&& mi->iAudioStreams <= 1*/ && iNonAVStreams == 0 && mi->fVideoLengthSec) {
			DWORD dwVideoFramesOverhead = uVideoFrames * (sizeof(WORD) + sizeof(WORD) + sizeof(DWORD));
			mi->video.dwBitRate = (DWORD)(((dwMovieChunkSize - dwVideoFramesOverhead) / mi->fVideoLengthSec - dwAllNonVideoAvgBytesPerSec/*mi->audio.nAvgBytesPerSec*/) * 8);
		}
	} else //if (fccMain == mmioFOURCC('W', 'A', 'V', 'E'))
		mi->strFileFormat = _T("WAV (RIFF)");
	//else
	//	mi->strFileFormat = _T("RIFF");

	mi->InitFileLength();
	bResult = true;

inv_format:
inv_format_errno:
cleanup:
	_close(hAviFile);
	return bResult;
}

#pragma pack(push, 1)
typedef struct
{
	DWORD	id;
	DWORD	size;
} SRmChunkHdr;

typedef struct
{
	DWORD   file_version;
	DWORD   num_headers;
} SRmRMF;

typedef struct
{
	DWORD   max_bit_rate;
	DWORD   avg_bit_rate;
	DWORD   max_packet_size;
	DWORD   avg_packet_size;
	DWORD   num_packets;
	DWORD   duration;
	DWORD   preroll;
	DWORD   index_offset;
	DWORD   data_offset;
	WORD    num_streams;
	WORD    flags;
} SRmPROP;

typedef struct
{
	WORD    stream_number;
	DWORD   max_bit_rate;
	DWORD   avg_bit_rate;
	DWORD   max_packet_size;
	DWORD   avg_packet_size;
	DWORD   start_time;
	DWORD   preroll;
	DWORD	duration;
	//BYTE                       stream_name_size;
	//BYTE[stream_name_size]     stream_name;
	//BYTE                       mime_type_size;
	//BYTE[mime_type_size]       mime_type;
	//DWORD                      type_specific_len;
	//BYTE[type_specific_len]    type_specific_data;
} SRmMDPR;
#pragma pack(pop)

struct SRmFileProp
{
	SRmFileProp() = default;
	SRmFileProp(const CStringA &rstrName, const CStringA &rstrValue)
		: strName(rstrName)
		, strValue(rstrValue)
	{
	}
	SRmFileProp(const SRmFileProp &r)
	{
		strName = r.strName;
		strValue = r.strValue;
	}

	SRmFileProp& operator=(const SRmFileProp &r)
	{
		strName = r.strName;
		strValue = r.strValue;
		return *this;
	}
	CStringA strName;
	CStringA strValue;
};

CStringA GetFOURCCString(DWORD fcc)
{
	return CStringA((LPCSTR)&fcc, sizeof(DWORD)).TrimRight();
}

struct SRmCodec
{
	LPCSTR	pszID;
	LPCTSTR pszDesc;
} g_aRealMediaCodecs[] = {
	{ "14.4", _T("Real Audio 1 (14.4)") },
	{ "14_4", _T("Real Audio 1 (14.4)") },
	{ "28.8", _T("Real Audio 2 (28.8)") },
	{ "28_8", _T("Real Audio 2 (28.8)") },
	{ "RV10", _T("Real Video 5") },
	{ "RV13", _T("Real Video 5") },
	{ "RV20", _T("Real Video G2") },
	{ "RV30", _T("Real Video 8") },
	{ "RV40", _T("Real Video 9") },
	{ "atrc", _T("Real & Sony Atrac3 Codec") },
	{ "cook", _T("Real Audio G2/7 Cook (Low Bitrate)") },
	{ "dnet", _T("Real Audio 3/4/5 Music (DNET)") },
	{ "lpcJ", _T("Real Audio 1 (14.4)") },
	{ "raac", _T("Real Audio 10 AAC (RAAC)") },
	{ "racp", _T("Real Audio 10 AAC+ (RACP)") },
	{ "ralf", _T("Real Audio Lossless Format") },
	{ "rtrc", _T("Real Audio 8 (RTRC)") },
	{ "rv10", _T("Real Video 5") },
	{ "rv20", _T("Real Video G2") },
	{ "rv30", _T("Real Video 8") },
	{ "rv40", _T("Real Video 9") },
	{ "sipr", _T("Real Audio 4 (Sipro)") },
};

static int __cdecl CmpRealMediaCodec(const void *p1, const void *p2) noexcept
{
	const SRmCodec *pCodec1 = reinterpret_cast<const SRmCodec*>(p1);
	const SRmCodec *pCodec2 = reinterpret_cast<const SRmCodec*>(p2);
	return strncmp(pCodec1->pszID, pCodec2->pszID, 4);
}

CString GetRealMediaCodecInfo(LPCSTR pszCodecID)
{
	CString strInfo(GetFOURCCString(*reinterpret_cast<const DWORD*>(pszCodecID)));
	SRmCodec CodecSearch;
	CodecSearch.pszID = pszCodecID;
	const SRmCodec *pCodecFound = static_cast<SRmCodec*>(bsearch(&CodecSearch, g_aRealMediaCodecs, _countof(g_aRealMediaCodecs), sizeof g_aRealMediaCodecs[0], CmpRealMediaCodec));
	if (pCodecFound)
		strInfo.AppendFormat(_T(" (%s)"), pCodecFound->pszDesc);
	return strInfo;
}

bool GetRMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsRM, bool bFullInfo)
{
	ASSERT(!bFullInfo || mi->strInfo.m_hWnd != NULL);

	CSafeBufferedFile file;
	if (!file.Open(pszFileName, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary))
		return false;
	setvbuf(file.m_pStream, NULL, _IOFBF, 16384);

	bool bReadPROP = false;
	bool bReadMDPR_Video = false;
	bool bReadMDPR_Audio = false;
	bool bReadMDPR_File = false;
	bool bReadCONT = false;
	UINT nFileBitrate = 0;
	CString strCopyright;
	CString strComment;
	CArray<SRmFileProp> aFileProps;
	try {
		ULONGLONG ullCurFilePos = 0ull;
		ULONGLONG ullChunkStartFilePos = ullCurFilePos;
		ULONGLONG ullChunkEndFilePos;

		SRmChunkHdr rmChunkHdr;
		file.Read(&rmChunkHdr, sizeof rmChunkHdr);
		DWORD dwID = _byteswap_ulong(rmChunkHdr.id);
		if (dwID != '.RMF')
			return false;
		DWORD dwSize = _byteswap_ulong(rmChunkHdr.size);
		if (dwSize < sizeof rmChunkHdr)
			return false;
		ullChunkEndFilePos = ullChunkStartFilePos + dwSize;
		dwSize -= sizeof rmChunkHdr;
		if (dwSize == 0)
			return false;

		WORD wVersion;
		file.Read(&wVersion, sizeof wVersion);
		wVersion = _byteswap_ushort(wVersion);
		if (wVersion >= 2)
			return false;

		SRmRMF rmRMF;
		file.Read(&rmRMF, sizeof rmRMF);

		rbIsRM = true;
		mi->strFileFormat = _T("Real Media");

		bool bBrokenFile = false;
		while (!bBrokenFile && (!bReadCONT || !bReadPROP || !bReadMDPR_Video || !bReadMDPR_Audio || (bFullInfo && !bReadMDPR_File))) {
			ullCurFilePos = file.GetPosition();
			if (ullCurFilePos > ullChunkEndFilePos)
				break; // expect broken DATA-chunk headers created by 'mplayer'
			if (ullCurFilePos < ullChunkEndFilePos)
				ullCurFilePos = file.Seek(ullChunkEndFilePos - ullCurFilePos, CFile::current);

			ullChunkStartFilePos = ullCurFilePos;

			file.Read(&rmChunkHdr, sizeof rmChunkHdr);
			dwID = _byteswap_ulong(rmChunkHdr.id);
			dwSize = _byteswap_ulong(rmChunkHdr.size);
			TRACE("%08x: id=%.4s  size=%u\n", (DWORD)ullCurFilePos, &rmChunkHdr.id, dwSize);
			if (dwSize < sizeof rmChunkHdr)
				break; // expect broken DATA-chunk headers created by 'mplayer'
			ullChunkEndFilePos = ullChunkStartFilePos + dwSize;
			dwSize -= sizeof rmChunkHdr;
			if (dwSize > 0) {
				switch (dwID) {
				case 'PROP':
					{ // Properties Header
						file.Read(&wVersion, sizeof wVersion);
						wVersion = _byteswap_ushort(wVersion);
						if (wVersion == 0) {
							SRmPROP rmPROP;
							file.Read(&rmPROP, sizeof rmPROP);
							nFileBitrate = _byteswap_ulong(rmPROP.avg_bit_rate);
							mi->fFileLengthSec = (_byteswap_ulong(rmPROP.duration) + 500.0) / 1000.0;
							bReadPROP = true;
						}
					}
					break;
				case 'MDPR':
					{ // Media Properties Header
						file.Read(&wVersion, sizeof wVersion);
						wVersion = _byteswap_ushort(wVersion);
						if (wVersion == 0) {
							SRmMDPR rmMDPR;
							file.Read(&rmMDPR, sizeof rmMDPR);

							// Read 'Stream Name'
							BYTE ucLen;
							CStringA strStreamName;
							file.Read(&ucLen, sizeof ucLen);
							file.Read(strStreamName.GetBuffer(ucLen), ucLen);
							strStreamName.ReleaseBuffer(ucLen);

							// Read 'MIME Type'
							CStringA strMimeType;
							file.Read(&ucLen, sizeof ucLen);
							file.Read(strMimeType.GetBuffer(ucLen), ucLen);
							strMimeType.ReleaseBuffer(ucLen);

							DWORD dwTypeDataLen;
							file.Read(&dwTypeDataLen, sizeof dwTypeDataLen);
							dwTypeDataLen = _byteswap_ulong(dwTypeDataLen);

							if (strMimeType == "video/x-pn-realvideo") {
								mi->iVideoStreams++;
								if (mi->iVideoStreams == 1) {
									mi->video.dwBitRate = _byteswap_ulong(rmMDPR.avg_bit_rate);
									mi->fVideoLengthSec = _byteswap_ulong(rmMDPR.duration) / 1000.0;

									if (dwTypeDataLen >= 22 + 2 && dwTypeDataLen < 8192) {
										BYTE *pucTypeData = new BYTE[dwTypeDataLen];
										try {
											file.Read(pucTypeData, dwTypeDataLen);
											if (_byteswap_ulong(*(DWORD*)(pucTypeData + 4)) == 'VIDO') {
												mi->strVideoFormat = GetRealMediaCodecInfo((LPCSTR)(pucTypeData + 8));
												mi->video.bmiHeader.biCompression = *(DWORD*)(pucTypeData + 8);
												mi->video.bmiHeader.biWidth = _byteswap_ushort(*(WORD*)(pucTypeData + 12));
												mi->video.bmiHeader.biHeight = _byteswap_ushort(*(WORD*)(pucTypeData + 14));
												mi->fVideoAspectRatio = abs(mi->video.bmiHeader.biWidth / (double)mi->video.bmiHeader.biHeight);
												mi->fVideoFrameRate = _byteswap_ushort(*(WORD*)(pucTypeData + 22));
												bReadMDPR_Video = true;
											}
										} catch (CException *ex) {
											ex->Delete();
											delete[] pucTypeData;
											break;
										}
										delete[] pucTypeData;
									}
								}
							} else if (strMimeType == "audio/x-pn-realaudio") {
								mi->iAudioStreams++;
								if (mi->iAudioStreams == 1) {
									mi->audio.nAvgBytesPerSec = _byteswap_ulong(rmMDPR.avg_bit_rate) / 8;
									mi->fAudioLengthSec = _byteswap_ulong(rmMDPR.duration) / 1000.0;

									if (dwTypeDataLen >= 4 + 2 && dwTypeDataLen < 8192) {
										BYTE *pucTypeData = new BYTE[dwTypeDataLen];
										try {
											file.Read(pucTypeData, dwTypeDataLen);
											DWORD dwFourCC = *(DWORD*)(pucTypeData + 0);
											WORD wVer = _byteswap_ushort(*(WORD*)(pucTypeData + 4));
											if (dwFourCC == MAKEFOURCC('.', 'r', 'a', '\xFD')) {
												if (wVer == 3) {
													if (dwTypeDataLen >= 8 + 2) {
														mi->audio.nSamplesPerSec = 8000;
														mi->audio.nChannels = _byteswap_ushort(*(WORD*)(pucTypeData + 8));
														mi->strAudioFormat = _T(".ra3");
														bReadMDPR_Audio = true;
													}
												} else if (wVer == 4) { // RealAudio G2, RealAudio 8
													if (dwTypeDataLen >= 62 + 4) {
														mi->audio.nSamplesPerSec = _byteswap_ushort(*(WORD*)(pucTypeData + 48));
														mi->audio.nChannels = _byteswap_ushort(*(WORD*)(pucTypeData + 54));
														if (strncmp((LPCSTR)(pucTypeData + 62), "sipr", 4) == 0)
															mi->audio.wFormatTag = WAVE_FORMAT_SIPROLAB_ACEPLNET;
														else if (strncmp((LPCSTR)(pucTypeData + 62), "cook", 4) == 0)
															mi->audio.wFormatTag = 0x2004;
														mi->strAudioFormat = GetRealMediaCodecInfo((LPCSTR)(pucTypeData + 62));
														bReadMDPR_Audio = true;
													}
												} else if (wVer == 5) {
													if (dwTypeDataLen >= 66 + 4) {
														mi->audio.nSamplesPerSec = _byteswap_ulong(*(DWORD*)(pucTypeData + 48));
														mi->audio.nChannels = _byteswap_ushort(*(WORD*)(pucTypeData + 60));
														if (strncmp((LPCSTR)(pucTypeData + 62), "sipr", 4) == 0)
															mi->audio.wFormatTag = WAVE_FORMAT_SIPROLAB_ACEPLNET;
														else if (strncmp((LPCSTR)(pucTypeData + 62), "cook", 4) == 0)
															mi->audio.wFormatTag = 0x2004;
														mi->strAudioFormat = GetRealMediaCodecInfo((LPCSTR)(pucTypeData + 66));
														bReadMDPR_Audio = true;
													}
												}
											}
										} catch (CException *ex) {
											ex->Delete();
											delete[] pucTypeData;
											break;
										}
										delete[] pucTypeData;
									}
								}
							} else if (bFullInfo && strcmp(strMimeType, "logical-fileinfo") == 0) {
								DWORD dwLogStreamLen;
								file.Read(&dwLogStreamLen, sizeof dwLogStreamLen);
								dwLogStreamLen = _byteswap_ulong(dwLogStreamLen);

								file.Read(&wVersion, sizeof wVersion);
								wVersion = _byteswap_ushort(wVersion);
								if (wVersion == 0) {
									WORD wStreams;
									file.Read(&wStreams, sizeof wStreams);
									wStreams = _byteswap_ushort(wStreams);

									// Skip 'Physical Stream Numbers'
									file.Seek(wStreams * sizeof(WORD), CFile::current);

									// Skip 'Data Offsets'
									file.Seek(wStreams * sizeof(DWORD), CFile::current);

									WORD wRules;
									file.Read(&wRules, sizeof wRules);
									wRules = _byteswap_ushort(wRules);

									// Skip 'Rule to Physical Stream Number Map'
									file.Seek(wRules * sizeof(WORD), CFile::current);

									WORD wProperties;
									file.Read(&wProperties, sizeof wProperties);
									wProperties = _byteswap_ushort(wProperties);

									while (wProperties) {
										DWORD dwPropSize;
										file.Read(&dwPropSize, sizeof dwPropSize);
										dwPropSize = _byteswap_ulong(dwPropSize);

										WORD wPropVersion;
										file.Read(&wPropVersion, sizeof wPropVersion);
										wPropVersion = _byteswap_ushort(wPropVersion);

										if (wPropVersion == 0) {
											BYTE ucLen1;
											CStringA strPropNameA;
											file.Read(&ucLen1, sizeof ucLen1);
											file.Read(strPropNameA.GetBuffer(ucLen1), ucLen1);
											strPropNameA.ReleaseBuffer(ucLen1);

											DWORD dwPropType;
											file.Read(&dwPropType, sizeof dwPropType);
											dwPropType = _byteswap_ulong(dwPropType);

											WORD wPropValueLen;
											file.Read(&wPropValueLen, sizeof wPropValueLen);
											wPropValueLen = _byteswap_ushort(wPropValueLen);

											CStringA strPropValueA;
											if (dwPropType == 0 && wPropValueLen == sizeof(DWORD)) {
												DWORD dwPropValue;
												file.Read(&dwPropValue, sizeof dwPropValue);
												dwPropValue = _byteswap_ulong(dwPropValue);
												strPropValueA.Format("%u", dwPropValue);
											} else if (dwPropType == 2) {
												LPSTR pszA = strPropValueA.GetBuffer(wPropValueLen);
												file.Read(pszA, wPropValueLen);
												if (wPropValueLen > 0 && pszA[wPropValueLen - 1] == '\0')
													--wPropValueLen;
												strPropValueA.ReleaseBuffer(wPropValueLen);
											} else {
												file.Seek(wPropValueLen, CFile::current);
												strPropValueA.Format("<%u bytes>", wPropValueLen);
											}

											aFileProps.Add(SRmFileProp(strPropNameA, strPropValueA));
											TRACE("Prop: %s, typ=%u, size=%u, value=%s\n", (LPCSTR)strPropNameA, dwPropType, wPropValueLen, (LPCSTR)strPropValueA);
										} else
											file.Seek(dwPropSize - sizeof(DWORD) - sizeof(WORD), CFile::current);

										--wProperties;
									}
									bReadMDPR_File = true;
								}
							}
						}
					}
					break;
				case 'CONT':
					{ // Content Description Header
						file.Read(&wVersion, sizeof wVersion);
						wVersion = _byteswap_ushort(wVersion);
						if (wVersion == 0) {
							WORD wLen;
							CStringA strA;
							file.Read(&wLen, sizeof wLen);
							wLen = _byteswap_ushort(wLen);
							file.Read(strA.GetBuffer(wLen), wLen);
							strA.ReleaseBuffer(wLen);
							mi->strTitle = strA;

							file.Read(&wLen, sizeof wLen);
							wLen = _byteswap_ushort(wLen);
							file.Read(strA.GetBuffer(wLen), wLen);
							strA.ReleaseBuffer(wLen);
							mi->strAuthor = strA;

							file.Read(&wLen, sizeof wLen);
							wLen = _byteswap_ushort(wLen);
							file.Read(strA.GetBuffer(wLen), wLen);
							strA.ReleaseBuffer(wLen);
							strCopyright = strA;

							file.Read(&wLen, sizeof wLen);
							wLen = _byteswap_ushort(wLen);
							file.Read(strA.GetBuffer(wLen), wLen);
							strA.ReleaseBuffer(wLen);
							strComment = strA;
							bReadCONT = true;
						}
					}
					break;
				case 'DATA':
				case 'INDX':
				case 'RMMD':
				case 'RMJE':
					// Expect broken DATA-chunk headers created by 'mplayer'. Thus catch
					// all valid tags just to have chance to detect the broken ones.
					break;
				default:
					// Expect broken DATA-chunk headers created by 'mplayer'. Stop reading
					// the file on first broken chunk header.
					bBrokenFile = true;
				}
			}
		}
	} catch (CException *ex) {
		ex->Delete();
	}

	// Expect broken DATA-chunk headers created by 'mplayer'. A broken DATA-chunk header
	// may not be a fatal error. We may have already successfully read the media properties
	// headers. Therefore we indicate 'success' if we read at least some valid headers.
	if (!bReadCONT && !bReadPROP && !bReadMDPR_Video && !bReadMDPR_Audio)
		return false;
	mi->InitFileLength();
	if (bFullInfo
		&& mi->strInfo.m_hWnd
		&& (nFileBitrate
			|| !mi->strTitle.IsEmpty()
			|| !mi->strAuthor.IsEmpty()
			|| !strCopyright.IsEmpty()
			|| !strComment.IsEmpty()
			|| !aFileProps.IsEmpty()))
	{
		if (!mi->strInfo.IsEmpty())
			mi->strInfo << _T("\n");
		mi->OutputFileName();
		mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
		mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");

		if (nFileBitrate) {
			CString strBitrate;
			strBitrate.Format(_T("%u %s"), (UINT)((nFileBitrate + 500) / 1000), (LPCTSTR)GetResString(IDS_KBITSSEC));
			mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << strBitrate << _T("\n");
		}
		if (!mi->strTitle.IsEmpty())
			mi->strInfo << _T("   ") << GetResString(IDS_TITLE) << _T(":\t") << mi->strTitle << _T("\n");
		if (!mi->strAuthor.IsEmpty())
			mi->strInfo << _T("   ") << GetResString(IDS_AUTHOR) << _T(":\t") << mi->strAuthor << _T("\n");
		if (!strCopyright.IsEmpty())
			mi->strInfo << _T("   ") << _T("Copyright") << _T(":\t") << strCopyright << _T("\n");
		if (!strComment.IsEmpty())
			mi->strInfo << _T("   ") << GetResString(IDS_COMMENT) << _T(":\t") << strComment << _T("\n");
		for (int i = 0; i < aFileProps.GetCount(); ++i)
			if (!aFileProps[i].strValue.IsEmpty())
				mi->strInfo << _T("   ") << CString(aFileProps[i].strName) << _T(":\t") << CString(aFileProps[i].strValue) << _T("\n");
	}

	return true;
}

#ifdef HAVE_WMSDK_H

template<class T, WMT_ATTR_DATATYPE attrTypeT>
bool GetAttributeT(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nValue)
{
	WMT_ATTR_DATATYPE attrType;
	WORD wValueSize = sizeof nValue;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByName(&wStream, pwszName, &attrType, (BYTE*)&nValue, &wValueSize);
	if (hr == ASF_E_NOTFOUND)
		return false;
	if (hr != S_OK || attrType != attrTypeT) {
		ASSERT(0);
		return false;
	}
	return true;
}

template<class T> bool GetAttributeT(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nData);

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, BOOL &nData)
{
	return GetAttributeT<BOOL, WMT_TYPE_BOOL>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, DWORD &nData)
{
	return GetAttributeT<DWORD, WMT_TYPE_DWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, QWORD &nData)
{
	return GetAttributeT<QWORD, WMT_TYPE_QWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttribute(IWMHeaderInfo *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CString &strValue)
{
	WMT_ATTR_DATATYPE attrType;
	WORD wValueSize;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByName(&wStream, pwszName, &attrType, NULL, &wValueSize);
	if (hr == ASF_E_NOTFOUND)
		return false;
	if (hr != S_OK || attrType != WMT_TYPE_STRING || wValueSize < sizeof(WCHAR) || (wValueSize % sizeof(WCHAR)) != 0) {
		ASSERT(0);
		return false;
	}

	// empty string?
	if (wValueSize == sizeof(WCHAR))
		return false;

	hr = pIWMHeaderInfo->GetAttributeByName(&wStream, pwszName, &attrType, (BYTE*)strValue.GetBuffer(wValueSize / sizeof(WCHAR)), &wValueSize);
	strValue.ReleaseBuffer();
	if (hr != S_OK || attrType != WMT_TYPE_STRING) {
		ASSERT(0);
		return false;
	}

	if (strValue.IsEmpty())
		return false;

	// SDK states that MP3 files could contain a BOM - never seen
	if (*(LPCWSTR)strValue == 0xFFFEui16 || *(LPCWSTR)strValue == 0xFEFFui16) {
		ASSERT(0);
		strValue.Delete(0, 1);
	}

	return true;
}

bool GetAttributeIndices(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CTempBuffer<WORD> &aIndices, WORD &wLangIndex)
{
	WORD wIndexCount;
	HRESULT hr = pIWMHeaderInfo->GetAttributeIndices(wStream, pwszName, &wLangIndex, NULL, &wIndexCount);
	if (hr == ASF_E_NOTFOUND)
		return false;
	if (hr != S_OK) {
		ASSERT(0);
		return false;
	}

	if (wIndexCount == 0)
		return false;

	hr = pIWMHeaderInfo->GetAttributeIndices(wStream, pwszName, &wLangIndex, aIndices.Allocate(wIndexCount), &wIndexCount);
	if (hr != S_OK || wIndexCount == 0) {
		ASSERT(0);
		return false;
	}

	return true;
}

template<class T, WMT_ATTR_DATATYPE attrTypeT>
bool GetAttributeExT(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nData)
{
	// Certain attributes (e.g. WM/StreamTypeInfo, WM/PeakBitrate) can not get read with "IWMHeaderInfo::GetAttributeByName",
	// those attributes are only returned with "IWMHeaderInfo3::GetAttributeByIndexEx".

	CTempBuffer<WORD> aIndices;
	WORD wLangIndex = 0;
	if (!GetAttributeIndices(pIWMHeaderInfo, wStream, pwszName, aIndices, wLangIndex))
		return false;
	WORD wIndex = aIndices[0];

	WORD wNameSize;
	DWORD dwDataSize;
	WMT_ATTR_DATATYPE attrType;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, NULL, &wNameSize, &attrType, &wLangIndex, NULL, &dwDataSize);
	if (hr != S_OK || attrType != attrTypeT || dwDataSize != sizeof nData) {
		ASSERT(0);
		return false;
	}

	WCHAR wszName[1024];
	if (wNameSize > _countof(wszName)) {
		ASSERT(0);
		return false;
	}

	hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, wszName, &wNameSize, &attrType, &wLangIndex, (BYTE*)&nData, &dwDataSize);
	if (hr != S_OK || attrType != attrTypeT || dwDataSize != sizeof nData) {
		ASSERT(0);
		return false;
	}
	return true;
}

template<class T> bool GetAttributeExT(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, T &nData);

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, BOOL &nData)
{
	return GetAttributeExT<BOOL, WMT_TYPE_BOOL>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, DWORD &nData)
{
	return GetAttributeExT<DWORD, WMT_TYPE_DWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, QWORD &nData)
{
	return GetAttributeExT<QWORD, WMT_TYPE_QWORD>(pIWMHeaderInfo, wStream, pwszName, nData);
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CTempBuffer<BYTE> &aData, DWORD &dwDataSize)
{
	// Certain attributes (e.g. WM/StreamTypeInfo, WM/PeakBitrate) can not get read with "IWMHeaderInfo::GetAttributeByName",
	// those attributes are only returned with "IWMHeaderInfo3::GetAttributeByIndexEx".

	CTempBuffer<WORD> aIndices;
	WORD wLangIndex = 0;
	if (!GetAttributeIndices(pIWMHeaderInfo, wStream, pwszName, aIndices, wLangIndex))
		return false;
	WORD wIndex = aIndices[0];

	WORD wNameSize;
	WMT_ATTR_DATATYPE attrType;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, NULL, &wNameSize, &attrType, &wLangIndex, NULL, &dwDataSize);
	if (hr != S_OK || attrType != WMT_TYPE_BINARY) {
		ASSERT(0);
		return false;
	}

	WCHAR wszName[1024];
	if (wNameSize > _countof(wszName)) {
		ASSERT(0);
		return false;
	}

	if (dwDataSize == 0)
		return false;

	hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, wszName, &wNameSize, &attrType, &wLangIndex, aData.Allocate(dwDataSize), &dwDataSize);
	if (hr != S_OK || attrType != WMT_TYPE_BINARY) {
		ASSERT(0);
		return false;
	}
	return true;
}

bool GetAttributeEx(IWMHeaderInfo3 *pIWMHeaderInfo, WORD wStream, LPCWSTR pwszName, CString &strValue)
{
	CTempBuffer<WORD> aIndices;
	WORD wLangIndex = 0;
	if (!GetAttributeIndices(pIWMHeaderInfo, wStream, pwszName, aIndices, wLangIndex))
		return false;
	WORD wIndex = aIndices[0];

	WORD wNameSize;
	WMT_ATTR_DATATYPE attrType;
	DWORD dwValueSize;
	HRESULT hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, NULL, &wNameSize, &attrType, &wLangIndex, NULL, &dwValueSize);
	if (hr != S_OK || attrType != WMT_TYPE_STRING || dwValueSize < sizeof(WCHAR) || (dwValueSize % sizeof(WCHAR)) != 0) {
		ASSERT(0);
		return false;
	}

	WCHAR wszName[1024];
	if (wNameSize > _countof(wszName)) {
		ASSERT(0);
		return false;
	}

	// empty string?
	if (dwValueSize == sizeof(WCHAR))
		return false;

	hr = pIWMHeaderInfo->GetAttributeByIndexEx(wStream, wIndex, wszName, &wNameSize, &attrType, &wLangIndex, (BYTE*)strValue.GetBuffer(dwValueSize / sizeof(WCHAR)), &dwValueSize);
	strValue.ReleaseBuffer();
	if (hr != S_OK || attrType != WMT_TYPE_STRING) {
		ASSERT(0);
		return false;
	}

	if (strValue.IsEmpty())
		return false;

	// SDK states that MP3 files could contain a BOM - never seen
	if (*(LPCWSTR)strValue == 0xFFFEui16 || *(LPCWSTR)strValue == 0xFEFFui16) {
		ASSERT(0);
		strValue.Delete(0, 1);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////
// CFileStream - Implements IStream interface on a file

class CFileStream : public IStream
{
public:
	static HRESULT OpenFile(LPCTSTR pszFileName, IStream **ppStream,
		DWORD dwDesiredAccess = GENERIC_READ,
		DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE,
		DWORD dwCreationDisposition = OPEN_EXISTING,
		DWORD grfMode = STGM_READ | STGM_SHARE_DENY_NONE)
	{
		HANDLE hFile = ::CreateFile(pszFileName, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return HRESULT_FROM_WIN32(::GetLastError());
		CFileStream *pFileStream = new CFileStream(hFile, grfMode);
		return pFileStream->QueryInterface(__uuidof(*ppStream), (void **)ppStream);
	}

	///////////////////////////////////////////////////////////////////////////
	// IUnknown

	STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject)
	{
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IStream) || iid == __uuidof(ISequentialStream)) {
			*ppvObject = static_cast<IStream*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return static_cast<ULONG>(InterlockedIncrement(&m_lRefCount));
	}

	STDMETHODIMP_(ULONG) Release()
	{
		LONG lRefCount = InterlockedDecrement(&m_lRefCount);
		if (lRefCount == 0)
			delete this;
		return static_cast<ULONG>(lRefCount);
	}

	///////////////////////////////////////////////////////////////////////////
	// ISequentialStream

	STDMETHODIMP Read(void *pv, ULONG cb, ULONG *pcbRead)
	{
		// If the stream was opened for 'write-only', no read access is allowed.
		if ((m_grfMode & 3) == STGM_WRITE) {
			ASSERT(0);
			return E_FAIL;
		}

		if (!::ReadFile(m_hFile, pv, cb, pcbRead, NULL))
			return HRESULT_FROM_WIN32(::GetLastError());

		// The specification of the 'IStream' interface allows to indicate an
		// end-of-stream condition by returning:
		//
		//	HRESULT		*pcbRead
		//  ------------------------
		//	S_OK		<less-than> 'cb'
		//	S_FALSE		<not used>
		//	E_xxx		<not used>
		//
		// If that object is used by the 'WMSyncReader', it seems to be better to
		// return an error code instead of 'S_OK'.
		//
		if (cb != 0 && *pcbRead == 0)
			return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

		return S_OK;
	}

	STDMETHODIMP Write(void const *pv, ULONG cb, ULONG *pcbWritten)
	{
		// If the stream was opened for 'read-only', no write access is allowed.
		if ((m_grfMode & 3) == STGM_READ) {
			ASSERT(0);
			return E_FAIL;
		}

		ULONG cbWritten;
		if (!::WriteFile(m_hFile, pv, cb, &cbWritten, NULL))
			return HRESULT_FROM_WIN32(::GetLastError());

		if (pcbWritten != NULL)
			*pcbWritten = cbWritten;

		return S_OK;
	}

	///////////////////////////////////////////////////////////////////////////
	// IStream

	STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
	{
		// 'dwOrigin' can get mapped to 'dwMoveMethod'
		ASSERT(STREAM_SEEK_SET == FILE_BEGIN);
		ASSERT(STREAM_SEEK_CUR == FILE_CURRENT);
		ASSERT(STREAM_SEEK_END == FILE_END);

		LONG lNewFilePointerHi = dlibMove.HighPart;
		DWORD dwNewFilePointerLo = SetFilePointer(m_hFile, dlibMove.LowPart, &lNewFilePointerHi, dwOrigin);
		if (dwNewFilePointerLo == INVALID_SET_FILE_POINTER && ::GetLastError() != NO_ERROR)
			return HRESULT_FROM_WIN32(::GetLastError());

		if (plibNewPosition != NULL) {
			plibNewPosition->HighPart = lNewFilePointerHi;
			plibNewPosition->LowPart = dwNewFilePointerLo;
		}

		return S_OK;
	}

	STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(libNewSize);
		return E_NOTIMPL;
	}

	STDMETHODIMP CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(pstm);
		UNREFERENCED_PARAMETER(cb);
		UNREFERENCED_PARAMETER(pcbRead);
		UNREFERENCED_PARAMETER(pcbWritten);
		return E_NOTIMPL;
	}

	STDMETHODIMP Commit(DWORD grfCommitFlags)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(grfCommitFlags);
		return E_NOTIMPL;
	}

	STDMETHODIMP Revert()
	{
		ASSERT(0);
		return E_NOTIMPL;
	}

	STDMETHODIMP LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(libOffset);
		UNREFERENCED_PARAMETER(cb);
		UNREFERENCED_PARAMETER(dwLockType);
		return E_NOTIMPL;
	}

	STDMETHODIMP UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(libOffset);
		UNREFERENCED_PARAMETER(cb);
		UNREFERENCED_PARAMETER(dwLockType);
		return E_NOTIMPL;
	}

	STDMETHODIMP Stat(STATSTG *pstatstg, DWORD grfStatFlag)
	{
		UNREFERENCED_PARAMETER(grfStatFlag);
		ASSERT(grfStatFlag == STATFLAG_NONAME);
		memset(pstatstg, 0, sizeof *pstatstg);
		BY_HANDLE_FILE_INFORMATION fileInfo;
		if (!GetFileInformationByHandle(m_hFile, &fileInfo))
			return HRESULT_FROM_WIN32(::GetLastError());
		pstatstg->type = STGTY_STREAM;
		pstatstg->cbSize.HighPart = fileInfo.nFileSizeHigh;
		pstatstg->cbSize.LowPart = fileInfo.nFileSizeLow;
		pstatstg->mtime = fileInfo.ftLastWriteTime;
		pstatstg->ctime = fileInfo.ftCreationTime;
		pstatstg->atime = fileInfo.ftLastAccessTime;
		pstatstg->grfMode = m_grfMode;
		return S_OK;
	}

	STDMETHODIMP Clone(IStream **ppstm)
	{
		ASSERT(0);
		UNREFERENCED_PARAMETER(ppstm);
		return E_NOTIMPL;
	}

private:
	CFileStream(HANDLE hFile, DWORD grfMode)
	{
		m_lRefCount = 0;
		m_hFile = hFile;
		m_grfMode = grfMode;
	}

	~CFileStream()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
			::CloseHandle(m_hFile);
	}

	HANDLE m_hFile;
	DWORD m_grfMode;
	LONG m_lRefCount;
};

class CWmvCoreDLL
{
public:
	CWmvCoreDLL()
	{
		m_bInitialized = false;
		m_hLib = NULL;
		m_pfnWMCreateEditor = NULL;
		m_pfnWMCreateSyncReader = NULL;
	}
	~CWmvCoreDLL()
	{
		if (m_hLib)
			FreeLibrary(m_hLib);
	}

	bool Initialize()
	{
		if (!m_bInitialized) {
			m_bInitialized = true;

			// Support for WMVCORE.DLL depends on Windows version and WMP version.
			//
			// If WMP64 is installed, WMVCORE.DLL is not available.
			// If WMP9+ is installed, WMVCORE.DLL is available.
			//
			m_hLib = LoadLibrary(_T("wmvcore.dll"));
			if (m_hLib != NULL) {
				(FARPROC &)m_pfnWMCreateEditor = GetProcAddress(m_hLib, "WMCreateEditor");
				(FARPROC &)m_pfnWMCreateSyncReader = GetProcAddress(m_hLib, "WMCreateSyncReader");
			}
		}
		return m_pfnWMCreateEditor != NULL;
	}

	bool m_bInitialized;
	HMODULE m_hLib;
	HRESULT(STDMETHODCALLTYPE *m_pfnWMCreateEditor)(IWMMetadataEditor **ppEditor);
	HRESULT(STDMETHODCALLTYPE *m_pfnWMCreateSyncReader)(IUnknown *pUnkCert, DWORD dwRights, IWMSyncReader **ppSyncReader);
};
static CWmvCoreDLL theWmvCoreDLL;


bool GetWMHeaders(LPCTSTR pszFileName, SMediaInfo *mi, bool &rbIsWM, bool bFullInfo)
{
	ASSERT(!bFullInfo || mi->strInfo.m_hWnd != NULL);

	if (!theWmvCoreDLL.Initialize())
		return false;

	CComPtr<IUnknown> pIUnkReader;
	try {
		HRESULT hr = E_FAIL;

		// 1st, try to read the file with the 'WMEditor'. This object tends to give more (stream) information than the 'WMSyncReader'.
		// Though the 'WMEditor' is not capable of reading files which are currently opened for 'writing'.
		if (pIUnkReader == NULL) {
			CComPtr<IWMMetadataEditor> pIWMMetadataEditor;
			if (theWmvCoreDLL.m_pfnWMCreateEditor != NULL && (hr = (*theWmvCoreDLL.m_pfnWMCreateEditor)(&pIWMMetadataEditor)) == S_OK) {
				CComQIPtr<IWMMetadataEditor2> pIWMMetadataEditor2 = pIWMMetadataEditor;
				if (pIWMMetadataEditor2 && (hr = pIWMMetadataEditor2->OpenEx(pszFileName, GENERIC_READ, FILE_SHARE_READ)) == S_OK)
					pIUnkReader = pIWMMetadataEditor2;
				else if ((hr = pIWMMetadataEditor->Open(pszFileName)) == S_OK)
					pIUnkReader = pIWMMetadataEditor;
			}
		}

		// 2nd, try to read the file with 'WMSyncReader'. This may give less (stream) information than using the 'WMEditor',
		// but it has at least the advantage that one can provide the file data via an 'IStream' - which is needed for
		// reading files which are currently opened for 'writing'.
		//
		// However, the 'WMSyncReader' may take a noticeable amount of time to parse a few bytes of a stream. E.g. reading
		// the short MP3 test files from ID3Lib takes suspicious long time. So, invoke that 'WMSyncReader' only if we know
		// that the file could not get opened due to that it is currently opened for 'writing'.
		//
		// Note also: If the file is DRM protected, 'IWMSyncReader' may return an 'NS_E_PROTECTED_CONTENT' error. This makes
		// sense, because 'IWMSyncReader' does not know that we want to open the file for reading the meta data only.
		// This error code could get used to indicate 'protected' files.
		//
		if (pIUnkReader == NULL && hr == NS_E_FILE_OPEN_FAILED) {
			CComPtr<IWMSyncReader> pIWMSyncReader;
			if (theWmvCoreDLL.m_pfnWMCreateSyncReader != NULL && (hr = (*theWmvCoreDLL.m_pfnWMCreateSyncReader)(NULL, 0, &pIWMSyncReader)) == S_OK) {
				CComPtr<IStream> pIStream;
				if (CFileStream::OpenFile(pszFileName, &pIStream) == S_OK) {
					if ((hr = pIWMSyncReader->OpenStream(pIStream)) == S_OK)
						pIUnkReader = pIWMSyncReader;
				}
			}
		} else
			ASSERT(hr == S_OK
				|| hr == NS_E_UNRECOGNIZED_STREAM_TYPE	// general: unknown file type
				|| hr == NS_E_INVALID_INPUT_FORMAT		// general: unknown file type
				|| hr == NS_E_INVALID_DATA				// general: unknown file type
				|| hr == NS_E_FILE_INIT_FAILED			// got for an SWF file?
				|| hr == NS_E_FILE_READ					// obviously if the file is too short
				);

		if (pIUnkReader) {
			CComQIPtr<IWMHeaderInfo> pIWMHeaderInfo = pIUnkReader;
			if (pIWMHeaderInfo) {
				// Although it is not following any logic, using "IWMHeaderInfo3" instead of "IWMHeaderInfo"
				// gives a few more (important) stream properties !?
				//
				// NOTE: The existence of 'IWMHeaderInfo3' does not automatically mean that one will also
				// get 'WM/StreamTypeInfo'.
				//
				// Windows Vista SP1; WMP 11.0.6001.7000
				// -------------------------------------
				// WMASF.DLL	11.0.6001.7000
				// WMVCORE.DLL	11.0.6001.7000
				// ---
				// IWMHeaderInfo3:    Yes
				// WM/StreamTypeInfo: Yes
				// Codec Info:	      Yes
				//
				//
				// Windows XP SP2; WMP 9.00.00.3354
				// -------------------------------------
				// WMASF.DLL	9.0.0.3267
				// WMVCORE.DLL	9.0.0.3267
				// ---
				// IWMHeaderInfo3:    Yes
				// WM/StreamTypeInfo: No!
				// Codec Info:	      Yes
				//
				//
				// Windows 98 SE; WMP 9.00.00.3349
				// -------------------------------------
				// WMASF.DLL	9.00.00.2980
				// WMVCORE.DLL	9.00.00.2980
				// ---
				// IWMHeaderInfo3:    Yes
				// WM/StreamTypeInfo: No!
				// Codec Info:	      Yes
				//
				CComQIPtr<IWMHeaderInfo3> pIWMHeaderInfo3 = pIUnkReader;

				WORD wStream = 0; // 0 = file level, 0xffff = all attributes from file and all streams

				DWORD dwContainer = _UI32_MAX;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMContainerFormat, dwContainer)) {
					if (dwContainer == WMT_Storage_Format_MP3) {
						// The file is either a real MP3 file or a WAV file with an embedded MP3 stream.
						//

						// NOTE: The detection for MP3 is *NOT* safe. There are some MKV test files which
						// are reported as MP3 files although they contain more than just a MP3 stream.
						// This is no surprise, MP3 can not get detected safely in couple of cases.
						//
						// If that function is invoked for getting meta data which is to get published,
						// special care has to be taken for publishing the audio/video codec info.
						// We do not want to announce non-MP3 files as MP3 files by accident.
						//
						if (!bFullInfo) {
							LPCTSTR pszExt = PathFindExtension(pszFileName);
							if (_tcsicmp(pszExt, _T(".mp3")) != 0 && _tcsicmp(pszExt, _T(".mpa")) != 0 && _tcsicmp(pszExt, _T(".wav")) != 0)
								throw new CNotSupportedException();
						}
						mi->strFileFormat = _T("MP3");
					} else if (dwContainer == WMT_Storage_Format_V1) {
						mi->strFileFormat = _T("Windows Media");
						rbIsWM = true;
					} else
						ASSERT(0);
				} else
					ASSERT(0);

				// WMFSDK 7.0 does not support the "WM/ContainerFormat" attribute at all. Though,
				// v7.0 has no importance any longer - it was originally shipped with WinME.
				//
				// If that function is invoked for getting meta data which is to get published,
				// special care has to be taken for publishing the audio/video codec info.
				// e.g. We do not want to announce non-MP3 files as MP3 files by accident.
				//
				if (!bFullInfo && dwContainer == _UI32_MAX) {
					ASSERT(0);
					throw new CNotSupportedException();
				}

				QWORD qwDuration = 0;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMDuration, qwDuration))
					mi->fFileLengthSec = max(qwDuration / 10000000.0, 1.0);
				else
					ASSERT(0);

				CString strValue;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMTitle, strValue) && !strValue.IsEmpty())
					mi->strTitle = strValue;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMAuthor, strValue) && !strValue.IsEmpty())
					mi->strAuthor = strValue;
				if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMAlbumTitle, strValue) && !strValue.IsEmpty())
					mi->strAlbum = strValue;

				if (bFullInfo && mi->strInfo.m_hWnd) {
					WORD wAttributes;
					if (pIWMHeaderInfo->GetAttributeCount(wStream, &wAttributes) == S_OK && wAttributes != 0) {
						bool bOutputHeader = true;
						for (WORD wAttr = 0; wAttr < wAttributes; ++wAttr) {
							WCHAR wszName[1024];
							WORD wNameSize = _countof(wszName);
							WORD wDataSize;
							WMT_ATTR_DATATYPE attrType;
							if (pIWMHeaderInfo->GetAttributeByIndex(wAttr, &wStream, wszName, &wNameSize, &attrType, NULL, &wDataSize) == S_OK) {
								if (attrType == WMT_TYPE_STRING) {
									if (GetAttribute(pIWMHeaderInfo, wStream, wszName, strValue)) {
										if (!strValue.Trim().IsEmpty()) {
											if (bOutputHeader) {
												bOutputHeader = false;
												if (!mi->strInfo.IsEmpty())
													mi->strInfo << _T("\n");
												mi->OutputFileName();
												mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
												mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");
											}
											CString strName(wszName);
											if (strName.Left(3).Compare(_T("WM/")) == 0)
												strName.Delete(0, 3);
											mi->strInfo << _T("   ") << strName << _T(":\t") << strValue << _T("\n");
										}
									}
								} else if (attrType == WMT_TYPE_BOOL) {
									BOOL bValue;
									if (GetAttribute(pIWMHeaderInfo, wStream, wszName, bValue) && bValue) {
										if (bOutputHeader) {
											bOutputHeader = false;
											if (!mi->strInfo.IsEmpty())
												mi->strInfo << _T("\n");
											mi->OutputFileName();
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
											mi->strInfo << GetResString(IDS_FD_GENERAL) << _T("\n");
										}
										CString strName(wszName);
										if (strName.Left(3).Compare(_T("WM/")) == 0)
											strName.Delete(0, 3);

										bool bWarnInRed = wcscmp(wszName, g_wszWMProtected) == 0;
										if (bWarnInRed)
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfRed);
										mi->strInfo << _T("   ") << strName << _T(":\t") << GetResString(IDS_YES) << _T("\n");
										if (bWarnInRed)
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfDef);
									}
								}
							}
						}
					}
				}

				while (wStream < 0x7F) {
					// Check if we reached the end of streams
					WORD wAttributes;
					if (pIWMHeaderInfo3) {
						if (pIWMHeaderInfo3->GetAttributeCountEx(wStream, &wAttributes) != S_OK)
							break;
					} else {
						if (pIWMHeaderInfo->GetAttributeCount(wStream, &wAttributes) != S_OK)
							break;
					}

					if (bFullInfo && mi->strAudioLanguage.IsEmpty()) {
						CString strLanguage;
						if (GetAttribute(pIWMHeaderInfo, wStream, g_wszWMLanguage, strLanguage))
							mi->strAudioLanguage = strLanguage;
					}

					bool bHaveStreamInfo = false;
					if (pIWMHeaderInfo3) {
						// 'WM/StreamTypeInfo' is supported only since v10. This means that, WinXP/WMP9 does not have that support.
						CTempBuffer<BYTE> aStreamInfo;
						DWORD dwStreamInfoSize;
						if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMStreamTypeInfo, aStreamInfo, dwStreamInfoSize) && dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO)) {
							const WM_STREAM_TYPE_INFO *pStreamTypeInfo = (WM_STREAM_TYPE_INFO*)(BYTE*)aStreamInfo;
							if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Video) {
								bHaveStreamInfo = true;
								mi->iVideoStreams++;
								if (dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO) + sizeof(WMVIDEOINFOHEADER)
									&& pStreamTypeInfo->cbFormat >= sizeof(WMVIDEOINFOHEADER))
								{
									const WMVIDEOINFOHEADER *pVideoInfo = (WMVIDEOINFOHEADER*)(pStreamTypeInfo + 1);
									ASSERT(sizeof(VIDEOINFOHEADER) == sizeof(WMVIDEOINFOHEADER));
									if (pVideoInfo->bmiHeader.biSize >= sizeof pVideoInfo->bmiHeader) {
										if (mi->iVideoStreams == 1) {
											mi->video = *(VIDEOINFOHEADER*)pVideoInfo;
											if (mi->video.bmiHeader.biWidth && mi->video.bmiHeader.biHeight)
												mi->fVideoAspectRatio = abs(mi->video.bmiHeader.biWidth / (double)mi->video.bmiHeader.biHeight);
											mi->strVideoFormat = GetVideoFormatName(pVideoInfo->bmiHeader.biCompression);

											if (pVideoInfo->bmiHeader.biCompression == MAKEFOURCC('D', 'V', 'R', ' ')
												&& pVideoInfo->bmiHeader.biSize >= sizeof(pVideoInfo->bmiHeader) + sizeof(WMMPEG2VIDEOINFO)
												&& dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO) + sizeof(WMVIDEOINFOHEADER) + sizeof(WMMPEG2VIDEOINFO)
												&& pStreamTypeInfo->cbFormat >= sizeof(WMVIDEOINFOHEADER) + sizeof(WMMPEG2VIDEOINFO))
											{
												const WMMPEG2VIDEOINFO *pMPEG2VideoInfo = (WMMPEG2VIDEOINFO*)(pVideoInfo + 1);
												if (pMPEG2VideoInfo->hdr.bmiHeader.biSize >= sizeof pMPEG2VideoInfo->hdr.bmiHeader
													&& pMPEG2VideoInfo->hdr.bmiHeader.biCompression != 0
													&& pMPEG2VideoInfo->hdr.bmiHeader.biWidth == pVideoInfo->bmiHeader.biWidth
													&& pMPEG2VideoInfo->hdr.bmiHeader.biHeight == pVideoInfo->bmiHeader.biHeight)
												{
													if (!IsRectEmpty(&pMPEG2VideoInfo->hdr.rcSource))
														mi->video.rcSource = pMPEG2VideoInfo->hdr.rcSource;
													if (!IsRectEmpty(&pMPEG2VideoInfo->hdr.rcTarget))
														mi->video.rcTarget = pMPEG2VideoInfo->hdr.rcTarget;
													if (pMPEG2VideoInfo->hdr.dwBitRate)
														mi->video.dwBitRate = pMPEG2VideoInfo->hdr.dwBitRate;
													if (pMPEG2VideoInfo->hdr.dwBitErrorRate)
														mi->video.dwBitErrorRate = pMPEG2VideoInfo->hdr.dwBitErrorRate;
													if (pMPEG2VideoInfo->hdr.AvgTimePerFrame)
														mi->video.AvgTimePerFrame = pMPEG2VideoInfo->hdr.AvgTimePerFrame;
													mi->video.bmiHeader = pMPEG2VideoInfo->hdr.bmiHeader;
													mi->strVideoFormat = GetVideoFormatName(mi->video.bmiHeader.biCompression);
													if (pMPEG2VideoInfo->hdr.dwPictAspectRatioX != 0 && pMPEG2VideoInfo->hdr.dwPictAspectRatioY != 0)
														mi->fVideoAspectRatio = pMPEG2VideoInfo->hdr.dwPictAspectRatioX / (double)pMPEG2VideoInfo->hdr.dwPictAspectRatioY;
													else if (mi->video.bmiHeader.biWidth && mi->video.bmiHeader.biHeight)
														mi->fVideoAspectRatio = abs(mi->video.bmiHeader.biWidth / (double)mi->video.bmiHeader.biHeight);
												}
											}
											if (mi->fVideoFrameRate == 0.0 && mi->video.AvgTimePerFrame)
												mi->fVideoFrameRate = 1.0 / (mi->video.AvgTimePerFrame / 10000000.0);
										} else if (bFullInfo && mi->strInfo.m_hWnd) {
											mi->OutputFileName();
											if (!mi->strInfo.IsEmpty())
												mi->strInfo << _T("\n");
											mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
											mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams << _T("\n");

											mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetVideoFormatName(pVideoInfo->bmiHeader.biCompression) << _T("\n");
											if (pVideoInfo->dwBitRate)
												mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((pVideoInfo->dwBitRate + 500) / 1000) << _T(" kbit/s\n");
											mi->strInfo << _T("   ") << GetResString(IDS_WIDTH) << _T(" x ") << GetResString(IDS_HEIGHT) << _T(":\t") << abs(pVideoInfo->bmiHeader.biWidth) << _T(" x ") << abs(pVideoInfo->bmiHeader.biHeight) << _T("\n");
											float fAspectRatio = abs(pVideoInfo->bmiHeader.biWidth / (float)pVideoInfo->bmiHeader.biHeight);
											mi->strInfo << _T("   ") << GetResString(IDS_ASPECTRATIO) << _T(":\t") << fAspectRatio << _T("  (") << GetKnownAspectRatioDisplayString(fAspectRatio) << _T(")\n");

											if (pVideoInfo->AvgTimePerFrame) {
												float fFrameRate = 1.0f / (pVideoInfo->AvgTimePerFrame / 10000000.0f);
												mi->strInfo << _T("   ") << GetResString(IDS_FPS) << _T(":\t") << fFrameRate << ("\n");
											}
										}
									}
								}
								if (mi->iVideoStreams == 1) {
									if (mi->video.dwBitRate == 0) {
										DWORD dwValue;
										if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwValue))
											mi->video.dwBitRate = dwValue;
									}
								}
							} else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Audio) {
								bHaveStreamInfo = true;
								mi->iAudioStreams++;
								if (dwStreamInfoSize >= sizeof(WM_STREAM_TYPE_INFO) + sizeof(WAVEFORMATEX)
									&& pStreamTypeInfo->cbFormat >= sizeof(WAVEFORMATEX))
								{
									const WAVEFORMATEX *pWaveFormatEx = (WAVEFORMATEX*)(pStreamTypeInfo + 1);
									ASSERT(sizeof(WAVEFORMAT) == sizeof(WAVEFORMATEX) - sizeof(WORD) * 2);
									if (mi->iAudioStreams == 1) {
										mi->audio = *(const WAVEFORMAT*)pWaveFormatEx;
										mi->strAudioFormat = GetAudioFormatName(pWaveFormatEx->wFormatTag);
									} else if (bFullInfo && mi->strInfo.m_hWnd) {
										if (!mi->strInfo.IsEmpty())
											mi->strInfo << _T("\n");
										mi->OutputFileName();
										mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
										mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams << _T("\n");
										mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetAudioFormatName(pWaveFormatEx->wFormatTag) << _T("\n");

										if (pWaveFormatEx->nAvgBytesPerSec) {
											CString strBitrate;
											if (pWaveFormatEx->nAvgBytesPerSec == _UI32_MAX)
												strBitrate = _T("Variable");
											else
												strBitrate.Format(_T("%u %s"), (UINT)((pWaveFormatEx->nAvgBytesPerSec * 8 + 500) / 1000), (LPCTSTR)GetResString(IDS_KBITSSEC));
											mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << strBitrate << _T("\n");
										}

										if (pWaveFormatEx->nChannels) {
											mi->strInfo << _T("   ") << GetResString(IDS_CHANNELS) << _T(":\t");
											if (pWaveFormatEx->nChannels == 1)
												mi->strInfo << _T("1 (Mono)");
											else if (pWaveFormatEx->nChannels == 2)
												mi->strInfo << _T("2 (Stereo)");
											else if (pWaveFormatEx->nChannels == 5)
												mi->strInfo << _T("5.1 (Surround)");
											else
												mi->strInfo << pWaveFormatEx->nChannels;
											mi->strInfo << _T("\n");
										}

										if (pWaveFormatEx->nSamplesPerSec)
											mi->strInfo << _T("   ") << GetResString(IDS_SAMPLERATE) << _T(":\t") << pWaveFormatEx->nSamplesPerSec / 1000.0 << _T(" kHz\n");

										if (pWaveFormatEx->wBitsPerSample)
											mi->strInfo << _T("   Bit/sample:\t") << pWaveFormatEx->wBitsPerSample << _T(" Bit\n");
									}
								}
								if (mi->iAudioStreams == 1 && mi->audio.nAvgBytesPerSec == 0) {
									DWORD dwValue;
									if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwValue))
										mi->audio.nAvgBytesPerSec = dwValue / 8;
								}
							} else if (bFullInfo && mi->strInfo.m_hWnd) {
								bHaveStreamInfo = true;
								mi->OutputFileName();
								if (!mi->strInfo.IsEmpty())
									mi->strInfo << _T("\n");
								mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
								if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Script)
									mi->strInfo << _T("Script Stream #") << (UINT)wStream << _T("\n");
								else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Image)
									mi->strInfo << _T("Image Stream #") << (UINT)wStream << _T("\n");
								else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_FileTransfer)
									mi->strInfo << _T("File Transfer Stream #") << (UINT)wStream << _T("\n");
								else if (pStreamTypeInfo->guidMajorType == WMMEDIATYPE_Text)
									mi->strInfo << _T("Text Stream #") << (UINT)wStream << _T("\n");
								else
									mi->strInfo << _T("Unknown Stream #") << (UINT)wStream << _T("\n");

								DWORD dwValue;
								if (GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwValue) && dwValue != 0)
									mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwValue + 500) / 1000) << _T(" kbit/s\n");
							}
						}
					}

					if (wStream > 0 && !bHaveStreamInfo) {
						WMT_CODEC_INFO_TYPE streamCodecType = WMT_CODECINFO_UNKNOWN;
						CString strStreamType;
						CString strStreamInfo;
						CString strDevTempl;
						if (GetAttribute(pIWMHeaderInfo, wStream, g_wszDeviceConformanceTemplate, strDevTempl)) {
							UINT uStreamType = 0;
							strStreamInfo = strDevTempl + _T(": ");
							if (strDevTempl == _T("L")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("All bit rates");
							} else if (strDevTempl == _T("L1")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("64 - 160 kbit/s");
							} else if (strDevTempl == _T("L2")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 160 kbit/s");
							} else if (strDevTempl == _T("L3")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 384 kbit/s");
							} else if (strDevTempl == _T("S1")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 20 kbit/s");
							} else if (strDevTempl == _T("S2")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 20 kbit/s");
							} else if (strDevTempl == _T("M")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("All bit rates");
							} else if (strDevTempl == _T("M1")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 384 kbit/s, <= 48 kHz");
							} else if (strDevTempl == _T("M2")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 768 kbit/s, <= 96 kHz");
							} else if (strDevTempl == _T("M3")) {
								streamCodecType = WMT_CODECINFO_AUDIO;
								uStreamType = IDS_AUDIO;
								strStreamInfo += _T("<= 1500 kbit/s, <= 96 kHz");
							} else if (strDevTempl == _T("SP@LL")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Simple Profile, Low Level, <= 176 x 144, <= 96 kbit/s");
							} else if (strDevTempl == _T("SP@ML")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Simple Profile, Medium Level, <= 352 x 288, <= 384 kbit/s");
							} else if (strDevTempl == _T("MP@LL")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Main Profile, Low Level, <= 352 x 288, 2 Mbit/s");
							} else if (strDevTempl == _T("MP@ML")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Main Profile, Medium Level, <= 720 x 576, 10 Mbit/s");
							} else if (strDevTempl == _T("MP@HL")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Main Profile, High Level, <= 1920 x 1080, 20 Mbit/s");
							} else if (strDevTempl == _T("CP")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Complex Profile");
							} else if (strDevTempl == _T("I1")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Video Image Level 1, <= 352 x 288, 192 kbit/s");
							} else if (strDevTempl == _T("I2")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Video Image Level 2, <= 1024 x 768, 384 kbit/s");
							} else if (strDevTempl == _T("I")) {
								streamCodecType = WMT_CODECINFO_VIDEO;
								uStreamType = IDS_VIDEO;
								strStreamInfo += _T("Generic Video Image");
							}
							strStreamType = GetResString(uStreamType);
						}

						DWORD dwCodecId = _UI32_MAX;
						CString strCodecName;
						CString strCodecDesc;
						if (streamCodecType != WMT_CODECINFO_UNKNOWN) {
							CComQIPtr<IWMHeaderInfo2> pIWMHeaderInfo2 = pIUnkReader;
							if (pIWMHeaderInfo2) {
								DWORD dwCodecInfos;
								if ((hr = pIWMHeaderInfo2->GetCodecInfoCount(&dwCodecInfos)) == S_OK) {
									for (DWORD dwCodec = 0; dwCodec < dwCodecInfos; ++dwCodec) {
										WORD wNameSize;
										WORD wDescSize;
										WORD wCodecInfoSize;
										WMT_CODEC_INFO_TYPE codecType;
										hr = pIWMHeaderInfo2->GetCodecInfo(dwCodec, &wNameSize, NULL, &wDescSize, NULL, &codecType, &wCodecInfoSize, NULL);
										if (hr == S_OK && codecType == streamCodecType) {
											CString strName;
											CString strDesc;
											CTempBuffer<BYTE> aCodecInfo;
											hr = pIWMHeaderInfo2->GetCodecInfo(dwCodec, &wNameSize, bFullInfo ? strName.GetBuffer(wNameSize) : NULL, &wDescSize, bFullInfo ? strDesc.GetBuffer(wDescSize) : NULL, &codecType, &wCodecInfoSize, aCodecInfo.Allocate(wCodecInfoSize));
											strName.ReleaseBuffer();
											strName.Trim();
											strDesc.ReleaseBuffer();
											strDesc.Trim();
											if (hr == S_OK) {
												strCodecName = strName;
												strCodecDesc = strDesc;
												if (codecType == WMT_CODECINFO_AUDIO) {
													if (wCodecInfoSize == sizeof(WORD))
														dwCodecId = *(WORD*)(BYTE*)aCodecInfo;
													else
														ASSERT(0);
												} else if (codecType == WMT_CODECINFO_VIDEO) {
													if (wCodecInfoSize == sizeof(DWORD))
														dwCodecId = *(DWORD*)(BYTE*)aCodecInfo;
													else
														ASSERT(0);
												} else
													ASSERT(0);
											} else
												ASSERT(0);
											break;
										}
									}
								}
							}
						}

						// Depending on the installed WMFSDK version and depending on the WMFSDK which
						// was used to create the WM file, we still may be missing the stream type info.
						// So, don't bother with printing 'Unknown' info.
						if (!strStreamType.IsEmpty()) {
							DWORD dwBitrate = 0;
							GetAttributeEx(pIWMHeaderInfo3, wStream, g_wszWMPeakBitrate, dwBitrate);

							if (streamCodecType == WMT_CODECINFO_VIDEO) {
								mi->iVideoStreams++;
								if (mi->iVideoStreams == 1) {
									mi->video.bmiHeader.biCompression = dwCodecId;
									mi->strVideoFormat = GetVideoFormatName(dwCodecId);
									mi->video.dwBitRate = dwBitrate;
									if (bFullInfo && mi->strInfo.m_hWnd) {
										mi->OutputFileName();
										if (!mi->strInfo.IsEmpty())
											mi->strInfo << _T("\n");
										mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
										mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams << _T("\n");
										if (!strCodecName.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
										if (!strCodecDesc.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
										if (!strStreamInfo.IsEmpty())
											mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									}
								} else if (bFullInfo && mi->strInfo.m_hWnd) {
									mi->OutputFileName();
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_VIDEO) << _T(" #") << mi->iVideoStreams << _T("\n");
									mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetVideoFormatName(dwCodecId) << _T("\n");
									if (!strCodecName.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
									if (!strCodecDesc.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
									if (!strStreamInfo.IsEmpty())
										mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									if (dwBitrate)
										mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwBitrate + 500) / 1000) << _T(" kbit/s\n");
								}
							} else if (streamCodecType == WMT_CODECINFO_AUDIO) {
								mi->iAudioStreams++;
								if (mi->iAudioStreams == 1) {
									mi->audio.wFormatTag = (WORD)dwCodecId;
									mi->strAudioFormat = GetAudioFormatName((WORD)dwCodecId);
									mi->audio.nAvgBytesPerSec = dwBitrate / 8;
									if (bFullInfo && mi->strInfo.m_hWnd) {
										if (!mi->strInfo.IsEmpty())
											mi->strInfo << _T("\n");
										mi->OutputFileName();
										mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
										mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams << _T("\n");
										if (!strCodecName.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
										if (!strCodecDesc.IsEmpty())
											mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
										if (!strStreamInfo.IsEmpty())
											mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									}
								} else if (bFullInfo && mi->strInfo.m_hWnd) {
									if (!mi->strInfo.IsEmpty())
										mi->strInfo << _T("\n");
									mi->OutputFileName();
									mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
									mi->strInfo << GetResString(IDS_AUDIO) << _T(" #") << mi->iAudioStreams << _T("\n");
									mi->strInfo << _T("   ") << GetResString(IDS_CODEC) << _T(":\t") << GetAudioFormatName((WORD)dwCodecId) << _T("\n");
									if (!strCodecName.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strCodecName << _T("\n");
									if (!strCodecDesc.IsEmpty())
										mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strCodecDesc << _T("\n");
									if (!strStreamInfo.IsEmpty())
										mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
									if (dwBitrate)
										mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwBitrate + 500) / 1000) << _T(" kbit/s\n");
								}
							} else if (bFullInfo && mi->strInfo.m_hWnd) {
								mi->OutputFileName();
								if (!mi->strInfo.IsEmpty())
									mi->strInfo << _T("\n");
								mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
								mi->strInfo << _T("Stream #") << (UINT)wStream << _T(": ") << strStreamType << _T("\n");
								if (!strStreamInfo.IsEmpty())
									mi->strInfo << _T("   Device Conformance:\t") << strStreamInfo << _T("\n");
								if (dwBitrate)
									mi->strInfo << _T("   ") << GetResString(IDS_BITRATE) << _T(":\t") << (UINT)((dwBitrate + 500) / 1000) << _T(" kbit/s\n");
							}
						}
					}

					++wStream;
				}

				// 'IWMHeaderInfo3' may not have returned any 'WM/StreamTypeInfo' attributes. If the WM file
				// indicates the existence of Audio/Video streams, try to query for the codec info.
				//
				if (mi->iAudioStreams == 0 && mi->iVideoStreams == 0) {
					BOOL bHasAudio = FALSE;
					GetAttribute(pIWMHeaderInfo, 0, g_wszWMHasAudio, bHasAudio);
					BOOL bHasVideo = FALSE;
					GetAttribute(pIWMHeaderInfo, 0, g_wszWMHasVideo, bHasVideo);
					if (bHasAudio || bHasVideo) {
						CComQIPtr<IWMHeaderInfo2> pIWMHeaderInfo2 = pIUnkReader;
						if (pIWMHeaderInfo2) {
							DWORD dwCodecInfos;
							if ((hr = pIWMHeaderInfo2->GetCodecInfoCount(&dwCodecInfos)) == S_OK) {
								bool bAddedBakedAudioStream = false;
								bool bAddedBakedVideoStream = false;
								for (DWORD dwCodec = 0; dwCodec < dwCodecInfos; ++dwCodec) {
									WORD wNameSize;
									WORD wDescSize;
									WORD wCodecInfoSize;
									WMT_CODEC_INFO_TYPE codecType;
									hr = pIWMHeaderInfo2->GetCodecInfo(dwCodec, &wNameSize, NULL, &wDescSize, NULL, &codecType, &wCodecInfoSize, NULL);
									if (hr == S_OK) {
										CString strName;
										CString strDesc;
										CTempBuffer<BYTE> aCodecInfo;
										hr = pIWMHeaderInfo2->GetCodecInfo(dwCodec,
											&wNameSize, bFullInfo ? strName.GetBuffer(wNameSize) : NULL,
											&wDescSize, bFullInfo ? strDesc.GetBuffer(wDescSize) : NULL,
											&codecType, &wCodecInfoSize, aCodecInfo.Allocate(wCodecInfoSize));
										strName.ReleaseBuffer();
										strName.Trim();
										strDesc.ReleaseBuffer();
										strDesc.Trim();
										if (hr == S_OK) {
											if (bHasAudio && codecType == WMT_CODECINFO_AUDIO) {
												if (wCodecInfoSize == sizeof(WORD)) {
													if (!bAddedBakedAudioStream) {
														bAddedBakedAudioStream = true;
														mi->iAudioStreams++;
														mi->audio.wFormatTag = *(WORD*)(BYTE*)aCodecInfo;
														mi->strAudioFormat = GetAudioFormatName(mi->audio.wFormatTag);
														if (bFullInfo && mi->strInfo.m_hWnd && (!strName.IsEmpty() || !strDesc.IsEmpty())) {
															mi->OutputFileName();
															if (!mi->strInfo.IsEmpty())
																mi->strInfo << _T("\n");
															mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
															mi->strInfo << GetResString(IDS_AUDIO) << _T("\n");
															if (!strName.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strName << _T("\n");
															if (!strDesc.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strDesc << _T("\n");
														}
													} else
														ASSERT(0);
												} else if (wCodecInfoSize == 0 && dwContainer == WMT_Storage_Format_MP3) {
													// MP3 files: no codec info returned
													if (!bAddedBakedAudioStream) {
														bAddedBakedAudioStream = true;
														mi->iAudioStreams++;
														mi->audio.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
														mi->strAudioFormat = GetAudioFormatName(mi->audio.wFormatTag);
														if (bFullInfo && mi->strInfo.m_hWnd && (!strName.IsEmpty() || !strDesc.IsEmpty())) {
															mi->OutputFileName();
															if (!mi->strInfo.IsEmpty())
																mi->strInfo << _T("\n");
															mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
															mi->strInfo << GetResString(IDS_AUDIO) << _T("\n");
															if (!strName.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strName << _T("\n");
															if (!strDesc.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strDesc << _T("\n");
														}
													} else
														ASSERT(0);
												} else
													ASSERT(0);
											} else if (bHasVideo && codecType == WMT_CODECINFO_VIDEO) {
												if (wCodecInfoSize == sizeof(DWORD)) {
													if (!bAddedBakedVideoStream) {
														bAddedBakedVideoStream = true;
														mi->iVideoStreams++;
														mi->video.bmiHeader.biCompression = *(DWORD*)(BYTE*)aCodecInfo;
														mi->strVideoFormat = GetVideoFormatName(mi->video.bmiHeader.biCompression);
														if (bFullInfo && mi->strInfo.m_hWnd && (!strName.IsEmpty() || !strDesc.IsEmpty())) {
															mi->OutputFileName();
															if (!mi->strInfo.IsEmpty())
																mi->strInfo << _T("\n");
															mi->strInfo.SetSelectionCharFormat(mi->strInfo.m_cfBold);
															mi->strInfo << GetResString(IDS_VIDEO) << _T("\n");
															if (!strName.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_SW_NAME) << _T(":\t") << strName << _T("\n");
															if (!strDesc.IsEmpty())
																mi->strInfo << _T("   ") << GetResString(IDS_DESCRIPTION) << _T(":\t") << strDesc << _T("\n");
														}
													} else
														ASSERT(0);
												} else
													ASSERT(0);
											} else
												ASSERT(0);
										}
									}
								}
							}
						}
					}
				}

				if (dwContainer == WMT_Storage_Format_V1) {
					if (mi->iAudioStreams == 1 && mi->iVideoStreams == 0 && (mi->audio.nAvgBytesPerSec == 0 || mi->audio.nAvgBytesPerSec == _UI32_MAX)) {
						DWORD dwCurrentBitrate;
						if (GetAttribute(pIWMHeaderInfo, 0, g_wszWMCurrentBitrate, dwCurrentBitrate) && dwCurrentBitrate)
							mi->audio.nAvgBytesPerSec = dwCurrentBitrate / 8;
					} else if (mi->iAudioStreams == 0 && mi->iVideoStreams == 1 && (mi->video.dwBitRate == 0 || mi->video.dwBitRate == _UI32_MAX)) {
						DWORD dwCurrentBitrate;
						if (GetAttribute(pIWMHeaderInfo, 0, g_wszWMCurrentBitrate, dwCurrentBitrate) && dwCurrentBitrate)
							mi->video.dwBitRate = dwCurrentBitrate;
					}
				} else if (dwContainer == WMT_Storage_Format_MP3) {
					if (mi->iAudioStreams == 1 && mi->iVideoStreams == 0 && (mi->audio.nAvgBytesPerSec == 0 || mi->audio.nAvgBytesPerSec == _UI32_MAX)) {
						// CBR MP3 stream: The average bit rate is equal to the nominal bit rate
						//
						// VBR MP3 stream: The average bit rate is usually not equal to the nominal
						// bit rate (sometimes even quite way off). However, the average bit rate is
						// always a reasonable value, thus it is the preferred value for the bit rate.
						//
						// MP3 streams which are enveloped in a WAV file: some WM bit rates are simply 0!
						//
						// Conclusion: For MP3 files always prefer the average bit rate, if available.
						//
						DWORD dwCurrentBitrate;
						if (GetAttribute(pIWMHeaderInfo, 0, g_wszWMCurrentBitrate, dwCurrentBitrate) && dwCurrentBitrate)
							mi->audio.nAvgBytesPerSec = dwCurrentBitrate / 8;
					}
				}
			}
		}
	} catch (CException *ex) {
		ASSERT(ex->IsKindOf(RUNTIME_CLASS(CNotSupportedException)));
		ex->Delete();
	}

	if (pIUnkReader) {
		CComQIPtr<IWMMetadataEditor> pIWMMetadataEditor(pIUnkReader);
		if (pIWMMetadataEditor)
			VERIFY(pIWMMetadataEditor->Close() == S_OK);
	}
	if (pIUnkReader) {
		CComQIPtr<IWMSyncReader> pIWMSyncReader(pIUnkReader);
		if (pIWMSyncReader)
			VERIFY(pIWMSyncReader->Close() == S_OK);
	}

	return mi->iAudioStreams > 0
		|| mi->iVideoStreams > 0
		|| mi->fFileLengthSec > 0
		|| !mi->strAlbum.IsEmpty()
		|| !mi->strAuthor.IsEmpty()
		|| !mi->strTitle.IsEmpty();
}

#endif//HAVE_WMSDK_H

bool GetMimeType(LPCTSTR pszFilePath, CString &rstrMimeType)
{
	int fd = _topen(pszFilePath, O_RDONLY | O_BINARY);
	if (fd != -1) {
		BYTE aucBuff[8192];
		int iRead = _read(fd, aucBuff, sizeof aucBuff);

		_close(fd);

		if (iRead > 0) {
			// Supports only 26 hardcoded types - and some more (undocumented)
			// ---------------------------------------------------------------
			// x text/richtext					.rtf
			// x text/html						.html
			// x text/xml						.xml
			// . audio/x-aiff
			// . audio/basic
			// x audio/wav						.wav
			// x audio/mid						.mid
			// x image/gif						.gif
			// . image/jpeg						( never seen, all .jpg files are "image/pjpeg" )
			// x image/pjpeg
			// . image/tiff						( never seen, all .tif files failed ??? )
			// x image/x-png					.png
			// . image/x-xbitmap
			// x image/bmp						.bmp
			// . image/x-jg
			// x image/x-emf					.emf
			// x image/x-wmf					.wmf
			// x video/avi						.avi
			// x video/mpeg						.mpg
			// x application/postscript			.ps
			// x application/base64				.b64
			// x application/macbinhex40		.hqx
			// x application/pdf				.pdf
			// . application/x-compressed
			// x application/x-zip-compressed	.zip
			// x application/x-gzip-compressed	.gz
			// x application/java				.class
			// x application/x-msdownload		.exe .dll
			//
#define FMFD_DEFAULT				0x00000000	// No flags specified. Use default behavior for the function.
#define FMFD_URLASFILENAME			0x00000001	// Treat the specified pwzUrl as a file name.
#ifndef FMFD_ENABLEMIMESNIFFING
#define FMFD_ENABLEMIMESNIFFING		0x00000002	// XP SP2 - Use MIME-type detection even if FEATURE_MIME_SNIFFING is detected. Usually, this feature control key would disable MIME-type detection.
#define FMFD_IGNOREMIMETEXTPLAIN	0x00000004	// XP SP2 - Perform MIME-type detection if "text/plain" is proposed, even if data sniffing is otherwise disabled.
#endif
			// Don't pass the file name to 'FindMimeFromData'. In case 'FindMimeFromData' can not determine the MIME type
			// from sniffing the header data it will parse the passed file name's extension to guess the MIME type.
			// That's basically OK for browser mode, but we can't use that here.
			LPWSTR pwszMime = NULL;
			HRESULT hr = FindMimeFromData(NULL, NULL/*pszFilePath*/, aucBuff, iRead, NULL, FMFD_ENABLEMIMESNIFFING | FMFD_IGNOREMIMETEXTPLAIN, &pwszMime, 0);
			// "application/octet-stream"	... means general "binary" file
			// "text/plain"					... means general "text" file
			if (SUCCEEDED(hr) && pwszMime != NULL && wcscmp(pwszMime, L"application/octet-stream") != 0) {
				rstrMimeType = pwszMime;
				::CoTaskMemFree(pwszMime);
				return true;
			}
			::CoTaskMemFree(pwszMime);

			// RAR file type
			if (iRead >= 7 && aucBuff[0] == 0x52) {
				if ((aucBuff[1] == 0x45 && aucBuff[2] == 0x7e && aucBuff[3] == 0x5e)
					|| (aucBuff[1] == 0x61 && aucBuff[2] == 0x72 && aucBuff[3] == 0x21 && aucBuff[4] == 0x1a && aucBuff[5] == 0x07 && aucBuff[6] == 0x00))
				{
					rstrMimeType = _T("application/x-rar-compressed");
					return true;
				}
			}

			// bzip (BZ2) file type
			if (aucBuff[0] == 'B' && aucBuff[1] == 'Z' && aucBuff[2] == 'h' && (aucBuff[3] >= '1' && aucBuff[3] <= '9')) {
				rstrMimeType = _T("application/x-bzip-compressed");
				return true;
			}

			// ACE file type
			static const char _cACEheader[] = "**ACE**";
			if ((unsigned)iRead >= 7 + _countof(_cACEheader) - 1 && memcmp(&aucBuff[7], _cACEheader, _countof(_cACEheader) - 1) == 0) {
				rstrMimeType = _T("application/x-ace-compressed");
				return true;
			}

			// LHA/LZH file type
			static const char _cLZHheader[] = "-lh5-";
			if ((unsigned)iRead >= 2 + _countof(_cLZHheader) - 1 && memcmp(&aucBuff[2], _cLZHheader, _countof(_cLZHheader) - 1) == 0) {
				rstrMimeType = _T("application/x-lha-compressed");
				return true;
			}
		}
	}
	return false;
}