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
#include <timeapi.h>
#include "emule.h"
#include "UploadBandwidthThrottler.h"
#include "EMSocket.h"
#include "opcodes.h"
#include "LastCommonRouteFinder.h"
#include "OtherFunctions.h"
#include "emuledlg.h"
#include "uploadqueue.h"
#include "preferences.h"
#include "UploadDiskIOThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/**
 * The constructor starts the thread.
 */
UploadBandwidthThrottler::UploadBandwidthThrottler()
{
	m_SentBytesSinceLastCall = 0;
	m_SentBytesSinceLastCallOverhead = 0;
	m_highestNumberOfFullyActivatedSlots = 0;

	threadEndedEvent = new CEvent(FALSE, TRUE);
	pauseEvent = new CEvent(TRUE, TRUE);

	doRun = true;
	AfxBeginThread(RunProc, (LPVOID)this);
}

/**
 * The destructor stops the thread. If the thread has already stopped, the destructor does nothing.
 */
UploadBandwidthThrottler::~UploadBandwidthThrottler()
{
	EndThread();
	delete threadEndedEvent;
	delete pauseEvent;
}

/**
 * Find out how many bytes that has been put on the sockets since the last call to this
 * method. Includes overhead of control packets.
 *
 * @return the number of bytes that has been put on the sockets since the last call
 */
uint64 UploadBandwidthThrottler::GetNumberOfSentBytesSinceLastCallAndReset()
{
	sendLocker.Lock();

	uint64 numberOfSentBytesSinceLastCall = m_SentBytesSinceLastCall;
	m_SentBytesSinceLastCall = 0;

	sendLocker.Unlock();

	return numberOfSentBytesSinceLastCall;
}

/**
 * Find out how many bytes that has been put on the sockets since the last call to this
 * method. Excludes overhead of control packets.
 *
 * @return the number of bytes that has been put on the sockets since the last call
 */
uint64 UploadBandwidthThrottler::GetNumberOfSentBytesOverheadSinceLastCallAndReset()
{
	sendLocker.Lock();

	uint64 numberOfSentBytesSinceLastCall = m_SentBytesSinceLastCallOverhead;
	m_SentBytesSinceLastCallOverhead = 0;

	sendLocker.Unlock();

	return numberOfSentBytesSinceLastCall;
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
	sendLocker.Lock();

	//if(m_highestNumberOfFullyActivatedSlots > GetStandardListSize())
	//	theApp.QueueDebugLogLine(true, _T("UploadBandwidthThrottler: Throttler wants new slot when get-method called. m_highestNumberOfFullyActivatedSlots: %i GetStandardListSize(): %i tick: %i"), m_highestNumberOfFullyActivatedSlots, GetStandardListSize(), timeGetTime());

	INT_PTR highestNumberOfFullyActivatedSlots = m_highestNumberOfFullyActivatedSlots;
	m_highestNumberOfFullyActivatedSlots = 0;

	sendLocker.Unlock();

	return highestNumberOfFullyActivatedSlots;
}

/**
 * Add a socket to the list of sockets that have upload slots. The main thread will
 * continuously call send on these sockets, to give them chance to work off their queues.
 * The sockets are called in the order they exist in the list, so the top socket (index 0)
 * will be given a chance first to use bandwidth, and then the next socket (index 1) etc.
 *
 * It is possible to add a socket several times to the list without removing it in between,
 * but that should be avoided.
 *
 * @param index insert the socket at this place in the list. An index that is higher than the
 *			  current number of sockets in the list will mean that the socket should be inserted
 *			  last in the list.
 *
 * @param socket the address to the socket that should be added to the list. If the address is NULL,
 *			   this method will do nothing.
 */
void UploadBandwidthThrottler::AddToStandardList(INT_PTR index, ThrottledFileSocket *socket)
{
	if (socket != NULL) {
		sendLocker.Lock();

		RemoveFromStandardListNoLock(socket);
		if (index > GetStandardListSize())
			index = GetStandardListSize();

		m_StandardOrder_list.InsertAt(index, socket);

		sendLocker.Unlock();
	}
//	else if (thePrefs.GetVerbose())
//		theApp.AddDebugLogLine(true,"Tried to add NULL socket to UploadBandwidthThrottler Standard list! Prevented.");
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
	sendLocker.Lock();

	bool returnValue = RemoveFromStandardListNoLock(socket);

	sendLocker.Unlock();

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
* for the supplied socket. It is allowed to call this method several times
* for the same socket, without having controlpacket send called for the socket
* first. The duplicate entries are never filtered, since it incurs less cpu
* overhead to simply call Send() in the socket for each double. Send() will
* already have done its work when the second Send() is called, and will just
* return with little cpu overhead.
*
* @param socket address to the socket that requests to have controlpacket send
*			   to be called on it
*/
void UploadBandwidthThrottler::QueueForSendingControlPacket(ThrottledControlSocket *socket, const bool hasSent)
{
	// Get critical section
	tempQueueLocker.Lock();

	if (doRun)
		if (hasSent)
			m_TempControlQueueFirst_list.AddTail(socket);
		else
			m_TempControlQueue_list.AddTail(socket);

	// End critical section
	tempQueueLocker.Unlock();
}

/**
 * Remove the socket from all lists and queues. This will make it safe to
 * erase/delete the socket. It will also cause the main thread to stop calling
 * send() for the socket.
 *
 * @param socket address to the socket that should be removed
 */
void UploadBandwidthThrottler::RemoveFromAllQueuesNoLock(ThrottledControlSocket *socket)
{
	if (doRun) {
		// Remove this socket from control packet queue
		for (POSITION pos = m_ControlQueue_list.GetHeadPosition(); pos != NULL;) {
			POSITION todel = pos;
			if (m_ControlQueue_list.GetNext(pos) == socket)
				m_ControlQueue_list.RemoveAt(todel);
		}
		for (POSITION pos = m_ControlQueueFirst_list.GetHeadPosition(); pos != NULL;) {
			POSITION todel = pos;
			if (m_ControlQueueFirst_list.GetNext(pos) == socket)
				m_ControlQueueFirst_list.RemoveAt(todel);
		}
		tempQueueLocker.Lock();
		for (POSITION pos = m_TempControlQueue_list.GetHeadPosition(); pos != NULL;) {
			POSITION todel = pos;
			if (m_TempControlQueue_list.GetNext(pos) == socket)
				m_TempControlQueue_list.RemoveAt(todel);
		}
		for (POSITION pos = m_TempControlQueueFirst_list.GetHeadPosition(); pos != NULL;) {
			POSITION todel = pos;
			if (m_TempControlQueueFirst_list.GetNext(pos) == socket)
				m_TempControlQueueFirst_list.RemoveAt(todel);
		}
		tempQueueLocker.Unlock();
	}
}

void UploadBandwidthThrottler::RemoveFromAllQueues(ThrottledFileSocket *socket)
{
	// Get critical section
	sendLocker.Lock();

	if (doRun) {
		RemoveFromAllQueuesNoLock(socket);

		// And remove it from upload slots
		RemoveFromStandardListNoLock(socket);
	}

	// End critical section
	sendLocker.Unlock();
}

void UploadBandwidthThrottler::RemoveFromAllQueuesLocked(ThrottledControlSocket *socket)
{
	sendLocker.Lock();
	RemoveFromAllQueuesNoLock(socket);
	sendLocker.Unlock();
}

/**
 * Make the thread exit. This method will not return until the thread has stopped
 * looping. This guarantees that the thread will not access the CEMSockets after this
 * call has exited.
 */
void UploadBandwidthThrottler::EndThread()
{
	sendLocker.Lock();

	// signal the thread to stop looping and exit.
	doRun = false;

	sendLocker.Unlock();

	Pause(false);

	// wait for the thread to signal that it has stopped looping.
	threadEndedEvent->Lock();
}

void UploadBandwidthThrottler::Pause(bool paused)
{
	if (paused)
		pauseEvent->ResetEvent();
	else
		pauseEvent->SetEvent();
}

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
	static bool estimateChangedLog = false;
	static bool lotsOfLog = false;

	sint64 realBytesToSpend = 0;
	INT_PTR rememberedSlotCounter = 0;

	uint32 nEstiminatedLimit = 0;
	int nSlotsBusyLevel = 0;
	DWORD nUploadStartTime = 0;
	uint32 numberOfConsecutiveUpChanges = 0;
	uint32 numberOfConsecutiveDownChanges = 0;
	uint32 changesCount = 0;
	uint32 loopsCount = 0;

	DWORD lastLoopTick = timeGetTime();
	DWORD lastTickReachedBandwidth = lastLoopTick;
	while (doRun) {
		pauseEvent->Lock();

		DWORD timeSinceLastLoop = timeGetTime() - lastLoopTick;

		// Get current speed from UploadSpeedSense
		uint32 allowedDataRate = theApp.lastCommonRouteFinder->GetUpload();

		// check busy level for all the slots (WSAEWOULDBLOCK status)
		uint32 nBusy = 0;
		uint32 nCanSend = 0;
		sendLocker.Lock();
		m_eventNewDataAvailable.ResetEvent();
		m_eventSocketAvailable.ResetEvent();
		for (INT_PTR i = mini(GetStandardListSize(), (INT_PTR)max(GetSlotLimit(theApp.uploadqueue->GetDatarate()), 3u)); --i >= 0;)
			if (m_StandardOrder_list[i] != NULL && m_StandardOrder_list[i]->HasQueues()) {
				++nCanSend;
				nBusy += static_cast<uint32>(m_StandardOrder_list[i]->IsBusyExtensiveCheck());
			}

		sendLocker.Unlock();

		// if this is kept, the loop above can be optimized a little (don't count nCanSend, just use nCanSend = GetSlotLimit(theApp.uploadqueue->GetDatarate())
		//if(theApp.uploadqueue)
		//   nCanSend = max(nCanSend, GetSlotLimit(theApp.uploadqueue->GetDatarate()));

		// When no upload limit has been set in options, try to guess a good upload limit.
		if (thePrefs.GetMaxUpload() == UNLIMITED) {
			++loopsCount;
			//if (lotsOfLog)
			//	theApp.QueueDebugLogLine(false,_T("Throttler: busy: %i/%i nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount: %i loopsCount: %i"), nBusy, nCanSend, nSlotsBusyLevel, nEstiminatedLimit/1024.0f, changesCount, loopsCount);
			if (nCanSend > 0) {
				//float fBusyFraction = nBusy / (float)nCanSend;
				//conditions were: "fBusyFraction > 0.75f" and "fBusyFraction < 0.25f"
				const int iBusyFraction = (nBusy << 5)/ nCanSend; //the limits would be 24 and 8
				if (nBusy > 2 && iBusyFraction > 24 && nSlotsBusyLevel < 255) {
					++nSlotsBusyLevel;
					++changesCount;
					if (thePrefs.GetVerbose() && lotsOfLog && nSlotsBusyLevel % 25 == 0)
						theApp.QueueDebugLogLine(false, _T("Throttler: nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount: %i loopsCount: %i"), nSlotsBusyLevel, nEstiminatedLimit / 1024.0f, changesCount, loopsCount);
				} else if ((nBusy <= 2 || iBusyFraction < 8) && nSlotsBusyLevel > -255) {
					--nSlotsBusyLevel;
					++changesCount;
					if (thePrefs.GetVerbose() && lotsOfLog && nSlotsBusyLevel % 25 == 0)
						theApp.QueueDebugLogLine(false, _T("Throttler: nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount %i loopsCount: %i"), nSlotsBusyLevel, nEstiminatedLimit / 1024.0f, changesCount, loopsCount);
				}
			}

			if (nUploadStartTime == 0) {
				if (GetStandardListSize() >= 3)
					nUploadStartTime = timeGetTime();
			} else if (timeGetTime() >= nUploadStartTime + SEC2MS(60)) {
				if (theApp.uploadqueue) {
					if (nEstiminatedLimit == 0) { // no autolimit was set yet
						if (nSlotsBusyLevel >= 250) { // sockets indicated that the BW limit has been reached
							nEstiminatedLimit = theApp.uploadqueue->GetDatarate();
							if (nEstiminatedLimit < allowedDataRate)
								allowedDataRate = nEstiminatedLimit;
							nSlotsBusyLevel = -200;
							if (thePrefs.GetVerbose() && estimateChangedLog)
								theApp.QueueDebugLogLine(false, _T("Throttler: Set initial estimated limit to %0.5f changesCount: %i loopsCount: %i"), nEstiminatedLimit / 1024.0f, changesCount, loopsCount);
							changesCount = 0;
							loopsCount = 0;
						}
					} else {
						if (nSlotsBusyLevel > 250) {
							if (changesCount > 500 || (changesCount > 300 && loopsCount > 1000) || loopsCount > 2000)
								numberOfConsecutiveDownChanges = 0;

							++numberOfConsecutiveDownChanges;
							uint32 changeDelta = CalculateChangeDelta(numberOfConsecutiveDownChanges);

							// Don't lower speed below 1 KiB/s
							if (nEstiminatedLimit < changeDelta + 1024)
								changeDelta = (nEstiminatedLimit > 1024) ? nEstiminatedLimit - 1024 : 0;

							ASSERT(nEstiminatedLimit >= changeDelta + 1024);
							nEstiminatedLimit -= changeDelta;

							if (thePrefs.GetVerbose() && estimateChangedLog)
								theApp.QueueDebugLogLine(false, _T("Throttler: REDUCED limit #%i with %i bytes to: %0.5f changesCount: %i loopsCount: %i"), numberOfConsecutiveDownChanges, changeDelta, nEstiminatedLimit / 1024.0f, changesCount, loopsCount);

							numberOfConsecutiveUpChanges = 0;
							nSlotsBusyLevel = 0;
							changesCount = 0;
							loopsCount = 0;
						} else if (nSlotsBusyLevel < -250) {
							if (changesCount > 500 || (changesCount > 300 && loopsCount > 1000) || loopsCount > 2000)
								numberOfConsecutiveUpChanges = 0;

							++numberOfConsecutiveUpChanges;
							uint32 changeDelta = CalculateChangeDelta(numberOfConsecutiveUpChanges);

							// Don't raise speed unless we are under current allowedDataRate
							if (nEstiminatedLimit + changeDelta > allowedDataRate)
								changeDelta = (nEstiminatedLimit < allowedDataRate) ? allowedDataRate - nEstiminatedLimit : 0;

							ASSERT(nEstiminatedLimit < allowedDataRate && nEstiminatedLimit + changeDelta <= allowedDataRate || nEstiminatedLimit >= allowedDataRate && changeDelta == 0);
							nEstiminatedLimit += changeDelta;

							if (thePrefs.GetVerbose() && estimateChangedLog)
								theApp.QueueDebugLogLine(false, _T("Throttler: INCREASED limit #%i with %i bytes to: %0.5f changesCount: %i loopsCount: %i"), numberOfConsecutiveUpChanges, changeDelta, nEstiminatedLimit / 1024.0f, changesCount, loopsCount);

							numberOfConsecutiveDownChanges = 0;
							nSlotsBusyLevel = 0;
							changesCount = 0;
							loopsCount = 0;
						}

						if (nEstiminatedLimit < allowedDataRate)
							allowedDataRate = nEstiminatedLimit;
					}
				}
			}

			if (nCanSend == nBusy && GetStandardListSize() > 0 && nSlotsBusyLevel < 125) {
				nSlotsBusyLevel = 125;
				if (thePrefs.GetVerbose() && lotsOfLog)
					theApp.QueueDebugLogLine(false, _T("Throttler: nSlotsBusyLevel: %i Guessed limit: %0.5f changesCount %i loopsCount: %i (set due to all slots busy)"), nSlotsBusyLevel, nEstiminatedLimit / 1024.0f, changesCount, loopsCount);
			}
		}

		uint32 minFragSize;
		uint32 doubleSendSize;
		if (allowedDataRate < 6 * 1024) {
			minFragSize = 536;
			doubleSendSize = minFragSize; // don't send two packets at a time at very low speeds to give a smoother upload
		} else {
			minFragSize = 1300;
			doubleSendSize = minFragSize * 2; // send two packets at a time so they can share an ACK
		}

#define TIME_BETWEEN_UPLOAD_LOOPS 1
		DWORD sleepTime;
		if (allowedDataRate == _UI32_MAX || realBytesToSpend >= 1000 || (allowedDataRate == 0 && nEstiminatedLimit == 0))
			// we could send at once, but sleep a while to not suck up all cpu
			sleepTime = TIME_BETWEEN_UPLOAD_LOOPS;
		else {
			if (allowedDataRate == 0)
				sleepTime = (DWORD)ceil((doubleSendSize * 1000.0) / nEstiminatedLimit);
			else
				// sleep for just as long as we need to get back to having one byte to send
				sleepTime = (DWORD)ceil((1000.0 - realBytesToSpend) / allowedDataRate);
			if (sleepTime < TIME_BETWEEN_UPLOAD_LOOPS)
				sleepTime = TIME_BETWEEN_UPLOAD_LOOPS;
		}
		if (timeSinceLastLoop < sleepTime) {
			DWORD dwSleep = sleepTime - timeSinceLastLoop;
			if (nCanSend == 0) {
				if (theApp.m_pUploadDiskIOThread != NULL)
					theApp.m_pUploadDiskIOThread->SocketNeedsMoreData();
				::WaitForSingleObject(m_eventNewDataAvailable, dwSleep);
			} else if (nCanSend == nBusy)
				::WaitForSingleObject(m_eventSocketAvailable, dwSleep);
			else
				::Sleep(dwSleep);
		}

		const DWORD thisLoopTick = timeGetTime();
		timeSinceLastLoop = thisLoopTick - lastLoopTick;

		// Calculate how many bytes we can spend
		sint64 bytesToSpend;
		if (allowedDataRate != _UI32_MAX) {
			// prevent overflow
			if (timeSinceLastLoop == 0) {
				// no time has passed, so don't add any bytes. Shouldn't happen.
				bytesToSpend = realBytesToSpend / SEC2MS(1);
			} else if (_I64_MAX / timeSinceLastLoop > (sint64)allowedDataRate && _I64_MAX - allowedDataRate * (sint64)timeSinceLastLoop > realBytesToSpend) {
				if (timeSinceLastLoop >= sleepTime + SEC2MS(2)) {
					theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: Time since last loop too long. time: %ims wanted: %ims Max: %ims"), timeSinceLastLoop, sleepTime, sleepTime + 2000);

					timeSinceLastLoop = sleepTime + SEC2MS(2);
				}

				realBytesToSpend += allowedDataRate * (uint64)timeSinceLastLoop;
				bytesToSpend = realBytesToSpend / SEC2MS(1);
			} else {
				realBytesToSpend = _I64_MAX;
				bytesToSpend = _I32_MAX;
			}
		} else {
			realBytesToSpend = 0; //_I64_MAX;
			bytesToSpend = _I32_MAX;
		}

		lastLoopTick = thisLoopTick;

		if (bytesToSpend > 0 || allowedDataRate == 0) {

			sendLocker.Lock();

			tempQueueLocker.Lock();

			// are there any sockets in m_TempControlQueue_list? Move them to normal m_ControlQueue_list;
			while (!m_TempControlQueueFirst_list.IsEmpty())
				m_ControlQueueFirst_list.AddTail(m_TempControlQueueFirst_list.RemoveHead());

			while (!m_TempControlQueue_list.IsEmpty())
				m_ControlQueue_list.AddTail(m_TempControlQueue_list.RemoveHead());

			tempQueueLocker.Unlock();

			uint64 spentBytes = 0;
			uint64 spentOverhead = 0;
			bool bNeedMoreData = false;
			// Send any queued up control packets first
			while ((bytesToSpend > 0 && spentBytes < (uint64)bytesToSpend || allowedDataRate == 0 && spentBytes < 500) && (!m_ControlQueueFirst_list.IsEmpty() || !m_ControlQueue_list.IsEmpty())) {
				ThrottledControlSocket *socket;

				if (!m_ControlQueueFirst_list.IsEmpty())
					socket = m_ControlQueueFirst_list.RemoveHead();
				else {
					if (m_ControlQueue_list.IsEmpty())
						break;
					socket = m_ControlQueue_list.RemoveHead();
				}
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

				if (socket != NULL) {
					if (!socket->IsBusyQuickCheck() && thisLoopTick >= socket->GetLastCalledSend() + SEC2MS(1)) {
						// trickle
						uint32 neededBytes = socket->GetNeededBytes();

						if (neededBytes > 0) {
							SocketSentBytes socketSentBytes = socket->SendFileAndControlData(neededBytes, minFragSize);
							uint32 lastSpentBytes = socketSentBytes.sentBytesControlPackets + socketSentBytes.sentBytesStandardPackets;
							spentBytes += lastSpentBytes;
							spentOverhead += socketSentBytes.sentBytesControlPackets;
							if (socketSentBytes.sentBytesStandardPackets > 0 && !socket->IsEnoughFileDataQueued(EMBLOCKSIZE))
								bNeedMoreData = true;
							if (lastSpentBytes > 0 && slotCounter < m_highestNumberOfFullyActivatedSlots)
								m_highestNumberOfFullyActivatedSlots = slotCounter;
						}
					}
				} else
					theApp.QueueDebugLogLine(false, _T("There was a NULL socket in the UploadBandwidthThrottler Standard list (trickle)! Prevented usage. Index: %u Size: %u"), (unsigned)slotCounter, (unsigned)GetStandardListSize());
			}

			// Equal bandwidth for all slots
			uint32 targetDataRate = theApp.uploadqueue->GetTargetClientDataRate(true);
			INT_PTR maxSlot = GetStandardListSize();
			if (maxSlot > (INT_PTR)(allowedDataRate / targetDataRate))
				maxSlot = allowedDataRate / targetDataRate;

			if (maxSlot > m_highestNumberOfFullyActivatedSlots)
				m_highestNumberOfFullyActivatedSlots = maxSlot;

			for (INT_PTR maxCounter = 0; maxCounter < min(maxSlot, GetStandardListSize()) && bytesToSpend > 0 && spentBytes < (uint64)bytesToSpend; ++maxCounter) {
				if (rememberedSlotCounter >= GetStandardListSize() || rememberedSlotCounter >= maxSlot)
					rememberedSlotCounter = 0;

				ThrottledFileSocket *socket = m_StandardOrder_list[rememberedSlotCounter];
				if (socket != NULL) {
					if (!socket->IsBusyQuickCheck()) {
						SocketSentBytes socketSentBytes = socket->SendFileAndControlData(mini(max((uint32)doubleSendSize, (uint32)(bytesToSpend / maxSlot)), (uint32)(bytesToSpend - spentBytes)), doubleSendSize);
						spentBytes += socketSentBytes.sentBytesStandardPackets;
						spentBytes += socketSentBytes.sentBytesControlPackets;
						spentOverhead += socketSentBytes.sentBytesControlPackets;
						if (socketSentBytes.sentBytesStandardPackets > 0 && !socket->IsEnoughFileDataQueued(EMBLOCKSIZE))
							bNeedMoreData = true;
					}
				} else
					theApp.QueueDebugLogLine(false, _T("There was a NULL socket in the UploadBandwidthThrottler Standard list (equal-for-all)! Prevented usage. Index: %u Size: %u"), (unsigned)rememberedSlotCounter, (unsigned)GetStandardListSize());

				++rememberedSlotCounter;
			}

			// Any bandwidth that hasn't been used yet are used first to last.
			for (INT_PTR slotCounter = 0; slotCounter < GetStandardListSize() && bytesToSpend > 0 && spentBytes < (uint64)bytesToSpend; ++slotCounter) {
				ThrottledFileSocket *socket = m_StandardOrder_list[slotCounter];

				if (socket != NULL) {
					if (!socket->IsBusyQuickCheck()) {
						uint32 bytesToSpendTemp = (uint32)(bytesToSpend - spentBytes);
						SocketSentBytes socketSentBytes = socket->SendFileAndControlData(max(bytesToSpendTemp, doubleSendSize), doubleSendSize);
						uint32 lastSpentBytes = socketSentBytes.sentBytesControlPackets + socketSentBytes.sentBytesStandardPackets;
						spentBytes += lastSpentBytes;
						spentOverhead += socketSentBytes.sentBytesControlPackets;
						if (socketSentBytes.sentBytesStandardPackets > 0 && !socket->IsEnoughFileDataQueued(EMBLOCKSIZE))
							bNeedMoreData = true;

						if (slotCounter + 1 > m_highestNumberOfFullyActivatedSlots && (lastSpentBytes < bytesToSpendTemp || lastSpentBytes >= doubleSendSize)) // || lastSpentBytes > 0 && spentBytes == bytesToSpend ))
							m_highestNumberOfFullyActivatedSlots = slotCounter + 1;
					}
				} else
					theApp.QueueDebugLogLine(false, _T("There was a NULL socket in the UploadBandwidthThrottler Standard list (fully activated)! Prevented usage. Index: %u Size: %u"), (unsigned)slotCounter, (unsigned)GetStandardListSize());
			}
			realBytesToSpend -= SEC2MS(spentBytes);

			// If we couldn't spend all allocated bandwidth this loop, some of it is allowed to be saved
			// and used the next loop
			sint64 newRealBytesToSpend = -(((sint64)GetStandardListSize() + 1) * minFragSize) * 1000;
			if (realBytesToSpend < newRealBytesToSpend) {
				realBytesToSpend = newRealBytesToSpend;
				lastTickReachedBandwidth = thisLoopTick;
			} else {
				if (realBytesToSpend > 999) {
					realBytesToSpend = 999;
					if (thisLoopTick - lastTickReachedBandwidth > max(500, timeSinceLastLoop) * 2) {
						m_highestNumberOfFullyActivatedSlots = GetStandardListSize() + 1;
						lastTickReachedBandwidth = thisLoopTick;
						//theApp.QueueDebugLogLine(false, _T("UploadBandwidthThrottler: Throttler requests new slot due to bw not reached. m_highestNumberOfFullyActivatedSlots: %i GetStandardListSize(): %i tick: %i"), m_highestNumberOfFullyActivatedSlots, GetStandardListSize(), thisLoopTick);
					}
				} else
					lastTickReachedBandwidth = thisLoopTick;
			}

			// save info about how much bandwidth we've managed to use since the last time someone polled us about used bandwidth
			m_SentBytesSinceLastCall += spentBytes;
			m_SentBytesSinceLastCallOverhead += spentOverhead;

			sendLocker.Unlock();

			if (bNeedMoreData && theApp.m_pUploadDiskIOThread != NULL)
				theApp.m_pUploadDiskIOThread->SocketNeedsMoreData();
		}
	}

	threadEndedEvent->SetEvent();

	tempQueueLocker.Lock();
	m_TempControlQueue_list.RemoveAll();
	m_TempControlQueueFirst_list.RemoveAll();
	tempQueueLocker.Unlock();

	sendLocker.Lock();
	m_ControlQueue_list.RemoveAll();
	m_StandardOrder_list.RemoveAll();
	sendLocker.Unlock();

	return 0;
}

void UploadBandwidthThrottler::NewUploadDataAvailable()
{
	if (doRun)
		m_eventNewDataAvailable.SetEvent();
}

void UploadBandwidthThrottler::SocketAvailable()
{
	if (doRun)
		m_eventSocketAvailable.SetEvent();
}