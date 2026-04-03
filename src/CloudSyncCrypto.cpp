#include "stdafx.h"
#include "CloudSyncCrypto.h"
#include "Md5.h"
#include "Misc.h"
#include "..\Shared\TextConvert.h"

#include <bcrypt.h>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
	const char *kCloudSyncMagic = "DITTOCS1";
	const BYTE kCloudSyncVersion = 1;

	void AppendUInt32(std::vector<BYTE> &buffer, DWORD value)
	{
		buffer.push_back((BYTE)(value & 0xFF));
		buffer.push_back((BYTE)((value >> 8) & 0xFF));
		buffer.push_back((BYTE)((value >> 16) & 0xFF));
		buffer.push_back((BYTE)((value >> 24) & 0xFF));
	}

	bool ReadUInt32(const BYTE *data, size_t size, size_t &offset, DWORD &value)
	{
		if (offset + 4 > size)
		{
			return false;
		}

		value = (DWORD)data[offset] | ((DWORD)data[offset + 1] << 8) | ((DWORD)data[offset + 2] << 16) | ((DWORD)data[offset + 3] << 24);
		offset += 4;
		return true;
	}

	CStringA Base64Encode(const BYTE *data, int size)
	{
		DWORD outputChars = 0;
		if (CryptBinaryToStringA(data, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outputChars) == FALSE)
		{
			return CStringA();
		}

		CStringA encoded;
		LPSTR buffer = encoded.GetBuffer(outputChars);
		if (CryptBinaryToStringA(data, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, buffer, &outputChars) == FALSE)
		{
			encoded.ReleaseBuffer(0);
			return CStringA();
		}

		encoded.ReleaseBuffer();
		return encoded;
	}

	bool Base64Decode(const CStringA &encoded, std::vector<BYTE> &decoded)
	{
		decoded.clear();
		DWORD outputSize = 0;
		if (CryptStringToBinaryA(encoded, encoded.GetLength(), CRYPT_STRING_BASE64, NULL, &outputSize, NULL, NULL) == FALSE)
		{
			return false;
		}

		decoded.resize(outputSize);
		if (CryptStringToBinaryA(encoded, encoded.GetLength(), CRYPT_STRING_BASE64, decoded.data(), &outputSize, NULL, NULL) == FALSE)
		{
			decoded.clear();
			return false;
		}

		decoded.resize(outputSize);
		return true;
	}
}

bool CCloudSyncCrypto::EncryptMessage(const CString &roomCode, const CString &contentType, const CStringA &plainPayload, CStringA &cipherPayload, CString &errorMessage)
{
	BYTE key[32] = { 0 };
	if (DeriveKey(roomCode, key, errorMessage) == false)
	{
		return false;
	}

	return EncryptBuffer(key, contentType, plainPayload, cipherPayload, errorMessage);
}

bool CCloudSyncCrypto::DecryptMessage(const CString &roomCode, const SupabaseSyncMessage &message, CString &contentType, CStringA &plainPayload, CString &errorMessage)
{
	CString payloadMode = message.m_payloadMode;
	payloadMode.Trim();
	payloadMode.MakeLower();
	if (payloadMode != _T("inline"))
	{
		errorMessage = _T("Unsupported cloud payload mode.");
		return false;
	}

	if (message.m_payloadInline.Left((int)strlen(kCloudSyncMagic)) != kCloudSyncMagic)
	{
		errorMessage = _T("Cloud payload is not encrypted.");
		return false;
	}

	BYTE key[32] = { 0 };
	if (DeriveKey(roomCode, key, errorMessage) == false)
	{
		return false;
	}

	return DecryptBuffer(key, message.m_payloadInline, contentType, plainPayload, errorMessage);
}

bool CCloudSyncCrypto::ComputePayloadHash(const CString &roomCode, const CString &contentType, const CStringA &plainPayload, CString &payloadHash, CString &errorMessage)
{
	payloadHash.Empty();

	BYTE key[32] = { 0 };
	if (DeriveKey(roomCode, key, errorMessage) == false)
	{
		return false;
	}

	CStringA utf8ContentType = CTextConvert::UnicodeToUTF8(contentType);
	std::vector<BYTE> hashInput;
	hashInput.insert(hashInput.end(), key, key + 32);
	hashInput.insert(hashInput.end(), (const BYTE *)(LPCSTR)utf8ContentType, (const BYTE *)(LPCSTR)utf8ContentType + utf8ContentType.GetLength());
	hashInput.push_back('|');
	hashInput.insert(hashInput.end(), (const BYTE *)(LPCSTR)plainPayload, (const BYTE *)(LPCSTR)plainPayload + plainPayload.GetLength());

	CMd5 md5;
	CStringA digest = md5.CalcMD5FromString((const char *)hashInput.data(), (int)hashInput.size());
	payloadHash = CTextConvert::AnsiToUnicode(digest);
	errorMessage.Empty();
	return true;
}

bool CCloudSyncCrypto::DeriveKey(const CString &roomCode, BYTE key[32], CString &errorMessage)
{
	CString normalizedRoomCode = roomCode;
	normalizedRoomCode.Trim();
	if (normalizedRoomCode.IsEmpty())
	{
		errorMessage = _T("Room code is required for cloud sync encryption.");
		return false;
	}

	CStringA utf8 = CTextConvert::UnicodeToUTF8(normalizedRoomCode + _T("|ditto-cloud-sync-key-v1"));
	BCRYPT_ALG_HANDLE hAlg = NULL;
	if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0)
	{
		errorMessage = _T("Failed to open SHA-256 provider.");
		return false;
	}

	DWORD hashObjectSize = 0;
	DWORD bytesCopied = 0;
	if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectSize, sizeof(hashObjectSize), &bytesCopied, 0) != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to query SHA-256 object length.");
		return false;
	}

	std::vector<BYTE> hashObject(hashObjectSize);
	BCRYPT_HASH_HANDLE hHash = NULL;
	if (BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, NULL, 0, 0) != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to create SHA-256 hash.");
		return false;
	}

	NTSTATUS status = BCryptHashData(hHash, (PUCHAR)(LPCSTR)utf8, utf8.GetLength(), 0);
	if (status == 0)
	{
		status = BCryptFinishHash(hHash, key, 32, 0);
	}

	BCryptDestroyHash(hHash);
	BCryptCloseAlgorithmProvider(hAlg, 0);

	if (status != 0)
	{
		errorMessage = _T("Failed to derive cloud sync key.");
		return false;
	}

	errorMessage.Empty();
	return true;
}

bool CCloudSyncCrypto::EncryptBuffer(const BYTE key[32], const CString &contentType, const CStringA &plainPayload, CStringA &cipherPayload, CString &errorMessage)
{
	BCRYPT_ALG_HANDLE hAlg = NULL;
	BCRYPT_KEY_HANDLE hKey = NULL;
	NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
	if (status != 0)
	{
		errorMessage = _T("Failed to open AES provider.");
		return false;
	}

	status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	DWORD keyObjectSize = 0;
	DWORD bytesCopied = 0;
	if (status == 0)
	{
		status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjectSize, sizeof(keyObjectSize), &bytesCopied, 0);
	}
	if (status != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to initialize AES-GCM provider.");
		return false;
	}

	std::vector<BYTE> keyObject(keyObjectSize);
	status = BCryptGenerateSymmetricKey(hAlg, &hKey, keyObject.data(), keyObjectSize, (PUCHAR)key, 32, 0);
	if (status != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to create AES key.");
		return false;
	}

	BYTE nonce[12] = { 0 };
	if (BCryptGenRandom(NULL, nonce, sizeof(nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
	{
		BCryptDestroyKey(hKey);
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to create AES nonce.");
		return false;
	}

	CStringA utf8ContentType = CTextConvert::UnicodeToUTF8(contentType);
	std::vector<BYTE> plainEnvelope;
	AppendUInt32(plainEnvelope, (DWORD)utf8ContentType.GetLength());
	plainEnvelope.insert(plainEnvelope.end(), (const BYTE *)(LPCSTR)utf8ContentType, (const BYTE *)(LPCSTR)utf8ContentType + utf8ContentType.GetLength());
	plainEnvelope.insert(plainEnvelope.end(), (const BYTE *)(LPCSTR)plainPayload, (const BYTE *)(LPCSTR)plainPayload + plainPayload.GetLength());

	BYTE tag[16] = { 0 };
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
	BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
	authInfo.pbNonce = nonce;
	authInfo.cbNonce = sizeof(nonce);
	authInfo.pbTag = tag;
	authInfo.cbTag = sizeof(tag);
	authInfo.pbAuthData = (PUCHAR)kCloudSyncMagic;
	authInfo.cbAuthData = (ULONG)strlen(kCloudSyncMagic);

	ULONG cipherSize = 0;
	status = BCryptEncrypt(hKey, plainEnvelope.data(), (ULONG)plainEnvelope.size(), &authInfo, NULL, 0, NULL, 0, &cipherSize, 0);
	if (status != 0)
	{
		BCryptDestroyKey(hKey);
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to size AES ciphertext.");
		return false;
	}

	std::vector<BYTE> cipher(cipherSize);
	status = BCryptEncrypt(hKey, plainEnvelope.data(), (ULONG)plainEnvelope.size(), &authInfo, NULL, 0, cipher.data(), cipherSize, &cipherSize, 0);
	BCryptDestroyKey(hKey);
	BCryptCloseAlgorithmProvider(hAlg, 0);
	if (status != 0)
	{
		errorMessage = _T("Failed to encrypt cloud payload.");
		return false;
	}

	cipher.resize(cipherSize);
	std::vector<BYTE> packed;
	packed.insert(packed.end(), kCloudSyncMagic, kCloudSyncMagic + strlen(kCloudSyncMagic));
	packed.push_back(kCloudSyncVersion);
	packed.insert(packed.end(), nonce, nonce + sizeof(nonce));
	packed.insert(packed.end(), tag, tag + sizeof(tag));
	AppendUInt32(packed, (DWORD)cipher.size());
	packed.insert(packed.end(), cipher.begin(), cipher.end());

	CStringA base64 = Base64Encode(packed.data(), (int)packed.size());
	if (base64.IsEmpty())
	{
		errorMessage = _T("Failed to encode encrypted cloud payload.");
		return false;
	}

	cipherPayload = CStringA(kCloudSyncMagic) + base64;
	errorMessage.Empty();
	return true;
}

bool CCloudSyncCrypto::DecryptBuffer(const BYTE key[32], const CStringA &cipherPayload, CString &contentType, CStringA &plainPayload, CString &errorMessage)
{
	CStringA encoded = cipherPayload.Mid((int)strlen(kCloudSyncMagic));
	std::vector<BYTE> packed;
	if (Base64Decode(encoded, packed) == false)
	{
		errorMessage = _T("Failed to decode encrypted cloud payload.");
		return false;
	}

	size_t offset = 0;
	if (packed.size() < strlen(kCloudSyncMagic) + 1 + 12 + 16 + 4)
	{
		errorMessage = _T("Encrypted cloud payload is too small.");
		return false;
	}

	if (memcmp(packed.data(), kCloudSyncMagic, strlen(kCloudSyncMagic)) != 0)
	{
		errorMessage = _T("Encrypted cloud payload magic header is invalid.");
		return false;
	}
	offset += strlen(kCloudSyncMagic);

	BYTE version = packed[offset++];
	if (version != kCloudSyncVersion)
	{
		errorMessage = _T("Encrypted cloud payload version is not supported.");
		return false;
	}

	if (offset + 12 + 16 > packed.size())
	{
		errorMessage = _T("Encrypted cloud payload nonce or tag is invalid.");
		return false;
	}

	BYTE *nonce = packed.data() + offset;
	offset += 12;
	BYTE *tag = packed.data() + offset;
	offset += 16;

	DWORD cipherSize = 0;
	if (ReadUInt32(packed.data(), packed.size(), offset, cipherSize) == false || offset + cipherSize > packed.size())
	{
		errorMessage = _T("Encrypted cloud payload ciphertext is invalid.");
		return false;
	}

	BYTE *cipherData = packed.data() + offset;

	BCRYPT_ALG_HANDLE hAlg = NULL;
	BCRYPT_KEY_HANDLE hKey = NULL;
	NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
	if (status != 0)
	{
		errorMessage = _T("Failed to open AES provider.");
		return false;
	}

	status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	DWORD keyObjectSize = 0;
	DWORD bytesCopied = 0;
	if (status == 0)
	{
		status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjectSize, sizeof(keyObjectSize), &bytesCopied, 0);
	}
	if (status != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to initialize AES-GCM provider.");
		return false;
	}

	std::vector<BYTE> keyObject(keyObjectSize);
	status = BCryptGenerateSymmetricKey(hAlg, &hKey, keyObject.data(), keyObjectSize, (PUCHAR)key, 32, 0);
	if (status != 0)
	{
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to create AES key.");
		return false;
	}

	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
	BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
	authInfo.pbNonce = nonce;
	authInfo.cbNonce = 12;
	authInfo.pbTag = tag;
	authInfo.cbTag = 16;
	authInfo.pbAuthData = (PUCHAR)kCloudSyncMagic;
	authInfo.cbAuthData = (ULONG)strlen(kCloudSyncMagic);

	ULONG plainSize = 0;
	status = BCryptDecrypt(hKey, cipherData, cipherSize, &authInfo, NULL, 0, NULL, 0, &plainSize, 0);
	if (status != 0)
	{
		BCryptDestroyKey(hKey);
		BCryptCloseAlgorithmProvider(hAlg, 0);
		errorMessage = _T("Failed to size decrypted cloud payload.");
		return false;
	}

	std::vector<BYTE> plainEnvelope(plainSize + 1);
	status = BCryptDecrypt(hKey, cipherData, cipherSize, &authInfo, NULL, 0, plainEnvelope.data(), plainSize, &plainSize, 0);
	BCryptDestroyKey(hKey);
	BCryptCloseAlgorithmProvider(hAlg, 0);
	if (status != 0)
	{
		errorMessage = _T("Failed to decrypt cloud payload.");
		return false;
	}

	plainEnvelope[plainSize] = 0;
	offset = 0;
	DWORD contentTypeLength = 0;
	if (ReadUInt32(plainEnvelope.data(), plainSize, offset, contentTypeLength) == false || offset + contentTypeLength > plainSize)
	{
		errorMessage = _T("Encrypted cloud payload content type is invalid.");
		return false;
	}

	CStringA utf8ContentType((LPCSTR)(plainEnvelope.data() + offset), contentTypeLength);
	contentType = CTextConvert::Utf8ToUnicode(utf8ContentType);
	offset += contentTypeLength;

	plainPayload = CStringA((LPCSTR)(plainEnvelope.data() + offset), (int)(plainSize - offset));
	errorMessage.Empty();
	return true;
}
