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

#include "ThrottledSocket.h" // ZZ:UploadBandWithThrottler (UDP)

class UploadBandwidthThrottler :
	public CWinThread
{
public:
	UploadBandwidthThrottler();
	~UploadBandwidthThrottler();
	UploadBandwidthThrottler(const UploadBandwidthThrottler&) = delete;
	UploadBandwidthThrottler& operator=(const UploadBandwidthThrottler&) = delete;

	uint64 GetNumberOfSentBytesSinceLastCallAndReset();
	uint64 GetNumberOfSentBytesOverheadSinceLastCallAndReset();
	INT_PTR GetHighestNumberOfFullyActivatedSlotsSinceLastCallAndReset();

	INT_PTR GetStandardListSize()		{ return m_StandardOrder_list.GetCount(); };

	void AddToStandardList(INT_PTR index, ThrottledFileSocket *socket);
	bool RemoveFromStandardList(ThrottledFileSocket *socket);

	void QueueForSendingControlPacket(ThrottledControlSocket *socket, const bool hasSent = false); // ZZ:UploadBandWithThrottler (UDP)
	void RemoveFromAllQueues(ThrottledFileSocket *socket);
	void RemoveFromAllQueuesLocked(ThrottledControlSocket *socket); // ZZ:UploadBandWithThrottler (UDP)
	void NewUploadDataAvailable();
	void SocketAvailable();
	HANDLE GetSocketAvailableEvent()	{ return m_eventSocketAvailable; }

	void EndThread();

	void Pause(bool paused);
	static uint32 GetSlotLimit(uint32 currentUpSpeed);
private:
	static UINT AFX_CDECL RunProc(LPVOID pParam);
	UINT RunInternal();

	void RemoveFromAllQueuesNoLock(ThrottledControlSocket *socket); // ZZ:UploadBandWithThrottler (UDP)
	bool RemoveFromStandardListNoLock(ThrottledFileSocket *socket);

	static uint32 CalculateChangeDelta(uint32 numberOfConsecutiveChanges);

	CTypedPtrList<CPtrList, ThrottledControlSocket*> m_ControlQueue_list; // a queue for all the sockets that want to have Send() called on them. // ZZ:UploadBandWithThrottler (UDP)
	CTypedPtrList<CPtrList, ThrottledControlSocket*> m_ControlQueueFirst_list; // a queue for all the sockets that want to have Send() called on them. // ZZ:UploadBandWithThrottler (UDP)
	CTypedPtrList<CPtrList, ThrottledControlSocket*> m_TempControlQueue_list; // sockets that wants to enter m_ControlQueue_list // ZZ:UploadBandWithThrottler (UDP)
	CTypedPtrList<CPtrList, ThrottledControlSocket*> m_TempControlQueueFirst_list; // sockets that wants to enter m_ControlQueue_list and has been able to send before // ZZ:UploadBandWithThrottler (UDP)

	CArray<ThrottledFileSocket*, ThrottledFileSocket*> m_StandardOrder_list; // sockets that have upload slots. Ordered so the most prioritized socket is first

	CCriticalSection sendLocker;
	CCriticalSection tempQueueLocker;

	CEvent *threadEndedEvent;
	CEvent *pauseEvent;
	CEvent m_eventNewDataAvailable;
	CEvent m_eventSocketAvailable;

	uint64 m_SentBytesSinceLastCall;
	uint64 m_SentBytesSinceLastCallOverhead;
	INT_PTR m_highestNumberOfFullyActivatedSlots;

	volatile bool doRun;
};