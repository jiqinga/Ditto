#pragma once

#include "Clip.h"

enum DittoSyncProviderType
{
	DITTO_SYNC_PROVIDER_DIRECT_FRIENDS = 0,
	DITTO_SYNC_PROVIDER_SUPABASE_RELAY = 1
};

class ISyncProvider
{
public:
	virtual ~ISyncProvider() {}

	virtual DittoSyncProviderType GetProviderType() const = 0;
	virtual void InitializeFromOptions() = 0;
	virtual bool StartBackgroundSync(HWND mainHwnd) = 0;
	virtual void StopBackgroundSync() = 0;
	virtual bool SendClips(CClipList *pClipList) = 0;
	virtual bool SupportsConnectionTest() const = 0;
	virtual bool TestConnection(CString &errorMessage) = 0;
	virtual bool ResetSyncState(CString &errorMessage) = 0;
};
