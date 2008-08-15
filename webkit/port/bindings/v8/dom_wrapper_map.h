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
