/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Note To Mods //
/*
Please do not change anything here and release it.
There is going to be a new forum created just for the Kademlia side of the client.
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it. If it is a real improvement,
it will be added to the official client. Changing something without knowing
what all it does can cause great harm to the network if released in mass form.
Any mod that changes anything within the Kademlia side will not be allowed to advertise
their client on the eMule forum.
*/
#include "stdafx.h"
#include "Preferences.h"
#include "Log.h"
#include "kademlia/kademlia/Entry.h"
#include "kademlia/kademlia/Indexed.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/io/BufferedFileIO.h"
#include "kademlia/io/ByteIO.h"
#include "kademlia/io/IOException.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "kademlia/utils/KadUDPKey.h"
#include "kademlia/utils/MiscUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

//void DebugSend(LPCTSTR pszMsg, uint32 uIP, uint16 uPort);

CString CIndexed::m_sKeyFileName;
CString CIndexed::m_sSourceFileName;
CString CIndexed::m_sLoadFileName;

static LPCTSTR const strLoadingInProgress = _T("CIndexed member function call failed because the data loading still in progress");

CIndexed::CIndexed()
	: m_uTotalIndexSource()
	, m_uTotalIndexKeyword()
	, m_uTotalIndexNotes()
	, m_uTotalIndexLoad()
	, m_bAbortLoading()
	, m_bDataLoaded()
{
	m_mapKeyword.InitHashTable(1031);
	m_mapNotes.InitHashTable(1031);
	m_mapLoad.InitHashTable(1031);
	m_mapSources.InitHashTable(1031);
	const CString &confdir = thePrefs.GetMuleDirectory(EMULE_CONFIGDIR);
	m_sSourceFileName = confdir + _T("src_index.dat");
	m_sKeyFileName = confdir + _T("key_index.dat");
	m_sLoadFileName = confdir + _T("load_index.dat");
	m_tNextClean = time(NULL) + MIN2S(30);
	ReadFile();
}

void CIndexed::ReadFile()
{
	m_bAbortLoading = false;
	CLoadDataThread *pLoadDataThread = static_cast<CLoadDataThread*>(AfxBeginThread(RUNTIME_CLASS(CLoadDataThread), THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED));
	pLoadDataThread->SetValues(this);
	pLoadDataThread->ResumeThread();
}

CIndexed::~CIndexed()
{
	if (!m_bDataLoaded) {
		// the user clicked on disconnect/close just after he started kad (and probably just before posting in the forum that emule doesn't works :P )
		// while the loading thread is still busy. First tell the thread to abort its loading, afterwards wait for it to terminate
		// and then delete all loaded items without writing them to the files (as they are incomplete and unchanged)
		m_bAbortLoading = true;
		DebugLogWarning(_T("Kad stopping while still loading CIndexed data, waiting for abort"));
		CSingleLock sLock(&m_mutSync, TRUE); // wait
		ASSERT(m_bDataLoaded);

		// cleanup without storing
		CCKey key1;
		for (POSITION pos = m_mapSources.GetStartPosition(); pos != NULL;) {
			SrcHash *pCurrSrcHash;
			m_mapSources.GetNextAssoc(pos, key1, pCurrSrcHash);
			CKadSourcePtrList *keyHashSrcMap = &pCurrSrcHash->ptrlistSource;
			for (POSITION pos2 = keyHashSrcMap->GetHeadPosition(); pos2 != NULL;) {
				Source *pCurrSource = keyHashSrcMap->GetNext(pos2);
				CKadEntryPtrList *srcEntryList = &pCurrSource->ptrlEntryList;
				while (!srcEntryList->IsEmpty())
					delete srcEntryList->RemoveHead();
				delete pCurrSource;
			}
			delete pCurrSrcHash;
		}

		for (POSITION pos = m_mapLoad.GetStartPosition(); pos != NULL;) {
			Load *pLoad;
			m_mapLoad.GetNextAssoc(pos, key1, pLoad);
			delete pLoad;
		}

		for (POSITION pos = m_mapKeyword.GetStartPosition(); pos != NULL;) {
			KeyHash *pCurrKeyHash;
			m_mapKeyword.GetNextAssoc(pos, key1, pCurrKeyHash);
			CSourceKeyMap *keySrcKeyMap = &pCurrKeyHash->mapSource;
			CCKey key2;
			for (POSITION pos2 = keySrcKeyMap->GetStartPosition(); pos2 != NULL;) {
				Source *pCurrSource;
				keySrcKeyMap->GetNextAssoc(pos2, key2, pCurrSource);
				for (CKadEntryPtrList *srcEntryList = &pCurrSource->ptrlEntryList; !srcEntryList->IsEmpty();) {
					CKeyEntry *pCurrName = static_cast<CKeyEntry*>(srcEntryList->RemoveHead());
					ASSERT(pCurrName->IsKeyEntry());
					pCurrName->DirtyDeletePublishData();
					delete pCurrName;
				}
				delete pCurrSource;
			}
			delete pCurrKeyHash;
		}
	} else {
		// standard store and cleanup
		try {
			uint32 uTotalSource = 0;
			uint32 uTotalKey = 0;
			uint32 uTotalLoad = 0;

			CBufferedFileIO fileLoad;
			if (fileLoad.Open(m_sLoadFileName, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite)) {
				setvbuf(fileLoad.m_pStream, NULL, _IOFBF, 32768);
				static const uint32 uVersion = 1;
				fileLoad.WriteUInt32(uVersion);
				fileLoad.WriteUInt32((uint32)time(NULL));
				fileLoad.WriteUInt32((uint32)m_mapLoad.GetCount());
				CCKey key1;
				for (POSITION pos = m_mapLoad.GetStartPosition(); pos != NULL;) {
					Load *pLoad;
					m_mapLoad.GetNextAssoc(pos, key1, pLoad);
					fileLoad.WriteUInt128(pLoad->uKeyID);
					fileLoad.WriteUInt32((uint32)pLoad->uTime);
					++uTotalLoad;
					delete pLoad;
				}
				fileLoad.Close();
			} else
				DebugLogError(_T("Unable to store Kad file: %s"), (LPCTSTR)m_sLoadFileName);

			CBufferedFileIO fileSource;
			if (fileSource.Open(m_sSourceFileName, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite)) {
				setvbuf(fileSource.m_pStream, NULL, _IOFBF, 32768);
				static const uint32 uVersion = 2;
				fileSource.WriteUInt32(uVersion);
				fileSource.WriteUInt32((uint32)(time(NULL) + KADEMLIAREPUBLISHTIMES));
				fileSource.WriteUInt32((uint32)m_mapSources.GetCount());
				CCKey key1;
				for (POSITION pos = m_mapSources.GetStartPosition(); pos != NULL;) {
					SrcHash *pCurrSrcHash;
					m_mapSources.GetNextAssoc(pos, key1, pCurrSrcHash);
					fileSource.WriteUInt128(pCurrSrcHash->uKeyID);
					CKadSourcePtrList *keyHashSrcMap = &pCurrSrcHash->ptrlistSource;
					fileSource.WriteUInt32((uint32)keyHashSrcMap->GetCount());
					while (!keyHashSrcMap->IsEmpty()) {
						Source *pCurrSource = keyHashSrcMap->RemoveHead();
						fileSource.WriteUInt128(pCurrSource->uSourceID);
						CKadEntryPtrList *srcEntryList = &pCurrSource->ptrlEntryList;
						fileSource.WriteUInt32((uint32)srcEntryList->GetCount());
						while (!srcEntryList->IsEmpty()) {
							CEntry *pCurrName = srcEntryList->RemoveHead();
							fileSource.WriteUInt32((uint32)pCurrName->m_tLifetime);
							pCurrName->WriteTagList(&fileSource);
							delete pCurrName;
							++uTotalSource;
						}
						delete pCurrSource;
					}
					delete pCurrSrcHash;
				}
				fileSource.Close();
			} else
				DebugLogError(_T("Unable to store Kad file: %s"), (LPCTSTR)m_sSourceFileName);

			CBufferedFileIO fileKey;
			if (fileKey.Open(m_sKeyFileName, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite)) {
				setvbuf(fileKey.m_pStream, NULL, _IOFBF, 32768);
				uint32 uVersion = 4;
				fileKey.WriteUInt32(uVersion);
				fileKey.WriteUInt32((uint32)(time(NULL) + KADEMLIAREPUBLISHTIMEK));
				fileKey.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
				fileKey.WriteUInt32((uint32)m_mapKeyword.GetCount());
				CCKey key1, key2;
				for (POSITION pos = m_mapKeyword.GetStartPosition(); pos != NULL;) {
					KeyHash *pCurrKeyHash;
					m_mapKeyword.GetNextAssoc(pos, key1, pCurrKeyHash);
					fileKey.WriteUInt128(pCurrKeyHash->uKeyID);
					CSourceKeyMap *keySrcKeyMap = &pCurrKeyHash->mapSource;
					fileKey.WriteUInt32((uint32)keySrcKeyMap->GetCount());
					for (POSITION pos2 = keySrcKeyMap->GetStartPosition(); pos2 != NULL;) {
						Source *pCurrSource;
						keySrcKeyMap->GetNextAssoc(pos2, key2, pCurrSource);
						fileKey.WriteUInt128(pCurrSource->uSourceID);
						CKadEntryPtrList *srcEntryList = &pCurrSource->ptrlEntryList;
						fileKey.WriteUInt32((uint32)srcEntryList->GetCount());
						while (!srcEntryList->IsEmpty()) {
							CKeyEntry *pCurrName = static_cast<CKeyEntry*>(srcEntryList->RemoveHead());
							ASSERT(pCurrName->IsKeyEntry());
							fileKey.WriteUInt32((uint32)pCurrName->m_tLifetime);
							pCurrName->WritePublishTrackingDataToFile(&fileKey);
							pCurrName->WriteTagList(&fileKey);
							pCurrName->DirtyDeletePublishData();
							delete pCurrName;
							++uTotalKey;
						}
						delete pCurrSource;
					}
					delete pCurrKeyHash;
				}
				fileKey.Close();
			} else
				DebugLogError(_T("Unable to store Kad file: %s"), (LPCTSTR)m_sKeyFileName);

			AddDebugLogLine(false, _T("Wrote %u source, %u keyword, and %u load entries"), uTotalSource, uTotalKey, uTotalLoad);


		} catch (CIOException *ioe) {
			AddDebugLogLine(false, _T("Exception in CIndexed::~CIndexed (IO error(%i))"), ioe->m_iCause);
			ioe->Delete();
		} catch (...) {
			AddDebugLogLine(false, _T("Exception in CIndexed::~CIndexed"));
		}
	}

	// leftover cleanup (same for both variants)
	CKeyEntry::ResetGlobalTrackingMap();
	CCKey key1;
	for (POSITION pos = m_mapNotes.GetStartPosition(); pos != NULL;) {
		SrcHash *pCurrNoteHash;
		m_mapNotes.GetNextAssoc(pos, key1, pCurrNoteHash);
		for (CKadSourcePtrList &keyHashNoteMap = pCurrNoteHash->ptrlistSource; !keyHashNoteMap.IsEmpty();) {
			Source *pCurrNote = keyHashNoteMap.RemoveHead();
			CKadEntryPtrList &noteEntryList = pCurrNote->ptrlEntryList;
			while (!noteEntryList.IsEmpty())
				delete noteEntryList.RemoveHead();
			delete pCurrNote;
		}
		delete pCurrNoteHash;
	}
}

void CIndexed::Clean()
{
	static LONG cleaning = 0;
	if (InterlockedExchange(&cleaning, 1))
		return; //already cleaning
	time_t tNow = time(NULL);
	if (tNow < m_tNextClean) {
		InterlockedExchange(&cleaning, 0);
		return;
	}

	try {
		uint32 uRemovedKey = 0;
		uint32 uRemovedSource = 0;
		uint32 uTotalSource = 0;
		uint32 uTotalKey = 0;

		CCKey key1, key2;
		for (POSITION pos = m_mapKeyword.GetStartPosition(); pos != NULL;) {
			KeyHash *pCurrKeyHash;
			m_mapKeyword.GetNextAssoc(pos, key1, pCurrKeyHash);
			for (POSITION pos2 = pCurrKeyHash->mapSource.GetStartPosition(); pos2 != NULL;) {
				Source *pCurrSource;
				pCurrKeyHash->mapSource.GetNextAssoc(pos2, key2, pCurrSource);
				for (POSITION pos3 = pCurrSource->ptrlEntryList.GetHeadPosition(); pos3 != NULL;) {
					POSITION pos4 = pos3;
					CKeyEntry *pCurrName = static_cast<CKeyEntry*>(pCurrSource->ptrlEntryList.GetNext(pos3));
					ASSERT(pCurrName->IsKeyEntry());
					++uTotalKey;
					if (!pCurrName->m_bSource && tNow >= pCurrName->m_tLifetime) {
						++uRemovedKey;
						pCurrSource->ptrlEntryList.RemoveAt(pos4);
						delete pCurrName;
					} else if (pCurrName->m_bSource)
						ASSERT(0);
					else
						pCurrName->CleanUpTrackedPublishers(); // intern cleanup
				}
				if (pCurrSource->ptrlEntryList.IsEmpty()) {
					pCurrKeyHash->mapSource.RemoveKey(key2);
					delete pCurrSource;
				}
			}
			if (pCurrKeyHash->mapSource.IsEmpty()) {
				m_mapKeyword.RemoveKey(key1);
				delete pCurrKeyHash;
			}
		}

		for (POSITION pos = m_mapSources.GetStartPosition(); pos != NULL;) {
			SrcHash *pCurrSrcHash;
			m_mapSources.GetNextAssoc(pos, key1, pCurrSrcHash);
			for (POSITION pos2 = pCurrSrcHash->ptrlistSource.GetHeadPosition(); pos2 != NULL; ) {
				POSITION pos3 = pos2;
				Source *pCurrSource = pCurrSrcHash->ptrlistSource.GetNext(pos2);
				for (POSITION pos4 = pCurrSource->ptrlEntryList.GetHeadPosition(); pos4 != NULL; ) {
					POSITION pos5 = pos4;
					CEntry *pCurrName = pCurrSource->ptrlEntryList.GetNext(pos4);
					++uTotalSource;
					if (tNow >= pCurrName->m_tLifetime) {
						++uRemovedSource;
						pCurrSource->ptrlEntryList.RemoveAt(pos5);
						delete pCurrName;
					}
				}
				if (pCurrSource->ptrlEntryList.IsEmpty()) {
					pCurrSrcHash->ptrlistSource.RemoveAt(pos3);
					delete pCurrSource;
				}
			}
			if (pCurrSrcHash->ptrlistSource.IsEmpty()) {
				m_mapSources.RemoveKey(key1);
				delete pCurrSrcHash;
			}
		}

		m_uTotalIndexSource = uTotalSource - uRemovedSource;
		m_uTotalIndexKeyword = uTotalKey - uRemovedKey;
		AddDebugLogLine(false, _T("Removed %u keyword out of %u and %u source out of %u"), uRemovedKey, uTotalKey, uRemovedSource, uTotalSource);
		m_tNextClean = time(NULL) + MIN2S(30);
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CIndexed::Clean"));
		ASSERT(0);
	}
	InterlockedExchange(&cleaning, 0);
}

bool CIndexed::AddKeyword(const CUInt128 &uKeyID, const CUInt128 &uSourceID, Kademlia::CKeyEntry *pEntry, uint8 &uLoad, bool bIgnoreThreadLock)
{
	// do not access any data while the loading thread is busy;
	// bIgnoreThreadLock should be only used by CLoadDataThread itself
	if (!bIgnoreThreadLock && !m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return false;
	}

	if (!pEntry)
		return false;

	if (!pEntry->IsKeyEntry()) {
		ASSERT(0);
		return false;
	}

	if (m_uTotalIndexKeyword > KADEMLIAMAXENTRIES) {
		uLoad = 100;
		return false;
	}

	if (!pEntry->m_uSize || pEntry->GetCommonFileName().IsEmpty() || !pEntry->GetTagCount() || time(NULL) >= pEntry->m_tLifetime)
		return false;

	KeyHash *pCurrKeyHash;
	if (!m_mapKeyword.Lookup(CCKey(uKeyID.GetData()), pCurrKeyHash)) {
		Source *pCurrSource = new Source;
		pCurrSource->uSourceID.SetValue(uSourceID);
		pEntry->MergeIPsAndFilenames(NULL); //IpTracking init
		pCurrSource->ptrlEntryList.AddHead(pEntry);
		pCurrKeyHash = new KeyHash;
		pCurrKeyHash->uKeyID.SetValue(uKeyID);
		pCurrKeyHash->mapSource.SetAt(CCKey(pCurrSource->uSourceID.GetData()), pCurrSource);
		m_mapKeyword.SetAt(CCKey(pCurrKeyHash->uKeyID.GetData()), pCurrKeyHash);
		uLoad = 1;
		++m_uTotalIndexKeyword;
		return true;
	}
	INT_PTR uIndexTotal = pCurrKeyHash->mapSource.GetCount();
	if (uIndexTotal > KADEMLIAMAXINDEX) {
		uLoad = 100;
		//Too many entries for this Keyword.
		return false;
	}
	Source *pCurrSource;
	if (pCurrKeyHash->mapSource.Lookup(CCKey(uSourceID.GetData()), pCurrSource)) {
		if (!pCurrSource->ptrlEntryList.IsEmpty()) {
			if (uIndexTotal > KADEMLIAMAXINDEX - 5000) {
				uLoad = 100;
				//We are in a hot node. If we continued to update all the publishes
				//while this index is full, popular files will be the only thing you index.
				return false;
			}
			// also check for size match
			CKeyEntry *pOldEntry = NULL;
			for (POSITION pos = pCurrSource->ptrlEntryList.GetHeadPosition(); pos != NULL;) {
				POSITION pos1 = pos;
				CKeyEntry *pCurEntry = static_cast<CKeyEntry*>(pCurrSource->ptrlEntryList.GetNext(pos));
				ASSERT(pCurEntry->IsKeyEntry());
				if (pCurEntry->m_uSize == pEntry->m_uSize) {
					pOldEntry = pCurEntry;
					pCurrSource->ptrlEntryList.RemoveAt(pos1);
					break;
				}
			}
			pEntry->MergeIPsAndFilenames(pOldEntry); // pOldEntry can be NULL, that's OK and we still need to do this call in this case
			if (pOldEntry == NULL) {
				++m_uTotalIndexKeyword;
				DebugLogWarning(_T("Kad: Indexing: Keywords: Multiple sizes published for file %s"), (LPCTSTR)pEntry->m_uSourceID.ToHexString());
			}
			DEBUG_ONLY(AddDebugLogLine(DLP_VERYLOW, false, _T("Indexed file %s"), (LPCTSTR)pEntry->m_uSourceID.ToHexString()));
			delete pOldEntry;
		} else {
			++m_uTotalIndexKeyword;
			pEntry->MergeIPsAndFilenames(NULL); //IpTracking init
		}
		uLoad = (uint8)((uIndexTotal * 100) / KADEMLIAMAXINDEX);
		pCurrSource->ptrlEntryList.AddHead(pEntry);
	} else {
		pCurrSource = new Source;
		pCurrSource->uSourceID.SetValue(uSourceID);
		pEntry->MergeIPsAndFilenames(NULL); //IpTracking init
		pCurrSource->ptrlEntryList.AddHead(pEntry);
		pCurrKeyHash->mapSource.SetAt(CCKey(pCurrSource->uSourceID.GetData()), pCurrSource);
		++m_uTotalIndexKeyword;
		uLoad = (uint8)((uIndexTotal * 100) / KADEMLIAMAXINDEX);
	}
	return true;
}

bool CIndexed::AddSources(const CUInt128 &uKeyID, const CUInt128 &uSourceID, Kademlia::CEntry *pEntry, uint8 &uLoad, bool bIgnoreThreadLock)
{
	// do not access any data while the loading thread is busy;
	// bIgnoreThreadLock should be only used by CLoadDataThread itself
	if (!bIgnoreThreadLock && !m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return false;
	}

	if (!pEntry
		|| pEntry->m_uIP == 0
		|| pEntry->m_uTCPPort == 0
		|| pEntry->m_uUDPPort == 0
		|| pEntry->GetTagCount() == 0
		|| pEntry->m_tLifetime < time(NULL))
	{
		return false;
	}

	SrcHash *pCurrSrcHash;
	if (!m_mapSources.Lookup(CCKey(uKeyID.GetData()), pCurrSrcHash)) {
		Source *pCurrSource = new Source;
		pCurrSource->uSourceID.SetValue(uSourceID);
		pCurrSource->ptrlEntryList.AddHead(pEntry);
		pCurrSrcHash = new SrcHash;
		pCurrSrcHash->uKeyID.SetValue(uKeyID);
		pCurrSrcHash->ptrlistSource.AddHead(pCurrSource);
		m_mapSources.SetAt(CCKey(pCurrSrcHash->uKeyID.GetData()), pCurrSrcHash);
		++m_uTotalIndexSource;
		uLoad = 1;
		return true;
	}

	INT_PTR uSize = pCurrSrcHash->ptrlistSource.GetCount();
	for (POSITION pos = pCurrSrcHash->ptrlistSource.GetHeadPosition(); pos != NULL; ) {
		Source *pCurrSource = pCurrSrcHash->ptrlistSource.GetNext(pos);
		if (pCurrSource->ptrlEntryList.IsEmpty()) {
			//This should never happen!
			pCurrSource->ptrlEntryList.AddHead(pEntry);
			ASSERT(0);
			uLoad = (uint8)((uSize * 100) / KADEMLIAMAXSOURCEPERFILE);
			++m_uTotalIndexSource;
			return true;
		}
		CEntry *pCurrEntry = pCurrSource->ptrlEntryList.GetHead();
		ASSERT(pCurrEntry != NULL);
		if (pCurrEntry->m_uIP == pEntry->m_uIP && (pCurrEntry->m_uTCPPort == pEntry->m_uTCPPort || pCurrEntry->m_uUDPPort == pEntry->m_uUDPPort)) {
			delete pCurrSource->ptrlEntryList.RemoveHead();
			pCurrSource->ptrlEntryList.AddHead(pEntry);
			uLoad = (uint8)((uSize * 100) / KADEMLIAMAXSOURCEPERFILE);
			return true;
		}
	}
	if (uSize > KADEMLIAMAXSOURCEPERFILE) {
		Source *pCurrSource = pCurrSrcHash->ptrlistSource.RemoveTail();
		delete pCurrSource->ptrlEntryList.RemoveTail();
		pCurrSource->uSourceID.SetValue(uSourceID);
		pCurrSource->ptrlEntryList.AddHead(pEntry);
		pCurrSrcHash->ptrlistSource.AddHead(pCurrSource);
		uLoad = 100;
	} else {
		Source *pCurrSource = new Source;
		pCurrSource->uSourceID.SetValue(uSourceID);
		pCurrSource->ptrlEntryList.AddHead(pEntry);
		pCurrSrcHash->ptrlistSource.AddHead(pCurrSource);
		++m_uTotalIndexSource;
		uLoad = (uint8)((uSize * 100) / KADEMLIAMAXSOURCEPERFILE);
	}
	return true;
}

bool CIndexed::AddNotes(const CUInt128 &uKeyID, const CUInt128 &uSourceID, Kademlia::CEntry *pEntry, uint8 &uLoad, bool bIgnoreThreadLock)
{
	// do not access any data while the loading thread is busy;
	// bIgnoreThreadLock should be only used by CLoadDataThread itself
	if (!bIgnoreThreadLock && !m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return false;
	}

	if (!pEntry)
		return false;
	if (pEntry->m_uIP == 0 || pEntry->GetTagCount() == 0)
		return false;

	SrcHash *pCurrNoteHash;
	if (!m_mapNotes.Lookup(CCKey(uKeyID.GetData()), pCurrNoteHash)) {
		Source *pCurrNote = new Source;
		pCurrNote->uSourceID.SetValue(uSourceID);
		pCurrNote->ptrlEntryList.AddHead(pEntry);
		pCurrNoteHash = new SrcHash;
		pCurrNoteHash->uKeyID.SetValue(uKeyID);
		pCurrNoteHash->ptrlistSource.AddHead(pCurrNote);
		m_mapNotes.SetAt(CCKey(pCurrNoteHash->uKeyID.GetData()), pCurrNoteHash);
		uLoad = 1;
		++m_uTotalIndexNotes;
		return true;
	}

	INT_PTR uSize = pCurrNoteHash->ptrlistSource.GetCount();
	for (POSITION pos = pCurrNoteHash->ptrlistSource.GetHeadPosition(); pos != NULL; ) {
		Source *pCurrNote = pCurrNoteHash->ptrlistSource.GetNext(pos);
		if (pCurrNote->ptrlEntryList.IsEmpty()) {
			//This should never happen!
			pCurrNote->ptrlEntryList.AddHead(pEntry);
			ASSERT(0);
			uLoad = (uint8)((uSize * 100) / KADEMLIAMAXNOTESPERFILE);
			++m_uTotalIndexNotes;
			return true;
		}
		CEntry *pCurrEntry = pCurrNote->ptrlEntryList.GetHead();
		if (pCurrEntry->m_uIP == pEntry->m_uIP || pCurrEntry->m_uSourceID == pEntry->m_uSourceID) {
			delete pCurrNote->ptrlEntryList.RemoveHead();
			pCurrNote->ptrlEntryList.AddHead(pEntry);
			uLoad = (uint8)((uSize * 100) / KADEMLIAMAXNOTESPERFILE);
			return true;
		}
	}
	if (uSize > KADEMLIAMAXNOTESPERFILE) {
		Source *pCurrNote = pCurrNoteHash->ptrlistSource.RemoveTail();
		delete pCurrNote->ptrlEntryList.RemoveTail();
		pCurrNote->uSourceID.SetValue(uSourceID);
		pCurrNote->ptrlEntryList.AddHead(pEntry);
		pCurrNoteHash->ptrlistSource.AddHead(pCurrNote);
		uLoad = 100;
	} else {
		Source *pCurrNote = new Source;
		pCurrNote->uSourceID.SetValue(uSourceID);
		pCurrNote->ptrlEntryList.AddHead(pEntry);
		pCurrNoteHash->ptrlistSource.AddHead(pCurrNote);
		uLoad = (uint8)((uSize * 100) / KADEMLIAMAXNOTESPERFILE);
		++m_uTotalIndexNotes;
	}
	return true;
}

bool CIndexed::AddLoad(const CUInt128 &uKeyID, time_t uTime, bool bIgnoreThreadLock)
{
	// do not access any data while the loading thread is busy;
	// bIgnoreThreadLock should be only used by CLoadDataThread itself
	if (!bIgnoreThreadLock && !m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return false;
	}

	//This is needed for when you restart the client.
	if (time(NULL) > uTime)
		return false;

	Load *pLoad;
	if (m_mapLoad.Lookup(CCKey(uKeyID.GetData()), pLoad))
		return false;

	pLoad = new Load{uKeyID, uTime};
	m_mapLoad.SetAt(CCKey(pLoad->uKeyID.GetData()), pLoad);
	++m_uTotalIndexLoad;
	return true;
}

void CIndexed::SendValidKeywordResult(const CUInt128 &uKeyID, const SSearchTerm *pSearchTerms, uint32 uIP, uint16 uPort, bool bOldClient, uint16 uStartPosition, const CKadUDPKey &senderUDPKey)
{
	// do not access any data while the loading thread is busy;
	if (!m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return;
	}

	KeyHash *pCurrKeyHash;
	if (m_mapKeyword.Lookup(CCKey(uKeyID.GetData()), pCurrKeyHash)) {
		byte byPacket[1024 * 5];
		byte bySmallBuffer[2048];
		CByteIO byIO(byPacket, sizeof byPacket);
		byIO.WriteByte(OP_KADEMLIAHEADER);
		byIO.WriteByte(KADEMLIA2_SEARCH_RES);
		byIO.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
		byIO.WriteUInt128(uKeyID);

		byte *const pbyCountPos = byPacket + byIO.GetUsed();
		ASSERT(byPacket + 18 + 16 == pbyCountPos);
		byIO.WriteUInt16(0);

		static const int iMaxResults = 300;
		int iUnsentCount = 0;
		CByteIO byIOTmp(bySmallBuffer, sizeof bySmallBuffer);
		// we do 2 loops: In the first one we ignore all results which have a trust value below 1
		// in the second one we then also consider those. That way we make sure our 300 max results
		// are not full of spam entries. We could also sort by trust value, but in that case
		// we would risk to send only popular files on very hot keywords
		int iCount = -(int)uStartPosition;
		CCKey key1;
		for (bool bOnlyTrusted = true; iCount < iMaxResults; bOnlyTrusted = false) {
			for (POSITION pos = pCurrKeyHash->mapSource.GetStartPosition(); pos != NULL && iCount < iMaxResults;) {
				Source *pCurrSource;
				pCurrKeyHash->mapSource.GetNextAssoc(pos, key1, pCurrSource);
				for (POSITION pos2 = pCurrSource->ptrlEntryList.GetHeadPosition(); pos2 != NULL && iCount < iMaxResults;) {
					CKeyEntry *pCurrName = static_cast<CKeyEntry*>(pCurrSource->ptrlEntryList.GetNext(pos2));
					ASSERT(pCurrName->IsKeyEntry());
					if ((bOnlyTrusted ^ (pCurrName->GetTrustValue() < 1.0f)) && (!pSearchTerms || pCurrName->StartSearchTermsMatch(*pSearchTerms))) {
						if (iCount < 0) {
							++iCount;
							continue;
						}
						if (!bOldClient || pCurrName->m_uSize <= OLD_MAX_EMULE_FILE_SIZE) {
							++iCount;

							byIOTmp.WriteUInt128(pCurrName->m_uSourceID);
							pCurrName->WriteTagListWithPublishInfo(&byIOTmp);

							if (byIO.GetUsed() + byIOTmp.GetUsed() > UDP_KAD_MAXFRAGMENT && iUnsentCount > 0) {
								uint32 uLen = sizeof byPacket - byIO.GetAvailable();
								PokeUInt16(pbyCountPos, (uint16)iUnsentCount);
								CKademlia::GetUDPListener()->SendPacket(byPacket, uLen, uIP, uPort, senderUDPKey, NULL);
								byIO.Reset();
								byIO.WriteByte(OP_KADEMLIAHEADER);
								if (thePrefs.GetDebugClientKadUDPLevel() > 0)
									DebugSend("KADEMLIA2_SEARCH_RES", uIP, uPort);
								byIO.WriteByte(KADEMLIA2_SEARCH_RES);
								byIO.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
								byIO.WriteUInt128(uKeyID);
								byIO.WriteUInt16(0);
								DEBUG_ONLY(DebugLog(_T("Sent %i keyword search results in one packet to avoid fragmentation"), iUnsentCount));
								iUnsentCount = 0;
							}
							ASSERT(byIO.GetUsed() + byIOTmp.GetUsed() <= UDP_KAD_MAXFRAGMENT);
							byIO.WriteArray(bySmallBuffer, byIOTmp.GetUsed());
							byIOTmp.Reset();
							++iUnsentCount;
						}
					}
				}
			}
			if (!bOnlyTrusted)
				break;
		}

		if (iUnsentCount > 0) {
			uint32 uLen = sizeof byPacket - byIO.GetAvailable();
			PokeUInt16(pbyCountPos, (uint16)iUnsentCount);
			if (thePrefs.GetDebugClientKadUDPLevel() > 0)
				DebugSend("KADEMLIA2_SEARCH_RES", uIP, uPort);

			CKademlia::GetUDPListener()->SendPacket(byPacket, uLen, uIP, uPort, senderUDPKey, NULL);
			DEBUG_ONLY(DebugLog(_T("Sent %i keyword search results in last packet to avoid fragmentation"), iUnsentCount));
		} else
			ASSERT(iCount <= 0);
	}
	Clean();
}

void CIndexed::SendValidSourceResult(const CUInt128 &uKeyID, uint32 uIP, uint16 uPort, uint16 uStartPosition, uint64 uFileSize, const CKadUDPKey &senderUDPKey)
{
	// do not access any data while the loading thread is busy;
	if (!m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return;
	}

	SrcHash *pCurrSrcHash;
	if (m_mapSources.Lookup(CCKey(uKeyID.GetData()), pCurrSrcHash)) {
		byte byPacket[1024 * 5];
		byte bySmallBuffer[2048];
		CByteIO byIO(byPacket, sizeof byPacket);
		byIO.WriteByte(OP_KADEMLIAHEADER);
		byIO.WriteByte(KADEMLIA2_SEARCH_RES);
		byIO.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
		byIO.WriteUInt128(uKeyID);

		byte *const pbyCountPos = byPacket + byIO.GetUsed();
		ASSERT(byPacket + 18 + 16 == pbyCountPos);
		byIO.WriteUInt16(0);

		static const int iMaxResults = 300;
		int iUnsentCount = 0;
		CByteIO byIOTmp(bySmallBuffer, sizeof bySmallBuffer);

		int iCount = -(int)uStartPosition;
		for (POSITION pos = pCurrSrcHash->ptrlistSource.GetHeadPosition(); pos != NULL && iCount < iMaxResults;) {
			const Source *pCurrSource = pCurrSrcHash->ptrlistSource.GetNext(pos);
			if (!pCurrSource->ptrlEntryList.IsEmpty()) {
				if (iCount < 0) {
					++iCount;
					continue;
				}
				CEntry *pCurrName = pCurrSource->ptrlEntryList.GetHead();
				if (!uFileSize || !pCurrName->m_uSize || pCurrName->m_uSize == uFileSize) {
					byIOTmp.WriteUInt128(pCurrName->m_uSourceID);
					pCurrName->WriteTagList(&byIOTmp);
					++iCount;
					if (byIO.GetUsed() + byIOTmp.GetUsed() > UDP_KAD_MAXFRAGMENT && iUnsentCount > 0) {
						uint32 uLen = sizeof byPacket - byIO.GetAvailable();
						PokeUInt16(pbyCountPos, (uint16)iUnsentCount);
						CKademlia::GetUDPListener()->SendPacket(byPacket, uLen, uIP, uPort, senderUDPKey, NULL);
						byIO.Reset();
						byIO.WriteByte(OP_KADEMLIAHEADER);
						if (thePrefs.GetDebugClientKadUDPLevel() > 0)
							DebugSend("KADEMLIA2_SEARCH_RES", uIP, uPort);
						byIO.WriteByte(KADEMLIA2_SEARCH_RES);
						byIO.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
						byIO.WriteUInt128(uKeyID);
						byIO.WriteUInt16(0);
						//DEBUG_ONLY(DebugLog(_T("Sent %i source search results in one packet to avoid fragmentation"), iUnsentCount));
						iUnsentCount = 0;
					}
					ASSERT(byIO.GetUsed() + byIOTmp.GetUsed() <= UDP_KAD_MAXFRAGMENT);
					byIO.WriteArray(bySmallBuffer, byIOTmp.GetUsed());
					byIOTmp.Reset();
					++iUnsentCount;
				}
			}
		}

		if (iUnsentCount > 0) {
			uint32 uLen = sizeof byPacket - byIO.GetAvailable();
			PokeUInt16(pbyCountPos, (uint16)iUnsentCount);
			if (thePrefs.GetDebugClientKadUDPLevel() > 0)
				DebugSend("KADEMLIA2_SEARCH_RES", uIP, uPort);

			CKademlia::GetUDPListener()->SendPacket(byPacket, uLen, uIP, uPort, senderUDPKey, NULL);
			//DEBUG_ONLY(DebugLog(_T("Sent %i source search results in last packet to avoid fragmentation"), iUnsentCount));
		} else
			ASSERT(iCount <= 0);
	}
	Clean();
}

void CIndexed::SendValidNoteResult(const CUInt128 &uKeyID, uint32 uIP, uint16 uPort, uint64 uFileSize, const CKadUDPKey &senderUDPKey)
{
	// do not access any data while the loading thread is busy;
	if (!m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return;
	}

	try {
		SrcHash *pCurrNoteHash;
		if (m_mapNotes.Lookup(CCKey(uKeyID.GetData()), pCurrNoteHash)) {
			byte byPacket[1024 * 5];
			byte bySmallBuffer[2048];
			CByteIO byIO(byPacket, sizeof byPacket);
			byIO.WriteByte(OP_KADEMLIAHEADER);
			byIO.WriteByte(KADEMLIA2_SEARCH_RES);
			byIO.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
			byIO.WriteUInt128(uKeyID);

			byte *const pbyCountPos = byPacket + byIO.GetUsed();
			ASSERT(byPacket + 18 + 16 == pbyCountPos);
			byIO.WriteUInt16(0);

			static const int iMaxResults = 150;
			int iUnsentCount = 0;
			CByteIO byIOTmp(bySmallBuffer, sizeof bySmallBuffer);
			int iCount = 0;
			for (POSITION pos = pCurrNoteHash->ptrlistSource.GetHeadPosition(); pos != NULL && iCount < iMaxResults; ) {
				const Source *pCurrNote = pCurrNoteHash->ptrlistSource.GetNext(pos);
				if (!pCurrNote->ptrlEntryList.IsEmpty()) {
					CEntry *pCurrName = pCurrNote->ptrlEntryList.GetHead();
					if (!uFileSize || !pCurrName->m_uSize || pCurrName->m_uSize == uFileSize) {
						byIOTmp.WriteUInt128(pCurrName->m_uSourceID);
						pCurrName->WriteTagList(&byIOTmp);
						++iCount;
						if (byIO.GetUsed() + byIOTmp.GetUsed() > UDP_KAD_MAXFRAGMENT && iUnsentCount > 0) {
							uint32 uLen = sizeof byPacket - byIO.GetAvailable();
							PokeUInt16(pbyCountPos, (uint16)iUnsentCount);
							CKademlia::GetUDPListener()->SendPacket(byPacket, uLen, uIP, uPort, senderUDPKey, NULL);
							byIO.Reset();
							byIO.WriteByte(OP_KADEMLIAHEADER);
							if (thePrefs.GetDebugClientKadUDPLevel() > 0)
								DebugSend("KADEMLIA2_SEARCH_RES", uIP, uPort);
							byIO.WriteByte(KADEMLIA2_SEARCH_RES);
							byIO.WriteUInt128(Kademlia::CKademlia::GetPrefs()->GetKadID());
							byIO.WriteUInt128(uKeyID);
							byIO.WriteUInt16(0);
							DEBUG_ONLY(DebugLog(_T("Sent %i note search results in one packet to avoid fragmentation"), iUnsentCount));
							iUnsentCount = 0;
						}
						ASSERT(byIO.GetUsed() + byIOTmp.GetUsed() <= UDP_KAD_MAXFRAGMENT);
						byIO.WriteArray(bySmallBuffer, byIOTmp.GetUsed());
						byIOTmp.Reset();
						++iUnsentCount;
					}
				}
			}
			if (iUnsentCount > 0) {
				uint32 uLen = sizeof byPacket - byIO.GetAvailable();
				PokeUInt16(pbyCountPos, (uint16)iUnsentCount);
				if (thePrefs.GetDebugClientKadUDPLevel() > 0)
					DebugSend("KADEMLIA2_SEARCH_RES", uIP, uPort);

				CKademlia::GetUDPListener()->SendPacket(byPacket, uLen, uIP, uPort, senderUDPKey, NULL);
				DEBUG_ONLY(DebugLog(_T("Sent %i note search results in last packet to avoid fragmentation"), iUnsentCount));
			} else
				ASSERT(!iCount);
		}
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CIndexed::SendValidNoteResult"));
	}
}

bool CIndexed::SendStoreRequest(const CUInt128 &uKeyID)
{
	// do not access any data while the loading thread is busy;
	if (!m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return true; // don't report overloaded with a false
	}

	Load *pLoad;
	if (m_mapLoad.Lookup(CCKey(uKeyID.GetData()), pLoad)) {
		if (pLoad->uTime >= time(NULL))
			return false;
		m_mapLoad.RemoveKey(CCKey(uKeyID.GetData()));
		--m_uTotalIndexLoad;
		delete pLoad;
	}
	return true;
}

uint32 CIndexed::GetFileKeyCount()
{
	// do not access any data while the loading thread is busy;
	if (!m_bDataLoaded) {
		DEBUG_ONLY(DebugLogWarning(strLoadingInProgress));
		return 0;
	}
	return (uint32)m_mapKeyword.GetCount();
}

SSearchTerm::SSearchTerm()
	: m_type(AND)
	, m_pTag()
	, m_pastr()
	, m_pLeft()
	, m_pRight()
{
}

SSearchTerm::~SSearchTerm()
{
	if (m_type == String)
		delete m_pastr;
	delete m_pTag;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CIndexed::CLoadDataThread Implementation
typedef CIndexed::CLoadDataThread CLoadDataThread;
IMPLEMENT_DYNCREATE(CLoadDataThread, CWinThread)

CIndexed::CLoadDataThread::CLoadDataThread()
{
	m_pOwner = NULL;
}

BOOL CIndexed::CLoadDataThread::InitInstance()
{
	InitThreadLocale();
	return TRUE;
}

int CIndexed::CLoadDataThread::Run()
{
	DbgSetThreadName("Kademlia Indexed Load Data");
	if (!m_pOwner)
		return 0;

	ASSERT(!m_pOwner->m_bDataLoaded);
	CSingleLock sLock(&m_pOwner->m_mutSync, TRUE);

	try {
		uint32 uTotalLoad = 0;
		//uint32 uTotalSource = 0;
		uint32 uTotalKeyword = 0;
		CUInt128 uKeyID, uID, uSourceID;

		if (!m_pOwner->m_bAbortLoading) {
			CBufferedFileIO fileLoad;
			if (fileLoad.Open(m_sLoadFileName, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite)) {
				setvbuf(fileLoad.m_pStream, NULL, _IOFBF, 32768);
				uint32 uVersion = fileLoad.ReadUInt32();
				if (uVersion < 2) {
					//time_t tSaveTime = fileLoad.ReadUInt32();
					fileLoad.Seek(sizeof(uint32), SEEK_CUR); //skip save time
					for (uint32 uNumLoad = fileLoad.ReadUInt32(); uNumLoad && !m_pOwner->m_bAbortLoading; --uNumLoad) {
						fileLoad.ReadUInt128(&uKeyID);
						if (m_pOwner->AddLoad(uKeyID, (time_t)fileLoad.ReadUInt32(), true))
							++uTotalLoad;
					}
				}
				fileLoad.Close();
			} else
				DebugLogWarning(_T("Unable to load Kad file: %s"), (LPCTSTR)m_sLoadFileName);
		}

		if (!m_pOwner->m_bAbortLoading) {
			CBufferedFileIO fileKey;
			if (fileKey.Open(m_sKeyFileName, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite)) {
				setvbuf(fileKey.m_pStream, NULL, _IOFBF, 32768);

				uint32 uVersion = fileKey.ReadUInt32();
				if (uVersion < 5) {
					time_t tSaveTime = fileKey.ReadUInt32();
					if (tSaveTime > time(NULL)) {
						fileKey.ReadUInt128(&uID);
						if (Kademlia::CKademlia::GetPrefs()->GetKadID() == uID) {
							for (uint32 uNumKeys = fileKey.ReadUInt32(); uNumKeys && !m_pOwner->m_bAbortLoading; --uNumKeys) {
								fileKey.ReadUInt128(&uKeyID);
								for (uint32 uNumSource = fileKey.ReadUInt32(); uNumSource && !m_pOwner->m_bAbortLoading; --uNumSource) {
									fileKey.ReadUInt128(&uSourceID);
									for (uint32 uNumName = fileKey.ReadUInt32(); uNumName && !m_pOwner->m_bAbortLoading; --uNumName) {
										CKeyEntry *pToAdd = new Kademlia::CKeyEntry();
										pToAdd->m_uKeyID.SetValue(uKeyID);
										pToAdd->m_uSourceID.SetValue(uSourceID);
										pToAdd->m_bSource = false;
										pToAdd->m_tLifetime = fileKey.ReadUInt32();
										if (uVersion >= 3)
											pToAdd->ReadPublishTrackingDataFromFile(&fileKey, uVersion >= 4);
										for (uint32 uTotalTags = fileKey.ReadByte(); uTotalTags; --uTotalTags) {
											CKadTag *pTag = fileKey.ReadTag();
											if (pTag) {
												if (!pTag->m_name.Compare(TAG_FILENAME)) {
													if (pToAdd->GetCommonFileName().IsEmpty())
														pToAdd->SetFileName(pTag->GetStr());
													delete pTag;
												} else if (!pTag->m_name.Compare(TAG_FILESIZE)) {
													pToAdd->m_uSize = pTag->GetInt();
													delete pTag;
												} else {
													if (!pTag->m_name.Compare(TAG_SOURCEIP))
														pToAdd->m_uIP = (uint32)pTag->GetInt();
													else if (!pTag->m_name.Compare(TAG_SOURCEPORT))
														pToAdd->m_uTCPPort = (uint16)pTag->GetInt();
													else if (!pTag->m_name.Compare(TAG_SOURCEUPORT))
														pToAdd->m_uUDPPort = (uint16)pTag->GetInt();

													pToAdd->AddTag(pTag);
												}
											}
										}
										uint8 uLoad;
										if (m_pOwner->AddKeyword(uKeyID, uSourceID, pToAdd, uLoad, true))
											++uTotalKeyword;
										else
											delete pToAdd;
									}
								}
							}
						}
					}
				}
				fileKey.Close();
			} else
				DebugLogWarning(_T("Unable to load Kad file: %s"), (LPCTSTR)m_sKeyFileName);
		}

		if (!m_pOwner->m_bAbortLoading) {
			CBufferedFileIO fileSource;
			if (fileSource.Open(m_sSourceFileName, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite)) {
				setvbuf(fileSource.m_pStream, NULL, _IOFBF, 32768);

				uint32 uTotalSource = 0;
				uint32 uVersion = fileSource.ReadUInt32();
				if (uVersion < 3) {
					time_t tSaveTime = fileSource.ReadUInt32();
					if (tSaveTime > time(NULL)) {
						for (uint32 uNumKeys = fileSource.ReadUInt32(); uNumKeys && !m_pOwner->m_bAbortLoading; --uNumKeys) {
							fileSource.ReadUInt128(&uKeyID);
							for (uint32 uNumSource = fileSource.ReadUInt32(); uNumSource && !m_pOwner->m_bAbortLoading; --uNumSource) {
								fileSource.ReadUInt128(&uSourceID);
								for (uint32 uNumName = fileSource.ReadUInt32(); uNumName && !m_pOwner->m_bAbortLoading; --uNumName) {
									CEntry *pToAdd = new Kademlia::CEntry();
									pToAdd->m_bSource = true;
									pToAdd->m_tLifetime = fileSource.ReadUInt32();
									for (uint32 uTotalTags = fileSource.ReadByte(); uTotalTags; --uTotalTags) {
										CKadTag *pTag = fileSource.ReadTag();
										if (pTag) {
											if (!pTag->m_name.Compare(TAG_SOURCEIP))
												pToAdd->m_uIP = (uint32)pTag->GetInt();
											else if (!pTag->m_name.Compare(TAG_SOURCEPORT))
												pToAdd->m_uTCPPort = (uint16)pTag->GetInt();
											else if (!pTag->m_name.Compare(TAG_SOURCEUPORT))
												pToAdd->m_uUDPPort = (uint16)pTag->GetInt();

											pToAdd->AddTag(pTag);
										}
									}
									pToAdd->m_uKeyID.SetValue(uKeyID);
									pToAdd->m_uSourceID.SetValue(uSourceID);
									uint8 uLoad;
									if (m_pOwner->AddSources(uKeyID, uSourceID, pToAdd, uLoad, true))
										++uTotalSource;
									else
										delete pToAdd;
								}
							}
						}
					}
				}
				fileSource.Close();

				m_pOwner->m_uTotalIndexSource = uTotalSource;
				m_pOwner->m_uTotalIndexKeyword = uTotalKeyword;
				m_pOwner->m_uTotalIndexLoad = uTotalLoad;
				AddDebugLogLine(false, _T("Read %u source, %u keyword, and %u load entries"), uTotalSource, uTotalKeyword, uTotalLoad);
			} else
				DebugLogWarning(_T("Unable to load Kad file: %s"), (LPCTSTR)m_sSourceFileName);
		}
	} catch (CIOException *ioe) {
		AddDebugLogLine(false, _T("CIndexed::CLoadDataThread::Run (IO error(%i))"), ioe->m_iCause);
		ioe->Delete();
	} catch (...) {
		AddDebugLogLine(false, _T("Exception in CIndexed::CLoadDataThread::Run"));
		ASSERT(0);
	}
	if (m_pOwner->m_bAbortLoading)
		AddDebugLogLine(false, _T("Terminating CIndexed::CLoadDataThread - early abort requested"));
	else
		AddDebugLogLine(false, _T("Terminating CIndexed::CLoadDataThread - finished loading data"));

	m_pOwner->m_bDataLoaded = true;
	return 0;
}