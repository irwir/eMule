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
#include <algorithm>
#include "emule.h"
#include "Opcodes.h"
#include "LastCommonRouteFinder.h"
#include "Server.h"
#include "UpDownClient.h"
#include "Preferences.h"
#include "Pinger.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


LastCommonRouteFinder::LastCommonRouteFinder()
	: m_eventThreadEnded(FALSE, TRUE)
	, m_eventNewTraceRouteHost(FALSE)
	, m_eventPrefs(FALSE)
	, m_pingDelaysTotal()
	, m_LowestInitialPingAllowed(20)
	, m_minUpload(1)
	, m_maxUpload(UNLIMITED)
	, m_CurUpload(1)
	, m_upload(UNLIMITED)
	, m_iPingToleranceMilliseconds(200)
	, m_iNumberOfPingsForAverage()
	, m_pingAverage()
	, m_lowestPing()
	, m_uState()
	, m_initiateFastReactionPeriod()
	, m_bRun(true)
	, m_enabled()
	, m_bUseMillisecondPingTolerance()
	, needMoreHosts()
{
	if (AfxBeginThread(RunProc, (LPVOID)this) == NULL) {
		m_bRun = false;
		m_eventThreadEnded.SetEvent();
		DebugLogError(_T("Failed to create LastCommonRouteFinder thread (Win32 error %lu)"), ::GetLastError());
	}
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
		hostsToTraceRoute[ip] = 0;

		if (hostsToTraceRoute.GetCount() >= 10) {
			needMoreHosts = false;

			// Signal that there are hosts to fetch.
			m_eventNewTraceRouteHost.SetEvent();
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
				POSITION pos = list.FindIndex(rand() % min(cnt, 100));
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
				POSITION pos = list.FindIndex(rand() % min(cnt, 100));
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

bool LastCommonRouteFinder::SetPrefs(const CurrentParamStruct &cur)
{
	prefsLocker.Lock();

	m_minUpload = cur.uMinUpload ? cur.uMinUpload * 1024 : 1024;

	if (cur.uMaxUpload) {
		m_maxUpload = cur.uMaxUpload;
		if (m_maxUpload != UNLIMITED) {
			m_maxUpload *= 1024;
			if (m_maxUpload < m_minUpload)
				m_minUpload = m_maxUpload;
		}
	} else
		m_maxUpload = m_upload + 10 * 1024; //_UI32_MAX;

	m_pingTolerance = cur.dPingTolerance;
	m_CurUpload = cur.uCurUpload;
	m_iPingToleranceMilliseconds = cur.uPingToleranceMilliseconds;
	m_goingUpDivider = cur.uGoingUpDivider;
	m_goingDownDivider = cur.uGoingDownDivider;
	m_iNumberOfPingsForAverage = cur.uNumberOfPingsForAverage;
	m_LowestInitialPingAllowed = cur.uLowestInitialPingAllowed;

	// this would show, resize or hide the ping info area in status bar
	bool bSetStatusBar = m_enabled || m_bUseMillisecondPingTolerance != cur.bUseMillisecondPingTolerance;

	bool bSetEvent = !cur.bEnabled || !m_enabled;

	m_bUseMillisecondPingTolerance = cur.bUseMillisecondPingTolerance;
	m_enabled = cur.bEnabled;

	prefsLocker.Unlock();

	if (m_upload > m_maxUpload || !m_enabled)
		m_upload = m_maxUpload;

	if (bSetEvent)
		m_eventPrefs.SetEvent();
	return bSetStatusBar;
}

/**
 * Make the thread exit. This method will not return until the thread has stopped
 * looping.
 */
void LastCommonRouteFinder::EndThread()
{
	// signal the thread to stop looping and exit.
	m_bRun = false;
	needMoreHosts = false;

	m_eventPrefs.SetEvent();
	m_eventNewTraceRouteHost.SetEvent();

	// wait for the thread to signal that looping has stopped
	m_eventThreadEnded.Lock();
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
	srand((unsigned)time(NULL));
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
	CString sErr;

	while (m_bRun) {
		// wait for updated prefs
		m_eventPrefs.Lock();

		bool enabled = m_enabled;

		// retry loop. enabled will be set to false in the end of this loop, if too many failures (too many tries)
		while (m_bRun && enabled) {
			uint32 lastCommonHost = 0;
			uint32 lastCommonTTL = 0;
			uint32 hostToPing = 0;
			bool foundLastCommonHost = false;
			bool useUdp = false;

			hostsToTraceRoute.RemoveAll();

			pingDelays.RemoveAll();
			m_pingDelaysTotal = 0;

			pingLocker.Lock();
			m_pingAverage = 0;
			m_lowestPing = 0;
			m_uState = IDS_USS_STATE_PREPARING;
			pingLocker.Unlock();

			// Calculate a good starting value for the upload control.
			// If the user has entered a max upload value, we use that.
			// Otherwise choose the highest of 10 KB/s and the current upload
			uint32 startUpload = (m_maxUpload != UNLIMITED) ? m_maxUpload : max(m_CurUpload, 10 * 1024);

			while (m_bRun && enabled && !foundLastCommonHost) {
				uint32 traceRouteTries = 0;
				while (m_bRun && enabled && !foundLastCommonHost && (traceRouteTries < 5 || (hasSucceededAtLeastOnce && traceRouteTries < _UI32_MAX)) && (hostsToTraceRoute.GetCount() < 10 || hasSucceededAtLeastOnce)) {
					++traceRouteTries;

					lastCommonHost = 0;

					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Try #%i. Collecting hosts..."), traceRouteTries);

					addHostLocker.Lock();
					needMoreHosts = m_bRun;
					addHostLocker.Unlock();

					// wait for hosts to traceroute
					m_eventNewTraceRouteHost.Lock();

					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Got enough hosts. Listing the hosts that will be tracerouted:"));

					int counter = 0;
					for (const CHostsToTraceRouteMap::CPair *pair = hostsToTraceRoute.PGetFirstAssoc(); pair != NULL; pair = hostsToTraceRoute.PGetNextAssoc(pair))
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host #%i: %s"), ++counter, (LPCTSTR)ipstr(pair->key));

					// find the last common host, using traceroute
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Starting traceroutes to find last common host."));

					// for the tracerouting phase (preparing...) we need to disable uploads so we get a faster traceroute and better ping values.
					m_upload = 2 * 1024; //expecting this to be always below m_maxUpload
					::Sleep(SEC2MS(1));
					//Disabled upload might be fine immediately after start.
					//When eMule was running and uploading, this severely disrupts upload

					if (!m_enabled)
						enabled = false;

					bool failed = false;

					uint32 curHost = 0;
					for (uint32 ttl = 1; m_bRun && enabled && (curHost != 0 && ttl <= 64 || curHost == 0 && ttl < 5) && !foundLastCommonHost && !failed; ++ttl) {
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Pinging for TTL %i..."), ttl);

						useUdp = false; // PENDING: Get default value from prefs?

						curHost = 0;
						if (!m_enabled)
							enabled = false;

						uint32 lastSuccessfulPingAddress = 0;
						uint32 lastDestinationAddress = 0;
						uint32 hostsToTraceRouteCounter = 0;
						bool failedThisTtl = false;
						for (const CHostsToTraceRouteMap::CPair *pair = hostsToTraceRoute.PGetFirstAssoc();
							m_bRun && enabled && !failed && !failedThisTtl && pair != NULL
								&& (lastDestinationAddress == 0 || lastDestinationAddress == curHost);) // || !pingStatus.success && pingStatus.error == IP_REQ_TIMED_OUT ))
						{
							PingStatus pingStatus{};
							++hostsToTraceRouteCounter;

							// this is the current address to be pinged in the loop below.
							uint32 uAddressToPing = pair->key;
							const CHostsToTraceRouteMap::CPair *pair2 = hostsToTraceRoute.PGetNextAssoc(pair);

							for (int cnt = 0; m_bRun && enabled && cnt < 2; ++cnt) {
								pingStatus = pinger.Ping(uAddressToPing, ttl, true, useUdp);
								if (!m_bRun || !enabled
									|| (pingStatus.bSuccess && (pingStatus.status == IP_SUCCESS || pingStatus.status == IP_TTL_EXPIRED_TRANSIT)))
								{
									break;
								}
								sErr.Format(_T("UploadSpeedSense: Failure #%i to ping host! (TTL: %u IP: %s).%s Error: %lu ")
									, cnt + 1
									, ttl
									, cnt ? _T("") : _T(" Sleeping 1 s before retry.")
									, (LPCTSTR)ipstr(uAddressToPing)
									, (pingStatus.bSuccess ? pingStatus.status : pingStatus.error));
								pinger.PIcmpErr(sErr, pingStatus.bSuccess ? pingStatus.status : pingStatus.error);
								if (!cnt)
									::Sleep(SEC2MS(1));

								if (!m_enabled)
									enabled = false;

								// trying other ping method
								useUdp = !useUdp;
							}

							if (pingStatus.bSuccess) {
								switch (pingStatus.status) {
								case IP_TTL_EXPIRED_TRANSIT:
									if (curHost == 0)
										curHost = pingStatus.destinationAddress;
									lastSuccessfulPingAddress = uAddressToPing;
									lastDestinationAddress = pingStatus.destinationAddress;
									break;
								case IP_SUCCESS:
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host was too close! Removing this host. (TTL: %u IP: %s status: %lu). Removing this host and restarting host collection."), ttl, (LPCTSTR)ipstr(uAddressToPing), pingStatus.status);
									hostsToTraceRoute.RemoveKey(uAddressToPing);
									break;
								case IP_DEST_HOST_UNREACHABLE:
									sErr.Format(_T("UploadSpeedSense: Host unreachable! (TTL: %u IP: %s status: %lu). Removing this host. "), ttl, (LPCTSTR)ipstr(uAddressToPing), pingStatus.status);
									pinger.PIcmpErr(sErr, pingStatus.status);
									hostsToTraceRoute.RemoveKey(uAddressToPing);
									break;
								default:
									sErr.Format(_T("UploadSpeedSense: Unknown ping status! (TTL: %u IP: %s status: %lu). Changing ping method. "), ttl, (LPCTSTR)ipstr(uAddressToPing), pingStatus.status);
									pinger.PIcmpErr(sErr, pingStatus.status);
									useUdp = !useUdp;
								}
							} else {
								if (pingStatus.error == IP_REQ_TIMED_OUT) {
									sErr.Format(_T("UploadSpeedSense: Timeout when pinging a host! (TTL: %u IP: %s Error: %lu). Keeping host. "), ttl, (LPCTSTR)ipstr(uAddressToPing), pingStatus.error);
									pinger.PIcmpErr(sErr, pingStatus.status);

									// several pings have timed out on this ttl. Probably we can't ping on this ttl at all
									if (hostsToTraceRouteCounter > 2 && lastSuccessfulPingAddress == 0)
										failedThisTtl = true;
								} else {
									sErr.Format(_T("UploadSpeedSense: Unknown pinging error! (TTL: %u IP: %s status: %lu). Changing ping method."), ttl, (LPCTSTR)ipstr(uAddressToPing), pingStatus.error);
									pinger.PIcmpErr(sErr, pingStatus.status);
									useUdp = !useUdp;
								}

								if (hostsToTraceRoute.GetCount() <= 8) {
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: To few hosts to traceroute left. Restarting host collection."));
									failed = true;
								}
							}
							pair = pair2;
						}

						if (!failed) {
							if (curHost != 0 && lastDestinationAddress != 0) {
								if (lastDestinationAddress == curHost) {
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Host at TTL %i: %s"), ttl, (LPCTSTR)ipstr(curHost));

									lastCommonHost = curHost;
									lastCommonTTL = ttl;
								} else { //lastSuccessfulPingAddress != 0
									foundLastCommonHost = true;
									hostToPing = lastSuccessfulPingAddress;

									const CString &hostToPingString(ipstr(hostToPing));

									if (lastCommonHost != 0)
										theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found differing host at TTL %i: %s. This will be the host to ping."), ttl, (LPCTSTR)hostToPingString);
									else {
										const CString &lastCommonHostString(ipstr(lastDestinationAddress));

										lastCommonHost = lastDestinationAddress;
										lastCommonTTL = ttl;
										theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found differing host at TTL %i, but last ttl couldn't be pinged so we don't know last common host. Taking a chance and using first differing ip as last common host. Host to ping: %s. Faked LastCommonHost: %s"), ttl, (LPCTSTR)hostToPingString, (LPCTSTR)lastCommonHostString);
									}
								}
							} else {
								if (ttl < 4)
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Could not ping at TTL %i. Trying next ttl."), ttl);
								else
									theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Could not ping at TTL %i. Giving up."), ttl);

								lastCommonHost = 0;
							}
						}
					}

					if (!foundLastCommonHost && traceRouteTries >= 3) {
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Tracerouting failed several times. Waiting a few minutes before trying again."));

						m_upload = m_maxUpload;

						pingLocker.Lock();
						m_uState = IDS_USS_STATE_WAITING;
						pingLocker.Unlock();

						m_eventPrefs.Lock(MIN2MS(3));

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
						// log result
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found last common host. LastCommonHost: %s @ TTL: %i"), (LPCTSTR)ipstr(lastCommonHost), lastCommonTTL);
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Found last common host. HostToPing: %s"), (LPCTSTR)ipstr(hostToPing));
					} else {
						theApp.QueueDebugLogLine(false, GetResString(IDS_USS_TRACEROUTEOFTENFAILED));
						theApp.QueueLogLine(true, GetResString(IDS_USS_TRACEROUTEOFTENFAILED));
						enabled = false;

						pingLocker.Lock();
						m_uState = IDS_USS_STATE_ERROR;
						pingLocker.Unlock();

						// PENDING: this may be not thread safe
						thePrefs.SetDynUpEnabled(false);
					}
				}
			}

			if (!m_enabled)
				enabled = false;
			else if (m_bRun && enabled)
				theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Finding a start value for lowest ping..."));

			// PENDING:
			prefsLocker.Lock();
			uint32 lowestInitialPingAllowed = m_LowestInitialPingAllowed;
			prefsLocker.Unlock();

			uint32 initial_ping = _I32_MAX;

			bool foundWorkingPingMethod = false;
			// finding lowest ping
			for (int initialPingCounter = 10; m_bRun && enabled && --initialPingCounter >= 0;) {
				::Sleep(200);

				PingStatus pingStatus = pinger.Ping(hostToPing, lastCommonTTL, true, useUdp);

				if (pingStatus.bSuccess && pingStatus.status == IP_TTL_EXPIRED_TRANSIT) {
					foundWorkingPingMethod = true;

					if (pingStatus.fDelay > 0 && pingStatus.fDelay < initial_ping)
						initial_ping = max((uint32)pingStatus.fDelay, lowestInitialPingAllowed);
				} else {
					sErr.Format(_T("UploadSpeedSense: %s-Ping #%i failed. "), useUdp ? _T("UDP") : _T("ICMP"), initialPingCounter);
					pinger.PIcmpErr(sErr, pingStatus.status);

					// trying other ping method
					if (!pingStatus.bSuccess && !foundWorkingPingMethod)
						useUdp = !useUdp;
				}

				if (!m_enabled)
					enabled = false;
			}

			// Set the upload to the starting point
			m_upload = startUpload;
			::Sleep(SEC2MS(1));
			DWORD initTime = ::GetTickCount();

			// if all pings returned 0, initial_ping will not be changed from default value.
			// then set initial_ping to lowestInitialPingAllowed
			if (initial_ping == _I32_MAX)
				initial_ping = lowestInitialPingAllowed;

			hasSucceededAtLeastOnce = true;

			uint32 upload;
			if (m_bRun && enabled) {
				if (initial_ping > lowestInitialPingAllowed)
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Lowest ping: %i ms"), initial_ping);
				else
					theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Lowest ping: %i ms. (Filtered lower values. Lowest ping is never allowed to go under %i ms)"), initial_ping, lowestInitialPingAllowed);

				prefsLocker.Lock();
				upload = max(m_CurUpload, m_minUpload);
				upload = min(upload, m_maxUpload);
				prefsLocker.Unlock();
			} else
				upload = 0;

			if (!m_enabled)
				enabled = false;
			else if (m_bRun && enabled) {
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

			while (m_bRun && enabled && !restart) {
				DWORD ticksBetweenPings;
				if (upload > 0) {
					// ping packages being 64 bytes, should use 1% of bandwidth or less (one hundredth of bw).
					ticksBetweenPings = SEC2MS(64 * 100) / upload;

					if (ticksBetweenPings < 125)
						ticksBetweenPings = 125; // never ping more than 8 times a second
					else if (ticksBetweenPings > SEC2MS(1))
						ticksBetweenPings = SEC2MS(1);
				} else
					ticksBetweenPings = SEC2MS(1);

				const DWORD curTick = ::GetTickCount();

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
				uint32 curUpload = m_CurUpload;
				lowestInitialPingAllowed = m_LowestInitialPingAllowed; // PENDING
				prefsLocker.Unlock();

				DWORD diffTick = ::GetTickCount();
				if (::InterlockedExchange8(&m_initiateFastReactionPeriod, 0)) {
					const CString &s(GetResString(IDS_USS_MANUALUPLOADLIMITDETECTED));
					theApp.QueueDebugLogLine(false, s);
					theApp.QueueLogLine(true, s);

					// the first 60 seconds will use hardcoded up/down slowness that is faster
					initTime = diffTick;
					diffTick = 0;
				} else
					diffTick -= initTime;

				if (diffTick < SEC2MS(20))
					goingUpDivider = goingDownDivider = 1;
				else {
					uint32 mul = 0;
					if (diffTick < SEC2MS(30))
						mul = 1;
					else if (diffTick < SEC2MS(40))
						mul = 2;
					else if (diffTick < SEC2MS(60))
						mul = 3;
					else if (diffTick < SEC2MS(61))
						upload = m_CurUpload;
					if (mul) { //25%, 50%, 75%
						goingDownDivider = goingDownDivider * mul / 4;
						goingUpDivider = goingUpDivider * mul / 4;
					}
					goingDownDivider = max(goingDownDivider, 1);
					goingUpDivider = max(goingUpDivider, 1);
				}

				uint32 soll_ping;
				if (useMillisecondPingTolerance)
					soll_ping = pingToleranceMilliseconds;
				else
					soll_ping = (uint32)(initial_ping * pingTolerance);

				uint32 raw_ping = soll_ping; // this value will cause the upload speed not to change at all.

				bool pingFailure = false;
				for (int pingTries = 0; m_bRun && enabled && (pingTries == 0 || pingFailure) && pingTries < 60; ++pingTries) {
					if (!m_enabled) {
						enabled = false;
						break;
					}
					// ping the hostToPing
					PingStatus pingStatus = pinger.Ping(hostToPing, lastCommonTTL, false, useUdp);

					if (pingStatus.bSuccess && pingStatus.status == IP_TTL_EXPIRED_TRANSIT) {
						if (pingStatus.destinationAddress != lastCommonHost) {
							// something has changed about the topology! We got other ip from this ttl, not the pinged one.
							// Do the tracerouting again to figure out new topology
							const CString &lastCommonHostAddressString(ipstr(lastCommonHost));
							const CString &destinationAddressString(ipstr(pingStatus.destinationAddress));

							theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: Network topology has changed. TTL: %i Expected ip: %s Got ip: %s Will do a new traceroute."), lastCommonTTL, (LPCTSTR)lastCommonHostAddressString, (LPCTSTR)destinationAddressString);
							restart = true;
						}

						raw_ping = (uint32)pingStatus.fDelay;
						// several pings in a row may fail, but the count doesn't matter
						// so reset after every successful ping
						pingFailure = false;
					} else {
						raw_ping = (soll_ping + initial_ping) * 3; // this value will cause the upload speed to be lowered

						pingFailure = true;

						if (m_enabled && pingTries > 3)
							m_eventPrefs.Lock(SEC2MS(1));

						//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: %s-Ping #%i failed. Reason follows"), useUdp?_T("UDP"):_T("ICMP"), pingTries);
						//pinger.PIcmpErr(pingStatus.error);
					}
				}

				if (pingFailure) {
					if (enabled)
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: No response to pings for a long time. Restarting..."));
					restart = true;
				}

				if (!restart) {
					if (raw_ping > 0 && raw_ping < initial_ping && initial_ping > lowestInitialPingAllowed) {
						theApp.QueueDebugLogLine(false, _T("UploadSpeedSense: New lowest ping: %i ms. Old: %i ms"), max(raw_ping, lowestInitialPingAllowed), initial_ping);
						initial_ping = max(raw_ping, lowestInitialPingAllowed);
					}

					m_pingDelaysTotal += raw_ping;
					pingDelays.AddTail(raw_ping);
					while (!pingDelays.IsEmpty() && (uint32)pingDelays.GetCount() > numberOfPingsForAverage)
						m_pingDelaysTotal -= pingDelays.RemoveHead();

					uint32 pingAverage = Median(pingDelays); //(m_pingDelaysTotal/pingDelays.GetCount());
					int normalized_ping = pingAverage - initial_ping;

					//{
					//    prefsLocker.Lock();
					//    uint32 tempCurUpload = m_CurUpload;
					//    prefsLocker.Unlock();
					//    theApp.QueueDebugLogLine(false, _T("USS-Debug: %i %i %i"), raw_ping, upload, tempCurUpload);
					//}

					pingLocker.Lock();
					m_pingAverage = pingAverage;
					m_lowestPing = initial_ping;
					pingLocker.Unlock();

					// Calculate Waiting Time
					sint64 hping = (sint64)soll_ping - normalized_ping;

					// Calculate change of upload speed
					if (hping < 0) {
						//Ping too high
						acceptNewClient = false;

						// lower the speed
						sint64 ulDiff = hping * 1024 * 10 / goingDownDivider / initial_ping;
						//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: Down! Ping cur %i ms. Ave %I64i ms %i values. New Upload %i + %I64i = %I64i"), raw_ping, m_pingDelaysTotal/pingDelays.GetCount(), pingDelays.GetCount(), upload, ulDiff, upload+ulDiff);

						// prevent underflow
						upload = (upload > -ulDiff) ? (uint32)(upload + ulDiff) : 0;
					} else if (hping > 0) {
						//Ping lower than max allowed
						acceptNewClient = true;

						if (curUpload + 30 * 1024 > upload) {
							// raise the speed
							sint64 ulDiff = hping * 1024 * 10 / (goingUpDivider * (sint64)initial_ping);
							//theApp.QueueDebugLogLine(false,_T("UploadSpeedSense: Up! Ping cur %i ms. Ave %I64i ms %i values. New Upload %i + %I64i = %I64i"), raw_ping, m_pingDelaysTotal/pingDelays.GetCount(), pingDelays.GetCount(), upload, ulDiff, upload+ulDiff);

							// prevent overflow
							upload = (_I32_MAX - upload > ulDiff) ? (uint32)(upload + ulDiff) : _I32_MAX;
						}
					}
					prefsLocker.Lock();
					if (upload < m_minUpload) {
						upload = m_minUpload;
						acceptNewClient = true;
					}
					m_upload = upload = min(upload, m_maxUpload);
					prefsLocker.Unlock();

					if (!m_enabled)
						enabled = false;
				}
			}
		}
	}

	// Signal that we have ended.
	m_eventThreadEnded.SetEvent();

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

	uint32 returnVal = (arr[size / 2 - (size & 1)] + arr[size / 2]) / 2;

	delete[] arr;

	return returnVal;
}