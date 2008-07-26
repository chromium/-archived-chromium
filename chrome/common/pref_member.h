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
//
// A helper class that stays in sync with a preference (bool, int, real, or
// string).  For example:
//
// class MyClass {
//  public:
//   MyClass(PrefService* prefs) {
//     my_string_.Init(prefs::kHomePage, prefs, NULL /* no observer */);
//   }
//  private:
//   StringPrefMember my_string_;
// };
//
// my_string_ should stay in sync with the prefs::kHomePage pref and will
// update if either the pref changes or if my_string_.SetValue is called.
//
// An optional observer can be passed into the Init method which can be used to
// notify MyClass of changes.

#ifndef CHROME_COMMON_PREF_MEMBER_H__
#define CHROME_COMMON_PREF_MEMBER_H__

#include <string>

#include "base/logging.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_service.h"

template <typename ValueType>
class PrefMember : public NotificationObserver {
 public:
  // Defer initialization to an Init method so it's easy to make this class be
  // a member variable.
  PrefMember() : observer_(NULL), prefs_(NULL), is_synced_(false),
                 setting_value_(false) {}

  // Do the actual initialization of the class.  |observer| may be null if you
  // don't want any notifications of changes.
  void Init(const wchar_t* pref_name, PrefService* prefs,
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

  virtual ~PrefMember() {
    if (!pref_name_.empty())
      prefs_->RemovePrefObserver(pref_name_.c_str(), this);
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(!pref_name_.empty());
    DCHECK(NOTIFY_PREF_CHANGED == type);
    UpdateValueFromPref();
    is_synced_ = true;
    if (!setting_value_ && observer_)
      observer_->Observe(type, source, details);
  }

  // Retrieve the value of the member variable.
  ValueType GetValue() {
    DCHECK(!pref_name_.empty());
    // We lazily fetch the value from the pref service the first time GetValue
    // is called.
    if (!is_synced_) {
      UpdateValueFromPref();
      is_synced_ = true;
    }
    return value_;
  }

  // Provided as a convenience.
  ValueType operator*() {
    return GetValue();
  }

  // Set the value of the member variable.
  void SetValue(const ValueType& value) {
    DCHECK(!pref_name_.empty());
    setting_value_ = true;
    UpdatePref(value);
    setting_value_ = false;
  }

 protected:
  // These methods are used to do the actual sync with pref of the specified
  // type.
  virtual void UpdateValueFromPref() = 0;
  virtual void UpdatePref(const ValueType& value) = 0;

  std::wstring pref_name_;
  PrefService* prefs_;

  // We cache the value of the pref so we don't have to keep walking the pref
  // tree.
  ValueType value_;

 private:
  NotificationObserver* observer_;
  bool is_synced_;
  bool setting_value_;
};

///////////////////////////////////////////////////////////////////////////////
// Implementations of Boolean, Integer, Real, and String PrefMember below.

class BooleanPrefMember : public PrefMember<bool> {
 public:
  BooleanPrefMember() : PrefMember() { }
  virtual ~BooleanPrefMember() { }

 protected:
  virtual void UpdateValueFromPref() {
    value_ = prefs_->GetBoolean(pref_name_.c_str());
  }

  virtual void UpdatePref(const bool& value) {
    prefs_->SetBoolean(pref_name_.c_str(), value);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(BooleanPrefMember);
};

class IntegerPrefMember : public PrefMember<int> {
 public:
  IntegerPrefMember() : PrefMember() { }
  virtual ~IntegerPrefMember() { }

 protected:
  virtual void UpdateValueFromPref() {
    value_ = prefs_->GetInteger(pref_name_.c_str());
  }

  virtual void UpdatePref(const int& value) {
    prefs_->SetInteger(pref_name_.c_str(), value);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(IntegerPrefMember);
};

class RealPrefMember : public PrefMember<double> {
 public:
  RealPrefMember() : PrefMember() { }
  virtual ~RealPrefMember() { }

 protected:
  virtual void UpdateValueFromPref() {
    value_ = prefs_->GetReal(pref_name_.c_str());
  }

  virtual void UpdatePref(const double& value) {
    prefs_->SetReal(pref_name_.c_str(), value);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(RealPrefMember);
};

class StringPrefMember : public PrefMember<std::wstring> {
 public:
  StringPrefMember() : PrefMember() { }
  virtual ~StringPrefMember() { }

 protected:
  virtual void UpdateValueFromPref() {
    value_ = prefs_->GetString(pref_name_.c_str());
  }

  virtual void UpdatePref(const std::wstring& value) {
    prefs_->SetString(pref_name_.c_str(), value);
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(StringPrefMember);
};

#endif  // CHROME_COMMON_PREF_MEMBER_H__
