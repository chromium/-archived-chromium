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

#ifndef SANDBOX_SRC_POLICY_ENGINE_PARAMS_H__
#define SANDBOX_SRC_POLICY_ENGINE_PARAMS_H__

#include "base/basictypes.h"
#include "sandbox/src/internal_types.h"
#include "sandbox/src/nt_internals.h"
#include "sandbox/src/sandbox_nt_util.h"

// This header defines the classes that allow the low level policy to select
// the input parameters. In order to better make sense of this header is
// recommended that you check policy_engine_opcodes.h first.

namespace sandbox {

// Models the set of interesting parameters of an intercepted system call
// normally you don't create objects of this class directly, instead you
// use the POLPARAMS_XXX macros.
// For example, if an intercepted function has the following signature:
//
// NTSTATUS NtOpenFileFunction (PHANDLE FileHandle,
//                              ACCESS_MASK DesiredAccess,
//                              POBJECT_ATTRIBUTES ObjectAttributes,
//                              PIO_STATUS_BLOCK IoStatusBlock,
//                              ULONG ShareAccess,
//                              ULONG OpenOptions);
//
// You could say that the following parameters are of interest to policy:
//
//   POLPARAMS_BEGIN(open_params)
//      POLPARAM(DESIRED_ACCESS)
//      POLPARAM(OBJECT_NAME)
//      POLPARAM(SECURITY_DESCRIPTOR)
//      POLPARAM(IO_STATUS)
//      POLPARAM(OPEN_OPTIONS)
//   POLPARAMS_END;
//
// and the actual code will use this for defining the parameters:
//
//   CountedParameterSet<open_params> p;
//   p[open_params::DESIRED_ACCESS] = ParamPickerMake(DesiredAccess);
//   p[open_params::OBJECT_NAME] =
//       ParamPickerMake(ObjectAttributes->ObjectName);
//   p[open_params::SECURITY_DESCRIPTOR] =
//       ParamPickerMake(ObjectAttributes->SecurityDescriptor);
//   p[open_params::IO_STATUS] = ParamPickerMake(IoStatusBlock);
//   p[open_params::OPEN_OPTIONS] = ParamPickerMake(OpenOptions);
//
//  These will create an stack-allocated array of ParameterSet objects which
//  have each 1) the address of the parameter 2) a numeric id that encodes the
//  original C++ type. This allows the policy to treat any set of supported
//  argument types uniformily and with some type safety.
//
//  TODO(cpu): support not fully implemented yet for unicode string and will
//  probably add other types as well.
class ParameterSet {
 public:
  ParameterSet() : real_type_(INVALID_TYPE), address_(NULL) {}

  // Retrieve the stored parameter. If the type does not match ulong fail.
  bool Get(unsigned long* destination) const {
    if (ULONG_TYPE != real_type_) {
      return false;
    }
    *destination = Void2TypePointerCopy<unsigned long>();
    return true;
  }

  // Retrieve the stored parameter. If the type does not match void* fail.
  bool Get(const void** destination) const {
    if (VOIDPTR_TYPE != real_type_) {
      return false;
    }
    *destination = Void2TypePointerCopy<void*>();
    return true;
  }

  // Retrieve the stored parameter. If the type does not match wchar_t* fail.
  bool Get(const wchar_t** destination) const {
    if (WCHAR_TYPE != real_type_) {
      return false;
    }
    *destination = Void2TypePointerCopy<const wchar_t*>();
    return true;
  }

  // False if the parameter is not properly initialized.
  bool IsValid() const {
    return INVALID_TYPE != real_type_;
  }

 protected:
  // The constructor can only be called by derived types, which should
  // safely provide the real_type and the address of the argument.
  ParameterSet(ArgType real_type, const void* address)
      : real_type_(real_type), address_(address) {
  }

 private:
  // This template provides the same functionality as bits_cast but
  // it works with pointer while the former works only with references.
  template <typename T>
  T Void2TypePointerCopy() const {
    return *(reinterpret_cast<const T*>(address_));
  }

  ArgType real_type_;
  const void* address_;
};

// To safely infer the type, we use a set of template specializations
// in ParameterSetEx with a template function ParamPickerMake to do the
// parameter type deduction.

// Base template class. Not implemented so using unsupported types should
// fail to compile.
template <typename T>
class ParameterSetEx : public ParameterSet {
 public:
  ParameterSetEx(const void* address);
};

template<>
class ParameterSetEx<void const*> : public ParameterSet {
 public:
  ParameterSetEx(const void* address)
      : ParameterSet(VOIDPTR_TYPE, address) {}
};

template<>
class ParameterSetEx<void*> : public ParameterSet {
 public:
  ParameterSetEx(const void* address)
      : ParameterSet(VOIDPTR_TYPE, address) {}
};


template<>
class ParameterSetEx<wchar_t*> : public ParameterSet {
 public:
  ParameterSetEx(const void* address)
      : ParameterSet(WCHAR_TYPE, address) {}
};

template<>
class ParameterSetEx<wchar_t const*> : public ParameterSet {
 public:
  ParameterSetEx(const void* address)
      : ParameterSet(WCHAR_TYPE, address) {}
};


template<>
class ParameterSetEx<unsigned long> : public ParameterSet {
 public:
  ParameterSetEx(const void* address)
      : ParameterSet(ULONG_TYPE, address) {}
};

template<>
class ParameterSetEx<UNICODE_STRING> : public ParameterSet {
 public:
  ParameterSetEx(const void* address)
      : ParameterSet(UNISTR_TYPE, address) {}
};

template <typename T>
ParameterSet ParamPickerMake(T& parameter) {
  return ParameterSetEx<T>(&parameter);
};

struct CountedParameterSetBase {
  int count;
  ParameterSet parameters[1];
};

// This template defines the actual list of policy parameters for a given
// interception.
// Warning: This template stores the address to the actual variables, in
// other words, the values are not copied.
template <typename T>
struct CountedParameterSet {
  CountedParameterSet() : count(T::PolParamLast) {}

  ParameterSet& operator[](typename T::Args n) {
    return parameters[n];
  }

  CountedParameterSetBase* GetBase() {
    return reinterpret_cast<CountedParameterSetBase*>(this);
  }

  int count;
  ParameterSet parameters[T::PolParamLast];
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_ENGINE_PARAMS_H__
