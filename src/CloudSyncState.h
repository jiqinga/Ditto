#pragma once

#include "sqlite/CppSQLite3.h"

class CCloudSyncState
{
public:
	static BOOL EnsureSchema(CppSQLite3DB &db);
	static BOOL HasMessage(CppSQLite3DB &db, const CString &scope, const CString &messageId);
	static BOOL HasPayloadHash(CppSQLite3DB &db, const CString &scope, const CString &payloadHash);
	static BOOL UpsertMessageState(CppSQLite3DB &db, const CString &scope, const CString &messageId, const CString &senderDeviceId, const CString &payloadHash, const CString &status);
	static CString GetLastCursor(CppSQLite3DB &db, const CString &scope);
	static BOOL SetLastCursor(CppSQLite3DB &db, const CString &scope, const CString &cursor);
	static BOOL ResetState(CppSQLite3DB &db, const CString &scope);
};
