// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_COMMON_SCOPEDPTR_H_
#define BAR_COMMON_SCOPEDPTR_H_

//  Boxer for dumb types, allows you to associate cleanup code when the object
//  falls off the stack. Destructor implementation must be provided for each
//  type.
template < class T >
class ScopedObject {
 public:
  explicit ScopedObject(const T& v) : v_(v) { }
  ~ScopedObject();

  operator T() const { return v_; }
  T get() const { return v_; }

 private:
  T v_;

  DISALLOW_COPY_AND_ASSIGN(ScopedObject);
};

// A scoped object for the various HANDLE- and LPVOID-based types.
// destroy() implementation must be provided for each type.
// Added by Breen Hagan of Google.
template < class T, int DIFFERENTIATOR >
class ScopedHandle {
 public:
  explicit ScopedHandle(const T& v) : v_(v) {}
  ~ScopedHandle() {
    destroy();
  }

  operator T() const { return v_; }
  T get() const { return v_; }

  void reset(const T& v) {
    if (v_ != v) {
      destroy();
      v_ = v;
    }
  }

  // Swap two scoped handlers.
  void swap(ScopedHandle& h2) {
    T tmp = v_;
    v_ = h2.v_;
    h2.v_ = tmp;
  }

  T release() {
    T released_value(v_);
    v_ = 0;
    return released_value;
  }

 private:
  void destroy();

  T v_;

  DISALLOW_COPY_AND_ASSIGN(ScopedHandle);
};

// Free functions.
template <class T, int DIFFERENTIATOR>
inline void swap(ScopedHandle<T, DIFFERENTIATOR>& h1,
                 ScopedHandle<T, DIFFERENTIATOR>& h2) {
  h1.swap(h2);
}


// Uses ScopedHandle to automatically call CloseHandle().
typedef ScopedHandle< HANDLE, 1 > SAFE_HANDLE;

template <>
inline void ScopedHandle< HANDLE, 1 >::destroy() {
  if (v_)
    ::CloseHandle(v_);
}

// Uses ScopedHandle to automatically call CryptReleaseContext().
typedef ScopedHandle< HCRYPTPROV, 2 > SAFE_HCRYPTPROV;

template <>
inline void ScopedHandle< HCRYPTPROV, 2 >::destroy() {
  if (v_)
    ::CryptReleaseContext(v_, 0);
}

// Uses ScopedHandle to automatically call CryptDestroyKey().
typedef ScopedHandle< HCRYPTKEY, 3 > SAFE_HCRYPTKEY;

template <>
inline void ScopedHandle< HCRYPTKEY, 3 >::destroy() {
  if (v_)
    ::CryptDestroyKey(v_);
}

// Uses ScopedHandle to automatically call CryptDestroyHash().
typedef ScopedHandle< HCRYPTHASH, 4 > SAFE_HCRYPTHASH;

template <>
inline void ScopedHandle< HCRYPTHASH, 4 >::destroy() {
  if (v_)
    ::CryptDestroyHash(v_);
}

// Uses ScopedHandle to automatically call UnmapViewOfFile().
typedef ScopedHandle< LPVOID, 5 > SAFE_MAPPEDVIEW;

template <>
inline void ScopedHandle< LPVOID, 5 >::destroy() {
  if (v_)
    ::UnmapViewOfFile(v_);
}

//  SAFE_HINTERNET
//    Uses ScopedHandle to automatically call InternetCloseHandle().
typedef ScopedHandle< HINTERNET, 6 > SAFE_HINTERNET;

template <>
inline void ScopedHandle< HINTERNET, 6 >::destroy() {
  if (v_)
    ::InternetCloseHandle(v_);
}

// SAFE_HMODULE
//     Uses ScopedHandle to automatically call ::FreeLibrary().
typedef ScopedHandle< HMODULE, 7 > SAFE_HMODULE;

template <>
inline void ScopedHandle< HMODULE, 7 >::destroy() {
  if (v_)
    ::FreeLibrary(v_);
}

// SAFE_RESOURCE
//     Uses ScopedHandle to automatically call ::FreeResource().
//     The type is HGLOBAL for backward compatibility, see MSDN, LoadResource()
//     function for details.
typedef ScopedHandle< HGLOBAL, 8 > SAFE_RESOURCE;

template <>
inline void ScopedHandle< HGLOBAL, 8 >::destroy() {
  if (v_)
    ::FreeResource(v_);
}


// ScopedIntCounter is a class that will increment given integet on construction
// and decrement it when the class is destructed.
class ScopedIntCounter {
 public:
  ScopedIntCounter(int *counter):
    counter_(counter) {
    (*counter_)++;
  }

  ~ScopedIntCounter() {
    (*counter_)--;
  }

  int count() {
    return *counter_;
  }

 private:
  int* counter_;
};

#endif // BAR_COMMON_SCOPEDPTR_H_
