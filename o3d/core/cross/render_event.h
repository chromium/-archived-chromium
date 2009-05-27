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


// This file defines the RenderEvent class.

#ifndef O3D_CORE_CROSS_RENDER_EVENT_H_
#define O3D_CORE_CROSS_RENDER_EVENT_H_

namespace o3d {

// This class is used to pass infomation to a registered onrender callback.
class RenderEvent {
 public:
  RenderEvent()
      : elapsed_time_(0.0f),
        render_time_(0.0f),
        active_time_(0.0f),
        transforms_processed_(0),
        transforms_culled_(0),
        draw_elements_processed_(0),
        draw_elements_culled_(0),
        draw_elements_rendered_(0),
        primitives_rendered_(0) {
  }

  // Use this function to get elapsed time since the last render event in
  // seconds.
  float elapsed_time() const {
    return elapsed_time_;
  }

  // The client will use this function to set the elapsed time. You should never
  // call this function.
  void set_elapsed_time(float time) {
    elapsed_time_ = time;
  }

  // Use this function to get the time it took to render the last frame.
  float render_time() const {
    return render_time_;
  }

  // The client will use this function to set the render time. You should never
  // call this function.
  void set_render_time(float time) {
    render_time_ = time;
  }

  // Use this function to get the time it took to both render the last frame.
  // and call the tick, counter and render callbacks.
  float active_time() const {
    return active_time_;
  }

  // The client will use this function to set the active time. You should never
  // call this function.
  void set_active_time(float time) {
    active_time_ = time;
  }

  // The number of transforms processed last frame.
  int transforms_processed() const {
    return transforms_processed_;
  }

  // The client uses this function to set this value
  void set_transforms_processed(int value) {
    transforms_processed_ = value;
  }

  // The number of transforms culled last frame.
  int transforms_culled() const {
    return transforms_culled_;
  }

  // The client uses this function to set this value
  void set_transforms_culled(int value) {
    transforms_culled_ = value;
  }

  // The number of draw elements processed last frame.
  int draw_elements_processed() const {
    return draw_elements_processed_;
  }

  // The client uses this function to set this value
  void set_draw_elements_processed(int value) {
    draw_elements_processed_ = value;
  }

  // The number of draw elements culled last frame.
  int draw_elements_culled() const {
    return draw_elements_culled_;
  }

  // The client uses this function to set this value
  void set_draw_elements_culled(int value) {
    draw_elements_culled_ = value;
  }

  // The number of draw elements rendered last frame. Note: a draw element can
  // be rendered more than once per frame based on how many transforms it is
  // under and how many DrawPasses use the DrawLists it is put on.
  int draw_elements_rendered() const {
    return draw_elements_rendered_;
  }

  // The client uses this function to set this value
  void set_draw_elements_rendered(int value) {
    draw_elements_rendered_ = value;
  }

  // The number of low-level primitives rendered last frame. This is the sum of
  // the number primitives (triangles, lines) submitted to the renderer.
  int primitives_rendered() const {
    return primitives_rendered_;
  }

  // The client uses this function to set this value
  void set_primitives_rendered(int value) {
    primitives_rendered_ = value;
  }
 private:
  // This is the elapsed time in seconds since the last render event.
  float elapsed_time_;

  // This is the time it took to render or rather, the time it took to submit
  // everything to D3D9 or GL and flip buffers.
  float render_time_;

  // The time it took to both render the last frame. and call the tick, counter
  // and render callbacks.
  float active_time_;

  int transforms_processed_;
  int transforms_culled_;
  int draw_elements_processed_;
  int draw_elements_culled_;
  int draw_elements_rendered_;
  int primitives_rendered_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_RENDER_EVENT_H_
