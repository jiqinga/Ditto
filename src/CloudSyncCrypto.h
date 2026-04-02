#pragma once

#include "SupabaseClient.h"

class CCloudSyncCrypto
{
public:
	static bool EncryptMessage(const CString &roomCode, const CString &contentType, const CStringA &plainPayload, CStringA &cipherPayload, CString &errorMessage);
	static bool DecryptMessage(const CString &roomCode, const SupabaseSyncMessage &message, CString &contentType, CStringA &plainPayload, CString &errorMessage);
	static bool ComputePayloadHash(const CString &roomCode, const CString &contentType, const CStringA &plainPayload, CString &payloadHash, CString &errorMessage);

protected:
	static bool DeriveKey(const CString &roomCode, BYTE key[32], CString &errorMessage);
	static bool EncryptBuffer(const BYTE key[32], const CString &contentType, const CStringA &plainPayload, CStringA &cipherPayload, CString &errorMessage);
	static bool DecryptBuffer(const BYTE key[32], const CStringA &cipherPayload, CString &contentType, CStringA &plainPayload, CString &errorMessage);
};
