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


// Tests the functionality defined in MessageQueue.cc/h

#include "core/cross/message_queue.h"
#include "core/cross/client.h"
#include "core/cross/types.h"
#include "tests/common/win/testing_common.h"


namespace o3d {

// Name of socket address used in this test
nacl::SocketAddress my_address = {
  "test-client"
};

class MessageQueueTest : public testing::Test {
 protected:

  MessageQueueTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }
  nacl::Handle my_socket_handle() { return my_socket_handle_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
  nacl::Handle my_socket_handle_;
};

void MessageQueueTest::SetUp() {
  pack_ = object_manager_->CreatePack(L"MessageQueueTest");

  // Create a bound socket to connect to the MessageQueue.
  my_socket_handle_ = nacl::BoundSocket(&my_address);

  ASSERT_TRUE(my_socket_handle_ != nacl::kInvalidHandle);
}

void MessageQueueTest::TearDown() {
  nacl::Close(my_socket_handle_);
  object_manager_->DestroyPack(pack_);
}

// Helper class that handles connecting to the MessageQueue and issuing
// commands to it.
class TextureUpdateHelper {
 public:
  TextureUpdateHelper() : o3d_handle_(nacl::kInvalidHandle) {}

  // Connects to the MessageQueue.
  bool ConnectToO3D(const char* o3d_address,
                    nacl::Handle my_socket_handle);

  // Makes a request for a shared memory buffer.
  bool RequestSharedMemory(size_t requested_size,
                           int* shared_mem_id,
                           void** shared_mem_address);

  // Makes a request to update a texture.
  bool RequestTextureUpdate(unsigned int texture_id,
                            int level,
                            int shared_memory_id,
                            size_t offset,
                            size_t number_of_bytes);

 private:
  // Handle of the socket that's connected to o3d.
  nacl::Handle o3d_handle_;

  bool ReceiveBooleanResponse();
};  // TextureUpdateHelper

// Waits for a message with a single integer value.  If the value is 0 it
// returns false otherwise returns true.
bool TextureUpdateHelper::ReceiveBooleanResponse() {
  int response = 0;
  nacl::IOVec vec;
  vec.base = &response;
  vec.length = sizeof(response);

  nacl::MessageHeader header;
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = 0;
  header.handle_count = 0;

  int result = nacl::ReceiveDatagram(o3d_handle_, &header, 0);

  EXPECT_EQ(sizeof(response), result);
  if (result != sizeof(response)) {
    return false;
  }

  return (response ? true : false);
}


// Send the initial handshake message to O3D.
bool TextureUpdateHelper::ConnectToO3D(const char* o3d_address,
                                       nacl::Handle my_socket_handle) {
  nacl::Handle pair[2];

  if (nacl::SocketPair(pair) != 0) {
    return false;
  }

  char buffer[128];
  int message_size = 0;
  MessageQueue::MessageId *msg_id =
      reinterpret_cast<MessageQueue::MessageId*>(buffer);
  *msg_id = MessageQueue::HELLO;
  message_size += sizeof(*msg_id);

  nacl::MessageHeader header;
  nacl::IOVec vec;
  vec.base = buffer;
  vec.length = message_size;

  nacl::SocketAddress o3d_address;
  sprintf_s(o3d_address.path, sizeof(o3d_address.path), "%s", o3d_address);

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = &pair[1];
  header.handle_count = 1;
  int result = nacl::SendDatagramTo(my_socket_handle,
                                    &header,
                                    0,
                                    &o3d_address);

  EXPECT_EQ(message_size, result);
  if (result != message_size) {
    return false;
  }

  // The socket handle we established the connection with o3d with.
  o3d_handle_ = pair[0];

  bool response = ReceiveBooleanResponse();

  EXPECT_TRUE(response);

  // We don't need to have that handle open anymore since the server has it now.
  result = nacl::Close(pair[1]);

  return response;
}

// Sends the server a request to allocate shared memory.  It received back
// from the server a shared memory handle which it then uses to map the shared
// memory into this process' address space.  The server also returns a unique
// id for the shared memory which can be used to identify the buffer in
// subsequent communcations with the server.
bool TextureUpdateHelper::RequestSharedMemory(size_t requested_size,
                                              int* shared_mem_id,
                                              void** shared_mem_address) {
  if (o3d_handle_ == nacl::kInvalidHandle) {
    return false;
  }

  MessageQueue::MessageId message_id = MessageQueue::ALLOCATE_SHARED_MEMORY;

  nacl::MessageHeader header;
  nacl::IOVec vec;
  char buffer[256];
  char *buffer_ptr = &buffer[0];

  // Message contains the ID and one argument (the size of the shared memory
  // buffer to be allocated).
  *(reinterpret_cast<MessageQueue::MessageId*>(buffer_ptr)) = message_id;
  buffer_ptr += sizeof(message_id);
  *(reinterpret_cast<size_t*>(buffer_ptr)) = requested_size;
  buffer_ptr += sizeof(requested_size);

  vec.base = buffer;
  vec.length = buffer_ptr - &buffer[0];

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;

  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);
  EXPECT_EQ(vec.length, result);
  if (result != vec.length) {
    return false;
  }

  // Wait for a message back from the server containing the handle to the
  // shared memory object.
  nacl::Handle shared_memory;
  int shared_memory_id = -1;
  nacl::IOVec shared_memory_vec;
  shared_memory_vec.base = &shared_memory_id;
  shared_memory_vec.length = sizeof(shared_memory_id);
  header.iov = &shared_memory_vec;
  header.iov_length = 1;
  header.handles = &shared_memory;
  header.handle_count = 1;
  result = nacl::ReceiveDatagram(o3d_handle_, &header, 0);

  EXPECT_LT(0, result);
  EXPECT_FALSE(header.flags & nacl::kMessageTruncated);
  EXPECT_EQ(1, header.handle_count);
  EXPECT_EQ(1, header.iov_length);

  if (result < 0 ||
      header.flags & nacl::kMessageTruncated ||
      header.handle_count != 1 ||
      header.iov_length != 1) {
    return false;
  }

  // Map the shared memory object to our address space.
  void* shared_region = nacl::Map(0,
                                  requested_size,
                                  nacl::kProtRead | nacl::kProtWrite,
                                  nacl::kMapShared,
                                  shared_memory,
                                  0);

  EXPECT_TRUE(shared_region != NULL);

  if (shared_region == NULL) {
    return false;
  }

  *shared_mem_address = shared_region;
  *shared_mem_id = shared_memory_id;

  return true;
}

// Sends a message to O3D to update the contents of the texture bitmap
// using the data stored in shared memory.  We pass in the shared memory handle
// returned by the server as well as an offset from the start of the shared
// memory buffer where the new texture data is.
bool TextureUpdateHelper::RequestTextureUpdate(unsigned int texture_id,
                                               int level,
                                               int shared_memory_id,
                                               size_t offset,
                                               size_t number_of_bytes) {
  MessageQueue::MessageId message_id = MessageQueue::UPDATE_TEXTURE2D;

  nacl::MessageHeader header;
  nacl::IOVec vec;
  char buffer[256];
  char *buffer_ptr = &buffer[0];

  // Message contains the message ID and two arguments, the id of the Texture
  // object in O3D and the offset in the shared memory region where the
  // bitmap is stored.
  *(reinterpret_cast<MessageQueue::MessageId*>(buffer_ptr)) = message_id;
  buffer_ptr += sizeof(message_id);
  *(reinterpret_cast<unsigned int*>(buffer_ptr)) = texture_id;
  buffer_ptr += sizeof(texture_id);
  *(reinterpret_cast<int*>(buffer_ptr)) = level;
  buffer_ptr += sizeof(level);
  *(reinterpret_cast<int*>(buffer_ptr)) = shared_memory_id;
  buffer_ptr += sizeof(shared_memory_id);
  *(reinterpret_cast<size_t*>(buffer_ptr)) = offset;
  buffer_ptr += sizeof(offset);
  *(reinterpret_cast<size_t*>(buffer_ptr)) = number_of_bytes;
  buffer_ptr += sizeof(number_of_bytes);

  vec.base = buffer;
  vec.length = buffer_ptr - &buffer[0];

  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;


  // Send message.
  int result = nacl::SendDatagram(o3d_handle_, &header, 0);

  EXPECT_EQ(vec.length, result);

  if (result != vec.length) {
    return false;
  }

  // Wait for a response from the server.  If the server returns true then
  // the texture update was successfully procesed.
  bool texture_updated = ReceiveBooleanResponse();

  EXPECT_TRUE(texture_updated);

  return texture_updated;
}



// Tests that the message queue socket is properly initialized.
TEST_F(MessageQueueTest, Initialize) {
  MessageQueue* message_queue = new MessageQueue(g_service_locator);

  EXPECT_TRUE(message_queue->Initialize());

  String socket_addr = message_queue->GetSocketAddress();

  // Make sure the name starts with "google-o3d"
  EXPECT_EQ(0, socket_addr.find("google-o3d"));

  delete message_queue;
}

// Tests that the a client can actually establish a connection to the
// MessageQueue.
TEST_F(MessageQueueTest, TestConnection) {
  MessageQueue* message_queue = new MessageQueue(g_service_locator);
  message_queue->Initialize();

  String socket_addr = message_queue->GetSocketAddress();
  std::string socket_addr_utf8;
  StringToUTF8(socket_addr, &socket_addr_utf8);

  TextureUpdateHelper helper;
  EXPECT_TRUE(helper.ConnectToO3D(socket_addr_utf8.c_str(),
                                  my_socket_handle()));

  delete message_queue;
}

// Tests a request for shared memory.
TEST_F(MessageQueueTest, GetSharedMemory) {
  MessageQueue* message_queue = new MessageQueue(g_service_locator);
  message_queue->Initialize();

  String socket_addr = message_queue->GetSocketAddress();
  std::string socket_addr_utf8;
  StringToUTF8(socket_addr, &socket_addr_utf8);

  TextureUpdateHelper helper;
  ASSERT_TRUE(helper.ConnectToO3D(socket_addr_utf8.c_str(),
                                  my_socket_handle()));

  void *shared_mem_address = NULL;
  int shared_mem_id = -1;
  bool memory_ok = helper.RequestSharedMemory(65536,
                                              &shared_mem_id,
                                              &shared_mem_address);
  EXPECT_NE(-1, shared_mem_id);
  EXPECT_TRUE(shared_mem_address != NULL);

  EXPECT_TRUE(memory_ok);

  delete message_queue;
}

// Tests a request to update a texture.
TEST_F(MessageQueueTest, UpdateTexture2D) {
  MessageQueue* message_queue = new MessageQueue(g_service_locator);
  message_queue->Initialize();

  String socket_addr = message_queue->GetSocketAddress();
  std::string socket_addr_utf8;
  StringToUTF8(socket_addr, &socket_addr_utf8);

  TextureUpdateHelper helper;
  ASSERT_TRUE(helper.ConnectToO3D(socket_addr_utf8.c_str(),
                                  my_socket_handle()));

  void *shared_mem_address = NULL;
  int shared_mem_id = -1;
  bool memory_ok = helper.RequestSharedMemory(65536,
                                              &shared_mem_id,
                                              &shared_mem_address);

  ASSERT_TRUE(memory_ok);

  Texture2D* texture = pack()->CreateTexture2D("test_texture",
                                               128,
                                               128,
                                               Texture::ARGB8,
                                               0);

  ASSERT_TRUE(texture != NULL);

  int texture_buffer_size = 128*128*4;

  EXPECT_TRUE(helper.RequestTextureUpdate(texture->id(),
                                          0,
                                          shared_mem_id,
                                          0,
                                          texture_buffer_size));

  delete message_queue;
}


}  // namespace o3d
