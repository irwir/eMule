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
#include "emule.h"
#include "Opcodes.h"
#include "LastCommonRouteFinder.h"
#include "Server.h"
#include "OtherFunctions.h"
#include "UpDownClient.h"
#include "Preferences.h"
#include "Pinger.h"
#include "emuledlg.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


LastCommonRouteFinder::LastCommonRouteFinder()
	: pingDelaysTotal()
	, m_LowestInitialPingAllowed(20)
	, minUpload(1)
	, maxUpload(_UI32_MAX)
	, m_CurUpload(1)
	, m_upload(_UI32_MAX)
	, m_iPingToleranceMilliseconds(200)
	, m_iNumberOfPingsForAverage()
	, m_pingAverage()
	, m_lowestPing()
	, m_uState()
	, doRun(true)
	, m_enabled()
	, m_initiateFastReactionPeriod()
	, m_bUseMillisecondPingTolerance()
	, needMoreHosts()
{
	threadEndedEvent = new CEvent(FALSE, TRUE);
	newTraceRouteHostEvent = new CEvent(FALSE);
	prefsEvent = new CEvent(FALSE);
	
	AfxBeginThread(RunProc, (LPVOID)this);
}

LastCommonRouteFinder::~LastCommonRouteFinder()
{
	delete threadEndedEvent;
	delete newTraceRouteHostEvent;
	delete prefsEvent;
}

bool LastCommonRouteFinder::AddHostToCheck(uint32 ip)
{
	bool gotEnoughHosts = true;

	if (needMoreHosts && IsGoodIP(ip, true)) {
		addHostLocker.Lock();

		if (needMoreHosts)
			gotEnoughHosts = AddHostToCheckNoLock(ip);

		addHostLocker.Unlock();
	}

	return gotEnoughHosts;
}

bool LastCommonRouteFinder::AddHostToCheckNoLock(uint32 ip)
{
	if (needMoreHosts && IsGoodIP(ip, true)) {
		//hostsToTraceRoute.AddTail(ip);
		hostsToTraceRoute.SetAt(ip, 0);

		if (hostsToTraceRoute.GetCount() >= 10) {
			needMoreHosts = false;

			// Signal that there's hosts to fetch.
			newTraceRouteHostEvent->SetEvent();
		}
	}

	return !needMoreHosts;
}

bool LastCommonRouteFinder::AddHostsToCheck(CTypedPtrList<CPtrList, CServer*> &list)
{
	bool gotEnoughHosts = true;

	if (needMoreHosts) {
		addHostLocker.Lock();

		if (needMoreHosts) {
			INT_PTR cnt = list.GetCount(); //index must be in the range [0, cnt-1]
			if (cnt > 0) {
				POSITION pos = list.FindIndex(rand() / (RAND_MAX / min(cnt, 100)));
				if (pos == NULL)
					pos = list.GetHeadPosition();
				while (needMoreHosts && --cnt >= 0) {
					AddHostToCheckNoLock(list.GetNext(pos)->GetIP());
					if (pos == NULL)
						pos = list.GetHeadPosition();
				}
			}
		}

		gotEnoughHosts = !needMoreHosts;

		addHostLocker.Unlock();
	}

	return gotEnoughHosts;
}

bool LastCommonRouteFinder::AddHostsToCheck(CUpDownClientPtrList &list)
{
	bool gotEnoughHosts = true;

	if (needMoreHosts) {
		addHostLocker.Lock();

		if (needMoreHosts) {
			int cnt = (int)list.GetCount(); //index must be in the range [0, count-1]
			if (cnt > 0) {
				POSITION pos = list.FindIndex(rand() / (RAND_MAX / min(cnt, 100)));
				if (pos == NULL)
					pos = list.GetHeadPosition();
				for (cnt = 0; needMoreHosts && cnt < list.GetCount(); ++cnt) {
					AddHostToCheckNoLock(list.GetNext(pos)->GetIP());
					if (pos == NULL)
						pos = list.GetHeadPosition();
				}
			}
		}

		gotEnoughHosts = !needMoreHosts;

		addHostLocker.Unlock();
	}

	return gotEnoughHosts;
}

CurrentPingStruct LastCommonRouteFinder::GetCurrentPing()
{
	CurrentPingStruct returnVal;

	if (m_enabled) {
		pingLocker.Lock();
		if (m_uState)
			returnVal.state = GetResString(m_uState);
		returnVal.latency = m_pingAverage;
		returnVal.lowest = m_lowestPing;
		returnVal.currentLimit = m_upload;
		pingLocker.Unlock();
	} else {
		returnVal.latency = 0;
		returnVal.lowest = 0;
		returnVal.currentLimit = 0;
	}

	return returnVal;
}

bool LastCommonRouteFinder::AcceptNewClient()
{
	return acceptNewClient || !m_enabled; // if enabled, then return acceptNewClient, otherwise return true
}

void LastCommonRouteFinder::SetPrefs(bool pEnabled, uint32 pCurUpload, uint32 pMinUpload, uint32 pMaxUpload, bool pUseMillisecondPingTolerance, double pPingTolerance, uint32 pPingToleranceMilliseconds, uint32 pGoingUpDivider, uint32 pGoingDownDivider, uint32 pNumberOfPingsForAverage, uint64 pLowestInitialPingAllowed)
{
	bool sendEvent = false;

	prefsLocker.Lock();

	minUpload = max(1024, pMinUpload);

	if (pMaxUpload != 0) {
		maxUpload = pMaxUpload;
		if (minUpload > maxUpload)
			minUpload = maxUpload;
	} else
		maxUpload = pCurUpload + 10 * 1024; //_UI32_MAX;

	if (pEnabled && m_enabled) {
		sendEvent = true;
		// this will show the area for ping info in status bar.
		theApp.emuledlg->SetStatusBarPartsSize();
	} else if (!pEnabled) {
		if (m_enabled) {
			// this will remove the area for ping info in status bar.
			theApp.emuledlg->SetStatusBarPartsSize();
		}
		//prefsEvent->ResetEvent();
		sendEvent = true;
	}

	// this will resize the area for ping info in status bar.
	if (m_bUseMillisecondPingTolerance != pUseMillisecondPingTolerance)
		theApp.emuledlg->SetStatusBarPartsSize();

	m_enabled = pEnabled;
	m_bUseMillisecondPingTolerance = pUseMillisecondPingTolerance;
	m_pingTolerance = pPingTolerance;
	m_iPingToleranceMilliseconds = pPingToleranceMilliseconds;
	m_goingUpDivider = pGoingUpDivider;
	m_goingDownDivider = pGoingDownDivider;
	m_CurUpload = pCurUpload;
	m_iNumberOfPingsForAverage = pNumberOfPingsForAverage;
	m_LowestInitialPingAllowed = pLowestInitialPingAllowed;

	prefsLocker.Unlock();

	if (m_upload > maxUpload || !pEnabled)
		m_upload = maxUpload;

	if (sendEvent)
		prefsEvent->SetEvent();
}

void LastCommonRouteFinder::InitiateFastReactionPeriod()
{
	prefsLocker.Lock();

	m_initiateFastReactionPeriod = true;

	prefsLocker.Unlock();
}

uint32 LastCommonRouteFinder::GetUpload() const
{
	return m_upload;
}

void LastCommonRouteFinder::SetUpload(uint32 newValue)
{
	m_upload = newValue;
}

/**
 * Make the thread exit. This method will not return until the thread has stopped
 * looping.
 */
void LastCommonRouteFinder::EndThread()
{
	// signal the thread to stop looping and exit.
	doRun = false;
	needMoreHosts = false;

	prefsEvent->SetEvent();
	newTraceRouteHostEvent->SetEvent();

	// wait for the thread to signal that it has stopped looping.
	threadEndedEvent->Lock();
}

/**
 * Start the thread. Called from the constructor in this class.
 *
 * @param pParam
 *
 * @return
 */
UINT AFX_CDECL LastCommonRouteFinder::RunProc(LPVOID pParam)
{
	DbgSetThreadName("LastCommonRouteFinder");
	InitThreadLocale();
	LastCommonRouteFinder *lastCommonRouteFinder = static_cast<LastCommonRouteFinder*>(pParam);
	return lastCommonRouteFinder->RunInternal();
}

/**
 * @return always returns 0.
 */
UINT LastCommonRouteFinder::RunInternal()
{
	Pinger pinger;
	bool hasSucceededAtLeastOnce = false;

	while (doRun) {
		// wait for updated prefs
		prefsEvent->Lock();

		bool enabled = m_enabled;

		// retry loop. enabled will be set to false in the end of this loop, if too many failures (too many tries)
		while (doRun && enabled) {
			bool foundLastCommonHost = false;
			uint32 lastCommonHost = 0;
			uint32 lastCommonTTL = 0;
			uint32 hostToPing = 0;
			bool useUdp = false;

			hostsToTraceRoute.RemoveAll();

			pingDelays.RemoveAll();
			pingDelaysTotal = 0;

			pingLocker.Lock();
			m_pingAverage = 0;
			m_lowestPing = 0;
			m_uState = IDS_USS_STATE_PREPARING;
			pingLocker.Unlock();

			// Calculate a good starting value for the upload control. If the user has entered a max upload value, we use that. Otherwise 10 kByte/s
			int startUpload = (maxUpload != _UI32_MAX) ? maxUpload : 10 * 1024;

			while (doRun && enabled && !foundLastCommonHost) {
				uint32 traceRouteTries = 0;
				while (doRun && enabled && !foundLastCommonHost && (traceRouteTries < 5 || (hasSucceededAtLeastOnce && traceRouteTries < _UI32_MAX)) && (hostsToTraceRoute.GetCount() < 10 || hasSucceededAtLeastOnce)) {
					++traceRouteTries;

					lastCommonHost = 0;

					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Try #%i. Collecting hosts..."), traceRouteTries);

					addHostLocker.Lock();
					needMoreHosts = doRun;
					addHostLocker.Unlock();

					// wait for hosts to traceroute
					newTraceRouteHostEvent->Lock();

					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Got enough hosts. Listing the hosts that will be tracerouted:"));

					int counter = 0;
					for (POSITION pos = hostsToTraceRoute.GetStartPosition(); pos != NULL;) {
						uint32 hostToTraceRoute, dummy;
						hostsToTraceRoute.GetNextAssoc(pos, hostToTraceRoute, dummy);
						IN_ADDR stDestAddr;
						stDestAddr.s_addr = hostToTraceRoute;

						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host #%i: %s"), ++counter, (LPCTSTR)ipstr(stDestAddr));
					}

					// find the last common host, using traceroute
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Starting traceroutes to find last common host."));

					// for the tracerouting phase (preparing...) we need to disable uploads so we get a faster traceroute and better ping values.
					SetUpload(2 * 1024);
					::Sleep(SEC2MS(1));

					if (!m_enabled)
						enabled = false;

					bool failed = false;

					uint32 curHost = 0;
					for (uint32 ttl = 1; doRun && enabled && (curHost != 0 && ttl <= 64 || curHost == 0 && ttl < 5) && !foundLastCommonHost && !failed; ++ttl) {
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Pinging for TTL %i..."), ttl);

						useUdp = false; // PENDING: Get default value from prefs?

						curHost = 0;
						if (!m_enabled)
							enabled = false;

						uint32 lastSuccedingPingAddress = 0;
						uint32 lastDestinationAddress = 0;
						uint32 hostsToTraceRouteCounter = 0;
						bool failedThisTtl = false;
						for (POSITION pos = hostsToTraceRoute.GetStartPosition();
							doRun && enabled && !failed && !failedThisTtl && pos != NULL && (lastDestinationAddress == 0 || lastDestinationAddress == curHost);) // || !pingStatus.success && pingStatus.error == IP_REQ_TIMED_OUT ))
						{
							PingStatus pingStatus = {};

							++hostsToTraceRouteCounter;

							// this is the current address we send ping to, in loop below.
							// PENDING: Don't confuse this with curHost, which is unfortunately almost
							// the same name. Will rename one of these variables as soon as possible, to
							// get more different names.
							uint32 curAddress, dummy;
							hostsToTraceRoute.GetNextAssoc(pos, curAddress, dummy);

							pingStatus.success = false;
							for (int cnt = 0; doRun && enabled && cnt < 2 && (!pingStatus.success || (pingStatus.status != IP_SUCCESS && pingStatus.status != IP_TTL_EXPIRED_TRANSIT)); ++cnt) {
								pingStatus = pinger.Ping(curAddress, ttl, true, useUdp);
								if (doRun && enabled
									&& (!pingStatus.success || (pingStatus.status != IP_SUCCESS && pingStatus.status != IP_TTL_EXPIRED_TRANSIT))
									&& cnt < 3 - 1)
								{
									IN_ADDR stDestAddr;
									stDestAddr.s_addr = curAddress;
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Failure #%i to ping host! (TTL: %i IP: %s error: %i). Sleeping 1 sec before retry. Error info follows."), cnt + 1, ttl, (LPCTSTR)ipstr(stDestAddr), (pingStatus.success) ? pingStatus.status : pingStatus.error);
									pinger.PIcmpErr(pingStatus.success ? pingStatus.status : pingStatus.error);

									::Sleep(SEC2MS(1));

									if (!m_enabled)
										enabled = false;

									// trying other ping method
									useUdp = !useUdp;
								}
							}

							if (pingStatus.success && pingStatus.status == IP_TTL_EXPIRED_TRANSIT) {
								if (curHost == 0)
									curHost = pingStatus.destinationAddress;
								lastSuccedingPingAddress = curAddress;
								lastDestinationAddress = pingStatus.destinationAddress;
							} else {
								// failed to ping this host for some reason.
								// Or we reached the actual host we are pinging. We don't want that, since it is too close.
								// Remove it.
								IN_ADDR stDestAddr;
								stDestAddr.s_addr = curAddress;
								if (pingStatus.success && pingStatus.status == IP_SUCCESS) {
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host was too close! Removing this host. (TTL: %i IP: %s status: %i). Removing this host and restarting host collection."), ttl, (LPCTSTR)ipstr(stDestAddr), pingStatus.status);

									hostsToTraceRoute.RemoveKey(curAddress);
								} else if (pingStatus.success && pingStatus.status == IP_DEST_HOST_UNREACHABLE) {
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host unreachable! (TTL: %i IP: %s status: %i). Removing this host. Status info follows."), ttl, (LPCTSTR)ipstr(stDestAddr), pingStatus.status);
									pinger.PIcmpErr(pingStatus.status);

									hostsToTraceRoute.RemoveKey(curAddress);
								} else if (pingStatus.success) {
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Unknown ping status! (TTL: %i IP: %s status: %i). Reason follows. Changing ping method to see if it helps."), ttl, (LPCTSTR)ipstr(stDestAddr), pingStatus.status);
									pinger.PIcmpErr(pingStatus.status);
									useUdp = !useUdp;
								} else {
									if (pingStatus.error == IP_REQ_TIMED_OUT) {
										theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Timeout when pinging a host! (TTL: %i IP: %s Error: %i). Keeping host. Error info follows."), ttl, (LPCTSTR)ipstr(stDestAddr), pingStatus.error);
										pinger.PIcmpErr(pingStatus.error);

										if (hostsToTraceRouteCounter > 2 && lastSuccedingPingAddress == 0) {
											// several pings have timed out on this ttl. Probably we can't ping on this ttl at all
											failedThisTtl = true;
										}
									} else {
										theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Unknown pinging error! (TTL: %i IP: %s status: %i). Reason follows. Changing ping method to see if it helps."), ttl, (LPCTSTR)ipstr(stDestAddr), pingStatus.error);
										pinger.PIcmpErr(pingStatus.error);
										useUdp = !useUdp;
									}
								}

								if (hostsToTraceRoute.GetCount() <= 8) {
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: To few hosts to traceroute left. Restarting host collection."));
									failed = true;
								}
							}
						}

						if (!failed) {
							if (curHost != 0 && lastDestinationAddress != 0) {
								if (lastDestinationAddress == curHost) {
									IN_ADDR stDestAddr;
									stDestAddr.s_addr = curHost;
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host at TTL %i: %s"), ttl, (LPCTSTR)ipstr(stDestAddr));

									lastCommonHost = curHost;
									lastCommonTTL = ttl;
								} else /*if(lastSuccedingPingAddress != 0)*/ {
									foundLastCommonHost = true;
									hostToPing = lastSuccedingPingAddress;

									const CString &hostToPingString = ipstr(hostToPing);

									if (lastCommonHost != 0)
										theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found differing host at TTL %i: %s. This will be the host to ping."), ttl, (LPCTSTR)hostToPingString);
									else {
										const CString &lastCommonHostString = ipstr(lastDestinationAddress);

										lastCommonHost = lastDestinationAddress;
										lastCommonTTL = ttl;
										theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found differing host at TTL %i, but last ttl couldn't be pinged so we don't know last common host. Taking a chance and using first differing ip as last commonhost. Host to ping: %s. Faked LastCommonHost: %s"), ttl, (LPCTSTR)hostToPingString, (LPCTSTR)lastCommonHostString);
									}
								}
							} else {
								if (ttl < 4)
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Could perform no ping at all at TTL %i. Trying next ttl."), ttl);
								else
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Could perform no ping at all at TTL %i. Giving up."), ttl);

								lastCommonHost = 0;
							}
						}
					}

					if (!foundLastCommonHost && traceRouteTries >= 3) {
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Tracerouting failed several times. Waiting a few minutes before trying again."));

						SetUpload(maxUpload);

						pingLocker.Lock();
						m_uState = IDS_USS_STATE_WAITING;
						pingLocker.Unlock();

						prefsEvent->Lock(MIN2MS(3));

						pingLocker.Lock();
						m_uState = IDS_USS_STATE_PREPARING;
						pingLocker.Unlock();
					}

					if (!m_enabled)
						enabled = false;
				}

				if (enabled) {
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Done tracerouting. Evaluating results."));

					if (foundLastCommonHost) {
						IN_ADDR stLastCommonHostAddr;
						stLastCommonHostAddr.s_addr = lastCommonHost;

						// log result
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found last common host. LastCommonHost: %s @ TTL: %i"), (LPCTSTR)ipstr(stLastCommonHostAddr), lastCommonTTL);

						IN_ADDR stHostToPingAddr;
						stHostToPingAddr.s_addr = hostToPing;
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found last common host. HostToPing: %s"), (LPCTSTR)ipstr(stHostToPingAddr));
					} else {
						theApp.QueueDebugLogLine(false, GetResString(IDS_USS_TRACEROUTEOFTENFAILED));
						theApp.QueueLogLine(true, GetResString(IDS_USS_TRACEROUTEOFTENFAILED));
						enabled = false;

						pingLocker.Lock();
						m_uState = IDS_USS_STATE_ERROR;
						pingLocker.Unlock();

						// PENDING: this may not be thread safe
						thePrefs.SetDynUpEnabled(false);
					}
				}
			}

			if (!m_enabled)
				enabled = false;

			if (doRun && enabled)
				theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Finding a start value for lowest ping..."));

			// PENDING:
			prefsLocker.Lock();
			uint64 lowestInitialPingAllowed = m_LowestInitialPingAllowed;
			prefsLocker.Unlock();

			uint32 initial_ping = _I32_MAX;

			bool foundWorkingPingMethod = false;
			// finding lowest ping
			for (int initialPingCounter = 10; doRun && enabled && --initialPingCounter >= 0;) {
				::Sleep(200);

				PingStatus pingStatus = pinger.Ping(hostToPing, lastCommonTTL, true, useUdp);

				if (pingStatus.success && pingStatus.status == IP_TTL_EXPIRED_TRANSIT) {
					foundWorkingPingMethod = true;

					if (pingStatus.delay > 0 && pingStatus.delay < initial_ping)
						initial_ping = (uint32)max(pingStatus.delay, lowestInitialPingAllowed);
				} else {
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: %s-Ping #%i failed. Reason follows"), useUdp ? _T("UDP") : _T("ICMP"), initialPingCounter);
					pinger.PIcmpErr(pingStatus.error);

					// trying other ping method
					if (!pingStatus.success && !foundWorkingPingMethod)
						useUdp = !useUdp;
				}

				if (!m_enabled)
					enabled = false;
			}

			// Set the upload to a good starting point
			SetUpload(startUpload);
			::Sleep(SEC2MS(1));
			DWORD initTime = ::GetTickCount();

			// if all pings returned 0, initial_ping will not have been changed from default value.
			// then set initial_ping to lowestInitialPingAllowed
			if (initial_ping == _I32_MAX)
				initial_ping = (uint32)lowestInitialPingAllowed;

			uint32 upload = 0;

			hasSucceededAtLeastOnce = true;

			if (doRun && enabled) {
				if (initial_ping > lowestInitialPingAllowed)
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Lowest ping: %i ms"), initial_ping);
				else
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Lowest ping: %i ms. (Filtered lower values. Lowest ping is never allowed to go under %i ms)"), initial_ping, lowestInitialPingAllowed);

				prefsLocker.Lock();

				upload = m_CurUpload;
				if (upload < minUpload)
					upload = minUpload;
				if (upload > maxUpload)
					upload = maxUpload;

				prefsLocker.Unlock();
			}

			if (!m_enabled)
				enabled = false;

			if (doRun && enabled) {
				theApp.QueueDebugLogLine(false, GetResString(IDS_USS_STARTING));
				theApp.QueueLogLine(true, GetResString(IDS_USS_STARTING));
			}

			pingLocker.Lock();
			m_uState = 0;
			pingLocker.Unlock();

			// There may be several reasons to start over with tracerouting again.
			// Currently we only restart if we get an unexpected ip back from the
			// ping at the set TTL.
			bool restart = false;

			DWORD lastLoopTick = ::GetTickCount();

			while (doRun && enabled && !restart) {
				DWORD ticksBetweenPings;
				if (upload > 0) {
					// ping packages being 64 bytes, this should use 1% of bandwidth (one hundredth of bw).
					ticksBetweenPings = SEC2MS(64 * 100) / upload;

					if (ticksBetweenPings < 125)
						ticksBetweenPings = 125; // never ping more than 8 packages a second
					else if (ticksBetweenPings > SEC2MS(1))
						ticksBetweenPings = SEC2MS(1);
				} else
					ticksBetweenPings = SEC2MS(1);

				DWORD curTick = ::GetTickCount();

				DWORD timeSinceLastLoop = curTick - lastLoopTick;
				if (timeSinceLastLoop < ticksBetweenPings) {
					//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: Sleeping %i ms, timeSinceLastLoop %i ms ticksBetweenPings %i ms"), ticksBetweenPings-timeSinceLastLoop, timeSinceLastLoop, ticksBetweenPings);
					::Sleep(ticksBetweenPings - timeSinceLastLoop);
				}

				lastLoopTick = curTick;

				prefsLocker.Lock();
				double pingTolerance = m_pingTolerance;
				uint32 pingToleranceMilliseconds = m_iPingToleranceMilliseconds;
				bool useMillisecondPingTolerance = m_bUseMillisecondPingTolerance;
				uint32 goingUpDivider = m_goingUpDivider;
				uint32 goingDownDivider = m_goingDownDivider;
				uint32 numberOfPingsForAverage = m_iNumberOfPingsForAverage;
				lowestInitialPingAllowed = m_LowestInitialPingAllowed; // PENDING
				uint32 curUpload = m_CurUpload;

				bool initiateFastReactionPeriod = m_initiateFastReactionPeriod;
				m_initiateFastReactionPeriod = false;
				prefsLocker.Unlock();

				if (initiateFastReactionPeriod) {
					theApp.QueueDebugLogLine(false, GetResString(IDS_USS_MANUALUPLOADLIMITDETECTED));
					theApp.QueueLogLine(true, GetResString(IDS_USS_MANUALUPLOADLIMITDETECTED));

					// the first 60 seconds will use hardcoded up/down slowness that is faster
					initTime = ::GetTickCount();
				}

				DWORD diffTick = ::GetTickCount() - initTime;

				if (diffTick < SEC2MS(20)) {
					goingUpDivider = 1;
					goingDownDivider = 1;
				} else if (diffTick < SEC2MS(30)) {
					goingUpDivider = (uint32)(goingUpDivider * 0.25);
					goingDownDivider = (uint32)(goingDownDivider * 0.25);
				} else if (diffTick < SEC2MS(40)) {
					goingUpDivider = (uint32)(goingUpDivider * 0.5);
					goingDownDivider = (uint32)(goingDownDivider * 0.5);
				} else if (diffTick < SEC2MS(60)) {
					goingUpDivider = (uint32)(goingUpDivider * 0.75);
					goingDownDivider = (uint32)(goingDownDivider * 0.75);
				} else if (diffTick < SEC2MS(61)) {
					prefsLocker.Lock();
					upload = m_CurUpload;
					prefsLocker.Unlock();
				}

				goingDownDivider = max(goingDownDivider, 1);
				goingUpDivider = max(goingUpDivider, 1);

				uint32 soll_ping;
				if (useMillisecondPingTolerance)
					soll_ping = pingToleranceMilliseconds;
				else
					soll_ping = (uint32)(initial_ping * pingTolerance);

				uint32 raw_ping = soll_ping; // this value will cause the upload speed not to change at all.

				bool pingFailure = false;
				for (uint64 pingTries = 0; doRun && enabled && (pingTries == 0 || pingFailure) && pingTries < 60; ++pingTries) {
					if (!m_enabled)
						enabled = false;

					// ping the hostToPing
					PingStatus pingStatus = pinger.Ping(hostToPing, lastCommonTTL, false, useUdp);

					if (pingStatus.success && pingStatus.status == IP_TTL_EXPIRED_TRANSIT) {
						if (pingStatus.destinationAddress != lastCommonHost) {
							// something has changed about the topology! We got another ip back from this ttl than expected.
							// Do the tracerouting again to figure out new topology
							const CString &lastCommonHostAddressString = ipstr(lastCommonHost);
							const CString &destinationAddressString = ipstr(pingStatus.destinationAddress);

							theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Network topology has changed. TTL: %i Expected ip: %s Got ip: %s Will do a new traceroute."), lastCommonTTL, (LPCTSTR)lastCommonHostAddressString, (LPCTSTR)destinationAddressString);
							restart = true;
						}

						raw_ping = (uint32)pingStatus.delay;

						if (pingFailure) {
							// only several pings in row should fails, the total doesn't count, so reset for each successful ping
							pingFailure = false;

							//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: Ping #%i successful. Continuing."), pingTries);
						}
					} else {
						raw_ping = (soll_ping + initial_ping) * 3; // this value will cause the upload speed to be lowered

						pingFailure = true;

						if (!m_enabled)
							enabled = false;
						else if (pingTries > 3)
							prefsEvent->Lock(SEC2MS(1));

						//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: %s-Ping #%i failed. Reason follows"), useUdp?_T("UDP"):_T("ICMP"), pingTries);
						//pinger.PIcmpErr(pingStatus.error);
					}

					if (!m_enabled)
						enabled = false;
				}

				if (pingFailure) {
					if (enabled)
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: No response to pings for a long time. Restarting..."));
					restart = true;
				}

				if (!restart) {
					if (raw_ping > 0 && raw_ping < initial_ping && initial_ping > lowestInitialPingAllowed) {
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: New lowest ping: %i ms. Old: %i ms"), max(raw_ping, lowestInitialPingAllowed), initial_ping);
						initial_ping = (uint32)max(raw_ping, lowestInitialPingAllowed);
					}

					pingDelaysTotal += raw_ping;
					pingDelays.AddTail(raw_ping);
					while (!pingDelays.IsEmpty() && (uint32)pingDelays.GetCount() > numberOfPingsForAverage)
						pingDelaysTotal -= pingDelays.RemoveHead();

					uint32 pingAverage = Median(pingDelays); //(pingDelaysTotal/pingDelays.GetCount());
					int normalized_ping = pingAverage - initial_ping;

					//{
					//    prefsLocker.Lock();
					//    uint32 tempCurUpload = m_CurUpload;
					//    prefsLocker.Unlock();
					//    theApp.QueueDebugLogLine(false, _T("USS-Debug: %i %i %i"), raw_ping, upload, tempCurUpload);
					//}

					pingLocker.Lock();
					m_pingAverage = (uint32)pingAverage;
					m_lowestPing = initial_ping;
					pingLocker.Unlock();

					// Calculate Waiting Time
					sint64 hping = (sint64)soll_ping - normalized_ping;

					// Calculate change of upload speed
					if (hping < 0) {
						//Ping too high
						acceptNewClient = false;

						// lower the speed
						sint64 ulDiff = hping * 1024 * 10 / (goingDownDivider * (sint64)initial_ping);

						//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: Down! Ping cur %i ms. Ave %I64i ms %i values. New Upload %i + %I64i = %I64i"), raw_ping, pingDelaysTotal/pingDelays.GetCount(), pingDelays.GetCount(), upload, ulDiff, upload+ulDiff);
						// prevent underflow
						upload = (upload > -ulDiff) ? (uint32)(upload + ulDiff) : 0;

					} else if (hping > 0) {
						//Ping lower than max allowed
						acceptNewClient = true;

						if (curUpload + 30 * 1024 > upload) {
							// raise the speed
							sint64 ulDiff = hping * 1024 * 10 / (goingUpDivider * (sint64)initial_ping);

							//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: Up! Ping cur %i ms. Ave %I64i ms %i values. New Upload %i + %I64i = %I64i"), raw_ping, pingDelaysTotal/pingDelays.GetCount(), pingDelays.GetCount(), upload, ulDiff, upload+ulDiff);
							// prevent overflow
							upload = (_I32_MAX - upload > ulDiff) ? (uint32)(upload + ulDiff) : _I32_MAX;
						}
					}
					prefsLocker.Lock();
					if (upload < minUpload) {
						upload = minUpload;
						acceptNewClient = true;
					}
					if (upload > maxUpload)
						upload = maxUpload;

					prefsLocker.Unlock();
					SetUpload(upload);
					if (!m_enabled)
						enabled = false;
				}
			}
		}
	}

	// Signal that we have ended.
	threadEndedEvent->SetEvent();

	return 0;
}

uint32 LastCommonRouteFinder::Median(const UInt32Clist &list)
{
	INT_PTR size = list.GetCount();
	switch (size) {
	case 1:
		return list.GetHead();
	case 2:
		return (list.GetHead() + list.GetTail()) / 2;
	default:
		if (size <= 0)
			return 0;	// Undefined! Shouldn't be called with an empty list.
	}
	// if more than 2 elements, we need to sort them to find the middle.
	uint32 *arr = new uint32[size];

	uint32 index = 0;
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;)
		arr[index++] = list.GetNext(pos);

	std::sort(arr, arr + size);

	double returnVal = arr[size / 2 - (size & 1)];
	returnVal = (returnVal + arr[size / 2]) / 2;

	delete[] arr;

	return (uint32)returnVal;
}