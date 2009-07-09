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


// This file contains the implementation for the Client class.

#include "core/cross/precompile.h"
#include "core/cross/client.h"
#include "core/cross/draw_context.h"
#include "core/cross/effect.h"
#include "core/cross/message_queue.h"
#include "core/cross/pack.h"
#include "core/cross/shape.h"
#include "core/cross/transform.h"
#include "core/cross/material.h"
#include "core/cross/renderer.h"
#include "core/cross/viewport.h"
#include "core/cross/clear_buffer.h"
#include "core/cross/state_set.h"
#include "core/cross/tree_traversal.h"
#include "core/cross/draw_pass.h"
#include "core/cross/bounding_box.h"
#include "core/cross/bitmap.h"
#include "core/cross/error.h"
#include "core/cross/evaluation_counter.h"
#include "core/cross/id_manager.h"
#include "core/cross/profiler.h"
#include "utils/cross/string_writer.h"
#include "utils/cross/json_writer.h"

#ifdef OS_WIN
#include "core/cross/core_metrics.h"
#endif

using std::map;
using std::vector;
using std::make_pair;

namespace o3d {

// Client constructor.  Creates the default root node for the scenegraph
Client::Client(ServiceLocator* service_locator)
    : service_locator_(service_locator),
      object_manager_(service_locator),
      profiler_(service_locator),
      error_status_(service_locator),
      draw_list_manager_(service_locator),
      counter_manager_(service_locator),
      transformation_context_(service_locator),
      semantic_manager_(service_locator),
      rendering_(false),
      render_tree_called_(false),
      renderer_(service_locator),
      evaluation_counter_(service_locator),
      event_manager_(),
      root_(NULL),
      render_mode_(RENDERMODE_CONTINUOUS),
      last_tick_time_(0),
#ifdef OS_WIN
      calls_(0),
#endif
      rendergraph_root_(NULL),
      id_(IdManager::CreateId()) {
  // Create and initialize the message queue to allow external code to
  // communicate with the Client via RPC calls.
  message_queue_.reset(new MessageQueue(service_locator_));

  if (!message_queue_->Initialize()) {
    LOG(ERROR) << "Client failed to initialize the message queue";
    message_queue_.reset(NULL);
  }
}

// Frees up all the resources allocated by the Client factory methods but
// does not destroy the "renderer_" object.
Client::~Client() {
  root_.Reset();
  rendergraph_root_.Reset();

  object_manager_->DestroyAllPacks();

  // Unmap the client from the renderer on exit.
  if (renderer_.IsAvailable()) {
    renderer_->UninitCommon();
  }
}

// Assigns a Renderer to the Client, and also assigns the Client
// to the Renderer and sets up the default render graph
void Client::Init() {
  if (!renderer_.IsAvailable())
    return;

  // Create the root node for the scenegraph.  Note that the root lives
  // outside of a pack object.  The root's lifetime is directly bound to that
  // of the client.
  root_ = Transform::Ref(new Transform(service_locator_));
  root_->set_name(O3D_STRING_CONSTANT("root"));

  // Creates the root for the render graph.
  rendergraph_root_ = RenderNode::Ref(new RenderNode(service_locator_));
  rendergraph_root_->set_name(O3D_STRING_CONSTANT("root"));

  // Let the renderer Init a few common things.
  renderer_->InitCommon();
}

void Client::Cleanup() {
  ClearRenderCallback();
  ClearPostRenderCallback();
  ClearTickCallback();
  event_manager_.ClearAll();
  counter_manager_.ClearAllCallbacks();
}

Pack* Client::CreatePack() {
  if (!renderer_.IsAvailable()) {
    O3D_ERROR(service_locator_)
        << "No Renderer available, Pack creation not allowed.";
    return NULL;
  }

  return object_manager_->CreatePack();
}

// Tick Methods ----------------------------------------------------------------

void Client::SetTickCallback(
    TickCallback* tick_callback) {
  tick_callback_manager_.Set(tick_callback);
}

void Client::ClearTickCallback() {
  tick_callback_manager_.Clear();
}

bool Client::Tick() {
  ElapsedTimeTimer timer;
  float seconds_elapsed = tick_elapsed_time_timer_.GetElapsedTimeAndReset();
  tick_event_.set_elapsed_time(seconds_elapsed);
  profiler_->ProfileStart("Tick callback");
  tick_callback_manager_.Run(tick_event_);
  profiler_->ProfileStop("Tick callback");

  evaluation_counter_->InvalidateAllParameters();

  counter_manager_.AdvanceCounters(1.0f, seconds_elapsed);

  // Processes any incoming message found in the message queue.  Note that this
  // call does not block if no new messages are found.
  bool message_check_ok = true;

  if (message_queue_.get()) {
    profiler_->ProfileStart("CheckForNewMessages");
    message_check_ok = message_queue_->CheckForNewMessages();
    profiler_->ProfileStop("CheckForNewMessages");
  }

  event_manager_.ProcessQueue();
  event_manager_.ProcessQueue();
  event_manager_.ProcessQueue();
  event_manager_.ProcessQueue();

  last_tick_time_ = timer.GetElapsedTimeAndReset();

  return message_check_ok;
}

// Render Methods --------------------------------------------------------------

void Client::SetLostResourcesCallback(LostResourcesCallback* callback) {
  if (!renderer_.IsAvailable()) {
    O3D_ERROR(service_locator_) << "No Renderer";
  } else {
    renderer_->SetLostResourcesCallback(callback);
  }
}

void Client::ClearLostResourcesCallback() {
  if (renderer_.IsAvailable()) {
    renderer_->ClearLostResourcesCallback();
  }
}

void Client::RenderClient() {
  ElapsedTimeTimer timer;
  rendering_ = true;
  render_tree_called_ = false;
  total_time_to_render_ = 0.0f;

  if (!renderer_.IsAvailable())
    return;

  if (renderer_->StartRendering()) {
    counter_manager_.AdvanceRenderFrameCounters(1.0f);

    profiler_->ProfileStart("Render callback");
    render_callback_manager_.Run(render_event_);
    profiler_->ProfileStop("Render callback");

    if (!render_tree_called_) {
      RenderNode* rendergraph_root = render_graph_root();
      // If nothing was rendered and there are no render graph nodes then
      // clear the client area.
      if (!rendergraph_root || rendergraph_root->children().empty()) {
          renderer_->Clear(Float4(0.4f, 0.3f, 0.3f, 1.0f),
                           true, 1.0, true, 0, true);
      } else if (rendergraph_root) {
        RenderTree(rendergraph_root);
      }
    }

    // Call post render callback.
    profiler_->ProfileStart("Post-render callback");
    post_render_callback_manager_.Run(render_event_);
    profiler_->ProfileStop("Post-render callback");

    renderer_->FinishRendering();

    // Update Render stats.
    render_event_.set_elapsed_time(
        render_elapsed_time_timer_.GetElapsedTimeAndReset());
    render_event_.set_render_time(total_time_to_render_);
    render_event_.set_transforms_culled(renderer_->transforms_culled());
    render_event_.set_transforms_processed(renderer_->transforms_processed());
    render_event_.set_draw_elements_culled(renderer_->draw_elements_culled());
    render_event_.set_draw_elements_processed(
        renderer_->draw_elements_processed());
    render_event_.set_draw_elements_rendered(
        renderer_->draw_elements_rendered());

    render_event_.set_primitives_rendered(renderer_->primitives_rendered());

    render_event_.set_active_time(
        timer.GetElapsedTimeAndReset() + last_tick_time_);
    last_tick_time_ = 0.0f;

#ifdef OS_WIN
    // Update render metrics
    metric_render_elapsed_time.AddSample(  // Convert to ms.
        static_cast<int>(1000 * render_event_.elapsed_time()));
    metric_render_time_seconds += static_cast<uint64>(
        render_event_.render_time());
    metric_render_xforms_culled.AddSample(render_event_.transforms_culled());
    metric_render_xforms_processed.AddSample(
        render_event_.transforms_processed());
    metric_render_draw_elts_culled.AddSample(
        render_event_.draw_elements_culled());
    metric_render_draw_elts_processed.AddSample(
        render_event_.draw_elements_processed());
    metric_render_draw_elts_rendered.AddSample(
        render_event_.draw_elements_rendered());
    metric_render_prims_rendered.AddSample(render_event_.primitives_rendered());
#endif  // OS_WIN
  }

  rendering_ = false;
}


// Executes draw calls for all visible shapes in a subtree
void Client::RenderTree(RenderNode *tree_root) {
  render_tree_called_ = true;

  if (!renderer_.IsAvailable())
    return;

  // Only render the shapes if BeginDraw() succeeds
  profiler_->ProfileStart("RenderTree");
  ElapsedTimeTimer time_to_render_timer;
  if (renderer_->BeginDraw()) {
    RenderContext render_context(renderer_.Get());

    if (tree_root) {
      tree_root->RenderTree(&render_context);
    }

    draw_list_manager_.Reset();

    // Finish up.
    renderer_->EndDraw();
  }
  total_time_to_render_ += time_to_render_timer.GetElapsedTimeAndReset();
  profiler_->ProfileStop("RenderTree");
}

void Client::SetRenderCallback(RenderCallback* render_callback) {
  render_callback_manager_.Set(render_callback);
}

void Client::ClearRenderCallback() {
  render_callback_manager_.Clear();
}

void Client::SetEventCallback(Event::Type type,
                              EventCallback* event_callback) {
  event_manager_.SetEventCallback(type, event_callback);
}

void Client::SetEventCallback(String type_name,
                              EventCallback* event_callback) {
  Event::Type type = Event::TypeFromString(type_name.c_str());
  if (!Event::ValidType(type)) {
    O3D_ERROR(service_locator_) << "Invalid event type: '" <<
        type_name << "'.";
  } else {
    event_manager_.SetEventCallback(type, event_callback);
  }
}

void Client::ClearEventCallback(Event::Type type) {
  event_manager_.ClearEventCallback(type);
}

void Client::ClearEventCallback(String type_name) {
  Event::Type type = Event::TypeFromString(type_name.c_str());
  if (!Event::ValidType(type)) {
    O3D_ERROR(service_locator_) << "Invalid event type: '" <<
        type_name << "'.";
  } else {
    event_manager_.ClearEventCallback(type);
  }
}

void Client::AddEventToQueue(const Event& event) {
  event_manager_.AddEventToQueue(event);
}

void Client::SendResizeEvent(int width, int height, bool fullscreen) {
  Event event(Event::TYPE_RESIZE);
  event.set_size(width, height, fullscreen);
  AddEventToQueue(event);
}

void Client::set_render_mode(RenderMode render_mode) {
  render_mode_ = render_mode;
}

void Client::SetPostRenderCallback(RenderCallback* post_render_callback) {
  post_render_callback_manager_.Set(post_render_callback);
}

void Client::ClearPostRenderCallback() {
  post_render_callback_manager_.Clear();
}

void Client::SetRenderOnDemandCallback(
    RenderOnDemandCallback* render_on_demand_callback) {
  render_on_demand_callback_manager_.Set(render_on_demand_callback);
}

void Client::ClearRenderOnDemandCallback() {
  render_on_demand_callback_manager_.Clear();
}

void Client::Render() {
  if (render_mode() == RENDERMODE_ON_DEMAND) {
    render_on_demand_callback_manager_.Run();
  }
}

void Client::SetErrorTexture(Texture* texture) {
  renderer_->SetErrorTexture(texture);
}

void Client::InvalidateAllParameters() {
  evaluation_counter_->InvalidateAllParameters();
}

bool Client::SaveScreen(const String& file_name) {
  if (!renderer_.IsAvailable()) {
    O3D_ERROR(service_locator_) << "No Render Device Available";
    return false;
  } else {
    return renderer_->SaveScreen(file_name);
  }
}

String Client::GetMessageQueueAddress() const {
  if (message_queue_.get()) {
    return message_queue_->GetSocketAddress();
  } else {
    O3D_ERROR(service_locator_) << "Message queue not initialized";
    return String("");
  }
}

// Error Related methods -------------------------------------------------------

void Client::SetErrorCallback(ErrorCallback* callback) {
  error_status_.SetErrorCallback(callback);
}

void Client::ClearErrorCallback() {
  error_status_.ClearErrorCallback();
}

const String& Client::GetLastError() const {
  return error_status_.GetLastError();
}

void Client::ClearLastError() {
  error_status_.ClearLastError();
}

void Client::ProfileStart(const std::string& key) {
  profiler_->ProfileStart(key);
}

void Client::ProfileStop(const std::string& key) {
  profiler_->ProfileStop(key);
}

void Client::ProfileReset() {
  profiler_->ProfileReset();
}

String Client::ProfileToString() {
  StringWriter string_writer(StringWriter::LF);
  JsonWriter json_writer(&string_writer, 2);
  profiler_->Write(&json_writer);
  json_writer.Close();
  return string_writer.ToString();
}
}  // namespace o3d
