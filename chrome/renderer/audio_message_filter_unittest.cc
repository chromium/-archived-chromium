// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/audio_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAudioDelegate : public AudioMessageFilter::Delegate {
 public:
  MockAudioDelegate() {
    Reset();
  }

  virtual void OnRequestPacket(size_t bytes_in_buffer,
                               const base::Time& message_timestamp) {
    request_packet_received_ = true;
    bytes_in_buffer_ = bytes_in_buffer;
    message_timestamp_ = message_timestamp;
  }

  virtual void OnStateChanged(AudioOutputStream::State state, int info) {
    state_changed_received_ = true;
    state_ = state;
    info_ = info;
  }

  virtual void OnCreated(base::SharedMemoryHandle handle, size_t length) {
    created_received_ = true;
    handle_ = handle;
    length_ = length;
  }

  virtual void OnVolume(double left, double right) {
    volume_received_ = true;
    left_ = left;
    right_ = right;
  }

  void Reset() {
    request_packet_received_ = false;
    bytes_in_buffer_ = 0;
    message_timestamp_ = base::Time();

    state_changed_received_ = false;
    state_ = AudioOutputStream::STATE_ERROR;
    info_ = 0;

    created_received_ = false;
    handle_ = base::SharedMemory::NULLHandle();
    length_ = 0;

    volume_received_ = false;
    left_ = 0;
    right_ = 0;
  }

  bool request_packet_received() { return request_packet_received_; }
  size_t bytes_in_buffer() { return bytes_in_buffer_; }
  const base::Time& message_timestamp() { return message_timestamp_; }

  bool state_changed_received() { return state_changed_received_; }
  AudioOutputStream::State state() { return state_; }
  int info() { return info_; }

  bool created_received() { return created_received_; }
  base::SharedMemoryHandle handle() { return handle_; }
  size_t length() { return length_; }

  bool volume_received() { return volume_received_; }
  double left() { return left_; }
  double right() { return right_; }

 private:
  bool request_packet_received_;
  size_t bytes_in_buffer_;
  base::Time message_timestamp_;

  bool state_changed_received_;
  AudioOutputStream::State state_;
  int info_;

  bool created_received_;
  base::SharedMemoryHandle handle_;
  size_t length_;

  bool volume_received_;
  double left_;
  double right_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDelegate);
};

}  // namespace

TEST(AudioMessageFilterTest, Basic) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<AudioMessageFilter> filter = new AudioMessageFilter(kRouteId);

  MockAudioDelegate delegate;
  int stream_id = filter->AddDelegate(&delegate);

  // ViewMsg_RequestAudioPacket
  const size_t kSizeInBuffer = 1024;
  const int64 kMessageTimestamp = 99;
  EXPECT_FALSE(delegate.request_packet_received());
  filter->OnMessageReceived(ViewMsg_RequestAudioPacket(kRouteId,
                                                       stream_id,
                                                       kSizeInBuffer,
                                                       kMessageTimestamp));
  EXPECT_TRUE(delegate.request_packet_received());
  EXPECT_EQ(kSizeInBuffer, delegate.bytes_in_buffer());
  EXPECT_EQ(kMessageTimestamp, delegate.message_timestamp().ToInternalValue());
  delegate.Reset();

  // ViewMsg_NotifyAudioStreamStateChanged
  const AudioOutputStream::State kState = AudioOutputStream::STATE_STARTED;
  const int kStateInfo = 100;
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      ViewMsg_NotifyAudioStreamStateChanged(kRouteId,
                                            stream_id,
                                            kState,
                                            kStateInfo));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_TRUE(kState == delegate.state());
  EXPECT_EQ(kStateInfo, delegate.info());
  delegate.Reset();

  // ViewMsg_NotifyAudioStreamCreated
  const size_t kLength = 1024;
  EXPECT_FALSE(delegate.created_received());
  filter->OnMessageReceived(
      ViewMsg_NotifyAudioStreamCreated(kRouteId,
                                       stream_id,
                                       base::SharedMemory::NULLHandle(),
                                       kLength));
  EXPECT_TRUE(delegate.created_received());
  EXPECT_FALSE(base::SharedMemory::IsHandleValid(delegate.handle()));
  EXPECT_EQ(kLength, delegate.length());
  delegate.Reset();

  // ViewMsg_NotifyAudioStreamVolume
  const double kLeftVolume = 1.0;
  const double kRightVolume = 2.0;
  EXPECT_FALSE(delegate.volume_received());
  filter->OnMessageReceived(
      ViewMsg_NotifyAudioStreamVolume(kRouteId, stream_id,
                                      kLeftVolume, kRightVolume));
  EXPECT_TRUE(delegate.volume_received());
  EXPECT_EQ(kLeftVolume, delegate.left());
  EXPECT_EQ(kRightVolume, delegate.right());
  delegate.Reset();

  message_loop.RunAllPending();
}

TEST(AudioMessageFilterTest, Delegates) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<AudioMessageFilter> filter = new AudioMessageFilter(kRouteId);

  MockAudioDelegate delegate1;
  MockAudioDelegate delegate2;

  int stream_id1 = filter->AddDelegate(&delegate1);
  int stream_id2 = filter->AddDelegate(&delegate2);

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id1, 0, 0));
  EXPECT_TRUE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id2, 0, 0));
  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_TRUE(delegate2.request_packet_received());
  delegate2.Reset();

  // Send a message of a different route id, a message is not received.
  EXPECT_FALSE(delegate1.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId + 1, stream_id1, 0, 0));
  EXPECT_FALSE(delegate1.request_packet_received());

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(stream_id1);
  EXPECT_FALSE(delegate1.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id1, 0, 0));
  EXPECT_FALSE(delegate1.request_packet_received());

  filter->RemoveDelegate(stream_id2);
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id2, 0, 0));
  EXPECT_FALSE(delegate2.request_packet_received());

  message_loop.RunAllPending();
}
