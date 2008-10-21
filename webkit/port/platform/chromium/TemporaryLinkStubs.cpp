/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "base/compiler_specific.h"

#define WIN32_COMPILE_HACK

MSVC_PUSH_WARNING_LEVEL(0);
#include "Color.h"
#include "SSLKeyGenerator.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
MSVC_POP_WARNING();

using namespace WebCore;

Color WebCore::focusRingColor() { notImplemented(); return 0xFF7DADD9; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { notImplemented(); }

String WebCore::signedPublicKeyAndChallengeString(unsigned, const String&, const KURL&) { notImplemented(); return String(); }

String KURL::fileSystemPath() const { notImplemented(); return String(); }

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    notImplemented();
    return 0;
}

namespace WTF {
void scheduleDispatchFunctionsOnMainThread() { notImplemented(); }
}

#if USE(JSC)
#include "c_instance.h"
#include "Database.h"
#include "DatabaseAuthorizer.h"
#include "Document.h"
#include "DOMWindow.h"
#include "EventLoop.h"
#include "JSStorageCustom.h"
#include "PluginView.h"
#include "SQLResultSet.h"
#include "SQLTransaction.h"
#include "SQLValue.h"
#include "Storage.h"
#include "StorageEvent.h"
#include "TimeRanges.h"

using namespace KJS;

Database::~Database() { notImplemented(); }
String Database::version() const { notImplemented(); return String(); }
void Database::transaction(PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback, PassRefPtr<VoidCallback> successCallback) { notImplemented(); }
void Database::changeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<SQLTransactionCallback> callback, PassRefPtr<SQLTransactionErrorCallback> errorCallback, PassRefPtr<VoidCallback> successCallback) { notImplemented(); }

void EventLoop::cycle() { notImplemented(); }

bool JSStorage::canGetItemsForName(ExecState*, Storage* impl, const Identifier& propertyName) { notImplemented(); return false; }
JSValue* JSStorage::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot) { notImplemented(); return 0; }
bool JSStorage::deleteProperty(ExecState* exec, const Identifier& propertyName) { notImplemented(); return false; }
bool JSStorage::customPut(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot&) { notImplemented(); return false; }
bool JSStorage::customGetPropertyNames(ExecState* exec, PropertyNameArray& propertyNames) { notImplemented(); return false; }

void PluginView::setJavaScriptPaused(bool) { notImplemented(); }
PassRefPtr<KJS::Bindings::Instance> PluginView::bindingInstance() { notImplemented(); return PassRefPtr<KJS::Bindings::Instance>(0); }

SQLiteDatabase::~SQLiteDatabase() { notImplemented(); }
SQLiteTransaction::~SQLiteTransaction() { notImplemented(); }

int64_t SQLResultSet::insertId(ExceptionCode&) const { notImplemented(); return 0; }
int SQLResultSet::rowsAffected() const { notImplemented(); return 0; }
SQLResultSetRowList* SQLResultSet::rows() const { notImplemented(); return 0; }

unsigned int SQLResultSetRowList::length() const { notImplemented(); return 0; }

SQLTransaction::~SQLTransaction() { notImplemented(); }
void SQLTransaction::executeSQL(const String&, const Vector<SQLValue>&, PassRefPtr<SQLStatementCallback>, PassRefPtr<SQLStatementErrorCallback>, ExceptionCode&) { notImplemented(); }

SQLValue::SQLValue(class WebCore::SQLValue const &) {}
String SQLValue::string() const { return String(); }
double SQLValue::number() const { return 0.0; }

unsigned Storage::length() const { notImplemented(); return 0; }
String Storage::key(unsigned index, ExceptionCode&) const { notImplemented(); return String(); }
String Storage::getItem(const String&) const { notImplemented(); return String(); }
void Storage::setItem(const String& key, const String& value, ExceptionCode&) { notImplemented(); }
void Storage::removeItem(const String&) { notImplemented(); }
void Storage::clear() { notImplemented(); }

void StorageEvent::initStorageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& key, const String& oldValue, const String& newValue, const String& uri, PassRefPtr<DOMWindow> source) { notImplemented(); }

float TimeRanges::start(unsigned int, int&) const { notImplemented(); return 0.0; }
float TimeRanges::end(unsigned int, int&) const { notImplemented(); return 0.0; }

#endif
