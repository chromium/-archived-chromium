// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/wow64.h"

#include <sstream>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/target_process.h"

namespace {

// Holds the information needed for the interception of NtMapViewOfSection on
// 64 bits.
// Warning: do not modify this definition without changing also the code on the
// 64 bit helper process.
struct PatchInfo32 {
  HANDLE dll_load;  // Event to signal the broker.
  ULONG pad1;
  HANDLE continue_load;  // Event to wait for the broker.
  ULONG pad2;
  HANDLE section;  // First argument of the call.
  ULONG pad3;
  void* orig_MapViewOfSection;
  ULONG original_high;
  void* signal_and_wait;
  ULONG pad4;
  void* patch_location;
  ULONG patch_high;
};

// Size of the 64 bit service entry.
const SIZE_T kServiceEntry64Size = 0x10;

// Removes the interception of ntdll64.
bool Restore64Code(HANDLE child, PatchInfo32* patch_info) {
  PatchInfo32 local_patch_info;
  SIZE_T actual;
  if (!::ReadProcessMemory(child, patch_info, &local_patch_info,
                           sizeof(local_patch_info), &actual))
    return false;
  if (sizeof(local_patch_info) != actual)
    return false;

  if (local_patch_info.original_high)
    return false;
  if (local_patch_info.patch_high)
    return false;

  char buffer[kServiceEntry64Size];

  if (!::ReadProcessMemory(child, local_patch_info.orig_MapViewOfSection,
                           &buffer, kServiceEntry64Size, &actual))
    return false;
  if (kServiceEntry64Size != actual)
    return false;

  if (!::WriteProcessMemory(child, local_patch_info.patch_location, &buffer,
                            kServiceEntry64Size, &actual))
    return false;
  if (kServiceEntry64Size != actual)
    return false;
  return true;
}

typedef BOOL (WINAPI* IsWow64ProcessFunction)(HANDLE process, BOOL* wow64);

}  // namespace

namespace sandbox {

Wow64::~Wow64() {
  if (dll_load_)
    ::CloseHandle(dll_load_);

  if (continue_load_)
    ::CloseHandle(continue_load_);
}

bool Wow64::IsWow64() {
  if (init_)
    return is_wow64_;

  is_wow64_ = false;

  HMODULE kernel32 = ::GetModuleHandle(sandbox::kKerneldllName);
  if (!kernel32)
    return false;

  IsWow64ProcessFunction is_wow64_process = reinterpret_cast<
      IsWow64ProcessFunction>(::GetProcAddress(kernel32, "IsWow64Process"));

  init_ = true;
  if (!is_wow64_process)
    return false;

  BOOL wow64;
  if (!is_wow64_process(::GetCurrentProcess(), &wow64))
    return false;

  if (wow64)
    is_wow64_ = true;

  return is_wow64_;
}

// The basic idea is to allocate one page of memory on the child, and initialize
// the first part of it with our version of PatchInfo32. Then launch the helper
// process passing it that address on the child. The helper process will patch
// the 64 bit version of NtMapViewOfFile, and the interception will signal the
// first event on the buffer. We'll be waiting on that event and after the 32
// bit version of ntdll is loaded, we'll remove the interception and return to
// our caller.
bool Wow64::WaitForNtdll(DWORD timeout_ms) {
  DCHECK(!init_);
  if (!IsWow64())
    return true;

  const size_t page_size = 4096;

  // Create some default manual reset un-named events, not signaled.
  dll_load_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  continue_load_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  HANDLE current_process = ::GetCurrentProcess();
  HANDLE remote_load, remote_continue;
  DWORD access = EVENT_MODIFY_STATE | SYNCHRONIZE;
  if (!::DuplicateHandle(current_process, dll_load_, child_->Process(),
                         &remote_load, access, FALSE, 0))
    return false;
  if (!::DuplicateHandle(current_process, continue_load_, child_->Process(),
                         &remote_continue, access, FALSE, 0))
    return false;

  void* buffer = ::VirtualAllocEx(child_->Process(), NULL, page_size,
                                  MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  DCHECK(buffer);
  if (!buffer)
    return false;

  PatchInfo32* patch_info = reinterpret_cast<PatchInfo32*>(buffer);
  PatchInfo32 local_patch_info = {0};
  local_patch_info.dll_load = remote_load;
  local_patch_info.continue_load = remote_continue;
  SIZE_T written;
  if (!::WriteProcessMemory(child_->Process(), patch_info, &local_patch_info,
                            offsetof(PatchInfo32, section), &written))
    return false;
  if (offsetof(PatchInfo32, section) != written)
    return false;

  if (!RunWowHelper(buffer, timeout_ms))
    return false;

  // The child is intercepted on 64 bit, go on and wait for our event.
  if (!DllMapped(timeout_ms))
    return false;

  // The 32 bit version is available, cleanup the child.
  return Restore64Code(child_->Process(), patch_info);
}

bool Wow64::RunWowHelper(void* buffer, DWORD timeout_ms) {
  COMPILE_ASSERT(sizeof(buffer) <= sizeof(timeout_ms), unsupported_64_bits);

  // Get the path to the helper (beside the exe).
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);
  std::wstring path(prog_name);
  size_t name_pos = path.find_last_of(L"\\");
  if (std::wstring::npos == name_pos)
    return false;
  path.resize(name_pos + 1);

  std::wstringstream command;
  command << std::hex << std::showbase << L"\"" << path <<
               L"wow_helper.exe\" " << child_->ProcessId() << " " <<
               bit_cast<ULONG>(buffer);

  scoped_ptr_malloc<wchar_t> writable_command(_wcsdup(command.str().c_str()));

  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info;
  if (!::CreateProcess(NULL, writable_command.get(), NULL, NULL, FALSE, 0, NULL,
                       NULL, &startup_info, &process_info))
    return false;

  DWORD reason = ::WaitForSingleObject(process_info.hProcess, timeout_ms);

  DWORD code;
  bool ok = ::GetExitCodeProcess(process_info.hProcess, &code) ? true : false;

  ::CloseHandle(process_info.hProcess);
  ::CloseHandle(process_info.hThread);

  if (WAIT_TIMEOUT == reason)
    return false;

  return ok && (0 == code);
}

// First we must wake up the child, then wait for dll loads on the child until
// the one we care is loaded; at that point we must suspend the child again.
bool Wow64::DllMapped(DWORD timeout_ms) {
  if (1 != ::ResumeThread(child_->MainThread())) {
    NOTREACHED();
    return false;
  }

  for (;;) {
    DWORD reason = ::WaitForSingleObject(dll_load_, timeout_ms);
    if (WAIT_TIMEOUT == reason || WAIT_ABANDONED == reason)
      return false;

    if (!::ResetEvent(dll_load_))
      return false;

    bool found = NtdllPresent();
    if (found) {
      if (::SuspendThread(child_->MainThread()))
        return false;
    }

    if (!::SetEvent(continue_load_))
      return false;

    if (found)
      return true;
  }
}

bool Wow64::NtdllPresent() {
  const size_t kBufferSize = 512;
  char buffer[kBufferSize];
  SIZE_T read;
  if (!::ReadProcessMemory(child_->Process(), ntdll_, &buffer, kBufferSize,
                           &read))
    return false;
  if (kBufferSize != read)
    return false;
  return true;
}

}  // namespace sandbox
