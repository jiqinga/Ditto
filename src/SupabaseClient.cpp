#include "stdafx.h"
#include "SupabaseClient.h"
#include "Misc.h"
#include "Shared/TextConvert.h"

#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace
{
	class CWinHttpHandle
	{
	public:
		CWinHttpHandle(HINTERNET h = NULL) : m_handle(h) {}
		~CWinHttpHandle()
		{
			if (m_handle)
			{
				WinHttpCloseHandle(m_handle);
			}
		}

		operator HINTERNET() const { return m_handle; }
		HINTERNET Get() const { return m_handle; }
		HINTERNET Release()
		{
			HINTERNET handle = m_handle;
			m_handle = NULL;
			return handle;
		}

	private:
		HINTERNET m_handle;
	};

	CString WinHttpErrorMessage(DWORD error)
	{
		LPWSTR buffer = NULL;
		DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&buffer,
			0,
			NULL);

		CString message;
		if (length > 0 && buffer)
		{
			message = buffer;
			message.Trim();
			LocalFree(buffer);
		}
		else
		{
			message.Format(_T("WinHTTP error %lu"), error);
		}

		return message;
	}


	CString BuildDeviceUpsertPath()
	{
		return _T("/rest/v1/sync_devices?on_conflict=room_id,device_name");
	}

	CString BuildDeviceHeartbeatPath(const CString &roomId, const CString &deviceName)
	{
		CString path;
		path.Format(_T("/rest/v1/sync_devices?room_id=eq.%s&device_name=eq.%s"), InternetEncode(roomId), InternetEncode(deviceName));
		return path;
	}
}

CSupabaseClient::CSupabaseClient()
{
}

CSupabaseClient::~CSupabaseClient()
{
}

void CSupabaseClient::Configure(const CString &supabaseUrl, const CString &anonKey)
{
	m_supabaseUrl = supabaseUrl;
	m_anonKey = anonKey;
}

bool CSupabaseClient::IsConfigured(CString &errorMessage) const
{
	CString supabaseUrl = m_supabaseUrl;
	CString anonKey = m_anonKey;
	supabaseUrl.Trim();
	anonKey.Trim();

	if (supabaseUrl.IsEmpty())
	{
		errorMessage = _T("Supabase URL is required.");
		return false;
	}

	if (anonKey.IsEmpty())
	{
		errorMessage = _T("Supabase anon key is required.");
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CSupabaseClient::TestConnection(CString &errorMessage)
{
	if (IsConfigured(errorMessage) == false)
	{
		return false;
	}

	long statusCode = 0;
	CStringA responseBody;
	std::vector<CString> headers;
	headers.push_back(_T("apikey: ") + m_anonKey);
	headers.push_back(_T("Range: 0-0"));
	if (SendJsonRequest(_T("GET"), _T("/rest/v1/sync_rooms?select=id&limit=1"), NULL, headers, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode < 200 || statusCode >= 300)
	{
		CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
		errorMessage.Format(_T("Supabase connection test failed with HTTP %ld. Response: %s"), statusCode, responseText);
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CSupabaseClient::EnsureRoom(const CString &lookupKey, const CString &roomSalt, CString &roomId, CString &errorMessage)
{
	roomId.Empty();

	if (IsConfigured(errorMessage) == false)
	{
		return false;
	}

	CString queryPath;
	queryPath.Format(_T("/rest/v1/sync_rooms?select=id,lookup_key&lookup_key=eq.%s&limit=1"), InternetEncode(lookupKey));

	long statusCode = 0;
	CStringA responseBody;
	std::vector<CString> getHeaders;
	getHeaders.push_back(_T("apikey: ") + m_anonKey);
	getHeaders.push_back(_T("Range: 0-0"));
	if (SendJsonRequest(_T("GET"), queryPath, NULL, getHeaders, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode >= 200 && statusCode <= 299)
	{
		try
		{
			json::JSON parsed = json::JSON::Load((LPCSTR)responseBody);
			if (parsed.JSONType() == json::JSON::Class::Array && parsed.size() > 0)
			{
				const json::JSON &first = parsed.at(0);
				if (first.has_key("id"))
				{
					roomId = CTextConvert::Utf8ToUnicode(first.at("id").to_string().c_str());
					return true;
				}
			}
		}
		catch (...)
		{
			// Fall through to insert path.
		}
	}
	else
	{
		CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
		errorMessage.Format(_T("Supabase room lookup failed with HTTP %ld. Response: %s"), statusCode, responseText);
		return false;
	}

	CStringA requestBody = BuildRoomLookupJson(lookupKey, roomSalt);
	std::vector<CString> postHeaders;
	postHeaders.push_back(_T("apikey: ") + m_anonKey);
	postHeaders.push_back(_T("Prefer: return=representation"));
	if (SendJsonRequest(_T("POST"), _T("/rest/v1/sync_rooms"), &requestBody, postHeaders, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode >= 200 && statusCode <= 299)
	{
		try
		{
			json::JSON parsed = json::JSON::Load((LPCSTR)responseBody);
			if (parsed.JSONType() == json::JSON::Class::Array && parsed.size() > 0)
			{
				const json::JSON &first = parsed.at(0);
				if (first.has_key("id"))
				{
					roomId = CTextConvert::Utf8ToUnicode(first.at("id").to_string().c_str());
					return true;
				}
			}
		}
		catch (...)
		{
		}

		errorMessage = _T("Supabase room create response did not contain an id.");
		return false;
	}

	if (statusCode == 409)
	{
		responseBody.Empty();
		if (SendJsonRequest(_T("GET"), queryPath, NULL, getHeaders, statusCode, responseBody, errorMessage) == false)
		{
			return false;
		}

		if (statusCode >= 200 && statusCode <= 299)
		{
			try
			{
				json::JSON parsed = json::JSON::Load((LPCSTR)responseBody);
				if (parsed.JSONType() == json::JSON::Class::Array && parsed.size() > 0)
				{
					const json::JSON &first = parsed.at(0);
					if (first.has_key("id"))
					{
						roomId = CTextConvert::Utf8ToUnicode(first.at("id").to_string().c_str());
						return true;
					}
				}
			}
			catch (...)
			{
			}
		}
	}

	CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
	errorMessage.Format(_T("Supabase room create failed with HTTP %ld. Response: %s"), statusCode, responseText);
	return false;
}

bool CSupabaseClient::UpsertDevice(const CString &roomId, const CString &deviceName, const CString &deviceFingerprint, CString &errorMessage)
{
	if (IsConfigured(errorMessage) == false)
	{
		return false;
	}

	CString roomIdCopy = roomId;
	CString deviceNameCopy = deviceName;
	CString deviceFingerprintCopy = deviceFingerprint;
	roomIdCopy.Trim();
	deviceNameCopy.Trim();
	deviceFingerprintCopy.Trim();
	if (roomIdCopy.IsEmpty())
	{
		errorMessage = _T("Supabase room id is required.");
		return false;
	}
	if (deviceNameCopy.IsEmpty())
	{
		errorMessage = _T("Supabase device name is required.");
		return false;
	}
	if (deviceFingerprintCopy.IsEmpty())
	{
		errorMessage = _T("Supabase device fingerprint is required.");
		return false;
	}

	CStringA requestBody = BuildDeviceJson(roomIdCopy, deviceNameCopy, deviceFingerprintCopy);
	long statusCode = 0;
	CStringA responseBody;
	std::vector<CString> headers;
	headers.push_back(_T("apikey: ") + m_anonKey);
	headers.push_back(_T("Prefer: resolution=merge-duplicates,return=representation"));
	if (SendJsonRequest(_T("POST"), BuildDeviceUpsertPath(), &requestBody, headers, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode < 200 || statusCode > 299)
	{
		CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
		errorMessage.Format(_T("Supabase device upsert failed with HTTP %ld. Response: %s"), statusCode, responseText);
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CSupabaseClient::TouchDeviceHeartbeat(const CString &roomId, const CString &deviceName, CString &errorMessage)
{
	if (IsConfigured(errorMessage) == false)
	{
		return false;
	}

	CString roomIdCopy = roomId;
	CString deviceNameCopy = deviceName;
	roomIdCopy.Trim();
	deviceNameCopy.Trim();
	if (roomIdCopy.IsEmpty())
	{
		errorMessage = _T("Supabase room id is required.");
		return false;
	}
	if (deviceNameCopy.IsEmpty())
	{
		errorMessage = _T("Supabase device name is required.");
		return false;
	}

	CStringA requestBody("{}");
	long statusCode = 0;
	CStringA responseBody;
	std::vector<CString> headers;
	headers.push_back(_T("apikey: ") + m_anonKey);
	headers.push_back(_T("Prefer: return=minimal"));
	headers.push_back(_T("Content-Type: application/json"));
	if (SendJsonRequest(_T("PATCH"), BuildDeviceHeartbeatPath(roomIdCopy, deviceNameCopy), &requestBody, headers, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode < 200 || statusCode > 299)
	{
		CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
		errorMessage.Format(_T("Supabase device heartbeat failed with HTTP %ld. Response: %s"), statusCode, responseText);
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CSupabaseClient::PostMessage(const CString &roomId, const CString &senderDeviceId, const CString &contentType, const CStringA &payloadInline, const CString &payloadHash, CString &messageId, CString &errorMessage)
{
	messageId.Empty();

	if (IsConfigured(errorMessage) == false)
	{
		return false;
	}

	CString roomIdCopy = roomId;
	CString senderDeviceIdCopy = senderDeviceId;
	CString contentTypeCopy = contentType;
	roomIdCopy.Trim();
	senderDeviceIdCopy.Trim();
	contentTypeCopy.Trim();
	if (roomIdCopy.IsEmpty())
	{
		errorMessage = _T("Supabase room id is required.");
		return false;
	}
	if (senderDeviceIdCopy.IsEmpty())
	{
		errorMessage = _T("Supabase sender device id is required.");
		return false;
	}
	if (contentTypeCopy.IsEmpty())
	{
		errorMessage = _T("Supabase content type is required.");
		return false;
	}

	CStringA requestBody = BuildMessageJson(roomIdCopy, senderDeviceIdCopy, contentTypeCopy, payloadInline, payloadHash);
	long statusCode = 0;
	CStringA responseBody;
	std::vector<CString> headers;
	headers.push_back(_T("apikey: ") + m_anonKey);
	headers.push_back(_T("Prefer: return=representation"));
	if (SendJsonRequest(_T("POST"), _T("/rest/v1/sync_messages"), &requestBody, headers, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode < 200 || statusCode > 299)
	{
		CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
		errorMessage.Format(_T("Supabase message post failed with HTTP %ld. Response: %s"), statusCode, responseText);
		return false;
	}

	try
	{
		json::JSON parsed = json::JSON::Load((LPCSTR)responseBody);
		if (parsed.JSONType() == json::JSON::Class::Array && parsed.size() > 0)
		{
			const json::JSON &first = parsed.at(0);
			if (first.has_key("id"))
			{
				messageId = CTextConvert::Utf8ToUnicode(first.at("id").to_string().c_str());
				errorMessage.Empty();
				return true;
			}
		}
	}
	catch (...)
	{
	}

	errorMessage = _T("Supabase message response did not contain an id.");
	return false;
}

bool CSupabaseClient::ListMessages(const CString &roomId, const CString &createdAfterInclusive, std::vector<SupabaseSyncMessage> &messages, CString &errorMessage)
{
	messages.clear();

	if (IsConfigured(errorMessage) == false)
	{
		return false;
	}

	CString roomIdCopy = roomId;
	roomIdCopy.Trim();
	if (roomIdCopy.IsEmpty())
	{
		errorMessage = _T("Supabase room id is required.");
		return false;
	}

	CString queryPath;
	queryPath.Format(_T("/rest/v1/sync_messages?select=id,room_id,sender_device_id,payload_mode,payload_inline,payload_hash,content_type,created_at&room_id=eq.%s&order=created_at.asc,id.asc&limit=200"), InternetEncode(roomIdCopy));

	CString createdAfter = createdAfterInclusive;
	createdAfter.Trim();
	if (createdAfter.IsEmpty() == FALSE)
	{
		queryPath += _T("&created_at=gte.") + InternetEncode(createdAfter);
	}

	long statusCode = 0;
	CStringA responseBody;
	std::vector<CString> headers;
	headers.push_back(_T("apikey: ") + m_anonKey);
	if (SendJsonRequest(_T("GET"), queryPath, NULL, headers, statusCode, responseBody, errorMessage) == false)
	{
		return false;
	}

	if (statusCode < 200 || statusCode >= 300)
	{
		CString responseText = CTextConvert::Utf8ToUnicode(responseBody);
		errorMessage.Format(_T("Supabase list messages failed with HTTP %ld. Response: %s"), statusCode, responseText);
		return false;
	}

	try
	{
		json::JSON parsed = json::JSON::Load((LPCSTR)responseBody);
		if (parsed.JSONType() != json::JSON::Class::Array)
		{
			errorMessage = _T("Supabase list messages response was not an array.");
			return false;
		}

		for (size_t i = 0; i < parsed.size(); i++)
		{
			const json::JSON &item = parsed.at(i);
			SupabaseSyncMessage message;
			if (item.has_key("id"))
			{
				message.m_id = CTextConvert::Utf8ToUnicode(item.at("id").to_string().c_str());
			}
			if (item.has_key("room_id"))
			{
				message.m_roomId = CTextConvert::Utf8ToUnicode(item.at("room_id").to_string().c_str());
			}
			if (item.has_key("sender_device_id"))
			{
				message.m_senderDeviceId = CTextConvert::Utf8ToUnicode(item.at("sender_device_id").to_string().c_str());
			}
			if (item.has_key("payload_mode"))
			{
				message.m_payloadMode = CTextConvert::Utf8ToUnicode(item.at("payload_mode").to_string().c_str());
			}
			if (item.has_key("payload_inline") && item.at("payload_inline").JSONType() == json::JSON::Class::String)
			{
				message.m_payloadInline = item.at("payload_inline").to_string().c_str();
			}
			if (item.has_key("payload_hash"))
			{
				message.m_payloadHash = CTextConvert::Utf8ToUnicode(item.at("payload_hash").to_string().c_str());
			}
			if (item.has_key("content_type"))
			{
				message.m_contentType = CTextConvert::Utf8ToUnicode(item.at("content_type").to_string().c_str());
			}
			if (item.has_key("created_at"))
			{
				message.m_createdAt = CTextConvert::Utf8ToUnicode(item.at("created_at").to_string().c_str());
			}

			messages.push_back(message);
		}
	}
	catch (...)
	{
		errorMessage = _T("Supabase list messages response could not be parsed.");
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CSupabaseClient::ParseBaseUrl(CString &host, int &port, bool &useHttps, CString &basePath, CString &errorMessage) const
{
	host.Empty();
	basePath = _T("/");
	port = 0;
	useHttps = false;

	CString url = m_supabaseUrl;
	url.Trim();

	if (url.Left(8).CompareNoCase(_T("https://")) == 0)
	{
		useHttps = true;
		port = INTERNET_DEFAULT_HTTPS_PORT;
		url = url.Mid(8);
	}
	else if (url.Left(7).CompareNoCase(_T("http://")) == 0)
	{
		useHttps = false;
		port = INTERNET_DEFAULT_HTTP_PORT;
		url = url.Mid(7);
	}
	else
	{
		errorMessage = _T("Supabase URL must start with http:// or https://.");
		return false;
	}

	int slash = url.Find(_T('/'));
	CString authority = url;
	if (slash >= 0)
	{
		authority = url.Left(slash);
		basePath = url.Mid(slash);
	}

	int colon = authority.Find(_T(':'));
	if (colon >= 0)
	{
		host = authority.Left(colon);
		CString portText = authority.Mid(colon + 1);
		port = _ttoi(portText);
	}
	else
	{
		host = authority;
	}

	host.Trim();
	basePath.Trim();
	if (basePath.IsEmpty())
	{
		basePath = _T("/");
	}

	if (basePath.Left(1) != _T("/"))
	{
		basePath = _T("/") + basePath;
	}

	if (host.IsEmpty() || port == 0)
	{
		errorMessage = _T("Supabase URL host or port is invalid.");
		return false;
	}

	errorMessage.Empty();
	return true;
}

CString CSupabaseClient::BuildRequestPath(const CString &basePath, const CString &relativePath) const
{
	CString path = basePath;
	if (path.Right(1) == _T("/") && relativePath.Left(1) == _T("/"))
	{
		path += relativePath.Mid(1);
	}
	else if (path.Right(1) != _T("/") && relativePath.Left(1) != _T("/"))
	{
		path += _T("/") + relativePath;
	}
	else
	{
		path += relativePath;
	}

	return path;
}

CString CSupabaseClient::BuildAuthHeader() const
{
	return StrF(_T("Bearer %s"), m_anonKey);
}

CStringA CSupabaseClient::BuildRoomLookupJson(const CString &lookupKey, const CString &roomSalt) const
{
	json::JSON payload(json::JSON::Class::Object);
	payload["lookup_key"] = CTextConvert::UnicodeToUTF8(lookupKey).GetString();
	payload["salt"] = CTextConvert::UnicodeToUTF8(roomSalt).GetString();
	return CStringA(payload.dump().c_str());
}

CStringA CSupabaseClient::BuildDeviceJson(const CString &roomId, const CString &deviceName, const CString &deviceFingerprint) const
{
	json::JSON payload(json::JSON::Class::Object);
	payload["room_id"] = CTextConvert::UnicodeToUTF8(roomId).GetString();
	payload["device_name"] = CTextConvert::UnicodeToUTF8(deviceName).GetString();
	payload["device_fingerprint"] = CTextConvert::UnicodeToUTF8(deviceFingerprint).GetString();
	return CStringA(payload.dump().c_str());
}

CStringA CSupabaseClient::BuildMessageJson(const CString &roomId, const CString &senderDeviceId, const CString &contentType, const CStringA &payloadInline, const CString &payloadHash) const
{
	json::JSON payload(json::JSON::Class::Object);
	payload["room_id"] = CTextConvert::UnicodeToUTF8(roomId).GetString();
	payload["sender_device_id"] = CTextConvert::UnicodeToUTF8(senderDeviceId).GetString();
	payload["content_type"] = CTextConvert::UnicodeToUTF8(contentType).GetString();
	payload["payload_mode"] = "inline";
	payload["payload_inline"] = std::string(payloadInline);
	payload["payload_hash"] = CTextConvert::UnicodeToUTF8(payloadHash).GetString();
	return CStringA(payload.dump().c_str());
}

bool CSupabaseClient::SendJsonRequest(const CString &method, const CString &relativePath, const CStringA *requestBody, const std::vector<CString> &extraHeaders, long &statusCode, CStringA &responseBody, CString &errorMessage) const
{
	statusCode = 0;
	responseBody.Empty();

	CString host;
	CString basePath;
	int port = 0;
	bool useHttps = false;
	if (ParseBaseUrl(host, port, useHttps, basePath, errorMessage) == false)
	{
		return false;
	}

	CString requestPath = BuildRequestPath(basePath, relativePath);

	CWinHttpHandle session(WinHttpOpen(L"Ditto Cloud Sync/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
	if (session.Get() == NULL)
	{
		errorMessage = WinHttpErrorMessage(GetLastError());
		return false;
	}

	CWinHttpHandle connection(WinHttpConnect(session, host, port, 0));
	if (connection.Get() == NULL)
	{
		errorMessage = WinHttpErrorMessage(GetLastError());
		return false;
	}

	DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
	CWinHttpHandle request(WinHttpOpenRequest(connection, method, requestPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
	if (request.Get() == NULL)
	{
		errorMessage = WinHttpErrorMessage(GetLastError());
		return false;
	}

	CString headers = _T("Accept: application/json\r\nContent-Type: application/json\r\nAuthorization: ") + BuildAuthHeader() + _T("\r\n");
	for (size_t i = 0; i < extraHeaders.size(); i++)
	{
		headers += extraHeaders[i] + _T("\r\n");
	}

	LPCVOID bodyPtr = NULL;
	DWORD bodyLength = 0;
	if (requestBody)
	{
		bodyPtr = (LPCSTR)(*requestBody);
		bodyLength = (DWORD)requestBody->GetLength();
	}

	if (WinHttpSendRequest(request, headers, (DWORD)-1L, (LPVOID)bodyPtr, bodyLength, bodyLength, 0) == FALSE)
	{
		errorMessage = WinHttpErrorMessage(GetLastError());
		return false;
	}

	if (WinHttpReceiveResponse(request, NULL) == FALSE)
	{
		errorMessage = WinHttpErrorMessage(GetLastError());
		return false;
	}

	DWORD statusCodeSize = sizeof(statusCode);
	if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX) == FALSE)
	{
		errorMessage = WinHttpErrorMessage(GetLastError());
		return false;
	}

	CStringA body;
	for (;;)
	{
		DWORD availableSize = 0;
		if (WinHttpQueryDataAvailable(request, &availableSize) == FALSE)
		{
			errorMessage = WinHttpErrorMessage(GetLastError());
			return false;
		}

		if (availableSize == 0)
		{
			break;
		}

		CStringA chunk;
		LPSTR buffer = chunk.GetBufferSetLength((int)availableSize);
		DWORD bytesRead = 0;
		if (WinHttpReadData(request, buffer, availableSize, &bytesRead) == FALSE)
		{
			chunk.ReleaseBuffer(0);
			errorMessage = WinHttpErrorMessage(GetLastError());
			return false;
		}

		chunk.ReleaseBuffer((int)bytesRead);
		body += chunk;
	}

	responseBody = body;
	errorMessage.Empty();
	return true;
}
