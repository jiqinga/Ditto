#pragma once

#include "CloudSyncThread.h"
#include "ISyncProvider.h"
#include "SupabaseClient.h"

class CSupabaseRelayProvider : public ISyncProvider
{
public:
	CSupabaseRelayProvider();
	virtual ~CSupabaseRelayProvider();

	virtual DittoSyncProviderType GetProviderType() const;
	virtual void InitializeFromOptions();
	virtual bool StartBackgroundSync(HWND mainHwnd);
	virtual void StopBackgroundSync();
	virtual bool SendClips(CClipList *pClipList);
	virtual bool SupportsConnectionTest() const;
	virtual bool TestConnection(CString &errorMessage);
	virtual bool ResetSyncState(CString &errorMessage);

protected:
	bool ValidateConfiguration(CString &errorMessage) const;
	bool EnsureRoomReady(CString &errorMessage);
	bool EnsureDeviceRegistered(CString &errorMessage);
	void TouchDeviceHeartbeat();
	CString BuildDeviceFingerprint() const;
	bool TryExtractAllowedTextPayload(const CClip &clip, CStringA &payload, CString &contentType) const;
	bool BuildEncryptedPayload(const CString &contentType, const CStringA &plainPayload, CStringA &cipherPayload, CString &payloadHash, CString &errorMessage) const;

	CString m_supabaseUrl;
	CString m_supabaseAnonKey;
	CString m_roomCode;
	CString m_deviceName;
	CString m_deviceFingerprint;
	CString m_roomId;
	CCloudSyncThread m_cloudSyncThread;
	CSupabaseClient m_client;
};
