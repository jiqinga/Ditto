#pragma once

#include "chaiscript/utility/json.hpp"
#include <vector>

struct SupabaseSyncMessage
{
	CString m_id;
	CString m_roomId;
	CString m_senderDeviceId;
	CString m_payloadMode;
	CStringA m_payloadInline;
	CString m_payloadHash;
	CString m_contentType;
	CString m_createdAt;
};

class CSupabaseClient
{
public:
	CSupabaseClient();
	~CSupabaseClient();

	void Configure(const CString &supabaseUrl, const CString &anonKey);
	bool IsConfigured(CString &errorMessage) const;
	bool TestConnection(CString &errorMessage);
	bool EnsureRoom(const CString &lookupKey, const CString &roomSalt, CString &roomId, CString &errorMessage);
	bool UpsertDevice(const CString &roomId, const CString &deviceName, const CString &deviceFingerprint, CString &errorMessage);
	bool TouchDeviceHeartbeat(const CString &roomId, const CString &deviceName, CString &errorMessage);
	bool PostMessage(const CString &roomId, const CString &senderDeviceId, const CString &contentType, const CStringA &payloadInline, const CString &payloadHash, CString &messageId, CString &errorMessage);
	bool ListMessages(const CString &roomId, const CString &createdAfterInclusive, std::vector<SupabaseSyncMessage> &messages, CString &errorMessage);

protected:
	bool ParseBaseUrl(CString &host, int &port, bool &useHttps, CString &basePath, CString &errorMessage) const;
	CString BuildRequestPath(const CString &basePath, const CString &relativePath) const;
	CString BuildAuthHeader() const;
	CStringA BuildRoomLookupJson(const CString &lookupKey, const CString &roomSalt) const;
	CStringA BuildDeviceJson(const CString &roomId, const CString &deviceName, const CString &deviceFingerprint) const;
	CStringA BuildMessageJson(const CString &roomId, const CString &senderDeviceId, const CString &contentType, const CStringA &payloadInline, const CString &payloadHash) const;
	bool SendJsonRequest(const CString &method, const CString &relativePath, const CStringA *requestBody, const std::vector<CString> &extraHeaders, long &statusCode, CStringA &responseBody, CString &errorMessage) const;

	CString m_supabaseUrl;
	CString m_anonKey;
};
