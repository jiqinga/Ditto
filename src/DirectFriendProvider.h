#pragma once

#include "ISyncProvider.h"

class CDirectFriendProvider : public ISyncProvider
{
public:
	CDirectFriendProvider();
	virtual ~CDirectFriendProvider();

	virtual DittoSyncProviderType GetProviderType() const;
	virtual void InitializeFromOptions();
	virtual bool StartBackgroundSync(HWND mainHwnd);
	virtual void StopBackgroundSync();
	virtual bool SendClips(CClipList *pClipList);
	virtual bool SupportsConnectionTest() const;
	virtual bool TestConnection(CString &errorMessage);
	virtual bool ResetSyncState(CString &errorMessage);

protected:
	bool SendToConfiguredClients(CClipList *pClipList);
};
