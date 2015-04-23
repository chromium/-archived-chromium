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


// This file contains the declaration of the MessageQueue, the class which
// allows external code (clients) to connect via the IMC library to O3D
// (server) and issue calls to it.

#ifndef O3D_CORE_CROSS_MESSAGE_QUEUE_H_
#define O3D_CORE_CROSS_MESSAGE_QUEUE_H_

#include <vector>
#include "native_client/src/shared/imc/nacl_imc.h"
#include "core/cross/types.h"

namespace o3d {

class ServiceLocator;
class ObjectManager;

// Structure keeping information about shared memory regions created on the
// request of a client connection.
typedef struct {
  // Unique (to the MessageQueue object that created it) id of the shared
  // memory buffer.
  int buffer_id_;
  // Handle to the shared memory.
  nacl::Handle shared_memory_handle_;
  // Address to which it maps in the local memory space.
  void* mapped_address_;
  // Size in bytes
  int32 size_;
} SharedMemoryInfo;

// The ConnectedClient class holds information about clients that have made
// contact with the this instance of the server.
class ConnectedClient {
 public:
  explicit ConnectedClient(nacl::Handle handle);

  ~ConnectedClient();

  // Registers a newly created shared memory buffer with the client.
  // Parameters:
  //  id - the unique id of the shared memory buffer.
  //  handle - the nacl handle to the shared memory.
  //  address - the beginning of the buffer in the local address space.
  //  size - the size of the buffer in bytes.
  void RegisterSharedMemory(int id,
                            nacl::Handle handle,
                            void *address,
                            int32 size);

  // Returns the socket handle the client uses to talk to the server.
  nacl::Handle client_handle() { return client_handle_; }

  // Returns the shared memory record that matches the given buffer id.
  const SharedMemoryInfo* GetSharedMemoryInfo(int buffer_id) const;

 private:
  // Handle of the socket the client uses.
  nacl::Handle client_handle_;

  std::vector<SharedMemoryInfo> shared_memory_array_;
};


// The MessageQueue class handles the communcation between external code and
// O3D.  It provides methods for initializing the communcation channel
// as well as reading data from and writing data to it.  It is currently using
// the NativeClient Inter-Module Communication (IMC) library.
class MessageQueue {
 public:
  enum MessageId {
    INVALID_ID = 0,
    HELLO,                   // Handshake between the client and the server
    ALLOCATE_SHARED_MEMORY,  // Request to allocate a shared memory buffer
    UPDATE_TEXTURE2D,        // Request to update a 2D texture bitmap
    MAX_NUM_IDS,

    ID_FORCE_DWORD = 0x7fffffff  // Forces a 32-bit size enum
  };

  // Creates a MessageQueue that is able to receive messages and execute calls
  // to the given Client object.
  explicit MessageQueue(ServiceLocator* service_locator);

  ~MessageQueue();

  // Initializes the communcation channel.
  // Returns:
  //   true if the channel was initialized successfully, false otherwise.
  bool Initialize();

  // Checks for new messages on the queue.  If messages are found then it
  // it processes them, otherwise it simply returns.
  // Returns:
  //   true if there were no new messages or new messages were succesfully
  //   received.
  bool CheckForNewMessages();

  // Returns the socket address used by the message queue.
  String GetSocketAddress() const;

 private:
  // Processes a request from an already connected client.  It parses the
  // parameters provided in the message and executes the appropriate action.
  // Parameters:
  //   client - pointer to the ConnectedClient the request came from.
  //   message_length - length of the received message in bytes.
  //   message_id - id of the request received by the message
  //   header - message header containing information about the received
  //            message.
  //   handles - the array of handles referenced by the header.
  // Returns:
  //   true if the message is properly formed and is succesfully handled by the
  //   Client.
  bool ProcessClientRequest(ConnectedClient* client,
                            int message_length,
                            MessageId message_id,
                            nacl::MessageHeader* header,
                            nacl::Handle* handles);

  // Processes the HELLO message, which initiates a new connection with a
  // client.
  // Parameters:
  //   header - message header containing information about the received
  //            message.
  //   handles - the array of handles referenced by the header.
  // Returns:
  //   true if the new client connected successfully.
  bool ProcessHelloMessage(nacl::MessageHeader* header, nacl::Handle* handles);

  // Processes a request by a connected client to allocate a shared memory
  // buffer.  The size of the requested buffer is determined from the data
  // passed in the message.  Once the memory is allocated, a message containting
  // the shared memory handle is sent back to the client.
  // Parameters:
  //   client - pointer to the ConnectedClient the request came from.
  //   message_length - length of the received message in bytes.
  //   message_id - id of the request received by the message
  //   header - message header containing information about the received
  //            message.
  //   handles - the array of handles referenced by the header.
  // Returns:
  //   true if the message is properly formed and is succesfully handled by the
  //   Client.
  bool ProcessAllocateSharedMemory(ConnectedClient* client,
                                   int message_length,
                                   MessageId message_id,
                                   nacl::MessageHeader* header,
                                   nacl::Handle* handles);

  // Processes a request by a connected client to update the contents of the
  // bitmap corresponding to a Texture2D object.
  // Parameters:
  //   client - pointer to the ConnectedClient the request came from.
  //   message_length - length of the received message in bytes.
  //   message_id - id of the request received by the message
  //   header - message header containing information about the received
  //            message.
  //   handles - the array of handles referenced by the header.
  // Returns:
  //   true if the message is properly formed and is succesfully handled by the
  //   Client.
  bool ProcessUpdateTexture2D(ConnectedClient* client,
                              int message_length,
                              MessageId message_id,
                              nacl::MessageHeader* header,
                              nacl::Handle* handles);

  // Sends a true of false (1 or 0) message using the given socket handle.
  // Parameters:
  //   client_handle - handle of socket to send the response to.
  //   value - value to send.
  // Returns:
  //   true on success.
  bool SendBooleanResponse(nacl::Handle client_handle, bool value);

  // Checks a socket for a message without blocking waiting for one.  If a
  // valid message is found then it returns the message id.
  // Parameters:
  //   socket - socket to check for messages.
  //   header - pointer to the message header used for receiving the message.
  //   message_id [out] - the ID of the message received.
  //   message_length [out] - the length of the message received.
  // Returns:
  //   true if a message with a valid ID is found.  true if no message is found.
  //   false in every other case.
  bool ReceiveMessageFromSocket(nacl::Handle socket,
                                nacl::MessageHeader* header,
                                MessageId* message_id,
                                int* message_length);

 private:

  ServiceLocator* service_locator_;
  ObjectManager* object_manager_;

  // A list of clients that are communicating via the message queue.
  std::vector<ConnectedClient*> connected_clients_;

  // Handle of server (i.e. O3D) bound socket.
  nacl::Handle server_socket_handle_;

  // Address of the server socket used by this MessageQueue.
  nacl::SocketAddress server_socket_address_;

  // Stores the next available unique id that can be assigned to a newly
  // created shared memory buffer.
  int32 next_shared_memory_id_;

  // Stores the next available unique id for message queues.  This allows
  // us to create multiple instances of the MessageQueue, each with a unique
  // address.
  static int next_message_queue_id_;
};


}  // namespace o3d


#endif  // O3D_CORE_CROSS_MESSAGE_QUEUE_H_
