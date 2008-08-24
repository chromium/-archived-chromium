// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "sandbox/src/crosscall_server.h"
#include "sandbox/src/crosscall_params.h"
#include "sandbox/src/crosscall_client.h"
#include "base/logging.h"

// This code performs the ipc message validation. Potential security flaws
// on the ipc are likelier to be found in this code than in the rest of
// the ipc code.

namespace {
  // The buffer for a message must match the max channel size.
  const size_t kMaxBufferSize = sandbox::kIPCChannelSize;
}

namespace sandbox {

// Returns the actual size for the parameters in an IPC buffer.
void GetActualBufferSize(size_t param_count, void* buffer_base,
                         size_t* actual_size) {
  // The template types are used to calculate the maximum expected size.
  typedef ActualCallParams<1, kMaxBufferSize> ActualCP1;
  typedef ActualCallParams<2, kMaxBufferSize> ActualCP2;
  typedef ActualCallParams<3, kMaxBufferSize> ActualCP3;
  typedef ActualCallParams<4, kMaxBufferSize> ActualCP4;
  typedef ActualCallParams<5, kMaxBufferSize> ActualCP5;
  typedef ActualCallParams<6, kMaxBufferSize> ActualCP6;
  typedef ActualCallParams<7, kMaxBufferSize> ActualCP7;
  typedef ActualCallParams<8, kMaxBufferSize> ActualCP8;
  typedef ActualCallParams<9, kMaxBufferSize> ActualCP9;

  // Retrieve the actual size and the maximum size of the params buffer.
  switch (param_count) {
    case 1:
      *actual_size = reinterpret_cast<ActualCP1*>(buffer_base)->GetSize();
      break;
    case 2:
      *actual_size = reinterpret_cast<ActualCP2*>(buffer_base)->GetSize();
      break;
    case 3:
      *actual_size = reinterpret_cast<ActualCP3*>(buffer_base)->GetSize();
      break;
    case 4:
      *actual_size = reinterpret_cast<ActualCP4*>(buffer_base)->GetSize();
      break;
    case 5:
      *actual_size = reinterpret_cast<ActualCP5*>(buffer_base)->GetSize();
      break;
    case 6:
      *actual_size = reinterpret_cast<ActualCP6*>(buffer_base)->GetSize();
      break;
    case 7:
      *actual_size = reinterpret_cast<ActualCP7*>(buffer_base)->GetSize();
      break;
    case 8:
      *actual_size = reinterpret_cast<ActualCP8*>(buffer_base)->GetSize();
      break;
    case 9:
      *actual_size = reinterpret_cast<ActualCP9*>(buffer_base)->GetSize();
      break;
    default:
      NOTREACHED();
      *actual_size = 0;
  }
}

CrossCallParamsEx::CrossCallParamsEx()
  :CrossCallParams(0, 0) {
}

// We override the delete operator because the object's backing memory
// is hand allocated in CreateFromBuffer. We don't override the new operator
// because the constructors are private so there is no way to mismatch
// new & delete.
void CrossCallParamsEx::operator delete(void* raw_memory) throw() {
  if (NULL == raw_memory) {
    // C++ standard allows 'delete 0' behavior.
    return;
  }
  delete[] reinterpret_cast<char*>(raw_memory);
}

// This function uses a SEH try block so cannot use C++ objects that
// have destructors or else you get Compiler Error C2712. So no DCHECKs
// inside this function.
CrossCallParamsEx* CrossCallParamsEx::CreateFromBuffer(void* buffer_base,
                                                       size_t buffer_size,
                                                       size_t* output_size) {
  if (NULL == buffer_base) {
    return NULL;
  }
  if (buffer_size < sizeof(CrossCallParams)) {
    return NULL;
  }
  if (buffer_size > kMaxBufferSize) {
    return NULL;
  }
  char* backing_mem = NULL;
  size_t param_count = 0;
  CrossCallParamsEx* copied_params = NULL;

  size_t actual_size = 0;

  // Touching the untrusted buffer is done under a SEH try block. This
  // will catch memory access violations so we don't crash.
  __try {
    CrossCallParams* call_params =
        reinterpret_cast<CrossCallParams*>(buffer_base);
    // Check against the minimum size given the number of stated params
    // if too small we bail out.
    param_count = call_params->GetParamsCount();
    if ((buffer_size - sizeof(CrossCallParams)) <
        (sizeof(ptrdiff_t) * (param_count + 1))) {
      // Too small.
      return NULL;
    }

    GetActualBufferSize(param_count, buffer_base, &actual_size);

    if (actual_size > buffer_size) {
      // It is too big.
      return NULL;
    }
    // Now we copy the actual amount of the message.
    actual_size += sizeof(ParamInfo);  // To get the last offset.
    *output_size = actual_size;
    backing_mem = new char[actual_size];
    memset(backing_mem, 0, actual_size);
    // Note that this is a placement new.
#pragma warning(push)
#pragma warning(disable: 4291)  // No matching operator delete.
    // TODO(cpu): Remove this warning.
    copied_params = new(backing_mem)CrossCallParamsEx();
#pragma warning(pop)
    memcpy(backing_mem, call_params, actual_size);

  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // In case of a windows exception we know it occurred while touching the
    // untrusted buffer so we bail out as is.
    return NULL;
  }

  char* last_byte = &backing_mem[actual_size - 1];
  // Verify here that all and each parameters make sense. This is done in the
  // local copy.
  for (size_t ix =0; ix != param_count; ++ix) {
    size_t size = 0;
    ArgType type;
    char* address = reinterpret_cast<char*>(
                        copied_params->GetRawParameter(ix, &size, &type));
    if ((NULL == address) ||               // No null params.
        (INVALID_TYPE >= type) || (LAST_TYPE <= type) ||  // Unknown type.
        (address < backing_mem) ||         // Start cannot point before buffer.
        (address > last_byte) ||           // Start cannot point past buffer.
        ((address + size) < address) ||    // Invalid size.
        ((address + size) > last_byte)) {  // End cannot point past buffer.
      // Malformed.
      delete[] backing_mem;
      return NULL;
    }
  }
  // The parameter buffer looks good.
  return copied_params;
}

// Accessors to the parameters in the raw buffer.
void* CrossCallParamsEx::GetRawParameter(size_t index, size_t* size,
                                         ArgType* type) {
  if (index > GetParamsCount()) {
    return NULL;
  }
  // The size is always computed from the parameter minus the next
  // parameter, this works because the message has an extra parameter slot
  *size = param_info_[index].size_;
  *type = param_info_[index].type_;

  return param_info_[index].offset_ + reinterpret_cast<char*>(this);
}

// Covers common case for 32 bit integers.
bool CrossCallParamsEx::GetParameter32(size_t index, void* param) {
  size_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);
  if ((NULL == start) || (4 != size) || (ULONG_TYPE != type)) {
    return false;
  }
  // Copy the 4 bytes.
  *(reinterpret_cast<uint32*>(param)) = *(reinterpret_cast<uint32*>(start));
  return true;
}

// Covers the common case of reading a string. Note that the string is not
// scanned for invalid characters.
bool CrossCallParamsEx::GetParameterStr(size_t index, std::wstring* string) {
  size_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);
  if (WCHAR_TYPE != type) {
    return false;
  }

  // Check if this is an empty string.
  if (size == 0) {
    *string = L"";
    return true;
  }

  if ((NULL == start) || ((size % sizeof(wchar_t)) != 0)) {
    return false;
  }
  string->append(reinterpret_cast<wchar_t*>(start), size/(sizeof(wchar_t)));
  return true;
}

bool CrossCallParamsEx::GetParameterPtr(size_t index, size_t expected_size,
                                        void** pointer) {
  size_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);

  if ((size != expected_size) || (INOUTPTR_TYPE != type)) {
    return false;
  }

  if (NULL == start) {
    return false;
  }

  *pointer = start;
  return true;
}

void SetCallError(ResultCode error, CrossCallReturn* call_return) {
  call_return->call_outcome = error;
  call_return->extended_count = 0;
}

void SetCallSuccess(CrossCallReturn* call_return) {
  call_return->call_outcome = SBOX_ALL_OK;
}

Dispatcher* Dispatcher::OnMessageReady(IPCParams* ipc,
                                      CallbackGeneric* callback) {
  DCHECK(callback);
  std::vector<IPCCall>::iterator it = ipc_calls_.begin();
  for (; it != ipc_calls_.end(); ++it) {
    if (it->params.Matches(ipc)) {
      *callback = it->callback;
      return this;
    }
  }
  return NULL;
}

}  // namespace sandbox

