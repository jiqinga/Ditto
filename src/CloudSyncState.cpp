#include "stdafx.h"
#include "CloudSyncState.h"
#include "Misc.h"

namespace
{
	CString EscapeSqlText(const CString &val)
	{
		CString escaped(val);
		escaped.Replace(_T("'"), _T("''"));
		return escaped;
	}
}

BOOL CCloudSyncState::EnsureSchema(CppSQLite3DB &db)
{
	try
	{
		db.execDML(_T("CREATE TABLE IF NOT EXISTS CloudSyncState(")
			_T("scope TEXT NOT NULL, ")
			_T("messageId TEXT NOT NULL, ")
			_T("senderDeviceId TEXT, ")
			_T("payloadHash TEXT, ")
			_T("status TEXT, ")
			_T("receivedAt TEXT DEFAULT CURRENT_TIMESTAMP, ")
			_T("appliedAt TEXT, ")
			_T("lastCursor TEXT, ")
			_T("PRIMARY KEY(scope, messageId))"));

		db.execDML(_T("CREATE INDEX IF NOT EXISTS CloudSyncState_ScopePayloadHash ON CloudSyncState(scope, payloadHash)"));
		db.execDML(_T("CREATE INDEX IF NOT EXISTS CloudSyncState_ScopeLastCursor ON CloudSyncState(scope, lastCursor)"));
	}
	CATCH_SQLITE_EXCEPTION_AND_RETURN(FALSE)

	return TRUE;
}

BOOL CCloudSyncState::HasMessage(CppSQLite3DB &db, const CString &scope, const CString &messageId)
{
	try
	{
		CString escapedScope = EscapeSqlText(scope);
		CString escapedMessageId = EscapeSqlText(messageId);
		CppSQLite3Query q = db.execQueryEx(_T("SELECT messageId FROM CloudSyncState WHERE scope = '%s' AND messageId = '%s' LIMIT 1"), escapedScope, escapedMessageId);
		return (q.eof() == false);
	}
	CATCH_SQLITE_EXCEPTION_AND_RETURN(FALSE)
}

BOOL CCloudSyncState::HasPayloadHash(CppSQLite3DB &db, const CString &scope, const CString &payloadHash)
{
	if (payloadHash.IsEmpty())
	{
		return FALSE;
	}

	try
	{
		CString escapedScope = EscapeSqlText(scope);
		CString escapedPayloadHash = EscapeSqlText(payloadHash);
		CppSQLite3Query q = db.execQueryEx(_T("SELECT payloadHash FROM CloudSyncState WHERE scope = '%s' AND payloadHash = '%s' LIMIT 1"), escapedScope, escapedPayloadHash);
		return (q.eof() == false);
	}
	CATCH_SQLITE_EXCEPTION_AND_RETURN(FALSE)
}

BOOL CCloudSyncState::UpsertMessageState(CppSQLite3DB &db, const CString &scope, const CString &messageId, const CString &senderDeviceId, const CString &payloadHash, const CString &status)
{
	try
	{
		CString escapedScope = EscapeSqlText(scope);
		CString escapedMessageId = EscapeSqlText(messageId);
		CString escapedSenderDeviceId = EscapeSqlText(senderDeviceId);
		CString escapedPayloadHash = EscapeSqlText(payloadHash);
		CString escapedStatus = EscapeSqlText(status);

		db.execDMLEx(_T("INSERT OR REPLACE INTO CloudSyncState(scope, messageId, senderDeviceId, payloadHash, status, receivedAt, appliedAt, lastCursor) ")
			_T("VALUES('%s', '%s', '%s', '%s', '%s', COALESCE((SELECT receivedAt FROM CloudSyncState WHERE scope = '%s' AND messageId = '%s'), CURRENT_TIMESTAMP), ")
			_T("CASE WHEN '%s' = 'applied' THEN CURRENT_TIMESTAMP ELSE (SELECT appliedAt FROM CloudSyncState WHERE scope = '%s' AND messageId = '%s') END, ")
			_T("(SELECT lastCursor FROM CloudSyncState WHERE scope = '%s' AND messageId = '%s'))"),
			escapedScope,
			escapedMessageId,
			escapedSenderDeviceId,
			escapedPayloadHash,
			escapedStatus,
			escapedScope,
			escapedMessageId,
			escapedStatus,
			escapedScope,
			escapedMessageId,
			escapedScope,
			escapedMessageId);
	}
	CATCH_SQLITE_EXCEPTION_AND_RETURN(FALSE)

	return TRUE;
}

CString CCloudSyncState::GetLastCursor(CppSQLite3DB &db, const CString &scope)
{
	try
	{
		CString escapedScope = EscapeSqlText(scope);
		CppSQLite3Query q = db.execQueryEx(_T("SELECT lastCursor FROM CloudSyncState WHERE scope = '%s' AND messageId = '__cursor__' LIMIT 1"), escapedScope);
		if (q.eof() == false)
		{
			return q.getStringField(0, _T(""));
		}
	}
	CATCH_SQLITE_EXCEPTION

	return _T("");
}

BOOL CCloudSyncState::SetLastCursor(CppSQLite3DB &db, const CString &scope, const CString &cursor)
{
	try
	{
		CString escapedScope = EscapeSqlText(scope);
		CString escapedCursor = EscapeSqlText(cursor);
		db.execDMLEx(_T("INSERT OR REPLACE INTO CloudSyncState(scope, messageId, senderDeviceId, payloadHash, status, receivedAt, appliedAt, lastCursor) ")
			_T("VALUES('%s', '__cursor__', '', '', 'cursor', COALESCE((SELECT receivedAt FROM CloudSyncState WHERE scope = '%s' AND messageId = '__cursor__'), CURRENT_TIMESTAMP), ")
			_T("(SELECT appliedAt FROM CloudSyncState WHERE scope = '%s' AND messageId = '__cursor__'), '%s')"),
			escapedScope,
			escapedScope,
			escapedScope,
			escapedCursor);
	}
	CATCH_SQLITE_EXCEPTION_AND_RETURN(FALSE)

	return TRUE;
}

BOOL CCloudSyncState::ResetState(CppSQLite3DB &db, const CString &scope)
{
	try
	{
		CString escapedScope = EscapeSqlText(scope);
		db.execDMLEx(_T("DELETE FROM CloudSyncState WHERE scope = '%s'"), escapedScope);
	}
	CATCH_SQLITE_EXCEPTION_AND_RETURN(FALSE)

	return TRUE;
}
