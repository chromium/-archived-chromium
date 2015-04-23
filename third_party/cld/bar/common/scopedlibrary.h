// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAR_COMMON_SCOPEDLIBRARY_H_
#define BAR_COMMON_SCOPEDLIBRARY_H_


// A scoped object to safely load and free a DLL referenced by name.
// Provides an access to a handle to loaded library (HMODULE type).
//
// Example:
//     ScopedLibrary library(LIBRARY_NAME);
//     ... = ::GetProcAddress(library.handle(), FUNCTION_NAME);
class ScopedLibrary {
 public:
  // Always creates initialized ScopedLibrary.
  // [in] file_name - library's file name.
  explicit ScopedLibrary(const TCHAR *file_name)
      : library_(::LoadLibrary(file_name)) {}
  // Unloads owned library, if any.
  ~ScopedLibrary() {
    if (library_ != NULL)
      ::FreeLibrary(library_);
  }
  inline HMODULE handle() const { return library_; }
  // Returns true if library was loaded successfully.
  bool IsValid() const { return library_ != NULL; }

 private:
  // Handle to loaded library.
  const HMODULE library_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLibrary);
};


// A class representing a pointer to a function retrieved from DLL.
// FunctionPrototype is a regular C-style pointer-to-function type
// definition. For example, type of WinAPI IsValidSid function:
//     BOOL (WINAPI*)(PSID)
//
// Example:
//     FunctionFromDll<BOOL (WINAPI*)(PSID)> is_valid_sid;
//     ... = is_valid_sid.function()(...);
template<typename FunctionPrototype>
class FunctionFromDll {
 public:
  FunctionFromDll() : function_(NULL) {}
  // Binds this object to a function from DLL.
  // [in] library - handle to a library containing a function.
  //     Must not be NULL.
  // [in] name - name of the function.
  void Bind(HMODULE library, const char *name) {
    function_ =
        reinterpret_cast<FunctionPrototype>(::GetProcAddress(library, name));
  }
  inline FunctionPrototype function() const { return function_; }
  // Returns true if function was bound successfully.
  bool IsValid() const { return function_ != NULL; }

 private:
  // Pointer to the function.
  FunctionPrototype function_;
  DISALLOW_COPY_AND_ASSIGN(FunctionFromDll);
};


#endif  // BAR_COMMON_SCOPEDLIBRARY_H_
