// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/common/pref_member.h"

#include "base/logging.h"
#include "chrome/common/pref_service.h"

namespace subtle {

PrefMemberBase::PrefMemberBase()
    : observer_(NULL),
      prefs_(NULL),
      is_synced_(false),
      setting_value_(false) {
}

PrefMemberBase::~PrefMemberBase() {
  if (!pref_name_.empty())
    prefs_->RemovePrefObserver(pref_name_.c_str(), this);
}


void PrefMemberBase::Init(const wchar_t* pref_name, PrefService* prefs,
          NotificationObserver* observer) {
  DCHECK(pref_name);
  DCHECK(prefs);
  DCHECK(pref_name_.empty());  // Check that Init is only called once.
  observer_ = observer;
  prefs_ = prefs;
  pref_name_ = pref_name;
  DCHECK(!pref_name_.empty());

  // Add ourself as a pref observer so we can keep our local value in sync.
  prefs_->AddPrefObserver(pref_name, this);
}

void PrefMemberBase::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(!pref_name_.empty());
  DCHECK(NOTIFY_PREF_CHANGED == type);
  UpdateValueFromPref();
  is_synced_ = true;
  if (!setting_value_ && observer_)
    observer_->Observe(type, source, details);
}

void PrefMemberBase::VerifyValuePrefName() {
  DCHECK(!pref_name_.empty());
}

}  // namespace subtle

void BooleanPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetBoolean(pref_name().c_str());
}

void BooleanPrefMember::UpdatePref(const bool& value) {
  prefs()->SetBoolean(pref_name().c_str(), value);
}

void IntegerPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetInteger(pref_name().c_str());
}

void IntegerPrefMember::UpdatePref(const int& value) {
  prefs()->SetInteger(pref_name().c_str(), value);
}

void RealPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetReal(pref_name().c_str());
}

void RealPrefMember::UpdatePref(const double& value) {
  prefs()->SetReal(pref_name().c_str(), value);
}

void StringPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetString(pref_name().c_str());
}

void StringPrefMember::UpdatePref(const std::wstring& value) {
  prefs()->SetString(pref_name().c_str(), value);
}
