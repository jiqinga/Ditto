#include "stdafx.h"
#include "CloudSyncThread.h"
#include "CloudSyncState.h"
#include "CloudSyncCrypto.h"
#include "CP_Main.h"
#include "Misc.h"
#include "..\Shared\TextConvert.h"

namespace
{
	CString TrimCursorValue(const CString &value)
	{
		CString trimmed = value;
		trimmed.Trim();
		return trimmed;
	}
}

CCloudSyncThread::CCloudSyncThread()
{
	m_threadName = "CCloudSyncThread";
	m_waitTimeout = 5000;
	m_pClient = NULL;
	m_mainHwnd = NULL;
}

CCloudSyncThread::~CCloudSyncThread()
{
}

void CCloudSyncThread::Configure(CSupabaseClient *pClient, const CString &roomId, const CString &roomCode, const CString &deviceName, const CString &deviceFingerprint, HWND mainHwnd)
{
	m_pClient = pClient;
	m_roomId = roomId;
	m_roomCode = roomCode;
	m_deviceName = deviceName;
	m_deviceFingerprint = deviceFingerprint;
	m_mainHwnd = mainHwnd;
}

void CCloudSyncThread::StartSync()
{
	Start();
}

void CCloudSyncThread::StopSync()
{
	Stop();
}

bool CCloudSyncThread::ShouldImportMessage(const SupabaseSyncMessage &message) const
{
	CString payloadMode = message.m_payloadMode;
	payloadMode.Trim();
	payloadMode.MakeLower();
	if (payloadMode != _T("inline"))
	{
		return false;
	}

	CString senderDeviceId = message.m_senderDeviceId;
	senderDeviceId.Trim();
	if (senderDeviceId.CompareNoCase(m_deviceFingerprint) == 0)
	{
		return false;
	}

	return true;
}

bool CCloudSyncThread::BuildClipListFromMessage(const SupabaseSyncMessage &message, CClipList *pClipList)
{
	if (pClipList == NULL)
	{
		return false;
	}

	CString contentType;
	CStringA payloadInline;
	CString errorMessage;
	if (CCloudSyncCrypto::DecryptMessage(m_roomCode, message, contentType, payloadInline, errorMessage) == false)
	{
		Log(StrF(_T("Cloud sync decrypt failed for message %s: %s"), message.m_id, errorMessage));
		return false;
	}

	contentType.Trim();
	contentType.MakeLower();
	if (payloadInline.IsEmpty())
	{
		return false;
	}

	bool allowHtml = CGetSetOptions::GetCloudSyncAllowHtml() && contentType.Find(_T("text/html")) == 0;
	bool allowRtf = CGetSetOptions::GetCloudSyncAllowRtf() && contentType.Find(_T("text/rtf")) == 0;
	bool allowText = CGetSetOptions::GetCloudSyncAllowText() && contentType.Find(_T("text/plain")) == 0;
	if (allowHtml == false && allowRtf == false && allowText == false)
	{
		return false;
	}

	CClip *pClip = new CClip();
	if (pClip == NULL)
	{
		return false;
	}

	bool addedFormat = false;
	if (allowHtml)
	{
		addedFormat = pClip->AddFormat(theApp.m_HTML_Format, (LPVOID)(LPCSTR)payloadInline, payloadInline.GetLength() + 1, true);
	}
	else if (allowRtf)
	{
		addedFormat = pClip->AddFormat(theApp.m_RTFFormat, (LPVOID)(LPCSTR)payloadInline, payloadInline.GetLength() + 1, true);
	}
	else if (contentType.Find(_T("text/plain;charset=utf-8")) == 0)
	{
		CString unicodeText = CTextConvert::Utf8ToUnicode(payloadInline);
		addedFormat = pClip->AddFormat(CF_UNICODETEXT, (LPVOID)(LPCWSTR)unicodeText, (unicodeText.GetLength() + 1) * sizeof(wchar_t), true);
	}
	else if (allowText)
	{
		addedFormat = pClip->AddFormat(CF_TEXT, (LPVOID)(LPCSTR)payloadInline, payloadInline.GetLength() + 1, true);
	}

	if (addedFormat == false)
	{
		delete pClip;
		return false;
	}

	pClipList->AddTail(pClip);
	return true;
}

void CCloudSyncThread::OnTimeOut(void *param)
{
	UNREFERENCED_PARAMETER(param);

	if (m_pClient == NULL || m_mainHwnd == NULL)
	{
		return;
	}

	CString roomId = TrimCursorValue(m_roomId);
	if (roomId.IsEmpty())
	{
		return;
	}

	CString cursor = CCloudSyncState::GetLastCursor(theApp.m_db, roomId);
	std::vector<SupabaseSyncMessage> messages;
	CString errorMessage;
	if (m_pClient->ListMessages(roomId, cursor, messages, errorMessage) == false)
	{
		Log(StrF(_T("Cloud sync poll failed: %s"), errorMessage));
		return;
	}

	for (size_t i = 0; i < messages.size(); i++)
	{
		const SupabaseSyncMessage &message = messages[i];
		if (message.m_id.IsEmpty())
		{
			continue;
		}

		CString messageCreatedAt = TrimCursorValue(message.m_createdAt);
		if (cursor.IsEmpty() == FALSE && messageCreatedAt.IsEmpty() == FALSE && messageCreatedAt.Compare(cursor) < 0)
		{
			continue;
		}

		if (CCloudSyncState::HasMessage(theApp.m_db, roomId, message.m_id))
		{
			if (messageCreatedAt.IsEmpty() == FALSE)
			{
				CCloudSyncState::SetLastCursor(theApp.m_db, roomId, messageCreatedAt);
				cursor = messageCreatedAt;
			}
			continue;
		}

		if (message.m_payloadHash.IsEmpty() == FALSE && CCloudSyncState::HasPayloadHash(theApp.m_db, roomId, message.m_payloadHash))
		{
			CCloudSyncState::UpsertMessageState(theApp.m_db, roomId, message.m_id, message.m_senderDeviceId, message.m_payloadHash, _T("duplicate"));
			if (messageCreatedAt.IsEmpty() == FALSE)
			{
				CCloudSyncState::SetLastCursor(theApp.m_db, roomId, messageCreatedAt);
				cursor = messageCreatedAt;
			}
			continue;
		}

		if (ShouldImportMessage(message) == false)
		{
			CCloudSyncState::UpsertMessageState(theApp.m_db, roomId, message.m_id, message.m_senderDeviceId, message.m_payloadHash, _T("skipped"));
			if (messageCreatedAt.IsEmpty() == FALSE)
			{
				CCloudSyncState::SetLastCursor(theApp.m_db, roomId, messageCreatedAt);
				cursor = messageCreatedAt;
			}
			continue;
		}

		CClipList *pClipList = new CClipList();
		if (BuildClipListFromMessage(message, pClipList))
		{
			::PostMessage(m_mainHwnd, WM_ADD_TO_DATABASE_FROM_SOCKET, (WPARAM)pClipList, 0);
			CCloudSyncState::UpsertMessageState(theApp.m_db, roomId, message.m_id, message.m_senderDeviceId, message.m_payloadHash, _T("applied"));
		}
		else
		{
			delete pClipList;
			CCloudSyncState::UpsertMessageState(theApp.m_db, roomId, message.m_id, message.m_senderDeviceId, message.m_payloadHash, _T("skipped"));
		}

		if (messageCreatedAt.IsEmpty() == FALSE)
		{
			CCloudSyncState::SetLastCursor(theApp.m_db, roomId, messageCreatedAt);
			cursor = messageCreatedAt;
		}
	}
}
