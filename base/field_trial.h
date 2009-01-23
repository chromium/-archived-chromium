// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FieldTrial is a class for handling details of statistical experiments
// performed by actual users in the field (i.e., in a shipped or beta product).
// All code is called exclusively on the UI thread currently.
//
// The simplest example is a test to see whether one of two options produces
// "better" results across our user population.  In that scenario, UMA data
// is uploaded to show the test results, and this class manages the state of
// each such test (state == which option was pseudo-randomly selected).
// States are typically generated randomly, either based on a one time
// randomization (reused during each run of the program), or by a startup
// randomization (keeping that tests state constant across a run), or by
// continuous randomization across a run.
// Only startup randomization is implemented (thus far).

#ifndef BASE_FIELD_TRIAL_H_
#define BASE_FIELD_TRIAL_H_

#include <map>
#include <string>

#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "base/time.h"

class FieldTrial : public base::RefCounted<FieldTrial> {
 public:
  // Constructor for a 2-state (boolean) trial.
  // The name is used to register the instance with the FieldTrialList class,
  // and can be used to find the trial (only one trial can be present for each
  // name) using the Find() method.
  // The probability is a number in the range [0, 1], and is the likliehood that
  // the assigned boolean value will be true.
  FieldTrial(const std::wstring& name, double probability);

  // Return the selected boolean value.
  bool boolean_value() const { return boolean_value_; }
  std::wstring name() const { return name_; }

 private:
  const std::wstring name_;
  bool boolean_value_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrial);
};

// Class with a list of all active field trials.  A trial is active if it has
// been registered, which includes evaluating its state based on its probaility.
// Only one instance of this class exists.
class FieldTrialList : NonThreadSafe {
 public:
  // This singleton holds the global list of registered FieldTrials.
  FieldTrialList();
  // Destructor Release()'s references to all registered FieldTrial instances.
  ~FieldTrialList();

  // Register() stores a pointer to the given trial in a global map.
  // This method also AddRef's the indicated trial.
  static void Register(FieldTrial* trial);

  // The Find() method can be used to test to see if a named Trial was already
  // registered, or to retrieve a pointer to it from the global map.
  static FieldTrial* Find(const std::wstring& name);

  // The time of construction of the global map is recorded in a static variable
  // and is commonly used by experiments to identify the time since the start
  // of the application.  In some experiments it may be useful to discount
  // data that is gathered before the application has reach sufficient
  // stability (example: most DLL have loaded, etc.)
  static base::Time application_start_time() {
    if (global_)
      return global_->application_start_time_;
    // For testing purposes only, or when we don't yet have a start time.
    return base::Time::Now();
  }

 private:
  typedef std::map<std::wstring, FieldTrial*> RegistrationList;

  static FieldTrialList* global_;  // The singleton of this class.

  base::Time application_start_time_;
  RegistrationList registered_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialList);
};

#endif  // BASE_FIELD_TRIAL_H_
