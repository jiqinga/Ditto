#pragma once

#include "EventThread.h"
#include "SupabaseClient.h"

class CClipList;

class CCloudSyncThread : public CEventThread
{
public:
	CCloudSyncThread();
	virtual ~CCloudSyncThread();

	void Configure(CSupabaseClient *pClient, const CString &roomId, const CString &roomCode, const CString &deviceName, const CString &deviceFingerprint, HWND mainHwnd);
	void StartSync();
	void StopSync();

protected:
	virtual void OnTimeOut(void *param);
	bool BuildClipListFromMessage(const SupabaseSyncMessage &message, CClipList *pClipList);
	bool ShouldImportMessage(const SupabaseSyncMessage &message) const;

	CSupabaseClient *m_pClient;
	CString m_roomId;
	CString m_roomCode;
	CString m_deviceName;
	CString m_deviceFingerprint;
	HWND m_mainHwnd;
};
