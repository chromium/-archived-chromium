/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <algorithm>
#include "command_buffer/common/cross/logging.h"
#include "command_buffer/common/cross/rpc_imc.h"
#include "third_party/native_client/googleclient/native_client/src/shared/imc/nacl_htp.h"

namespace o3d {
namespace command_buffer {

int NaclSendData(nacl::HtpHandle channel,
                 const void *data,
                 size_t size,
                 int flags) {
  nacl::HtpHeader msg;
  nacl::IOVec vec;
  vec.base = const_cast<void *>(data);
  vec.length = size;
  msg.iov = &vec;
  msg.iov_length = 1;
  msg.handles = NULL;
  msg.handle_count = 0;
  msg.flags = 0;
  return nacl::SendDatagram(channel, &msg, flags);
}

int NaclSendHandles(nacl::HtpHandle channel,
                    const nacl::HtpHandle *handles,
                    size_t count,
                    int flags) {
  nacl::HtpHeader msg;
  msg.iov = NULL;
  msg.iov_length = 0;
  msg.handles = const_cast<nacl::HtpHandle *>(handles);
  msg.handle_count = count;
  msg.flags = 0;
  return nacl::SendDatagram(channel, &msg, flags);
}

int NaclReceiveData(nacl::HtpHandle channel,
                    void *data,
                    size_t size,
                    int flags) {
  nacl::HtpHeader msg;
  nacl::IOVec vec[1];
  vec[0].base = data;
  vec[0].length = size;
  msg.iov = vec;
  msg.iov_length = 1;
  msg.handles = NULL;
  msg.handle_count = 0;
  msg.flags = 0;
  int result = nacl::ReceiveDatagram(channel, &msg, flags);
  return result;
}

int NaclReceiveHandles(nacl::HtpHandle channel,
                       nacl::HtpHandle *handles,
                       size_t count,
                       int flags) {
  nacl::HtpHeader msg;
  msg.iov = NULL;
  msg.iov_length = 0;
  msg.handles = handles;
  msg.handle_count = count;
  msg.flags = 0;
  return nacl::ReceiveDatagram(channel, &msg, flags);
}

struct RPCMessageHeader {
  int message_id;
  size_t size;
  size_t handle_count;
};

RPCImplInterface::ReturnValue IMCSender::SendCall(
    int message_id,
    const void * data,
    size_t size,
    RPCHandle const *handles,
    size_t handle_count) {
  // Send 3 messages: first contains message id, and data size, second
  // contains data and third handles.
  // The reason for that is to have the size first so that we can allocate the
  // data buffer on the receiving side.
  RPCMessageHeader msg = {message_id, size, handle_count};
  int result = NaclSendData(handle_, &msg, sizeof(msg), 0);
  DCHECK_NE(-1, result);
  if (size > 0) {
    result = NaclSendData(handle_, data, size, 0);
    DCHECK_NE(-1, result);
  }
  if (handle_count > 0) {
    result = NaclSendHandles(handle_, handles, handle_count, 0);
    DCHECK_NE(-1, result);
  }

  RPCImplInterface::ReturnValue ret;
  result = NaclReceiveData(handle_, &ret, sizeof(ret), 0);
  DCHECK_EQ(sizeof(ret), result);
  return ret;
}

bool IMCMessageProcessor::GetMessageIDSize(bool wait) {
  if (has_message_) return true;
  RPCMessageHeader msg = {0};
  int result = NaclReceiveData(handle_, &msg, sizeof(msg),
                               wait ? 0 : nacl::kDontWait);
  if (result != sizeof(msg)) {
    if (result >=0 || wait || !nacl::WouldBlock()) return false;
    has_message_ = false;
  } else {
    incoming_message_id_ = msg.message_id;
    incoming_message_size_ = msg.size;
    incoming_message_handles_ = msg.handle_count;
    has_message_ = true;
  }
  return true;
}

bool IMCMessageProcessor::ProcessMessage() {
  if (!GetMessageIDSize(true)) return false;
  if (incoming_message_size_ > 0) {
    if (incoming_message_size_ > data_size_) {
      data_size_ = incoming_message_size_;
      data_.reset(new char[incoming_message_size_]);
    }
    int result = NaclReceiveData(handle_, data_.get(), incoming_message_size_,
                                 0);
    if (result < 0) return false;
  }
  if (incoming_message_handles_ > 0) {
    if (incoming_message_handles_ > handle_count_) {
      handle_count_ = incoming_message_handles_;
      handles_.reset(new nacl::HtpHandle[incoming_message_handles_]);
    }
    int result = NaclReceiveHandles(handle_, handles_.get(),
                                    incoming_message_handles_, 0);
    if (result < 0) return false;
  }
  has_message_ = false;
  RPCImplInterface::ReturnValue retval = 0;
  bool continue_processing = true;
  if (incoming_message_id_ == POISONED_MESSAGE_ID) {
    continue_processing = false;
  } else {
    retval = impl_->DoCall(incoming_message_id_, data_.get(),
                           incoming_message_size_, handles_.get(),
                           incoming_message_handles_);
    // Note: the handles that got received are a "copy" of the original handle,
    // so they need to be closed eventually. It's hard to impose an
    // "ownership" policy onto the RPCImplInterface, because it makes the API
    // very clunky. Currently, we only pass handles across for
    // RegisterSharedMemory, and so we can safely close the handle immediately
    // (the memory will stay mapped).
    // TODO: Fix this. Possibly using a global registration mechanism
    // and ref-counting would make it work nicely.
    for (unsigned int i = 0; i < incoming_message_handles_; ++i) {
      nacl::Close(handles_[i]);
    }
  }

  int result = NaclSendData(handle_, &retval, sizeof(retval), 0);
  if (result < 0) return false;
  return continue_processing;
}

bool IMCMessageProcessor::HasMessage() {
  GetMessageIDSize(false);
  return has_message_;
}

RPCSendInterface *MakeSendInterface(RPCSocketHandle handle) {
  return new IMCSender(handle);
}

RPCShmHandle CreateShm(size_t size) {
  size = (size + nacl::kMapPageSize - 1) & ~(nacl::kMapPageSize - 1);
  nacl::Handle nacl_handle = nacl::CreateMemoryObject(size);
  return nacl_handle == nacl::kInvalidHandle ? kRPCInvalidHandle :
      nacl::CreateShmDesc(nacl_handle, size);
}

void DestroyShm(RPCShmHandle handle) {
  nacl::Close(handle);
}

void *MapShm(RPCShmHandle handle, size_t size) {
  size = (size + nacl::kMapPageSize - 1) & ~(nacl::kMapPageSize - 1);
  void *address = nacl::Map(NULL, size, nacl::kProtRead | nacl::kProtWrite,
                            nacl::kMapShared, handle, 0);
  return address == nacl::kMapFailed ? NULL : address;
}

void UnmapShm(void *address, size_t size) {
  size = (size + nacl::kMapPageSize - 1) & ~(nacl::kMapPageSize - 1);
  nacl::Unmap(address, size);
}

}  // namespace command_buffer
}  // namespace o3d
