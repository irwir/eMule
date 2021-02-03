// ZZ:UploadBandWithThrottler (UDP) -->

#pragma once

struct SocketSentBytes
{
	uint32	sentBytesStandardPackets;
	uint32	sentBytesControlPackets;
	bool	success;
};

class ThrottledControlSocket
{
public:
	virtual SocketSentBytes SendControlData(uint32 maxNumberOfBytesToSend, uint32 minFragSize) = 0;
protected:
	virtual	~ThrottledControlSocket() = default;
};

class ThrottledFileSocket : public ThrottledControlSocket
{
public:
	virtual SocketSentBytes SendFileAndControlData(uint32 maxNumberOfBytesToSend, uint32 minFragSize) = 0;
	virtual DWORD GetLastCalledSend() const = 0;
	virtual uint32	GetNeededBytes() = 0;
	virtual bool IsBusyExtensiveCheck() = 0;
	virtual bool IsBusyQuickCheck() const = 0;
	virtual bool IsEnoughFileDataQueued(uint32 nMinFilePayloadBytes) const = 0;
	virtual bool HasQueues(bool bOnlyStandardPackets = false) const = 0;
	virtual bool UseBigSendBuffer()					{ return false; }
};

// <-- ZZ:UploadBandWithThrottler (UDP)