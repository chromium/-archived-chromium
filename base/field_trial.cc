// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/field_trial.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"

using base::Time;

// static
const int FieldTrial::kNotParticipating = -1;

// static
const int FieldTrial::kAllRemainingProbability = -2;

// static
const char FieldTrialList::kPersistentStringSeparator('/');

//------------------------------------------------------------------------------
// FieldTrial methods and members.

FieldTrial::FieldTrial(const std::string& name,
                       const Probability total_probability)
  : name_(name),
    divisor_(total_probability),
    random_(static_cast<Probability>(divisor_ * base::RandDouble())),
    accumulated_group_probability_(0),
    next_group_number_(0),
    group_(kNotParticipating) {
  FieldTrialList::Register(this);
}

int FieldTrial::AppendGroup(const std::string& name,
                            Probability group_probability) {
  DCHECK(group_probability <= divisor_);
  DCHECK(group_probability >=0 ||
         group_probability == kAllRemainingProbability);
  if (group_probability == kAllRemainingProbability)
    accumulated_group_probability_ = divisor_;
  else
    accumulated_group_probability_ += group_probability;
  DCHECK(accumulated_group_probability_ <= divisor_);
  if (group_ == kNotParticipating && accumulated_group_probability_ > random_) {
    // This is the group that crossed the random line, so we do the assignment.
    group_ = next_group_number_;
    if (name.empty())
      StringAppendF(&group_name_, "_%d", group_);
    else
      group_name_ = name;
  }
  return next_group_number_++;
}

// static
std::string FieldTrial::MakeName(const std::string& name_prefix,
                                 const std::string& trial_name) {
  std::string big_string(name_prefix);
  return big_string.append(FieldTrialList::FindFullName(trial_name));
}

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
  AutoLock auto_lock(lock_);
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
  DCHECK(global_);
  if (!global_)
    return;
  AutoLock auto_lock(global_->lock_);
  DCHECK(!global_->PreLockedFind(trial->name()));
  trial->AddRef();
  global_->registered_[trial->name()] = trial;
}

// static
int FieldTrialList::FindValue(const std::string& name) {
  FieldTrial* field_trial = Find(name);
  if (field_trial)
    return field_trial->group();
  return FieldTrial::kNotParticipating;
}

// static
std::string FieldTrialList::FindFullName(const std::string& name) {
  FieldTrial* field_trial = Find(name);
  if (field_trial)
    return field_trial->group_name();
  return "";
}

// static
FieldTrial* FieldTrialList::Find(const std::string& name) {
  if (!global_)
    return NULL;
  AutoLock auto_lock(global_->lock_);
  return global_->PreLockedFind(name);
}

FieldTrial* FieldTrialList::PreLockedFind(const std::string& name) {
  RegistrationList::iterator it = registered_.find(name);
  if (registered_.end() == it)
    return NULL;
  return it->second;
}

// static
void FieldTrialList::StatesToString(std::string* output) {
  if (!global_)
    return;
  DCHECK(output->empty());
  for (RegistrationList::iterator it = global_->registered_.begin();
       it != global_->registered_.end(); ++it) {
    const std::string name = it->first;
    const std::string group_name = it->second->group_name();
    if (group_name.empty())
      continue;  // No definitive winner in this trial.
    DCHECK_EQ(name.find(kPersistentStringSeparator), std::string::npos);
    DCHECK_EQ(group_name.find(kPersistentStringSeparator), std::string::npos);
    output->append(name);
    output->append(1, kPersistentStringSeparator);
    output->append(group_name);
    output->append(1, kPersistentStringSeparator);
  }
}

// static
bool FieldTrialList::StringAugmentsState(const std::string& prior_state) {
  DCHECK(global_);
  if (prior_state.empty() || !global_)
    return true;

  size_t next_item = 0;
  while (next_item < prior_state.length()) {
    size_t name_end = prior_state.find(kPersistentStringSeparator, next_item);
    if (name_end == prior_state.npos || next_item == name_end)
      return false;
    size_t group_name_end = prior_state.find(kPersistentStringSeparator,
                                             name_end + 1);
    if (group_name_end == prior_state.npos || name_end + 1 == group_name_end)
      return false;
    std::string name(prior_state, next_item, name_end - next_item);
    std::string group_name(prior_state, name_end + 1,
                           group_name_end - name_end - 1);
    next_item = group_name_end + 1;

    FieldTrial *field_trial(FieldTrialList::Find(name));
    if (field_trial) {
      // In single process mode, we may have already created the field trial.
      if (field_trial->group_name() != group_name)
        return false;
      continue;
    }
    const int kTotalProbability = 100;
    field_trial = new FieldTrial(name, kTotalProbability);
    field_trial->AppendGroup(group_name, kTotalProbability);
  }
  return true;
}

