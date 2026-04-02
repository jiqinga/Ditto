#include "stdafx.h"
#include "SupabaseRelayProvider.h"
#include "CloudSyncCrypto.h"
#include "CP_Main.h"
#include "CloudSyncState.h"
#include "DatabaseUtilities.h"
#include "Md5.h"
#include "Misc.h"
#include "Options.h"
#include "Shared/TextConvert.h"

namespace
{
	CString BuildRoomLookupKey(const CString &roomCode)
	{
		CString normalizedRoomCode = roomCode;
		normalizedRoomCode.Trim();
		normalizedRoomCode.MakeLower();

		CStringA utf8 = CTextConvert::UnicodeToUTF8(normalizedRoomCode + _T("|ditto-cloud-sync-v1"));
		CMd5 md5;
		CStringA digest = md5.CalcMD5FromString(utf8, utf8.GetLength());
		return CTextConvert::AnsiToUnicode(digest);
	}

	CString BuildRoomSalt(const CString &roomCode)
	{
		CString normalizedRoomCode = roomCode;
		normalizedRoomCode.Trim();
		normalizedRoomCode.MakeUpper();

		CStringA utf8 = CTextConvert::UnicodeToUTF8(_T("salt|") + normalizedRoomCode + _T("|ditto-cloud-sync-v1"));
		CMd5 md5;
		CStringA digest = md5.CalcMD5FromString(utf8, utf8.GetLength());
		return CTextConvert::AnsiToUnicode(digest);
	}

	CStringA TrimInlinePayload(const CStringA &payload)
	{
		CStringA trimmed = payload;
		while (trimmed.IsEmpty() == FALSE && trimmed[trimmed.GetLength() - 1] == '\0')
		{
			trimmed = trimmed.Left(trimmed.GetLength() - 1);
		}

		return trimmed;
	}

	CString GetEncryptedEnvelopeContentType()
	{
		return _T("application/ditto-cloud-sync+encrypted");
	}
}

CSupabaseRelayProvider::CSupabaseRelayProvider()
{
}

CSupabaseRelayProvider::~CSupabaseRelayProvider()
{
}

DittoSyncProviderType CSupabaseRelayProvider::GetProviderType() const
{
	return DITTO_SYNC_PROVIDER_SUPABASE_RELAY;
}

void CSupabaseRelayProvider::InitializeFromOptions()
{
	m_supabaseUrl = CGetSetOptions::GetCloudSyncSupabaseUrl();
	m_supabaseAnonKey = CGetSetOptions::GetCloudSyncSupabaseAnonKey();
	m_roomCode = CGetSetOptions::GetCloudSyncRoomCode();
	m_deviceName = CGetSetOptions::GetCloudSyncDeviceName();
	m_deviceFingerprint = BuildDeviceFingerprint();
	m_roomId.Empty();
	m_client.Configure(m_supabaseUrl, m_supabaseAnonKey);
}

bool CSupabaseRelayProvider::StartBackgroundSync(HWND mainHwnd)
{
	CString errorMessage;
	if (ValidateConfiguration(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay sync is not configured: %s"), errorMessage));
		return false;
	}

	if (m_client.IsConfigured(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay client is not configured: %s"), errorMessage));
		return false;
	}

	if (EnsureRoomReady(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay room initialization failed: %s"), errorMessage));
		return false;
	}

	if (EnsureDeviceRegistered(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay device registration failed: %s"), errorMessage));
		return false;
	}

	TouchDeviceHeartbeat();
	m_cloudSyncThread.Configure(&m_client, m_roomId, m_roomCode, m_deviceName, m_deviceFingerprint, mainHwnd);
	if (m_cloudSyncThread.IsRunning() == false)
	{
		m_cloudSyncThread.StartSync();
	}

	return true;
}

void CSupabaseRelayProvider::StopBackgroundSync()
{
	m_cloudSyncThread.StopSync();
	m_roomId.Empty();
}

bool CSupabaseRelayProvider::SendClips(CClipList *pClipList)
{
	CString errorMessage;
	if (ValidateConfiguration(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay sync send blocked: %s"), errorMessage));
		return false;
	}

	if (pClipList == NULL)
	{
		Log(_T("Cloud relay sync send blocked: clip list is null"));
		return false;
	}

	if (EnsureRoomReady(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay sync send blocked: %s"), errorMessage));
		return false;
	}

	if (EnsureDeviceRegistered(errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay device registration failed before send: %s"), errorMessage));
		return false;
	}

	bool sentAny = false;
	POSITION pos = pClipList->GetHeadPosition();
	while (pos)
	{
		CClip *pClip = pClipList->GetNext(pos);
		if (pClip == NULL)
		{
			continue;
		}

		CStringA plainPayload;
		CString contentType;
		if (TryExtractAllowedTextPayload(*pClip, plainPayload, contentType) == false)
		{
			continue;
		}

		CStringA cipherPayload;
		CString payloadHash;
		if (BuildEncryptedPayload(contentType, plainPayload, cipherPayload, payloadHash, errorMessage) == false)
		{
			Log(StrF(_T("Cloud relay encryption failed for clip %d: %s"), pClip->m_id, errorMessage));
			continue;
		}

		CString messageId;
		if (m_client.PostMessage(m_roomId, m_deviceFingerprint, GetEncryptedEnvelopeContentType(), cipherPayload, payloadHash, messageId, errorMessage) == false)
		{
			Log(StrF(_T("Cloud relay message post failed for clip %d: %s"), pClip->m_id, errorMessage));
			continue;
		}

		TouchDeviceHeartbeat();
		theApp.m_lClipsSent++;
		sentAny = true;
		Log(StrF(_T("Cloud relay message posted, clip id: %d, message id: %s, content type: %s"), pClip->m_id, messageId, contentType));
	}

	if (sentAny == false)
	{
		Log(_T("Cloud relay send skipped: no eligible text payloads found"));
	}

	return sentAny;
}

bool CSupabaseRelayProvider::SupportsConnectionTest() const
{
	return true;
}

bool CSupabaseRelayProvider::TestConnection(CString &errorMessage)
{
	if (ValidateConfiguration(errorMessage) == false)
	{
		return false;
	}

	if (m_client.TestConnection(errorMessage) == false)
	{
		return false;
	}

	if (EnsureRoomReady(errorMessage) == false)
	{
		return false;
	}

	return EnsureDeviceRegistered(errorMessage);
}

bool CSupabaseRelayProvider::ResetSyncState(CString &errorMessage)
{
	if (theApp.m_db.IsDatabaseOpen() == FALSE)
	{
		errorMessage = _T("Local database is not open.");
		return false;
	}

	if (EnsureCloudSyncStateTable() == FALSE)
	{
		errorMessage = _T("Cloud sync state table is unavailable.");
		return false;
	}

	CString roomId = m_roomId;
	roomId.Trim();
	if (roomId.IsEmpty())
	{
		if (ValidateConfiguration(errorMessage) == false)
		{
			return false;
		}

		if (EnsureRoomReady(errorMessage) == false)
		{
			return false;
		}

		roomId = m_roomId;
		roomId.Trim();
	}

	if (roomId.IsEmpty())
	{
		errorMessage = _T("Supabase room id is empty; cannot reset cloud sync state.");
		return false;
	}

	if (CCloudSyncState::ResetState(theApp.m_db, roomId) == FALSE)
	{
		errorMessage = _T("Failed to reset local cloud sync state.");
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CSupabaseRelayProvider::EnsureRoomReady(CString &errorMessage)
{
	CString roomId = m_roomId;
	roomId.Trim();
	if (roomId.IsEmpty() == false)
	{
		errorMessage.Empty();
		return true;
	}

	CString lookupKey = BuildRoomLookupKey(m_roomCode);
	CString roomSalt = BuildRoomSalt(m_roomCode);
	if (m_client.EnsureRoom(lookupKey, roomSalt, roomId, errorMessage) == false)
	{
		return false;
	}

	roomId.Trim();
	if (roomId.IsEmpty())
	{
		errorMessage = _T("Supabase room id is empty after room initialization.");
		return false;
	}

	m_roomId = roomId;
	errorMessage.Empty();
	return true;
}

bool CSupabaseRelayProvider::EnsureDeviceRegistered(CString &errorMessage)
{
	if (m_roomId.IsEmpty())
	{
		errorMessage = _T("Supabase room id is empty before device registration.");
		return false;
	}

	return m_client.UpsertDevice(m_roomId, m_deviceName, m_deviceFingerprint, errorMessage);
}

void CSupabaseRelayProvider::TouchDeviceHeartbeat()
{
	CString roomId = m_roomId;
	CString deviceName = m_deviceName;
	roomId.Trim();
	deviceName.Trim();
	if (roomId.IsEmpty() || deviceName.IsEmpty())
	{
		return;
	}

	CString errorMessage;
	if (m_client.TouchDeviceHeartbeat(roomId, deviceName, errorMessage) == false)
	{
		Log(StrF(_T("Cloud relay device heartbeat failed: %s"), errorMessage));
	}
}

CString CSupabaseRelayProvider::BuildDeviceFingerprint() const
{
	CString raw = StrF(_T("%s|%s|ditto-cloud-sync-device-v1"), GetComputerName(), m_deviceName);
	CStringA utf8 = CTextConvert::UnicodeToUTF8(raw);
	CMd5 md5;
	return CTextConvert::AnsiToUnicode(md5.CalcMD5FromString(utf8, utf8.GetLength()));
}

bool CSupabaseRelayProvider::BuildEncryptedPayload(const CString &contentType, const CStringA &plainPayload, CStringA &cipherPayload, CString &payloadHash, CString &errorMessage) const
{
	if (CCloudSyncCrypto::EncryptMessage(m_roomCode, contentType, plainPayload, cipherPayload, errorMessage) == false)
	{
		return false;
	}

	return CCloudSyncCrypto::ComputePayloadHash(m_roomCode, contentType, plainPayload, payloadHash, errorMessage);
}

bool CSupabaseRelayProvider::TryExtractAllowedTextPayload(const CClip &clip, CStringA &payload, CString &contentType) const
{
	payload.Empty();
	contentType.Empty();

	if (clip.m_id <= 0)
	{
		return false;
	}

	CClip dbClip;
	if (dbClip.LoadFormats(clip.m_id, false, true) == false)
	{
		return false;
	}

	if (CGetSetOptions::GetCloudSyncAllowHtml())
	{
		IClipFormat *pHtml = dbClip.Clips()->FindFormatEx(theApp.m_HTML_Format);
		if (pHtml)
		{
			payload = TrimInlinePayload(pHtml->GetAsCStringA());
			if (payload.IsEmpty() == FALSE)
			{
				contentType = _T("text/html");
				return true;
			}
		}
	}

	if (CGetSetOptions::GetCloudSyncAllowRtf())
	{
		IClipFormat *pRtf = dbClip.Clips()->FindFormatEx(theApp.m_RTFFormat);
		if (pRtf)
		{
			payload = TrimInlinePayload(pRtf->GetAsCStringA());
			if (payload.IsEmpty() == FALSE)
			{
				contentType = _T("text/rtf");
				return true;
			}
		}
	}

	if (CGetSetOptions::GetCloudSyncAllowText())
	{
		CString unicodeText = dbClip.GetUnicodeTextFormat();
		if (unicodeText.IsEmpty() == FALSE)
		{
			payload = TrimInlinePayload(CTextConvert::UnicodeToUTF8(unicodeText));
			if (payload.IsEmpty() == FALSE)
			{
				contentType = _T("text/plain;charset=utf-8");
				return true;
			}
		}

		CStringA ansiText = dbClip.GetCFTextTextFormat();
		if (ansiText.IsEmpty() == FALSE)
		{
			payload = TrimInlinePayload(ansiText);
			if (payload.IsEmpty() == FALSE)
			{
				contentType = _T("text/plain");
				return true;
			}
		}
	}

	return false;
}

bool CSupabaseRelayProvider::ValidateConfiguration(CString &errorMessage) const
{
	CString supabaseUrl = m_supabaseUrl;
	CString supabaseAnonKey = m_supabaseAnonKey;
	CString roomCode = m_roomCode;
	CString deviceName = m_deviceName;

	supabaseUrl.Trim();
	supabaseAnonKey.Trim();
	roomCode.Trim();
	deviceName.Trim();

	if (supabaseUrl.IsEmpty())
	{
		errorMessage = _T("Supabase URL is required.");
		return false;
	}

	if (supabaseAnonKey.IsEmpty())
	{
		errorMessage = _T("Supabase anon key is required.");
		return false;
	}

	if (roomCode.IsEmpty())
	{
		errorMessage = _T("Room code is required.");
		return false;
	}

	if (deviceName.IsEmpty())
	{
		errorMessage = _T("Device name is required.");
		return false;
	}

	errorMessage.Empty();
	return true;
}
