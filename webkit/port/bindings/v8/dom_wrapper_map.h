// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BINDINGS_V8_DOM_WRAPPER_MAP
#define BINDINGS_V8_DOM_WRAPPER_MAP

#include <wtf/HashMap.h>

// A table of wrappers with weak pointers.
// This table allows us to avoid track wrapped objects for debugging
// and for ensuring that we don't double wrap the same object.
template<class KeyType, class ValueType>
class WeakReferenceMap {
 public:
  WeakReferenceMap(v8::WeakReferenceCallback callback) :
       weak_reference_callback_(callback) { }
#ifndef NDEBUG
  virtual ~WeakReferenceMap() {
    if (map_.size() > 0) {
      fprintf(stderr, "Leak %d JS wrappers.\n", map_.size());
      // Print out details.
    }
  }
#endif

  // Get the JS wrapper object of an object.
  virtual v8::Persistent<ValueType> get(KeyType* obj) {
    ValueType* wrapper = map_.get(obj);
    return wrapper ? v8::Persistent<ValueType>(wrapper)
      : v8::Persistent<ValueType>();
  }

  virtual void set(KeyType* obj, v8::Persistent<ValueType> wrapper) {
    ASSERT(!map_.contains(obj));
    wrapper.MakeWeak(obj, weak_reference_callback_);
    map_.set(obj, *wrapper);
  }

  virtual void forget(KeyType* obj) {
    ASSERT(obj);
    ValueType* wrapper = map_.take(obj);
    if (wrapper) {
      v8::Persistent<ValueType> handle(wrapper);
      handle.Dispose();
      handle.Clear();
    }
  }

  bool contains(KeyType* obj) {
    return map_.contains(obj);
  }

  HashMap<KeyType*, ValueType*>& impl() {
    return map_;
  }

 protected:
  HashMap<KeyType*, ValueType*> map_;
  v8::WeakReferenceCallback weak_reference_callback_;
};


template <class KeyType>
class DOMWrapperMap : public WeakReferenceMap<KeyType, v8::Object> {
 public:
  DOMWrapperMap(v8::WeakReferenceCallback callback) :
       WeakReferenceMap<KeyType, v8::Object>(callback) { }
};

#endif  // BINDINGS_V8_DOM_WRAPPER_MAP

