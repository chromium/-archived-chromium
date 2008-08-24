// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_COMPLETION_CALLBACK_H__
#define NET_BASE_COMPLETION_CALLBACK_H__

#include "base/task.h"

namespace net {

// A callback specialization that takes a single int parameter.  Usually this
// is used to report a byte count or network error code.
typedef Callback1<int>::Type CompletionCallback;

// Used to implement a CompletionCallback.
template <class T>
class CompletionCallbackImpl :
    public CallbackImpl< T, void (T::*)(int), Tuple1<int> > {
 public:
  CompletionCallbackImpl(T* obj, void (T::* meth)(int))
    : CallbackImpl< T, void (T::*)(int), Tuple1<int> >::CallbackImpl(obj, meth) {
  }
};

}  // namespace net

#endif  // NET_BASE_COMPLETION_CALLBACK_H__

