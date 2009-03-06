// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Mock implementation of Pipeline.  Simply provides getters/setters for every
// pipeline state variable and queues all tasks posted to the "pipeline thread."
// Since there actually isn't a separate thread unit tests can control when
// they want to execute queued tasks by calling RunAllTasks(), which helps to
// assert pre- and post-conditions.

#ifndef MEDIA_BASE_MOCK_PIPELINE_H_
#define MEDIA_BASE_MOCK_PIPELINE_H_

#include <deque>
#include <string>

#include "media/base/media_format.h"
#include "media/base/pipeline.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockPipeline : public media::Pipeline {
 public:
  MockPipeline() {
    Reset(false);
  }

  virtual ~MockPipeline() {}

  // Implementation of PipelineStatus interface.
  virtual bool IsInitialized() const {
    return initialized_;
  }

  virtual base::TimeDelta GetDuration() const {
    return duration_;
  }

  virtual base::TimeDelta GetBufferedTime() const {
    return buffered_time_;
  }

  virtual int64 GetTotalBytes() const {
    return total_bytes_;
  }

  virtual int64 GetBufferedBytes() const {
    return buffered_bytes_;
  }

  virtual void GetVideoSize(size_t* width_out, size_t* height_out) const {
    *width_out = width_;
    *height_out = height_;
  }

  virtual float GetVolume() const {
    return volume_;
  }

  virtual float GetPlaybackRate() const {
    return playback_rate_;
  }

  virtual base::TimeDelta GetTime() const {
    return time_;
  }

  virtual base::TimeDelta GetInterpolatedTime() const {
    return time_;
  }

  virtual PipelineError GetError() const {
    return error_;
  }

  // Implementation of Pipeline interface.
  virtual bool Start(FilterFactory* filter_factory,
                     const std::string& url,
                     Callback1<bool>::Type* init_complete_callback) {
    EXPECT_FALSE(initialized_);
    initialized_ = true;
    if (init_complete_callback) {
      init_complete_callback->Run(true);
      delete init_complete_callback;
    }
    return true;
  }

  virtual void Stop() {
    EXPECT_TRUE(initialized_ || error_ != media::PIPELINE_OK);
    Reset(false);
  }

  virtual void SetPlaybackRate(float playback_rate) {
    playback_rate_ = playback_rate;
  }

  virtual void Seek(base::TimeDelta time) {
    time_ = time;
  }

  virtual void SetVolume(float volume) {
    volume_ = volume;
  }

  // Public methods used by tests and by MockFilterHost to manipulate the
  // state of the mock pipeline.

  // Set the state to the same as a newly created MockPipeline.  If
  // |reset_to_initialized| is true then the pipeline's |initialized_| state
  // will be true when this method returns.
  void Reset(bool reset_to_initialized) {
    error_ = media::PIPELINE_OK;
    volume_ = 1.0f;
    playback_rate_ = 0.0f;
    initialized_ = reset_to_initialized;
    time_ = base::TimeDelta();
    duration_ = base::TimeDelta();
    buffered_time_ = base::TimeDelta();
    width_ = 0;
    height_ = 0;
    buffered_bytes_ = 0;
    total_bytes_ = 0;
  }

  // Runs all queued tasks until there are no more.
  //
  // Although it is possible for tasks to run indefinitely (executing tasks post
  // additional tasks), such situations should be treated as a bug.  Since the
  // pipeline is request/pull-based, only enough tasks to satisfy the request
  // should ever be executed.
  void RunAllTasks() {
    while (!task_queue_.empty()) {
      Task* task = task_queue_.front();
      task_queue_.pop_front();
      task->Run();
      delete task;
    }
  }

  void PostTask(Task* task) {
    EXPECT_TRUE(task);
    task_queue_.push_back(task);
  }

  void Error(media::PipelineError error) {
    initialized_ = false;
    error_ = error;
  }

  void SetTime(base::TimeDelta time) {
    time_ = time;
  }

  virtual void SetDuration(base::TimeDelta duration) {
    duration_ = duration;
  }

  virtual void SetBufferedTime(base::TimeDelta buffered_time) {
    buffered_time = buffered_time;
  }

  virtual void SetTotalBytes(int64 total_bytes) {
    total_bytes_ = total_bytes;
  }

  virtual void SetBufferedBytes(int64 buffered_bytes) {
    buffered_bytes_ = buffered_bytes;
  }

  virtual void SetVideoSize(size_t width, size_t height) {
    width_ = width;
    height_ = height;
  }

 private:
  PipelineError error_;
  float volume_;
  float playback_rate_;
  bool initialized_;
  base::TimeDelta time_;
  base::TimeDelta duration_;
  base::TimeDelta buffered_time_;
  size_t width_;
  size_t height_;
  int64 buffered_bytes_;
  int64 total_bytes_;

  typedef std::deque<Task*> TaskQueue;
  TaskQueue task_queue_;

  DISALLOW_COPY_AND_ASSIGN(MockPipeline);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_PIPELINE_H_
