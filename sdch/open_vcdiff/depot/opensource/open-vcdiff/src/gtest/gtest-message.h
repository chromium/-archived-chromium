// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: wan@google.com (Zhanyong Wan)
//
// The Google C++ Testing Framework (Google Test)
//
// This header file defines the Message class.
//
// IMPORTANT NOTE: Due to limitation of the C++ language, we have to
// leave some internal implementation details in this header file.
// They are clearly marked by comments like this:
//
//   // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
//
// Such code is NOT meant to be used by a user directly, and is subject
// to CHANGE WITHOUT NOTICE.  Therefore DO NOT DEPEND ON IT in a user
// program!

#ifndef GTEST_INCLUDE_GTEST_GTEST_MESSAGE_H_
#define GTEST_INCLUDE_GTEST_GTEST_MESSAGE_H_

#include <gtest/internal/gtest-string.h>
#include <gtest/internal/gtest-internal.h>

namespace testing {

// The Message class works like an ostream repeater.
//
// Typical usage:
//
//   1. You stream a bunch of values to a Message object.
//      It will remember the text in a StrStream.
//   2. Then you stream the Message object to an ostream.
//      This causes the text in the Message to be streamed
//      to the ostream.
//
// For example;
//
//   testing::Message foo;
//   foo << 1 << " != " << 2;
//   std::cout << foo;
//
// will print "1 != 2".
//
// Message is not intended to be inherited from.  In particular, its
// destructor is not virtual.
//
// Note that StrStream behaves differently in gcc and in MSVC.  You
// can stream a NULL char pointer to it in the former, but not in the
// latter (it causes an access violation if you do).  The Message
// class hides this difference by treating a NULL char pointer as
// "(null)".
class Message {
 private:
  // The type of basic IO manipulators (endl, ends, and flush) for
  // narrow streams.
  typedef std::ostream& (*BasicNarrowIoManip)(std::ostream&);

 public:
  // Constructs an empty Message.
  // We allocate the StrStream separately because it otherwise each use of
  // ASSERT/EXPECT in a procedure adds over 200 bytes to the procedure's
  // stack frame leading to huge stack frames in some cases; gcc does not reuse
  // the stack space.
  Message() : ss_(new internal::StrStream) {}

  // Copy constructor.
  Message(const Message& msg) : ss_(new internal::StrStream) {  // NOLINT
    *ss_ << msg.GetString();
  }

  // Constructs a Message from a C-string.
  explicit Message(const char* str) : ss_(new internal::StrStream) {
    *ss_ << str;
  }

  ~Message() { delete ss_; }
#ifdef __SYMBIAN32__
  // Streams a value (either a pointer or not) to this object.
  template <typename T>
  inline Message& operator <<(const T& value) {
    StreamHelper(typename internal::is_pointer<T>::type(), value);
    return *this;
  }
#else
  // Streams a non-pointer value to this object.
  template <typename T>
  inline Message& operator <<(const T& val) {
    ::GTestStreamToHelper(ss_, val);
    return *this;
  }

  // Streams a pointer value to this object.
  //
  // This function is an overload of the previous one.  When you
  // stream a pointer to a Message, this definition will be used as it
  // is more specialized.  (The C++ Standard, section
  // [temp.func.order].)  If you stream a non-pointer, then the
  // previous definition will be used.
  //
  // The reason for this overload is that streaming a NULL pointer to
  // ostream is undefined behavior.  Depending on the compiler, you
  // may get "0", "(nil)", "(null)", or an access violation.  To
  // ensure consistent result across compilers, we always treat NULL
  // as "(null)".
  template <typename T>
  inline Message& operator <<(T* const& pointer) {  // NOLINT
    if (pointer == NULL) {
      *ss_ << "(null)";
    } else {
      ::GTestStreamToHelper(ss_, pointer);
    }
    return *this;
  }
#endif  // __SYMBIAN32__

  // Since the basic IO manipulators are overloaded for both narrow
  // and wide streams, we have to provide this specialized definition
  // of operator <<, even though its body is the same as the
  // templatized version above.  Without this definition, streaming
  // endl or other basic IO manipulators to Message will confuse the
  // compiler.
  Message& operator <<(BasicNarrowIoManip val) {
    *ss_ << val;
    return *this;
  }

  // Instead of 1/0, we want to see true/false for bool values.
  Message& operator <<(bool b) {
    return *this << (b ? "true" : "false");
  }

  // These two overloads allow streaming a wide C string to a Message
  // using the UTF-8 encoding.
  Message& operator <<(const wchar_t* wide_c_str) {
    return *this << internal::String::ShowWideCString(wide_c_str);
  }
  Message& operator <<(wchar_t* wide_c_str) {
    return *this << internal::String::ShowWideCString(wide_c_str);
  }

#if GTEST_HAS_STD_WSTRING
  // Converts the given wide string to a narrow string using the UTF-8
  // encoding, and streams the result to this Message object.
  Message& operator <<(const ::std::wstring& wstr);
#endif  // GTEST_HAS_STD_WSTRING

#if GTEST_HAS_GLOBAL_WSTRING
  // Converts the given wide string to a narrow string using the UTF-8
  // encoding, and streams the result to this Message object.
  Message& operator <<(const ::wstring& wstr);
#endif  // GTEST_HAS_GLOBAL_WSTRING

  // Gets the text streamed to this object so far as a String.
  // Each '\0' character in the buffer is replaced with "\\0".
  //
  // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
  internal::String GetString() const {
    return internal::StrStreamToString(ss_);
  }

 private:
#ifdef __SYMBIAN32__
  // These are needed as the Nokia Symbian Compiler cannot decide between
  // const T& and const T* in a function template. The Nokia compiler _can_
  // decide between class template specializations for T and T*, so a
  // tr1::type_traits-like is_pointer works, and we can overload on that.
  template <typename T>
  inline void StreamHelper(internal::true_type dummy, T* pointer) {
    if (pointer == NULL) {
      *ss_ << "(null)";
    } else {
      ::GTestStreamToHelper(ss_, pointer);
    }
  }
  template <typename T>
  inline void StreamHelper(internal::false_type dummy, const T& value) {
    ::GTestStreamToHelper(ss_, value);
  }
#endif  // __SYMBIAN32__

  // We'll hold the text streamed to this object here.
  internal::StrStream* const ss_;

  // We declare (but don't implement) this to prevent the compiler
  // from implementing the assignment operator.
  void operator=(const Message&);
};

// Streams a Message to an ostream.
inline std::ostream& operator <<(std::ostream& os, const Message& sb) {
  return os << sb.GetString();
}

}  // namespace testing

#endif  // GTEST_INCLUDE_GTEST_GTEST_MESSAGE_H_

