//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
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
#include <timeapi.h>
#include "emule.h"
#include "UploadBandwidthThrottler.h"
#include "EMSocket.h"
#include "opcodes.h"
#include "LastCommonRouteFinder.h"
#include "OtherFunctions.h"
#include "uploadqueue.h"
#include "preferences.h"
#include "UploadDiskIOThread.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/**
 * The constructor starts the thread.
 */
UploadBandwidthThrottler::UploadBandwidthThrottler()
	: m_eventThreadEnded(FALSE, TRUE)
	, m_eventPaused(TRUE, TRUE)
	, m_SentBytesSinceLastCall()
	, m_SentBytesSinceLastCallOverhead()
	, m_highestNumberOfFullyActivatedSlots()
	, m_bRun(true)
{
	if (AfxBeginThread(RunProc, (LPVOID)this) == NULL) {
		m_bRun = false;
		m_eventThreadEnded.SetEvent();
		DebugLogError(_T("Failed to create UploadBandwidthThrottler thread (Win32 error %lu)"), ::GetLastError());
	}
}

/**
 * The destructor stops the thread. If the thread has already stopped, the destructor does nothing.
 */
UploadBandwidthThrottler::~UploadBandwidthThrottler()
{
	EndThread();
}

/**
 * Find out the highest number of slots that has been fed data in the normal standard loop
 * of the thread since the last call of this method. This means all slots that haven't
 * been in the trickle state during the entire time since the last call.
 *
 * @return the highest number of fully activated slots during any loop since last call
 */
INT_PTR UploadBandwidthThrottler::GetHighestNumberOfFullyActivatedSlotsSinceLastCallAndReset()
{
	queueLocker.Lock();
	//if(m_highestNumberOfFullyActivatedSlots > GetStandardListSize())
	//	theApp.QueueDebugLogLine(true, _T("UploadBandwidthThrottler: Throttler wants new slot when get-method called. m_highestNumberOfFullyActivatedSlots: %i GetStandardListSize(): %i tick: %i"), m_highestNumberOfFullyActivatedSlots, GetStandardListSize(), timeGetTime());

	INT_PTR highestNumberOfFullyActivatedSlots
#ifdef _WIN64
		= (INT_PTR)::InterlockedExchange64((LONG64*)&m_highestNumberOfFullyActivatedSlots, 0);
#else
		= (INT_PTR)::InterlockedExchange((LONG*)&m_highestNumberOfFullyActivatedSlots, 0);
#endif
	queueLocker.Unlock();

	return highestNumberOfFullyActivatedSlots;
}

/**
 * Add a socket to the list of sockets with an upload slot. The main thread will
 * continuously call send on these sockets, to give them chance to work off their queues.
 * The sockets are called in the order they exist in the list, so the top socket (index 0)
 * will be given a chance to use bandwidth first, then the next socket (index 1) etc.
 *
 * It is possible to add a socket several times to the list without removing it in between,
 * but that should be avoided.
 *
 * @param index		insert the socket at this place in the list. An index that is higher than the
 *				current number of sockets in the list will mean that the socket should be added
 *				last to the list.
 *
 * @param socket	the address of the socket that should be inserted into the list. If the address
 *				is NULL, this method will do nothing.
 */
void UploadBandwidthThrottler::AddToStandardList(INT_PTR index, ThrottledFileSocket *socket)
{
	if (socket != NULL) {
		queueLocker.Lock();

		RemoveFromStandardListNoLock(socket);
		m_StandardOrder_list.InsertAt(min(index, GetStandardListSize()), socket);

		queueLocker.Unlock();
	}
//	else if (thePrefs.GetVerbose())
//		theApp.QueueDebugLogLine(true, _T("UploadBandwidthThrottler: prevented adding a NULL socket to the Standard list!"));
}

/**
 * Remove a socket from the list of sockets that have upload slots.
 *
 * If the socket has mistakenly been added several times to the list, this method
 * will remove all of the entries for the socket.
 *
 * @param socket the address of the socket that should be removed from the list. If this socket
 *			   does not exist in the list, this method will do nothing.
 */
bool UploadBandwidthThrottler::RemoveFromStandardList(ThrottledFileSocket *socket)
{
	queueLocker.Lock();

	bool returnValue = RemoveFromStandardListNoLock(socket);

	queueLocker.Unlock();

	return returnValue;
}

/**
 * Remove a socket from the list of sockets that have upload slots. NOT THREADSAFE!
 * This is an internal method that doesn't take the necessary lock before it removes
 * the socket. This method should only be called when the current thread already owns
 * the sendLocker lock!
 *
 * @param socket address of the socket that should be removed from the list. If this socket
 *			   does not exist in the list, this method will do nothing.
 */
bool UploadBandwidthThrottler::RemoveFromStandardListNoLock(ThrottledFileSocket *socket)
{
	// Find the slot
	for (INT_PTR slotCounter = GetStandardListSize(); --slotCounter >= 0;)
		if (m_StandardOrder_list[slotCounter] == socket) {
			// Remove the slot
			m_StandardOrder_list.RemoveAt(slotCounter);
			if (m_highestNumberOfFullyActivatedSlots > GetStandardListSize())
				m_highestNumberOfFullyActivatedSlots = GetStandardListSize();
			return true;
		}

	return false;
}

/**
* Notifies the send thread that it should try to call controlpacket send
* for the given socket. It is allowed to call this method several times
* for the same socket, without having controlpacket send called for the socket
* first. The duplicate entries are never filtered, since it incurs less CPU
* overhead to simply call Send() for each entry. Send() already would
* have done its work when the second Send() was called, and will just
* return with little CPU overhead.
*
* @param socket address to the socket that requests a call of controlpacket send
*/
void UploadBandwidthThrottler::QueueForSendingControlPacket(ThrottledControlSocket *socket, const bool hasSent)
{
	if (m_bRun) {
		tempQueueLocker.Lock();

		if (hasSent)
			m_TempControlQueueFirst_list.push_back(socket);
		else
			m_TempControlQueue_list.push_back(socket);

		tempQueueLocker.Unlock();
	}
}

/**
 * Remove the socket from all lists and queues. This will make it safe to
 * erase/delete the socket. Also, the main thread will stop calling
 * send() for the socket.
 *
 * @param socket address to the socket that should be removed
 */
void UploadBandwidthThrottler::RemoveFromAllQueuesNoLock(ThrottledControlSocket *socket)
{
	// Remove this socket from control packet queue
	m_ControlQueue_list.remove(socket);
	m_ControlQueueFirst_list.remove(socket);

	tempQueueLocker.Lock();
	m_TempControlQueue_list.remove(socket);
	m_TempControlQueueFirst_list.remove(socket);
	tempQueueLocker.Unlock();
}

void UploadBandwidthThrottler::RemoveFromAllQueues(ThrottledFileSocket *socket)
{
	if (m_bRun) {
		queueLocker.Lock(); // Get critical section

		RemoveFromAllQueuesNoLock(socket);

		// And remove it from upload slots
		RemoveFromStandardListNoLock(socket);

		queueLocker.Unlock(); // End critical section
	}
}

void UploadBandwidthThrottler::RemoveFromAllQueuesLocked(ThrottledControlSocket *socket)
{
	if (m_bRun) {
		queueLocker.Lock();
		RemoveFromAllQueuesNoLock(socket);
		queueLocker.Unlock();
	}
}

/**
 * Make the thread exit. This method will not return until the thread has stopped
 * looping. This guarantees that the thread will not access the CEMSockets after this
 * call has exited.
 */
void UploadBandwidthThrottler::EndThread()
{
	//the flag is never checked in the thread loop, no need to get locks

	// signal the thread to stop looping and exit.
	m_bRun = false;

	//Pause(false);

	// wait for the thread to signal that it has stopped looping.
	m_eventThreadEnded.Lock();
}

/*void UploadBandwidthThrottler::Pause(bool paused)
{
	if (paused)
		m_eventPaused.ResetEvent();
	else
		m_eventPaused.SetEvent();
}
*/
uint32 UploadBandwidthThrottler::GetSlotLimit(uint32 currentUpSpeed)
{
	uint32 upPerClient = theApp.uploadqueue->GetTargetClientDataRate(true);
	// if throttler doesn't require another slot, go with a slightly more restrictive method
	if (currentUpSpeed > 49 * 1024) {
		upPerClient += currentUpSpeed / 43;
		if (upPerClient > UPLOAD_CLIENT_MAXDATARATE)
			upPerClient = UPLOAD_CLIENT_MAXDATARATE;
	}

	//now the final check
	if (currentUpSpeed > 25 * 1024)
		return max(currentUpSpeed / upPerClient, MIN_UP_CLIENTS_ALLOWED + 3);
	if (currentUpSpeed > 16 * 1024)
		return MIN_UP_CLIENTS_ALLOWED + 2;
	if (currentUpSpeed > 9 * 1024)
		return MIN_UP_CLIENTS_ALLOWED + 1;
	return MIN_UP_CLIENTS_ALLOWED;
}

uint32 UploadBandwidthThrottler::CalculateChangeDelta(uint32 numberOfConsecutiveChanges)
{
	static const uint32 deltas[9] =
		{50u, 50u, 128u, 256u, 512u, 512u + 256u, 1024u, 1024u + 256u, 1024u + 512u};
	return deltas[min(numberOfConsecutiveChanges, _countof(deltas) - 1)]; //use the last element for 8 and above
}

/**
 * Start the thread. Called from the constructor in this class.
 *
 * @param pParam
 *
 * @return
 */
UINT AFX_CDECL UploadBandwidthThrottler::RunProc(LPVOID pParam)
{
	DbgSetThreadName("UploadBandwidthThrottler");
	InitThreadLocale();
	UploadBandwidthThrottler *uploadBandwidthThrottler = static_cast<UploadBandwidthThrottler*>(pParam);
	return uploadBandwidthThrottler->RunInternal();
}

/**
 * The thread method that handles calling send for the individual sockets.
 *
 * Control packets will always be tried to be sent first. If there is any bandwidth leftover
 * after that, send() for the upload slot sockets will be called in priority order until we have run
 * out of available bandwidth for this loop. Upload slots will not be allowed to go without having sent
 * called for more than a defined amount of time (i.e. two seconds).
 *
 * @return always returns 0.
 */
UINT UploadBandwidthThrottler::RunInternal()
{
	static const bool estimateChangedLog = false;
	static const bool lotsOfLog = false;

	sint64 spendingRate = 0; //bytes per second
	INT_PTR rememberedSlotCounter = 0;
	DWORD nUploadStartTime = 0;
	DWORD lastLoopTick, lastTickReachedBandwidth;
	uint32 nEstiminatedDataRate = 0;
	uint32 numberOfConsecutiveUpChanges = 0;
	uint32 numberOfConsecutiveDownChanges = 0;
	uint32 changesCount = 0;
	uint32 loopsCount = 0;
	int nSlotsBusyLevel = 0;

	lastTickReachedBandwidth = lastLoopTick = timeGetTime();
	while (m_bRun) {
//		m_eventPaused.Lock();

		DWORD timeSinceLastLoop = timeGetTime() - lastLoopTick;

		// Get the current speed from UploadSpeedSense
		uint32 allowedDataRate = theApp.lastCommonRouteFinder->GetUpload();

		// check busy level for all the slots (WSAEWOULDBLOCK status)
		uint32 nBusy = 0;
		uint32 nCanSend = 0;

		queueLocker.Lock();
		m_eventDataAvailable.ResetEvent();
		m_eventSocketAvailable.ResetEvent();
		for (INT_PTR i = mini(GetStandardListSize(), (INT_PTR)max(GetSlotLimit(theApp.uploadqueue->GetDatarate()), 3u)); --i >= 0;) {
			ThrottledFileSocket *pSocket = m_StandardOrder_list[i];
			if (pSocket != NULL && pSocket->HasQueues()) {
				++nCanSend;
				nBusy += static_cast<uint32>(pSocket->IsBusyExtensiveCheck());
			}
		}
		queueLocker.Unlock();

		// if this is kept, the loop above can be optimized a little (don't count nCanSend,
		// just use nCanSend = GetSlotLimit(theApp.uploadqueue->GetDatarate())
		//if (theApp.uploadqueue)
		//   nCanSend = max(nCanSend, GetSlotLimit(theApp.uploadqueue->GetDatarate()));

		// When no upload limit has been set in options, try to guess a good upload limit.
		if (thePrefs.GetMaxUpload() == UNLIMITED) {
			++loopsCount;
			//if (lotsOfLog)
			//	theApp.QueueDebugLogLine(false,_T("Throttler: busy: %i/%i nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount: %i loopsCount: %i"), nBusy, nCanSend, nSlotsBusyLevel, nEstiminatedLimit/1024.0f, changesCount, loopsCount);
			if (nCanSend > 0) {
				//float fBusyFraction = nBusy / (float)nCanSend;
				//the limits were: "fBusyFraction > 0.75f" and "fBusyFraction < 0.25f"
				const int iBusyFraction = (nBusy << 5) / nCanSend; //now the limits will be 24 and 8
				if (nBusy > 2 && iBusyFraction > 24 && nSlotsBusyLevel < 255) {
					++nSlotsBusyLevel;
					++changesCount;
					if (thePrefs.GetVerbose() && nSlotsBusyLevel % 25 == 0 && lotsOfLog)
						theApp.QueueDebugLogLine(false, _T("Throttler: nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount: %i loopsCount: %i"), nSlotsBusyLevel, nEstiminatedDataRate / 1024.0f, changesCount, loopsCount);
				} else if ((nBusy <= 2 || iBusyFraction < 8) && nSlotsBusyLevel > -255) {
					--nSlotsBusyLevel;
					++changesCount;
					if (thePrefs.GetVerbose() && nSlotsBusyLevel % 25 == 0 && lotsOfLog)
						theApp.QueueDebugLogLine(false, _T("Throttler: nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount %i loopsCount: %i"), nSlotsBusyLevel, nEstiminatedDataRate / 1024.0f, changesCount, loopsCount);
				}
			}

			if (nUploadStartTime == 0) {
				if (GetStandardListSize() >= 3)
					nUploadStartTime = timeGetTime();
			} else if (timeGetTime() >= nUploadStartTime + SEC2MS(60) && theApp.uploadqueue) {
				if (nEstiminatedDataRate == 0) { // no auto limit was set yet
					if (nSlotsBusyLevel >= 250) { // sockets indicated that the BW limit has been reached
						nEstiminatedDataRate = theApp.uploadqueue->GetDatarate();
						nSlotsBusyLevel = -200;
						if (thePrefs.GetVerbose() && estimateChangedLog)
							theApp.QueueDebugLogLine(false, _T("Throttler: Set initial estimated limit to %0.5f changesCount: %i loopsCount: %i"), nEstiminatedDataRate / 1024.0f, changesCount, loopsCount);
						changesCount = 0;
						loopsCount = 0;
					}
				} else if (nSlotsBusyLevel > 250) {
					if (changesCount > 500 || (changesCount > 300 && loopsCount > 1000) || loopsCount > 2000)
						numberOfConsecutiveDownChanges = 0;
					else
						++numberOfConsecutiveDownChanges;
					uint32 changeDelta = CalculateChangeDelta(numberOfConsecutiveDownChanges);

					// Don't lower speed below 1 KiB/s
					if (nEstiminatedDataRate < changeDelta + 1024)
						changeDelta = (nEstiminatedDataRate > 1024) ? nEstiminatedDataRate - 1024 : 0;

					ASSERT(nEstiminatedDataRate >= changeDelta + 1024);
					nEstiminatedDataRate -= changeDelta;

					if (thePrefs.GetVerbose() && estimateChangedLog)
						theApp.QueueDebugLogLine(false, _T("Throttler: REDUCED limit #%i by %i bytes to: %0.5f changesCount: %i loopsCount: %i"), numberOfConsecutiveDownChanges, changeDelta, nEstiminatedDataRate / 1024.0f, changesCount, loopsCount);

					numberOfConsecutiveUpChanges = 0;
					nSlotsBusyLevel = 0;
					changesCount = 0;
					loopsCount = 0;
				} else if (nSlotsBusyLevel < -250) {
					if (changesCount > 500 || (changesCount > 300 && loopsCount > 1000) || loopsCount > 2000)
						numberOfConsecutiveUpChanges = 0;
					else
						++numberOfConsecutiveUpChanges;
					uint32 changeDelta = CalculateChangeDelta(numberOfConsecutiveUpChanges);
					nEstiminatedDataRate += changeDelta;
					// Don't raise speed unless we are under current allowedDataRate
					if (nEstiminatedDataRate > allowedDataRate) {
						if (estimateChangedLog)
							changeDelta = nEstiminatedDataRate - allowedDataRate; //for logs only
						nEstiminatedDataRate = allowedDataRate;
					}

					if (thePrefs.GetVerbose() && estimateChangedLog)
						theApp.QueueDebugLogLine(false, _T("Throttler: INCREASED limit #%i by %i bytes to: %0.5f changesCount: %i loopsCount: %i"), numberOfConsecutiveUpChanges, changeDelta, nEstiminatedDataRate / 1024.0f, changesCount, loopsCount);

					numberOfConsecutiveDownChanges = 0;
					nSlotsBusyLevel = 0;
					changesCount = 0;
					loopsCount = 0;
				}

				if (allowedDataRate > nEstiminatedDataRate)
					allowedDataRate = nEstiminatedDataRate;
			}

			if (nCanSend == nBusy && GetStandardListSize() > 0 && nSlotsBusyLevel < 125) {
				nSlotsBusyLevel = 125;
				if (thePrefs.GetVerbose() && lotsOfLog)
					theApp.QueueDebugLogLine(false, _T("Throttler: nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount %i loopsCount: %i (set due to all slots busy)"), nSlotsBusyLevel, nEstiminatedDataRate / 1024.0f, changesCount, loopsCount);
			}
		}

		uint32 minFragSize, doubleSendSize;
		if (allowedDataRate < 6 * 1024)
			doubleSendSize = minFragSize = 536; // send one packet at a time at very low speeds for smoother upload
		else {
			minFragSize = 1300;
			doubleSendSize = minFragSize * 2; // send two packets at a time so they can share an ACK
		}

#define TIME_BETWEEN_UPLOAD_LOOPS 1
		DWORD sleepTime;
		if (allowedDataRate == _UI32_MAX || spendingRate >= SEC2MS(1) || (allowedDataRate | nEstiminatedDataRate) == 0)
			// we could send immediately, but sleep a while to not suck up all CPU
			sleepTime = TIME_BETWEEN_UPLOAD_LOOPS;
		else {
			if (allowedDataRate)
				// sleep untill we need to send at least one byte
				sleepTime = (DWORD)ceil((SEC2MS(1) - spendingRate) / (double)allowedDataRate);
			else
				sleepTime = (DWORD)ceil(SEC2MS(doubleSendSize) / (double)nEstiminatedDataRate);
			if (sleepTime < TIME_BETWEEN_UPLOAD_LOOPS)
				sleepTime = TIME_BETWEEN_UPLOAD_LOOPS;
		}
		if (timeSinceLastLoop < sleepTime) {
			DWORD dwSleep = sleepTime - timeSinceLastLoop;
			if (nCanSend == 0) {
				if (theApp.uploadqueue->GetUploadQueueLength() > 0 && theApp.m_pUploadDiskIOThread)
					theApp.m_pUploadDiskIOThread->WakeUpCall();
				::WaitForSingleObject(m_eventDataAvailable, dwSleep);
			} else if (nCanSend <= nBusy)
				::WaitForSingleObject(m_eventSocketAvailable, dwSleep);
			else
				::Sleep(dwSleep);
		}
		if (!m_bRun)
			break;

		const DWORD thisLoopTick = timeGetTime();
		timeSinceLastLoop = thisLoopTick - lastLoopTick;

		// Calculate how many bytes we can spend
		sint64 bytesToSpend;
		if (allowedDataRate >= _UI32_MAX) {
			spendingRate = 0; //_I64_MAX;
			bytesToSpend = _I32_MAX;
		} else if (timeSinceLastLoop == 0) {
			// no time has passed, so don't add any bytes. Shouldn't happen.
			bytesToSpend = spendingRate / SEC2MS(1);
		} else {
			// prevent overflow
			uint64 uBytes = (uint64)timeSinceLastLoop * allowedDataRate;
			if (uBytes < _I64_MAX && _I64_MAX - (sint64)uBytes > spendingRate) {
				if (timeSinceLastLoop >= sleepTime + SEC2MS(2)) {
					theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: Time since last loop too long. time: %ims wanted: %ims Max: %ims"), timeSinceLastLoop, sleepTime, sleepTime + SEC2MS(2));
					timeSinceLastLoop = sleepTime + SEC2MS(2);
				}
				spendingRate += (sint64)uBytes;
				bytesToSpend = spendingRate / SEC2MS(1);
			} else {
				spendingRate = _I64_MAX;
				bytesToSpend = _I32_MAX;
			}
		}

		lastLoopTick = thisLoopTick;

		if (bytesToSpend > 0 || allowedDataRate == 0) {
			uint64 spentBytes = 0;
			uint64 spentOverhead = 0;
			bool bNeedMoreData = false;

			queueLocker.Lock();

			tempQueueLocker.Lock();
			// Move all sockets from m_TempControlQueue_list to normal m_ControlQueue_list
			m_ControlQueueFirst_list.splice(m_ControlQueueFirst_list.cend(), m_TempControlQueueFirst_list);
			m_ControlQueue_list.splice(m_ControlQueue_list.cend(), m_TempControlQueue_list);
			tempQueueLocker.Unlock();

			// Send any queued up control packets first
			while ((bytesToSpend > 0 && spentBytes < (uint64)bytesToSpend || allowedDataRate == 0 && spentBytes < 500)
				&& (!m_ControlQueueFirst_list.empty() || !m_ControlQueue_list.empty()))
			{
				ThrottledControlSocket *socket;
				if (!m_ControlQueueFirst_list.empty()) {
					socket = m_ControlQueueFirst_list.front();
					m_ControlQueueFirst_list.pop_front();
				} else if (!m_ControlQueue_list.empty()) {
					socket = m_ControlQueue_list.front();
					m_ControlQueue_list.pop_front();
				} else
					break;

				if (socket != NULL) {
					SocketSentBytes socketSentBytes = socket->SendControlData(allowedDataRate > 0 ? (uint32)(bytesToSpend - spentBytes) : 1u, minFragSize);
					spentBytes += socketSentBytes.sentBytesStandardPackets;
					spentBytes += socketSentBytes.sentBytesControlPackets;
					spentOverhead += socketSentBytes.sentBytesControlPackets;
				}
			}

			// Check if any sockets have got no data for a long time. Then trickle them a packet.
			for (INT_PTR slotCounter = 0; slotCounter < GetStandardListSize(); ++slotCounter) {
				ThrottledFileSocket *socket = m_StandardOrder_list[slotCounter];
				if (!socket) //should never happen
					theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: a NULL socket in the Standard list (trickle)! Prevented usage. Index: %u Size: %u"), (unsigned)slotCounter, (unsigned)GetStandardListSize());
				else if (!socket->IsBusyQuickCheck() && thisLoopTick >= socket->GetLastCalledSend() + SEC2MS(1)) {
					// trickle
					uint32 neededBytes = socket->GetNeededBytes();
					if (neededBytes > 0) {
						SocketSentBytes socketSentBytes = socket->SendFileAndControlData(neededBytes, minFragSize);
						uint32 lastSpentBytes = socketSentBytes.sentBytesControlPackets + socketSentBytes.sentBytesStandardPackets;
						if (lastSpentBytes) {
							spentBytes += lastSpentBytes;
							spentOverhead += socketSentBytes.sentBytesControlPackets;
							if (!bNeedMoreData && socketSentBytes.sentBytesStandardPackets > 0)
								bNeedMoreData = socket->IsLowOnFileDataQueued(EMBLOCKSIZE);
							if (slotCounter < m_highestNumberOfFullyActivatedSlots)
								m_highestNumberOfFullyActivatedSlots = slotCounter;
						}
					}
				}
			}

			// Equal bandwidth for all slots
			uint32 targetDataRate = theApp.uploadqueue->GetTargetClientDataRate(true);
			INT_PTR maxSlot = min(GetStandardListSize(), (INT_PTR)(allowedDataRate / targetDataRate));

			if (maxSlot > m_highestNumberOfFullyActivatedSlots)
				m_highestNumberOfFullyActivatedSlots = maxSlot;

			for (INT_PTR maxCounter = 0; maxCounter < min(maxSlot, GetStandardListSize()) && bytesToSpend > 0 && spentBytes < (uint64)bytesToSpend; ++maxCounter) {
				if (rememberedSlotCounter >= GetStandardListSize() || rememberedSlotCounter >= maxSlot)
					rememberedSlotCounter = 0;
				ThrottledFileSocket *socket = m_StandardOrder_list[rememberedSlotCounter];
				if (!socket)
					theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: a NULL socket in the Standard list (equal-for-all)! Prevented usage. Index: %u Size: %u"), (unsigned)rememberedSlotCounter, (unsigned)GetStandardListSize());
				else if (!socket->IsBusyQuickCheck()) {
					SocketSentBytes socketSentBytes = socket->SendFileAndControlData(mini(max(doubleSendSize, (uint32)(bytesToSpend / maxSlot)), (uint32)(bytesToSpend - spentBytes)), doubleSendSize);
					spentBytes += socketSentBytes.sentBytesControlPackets;
					spentOverhead += socketSentBytes.sentBytesControlPackets;
					if (socketSentBytes.sentBytesStandardPackets > 0) {
						spentBytes += socketSentBytes.sentBytesStandardPackets;
						if (!bNeedMoreData)
							bNeedMoreData = socket->IsLowOnFileDataQueued(EMBLOCKSIZE);
					}
				}
				++rememberedSlotCounter;
			}

			// Any remaining bandwidth will be used - from first to last.
			for (INT_PTR slotCounter = 0; slotCounter < GetStandardListSize() && bytesToSpend > 0 && spentBytes < (uint64)bytesToSpend; ++slotCounter) {
				ThrottledFileSocket *socket = m_StandardOrder_list[slotCounter];
				if (!socket)
					theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: a NULL socket in the Standard list (fully activated)! Prevented usage. Index: %u Size: %u"), (unsigned)slotCounter, (unsigned)GetStandardListSize());
				else if (!socket->IsBusyQuickCheck()) {
					uint32 bytesToSpendTemp = (uint32)(bytesToSpend - spentBytes);
					SocketSentBytes socketSentBytes = socket->SendFileAndControlData(max(bytesToSpendTemp, doubleSendSize), doubleSendSize);
					uint32 lastSpentBytes = socketSentBytes.sentBytesControlPackets + socketSentBytes.sentBytesStandardPackets;
					spentBytes += lastSpentBytes;
					spentOverhead += socketSentBytes.sentBytesControlPackets;
					if (!bNeedMoreData && socketSentBytes.sentBytesStandardPackets > 0)
						bNeedMoreData = socket->IsLowOnFileDataQueued(EMBLOCKSIZE);
					if (slotCounter >= m_highestNumberOfFullyActivatedSlots && (lastSpentBytes < bytesToSpendTemp || lastSpentBytes >= doubleSendSize)) // || lastSpentBytes > 0 && spentBytes == bytesToSpend ))
						m_highestNumberOfFullyActivatedSlots = slotCounter + 1;
				}
			}
			spendingRate -= SEC2MS(spentBytes);

			// If we couldn't spend all allocated bandwidth in this loop,
			// some of it is allowed to be saved and used the next loop
			sint64 newSpendingRate = -SEC2MS((GetStandardListSize() + 1i64) * minFragSize);
			if (spendingRate < newSpendingRate) {
				spendingRate = newSpendingRate;
				lastTickReachedBandwidth = thisLoopTick;
			} else if (spendingRate >= SEC2MS(1)) {
				spendingRate = SEC2MS(1) - 1;
				if (thisLoopTick >= lastTickReachedBandwidth + max(SEC2MS(1), timeSinceLastLoop * 2)) {
					m_highestNumberOfFullyActivatedSlots = GetStandardListSize() + 1;
					lastTickReachedBandwidth = thisLoopTick;
					//theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: request new slot due to bw not reached. m_highestNumberOfFullyActivatedSlots: %i GetStandardListSize(): %i tick: %i"), m_highestNumberOfFullyActivatedSlots, GetStandardListSize(), thisLoopTick);
				}
			} else
				lastTickReachedBandwidth = thisLoopTick;
			queueLocker.Unlock();

			// save info about how much data we've spent since the last time someone polled us about used bandwidth
			InterlockedAdd64((LONG64*)&m_SentBytesSinceLastCall, (LONG64)spentBytes);
			InterlockedAdd64((LONG64*)&m_SentBytesSinceLastCallOverhead, (LONG64)spentOverhead);

			if (bNeedMoreData && theApp.uploadqueue->GetUploadQueueLength() > 0 && theApp.m_pUploadDiskIOThread)
				theApp.m_pUploadDiskIOThread->WakeUpCall();
		}
	}

	queueLocker.Lock();
	tempQueueLocker.Lock();
	m_TempControlQueue_list.clear();
	m_TempControlQueueFirst_list.clear();
	tempQueueLocker.Unlock();

	m_ControlQueue_list.clear();
	m_StandardOrder_list.RemoveAll();
	queueLocker.Unlock();

	m_eventThreadEnded.SetEvent();
	return 0;
}

void UploadBandwidthThrottler::NewUploadDataAvailable()
{
	if (m_bRun)
		m_eventDataAvailable.SetEvent();
}

void UploadBandwidthThrottler::SocketAvailable()
{
	if (m_bRun)
		m_eventSocketAvailable.SetEvent();
}