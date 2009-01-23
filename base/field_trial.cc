// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/field_trial.h"
#include "base/logging.h"
#include "base/rand_util.h"

using base::Time;

//------------------------------------------------------------------------------
// FieldTrialList methods and members.

// static
FieldTrialList* FieldTrialList::global_ = NULL;

FieldTrialList::FieldTrialList()
  : application_start_time_(Time::Now()) {
  DCHECK(!global_);
  global_ = this;
}

FieldTrialList::~FieldTrialList() {
  while (!registered_.empty()) {
    RegistrationList::iterator it = registered_.begin();
    it->second->Release();
    registered_.erase(it->first);
  }
  DCHECK(this == global_);
  global_ = NULL;
}

// static
void FieldTrialList::Register(FieldTrial* trial) {
  DCHECK(global_->CalledOnValidThread());
  DCHECK(!Find(trial->name()));
  trial->AddRef();
  global_->registered_[trial->name()] = trial;
}

// static
FieldTrial* FieldTrialList::Find(const std::wstring& name) {
  DCHECK(global_->CalledOnValidThread());
  RegistrationList::iterator it = global_->registered_.find(name);
  if (global_->registered_.end() == it)
    return NULL;
  return it->second;
}

//------------------------------------------------------------------------------
// FieldTrial methods and members.

FieldTrial::FieldTrial(const std::wstring& name, double probability)
  : name_(name) {
  double rand = base::RandDouble();
  DCHECK(rand >= 0.0  && rand < 1.0);
  boolean_value_ = rand < probability;
  FieldTrialList::Register(this);
}
