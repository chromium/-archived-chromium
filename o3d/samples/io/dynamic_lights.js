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


/**
 * @fileoverview  Manage all the dynamic lights in a level.
 */


var s_light_array = new Array();
var s_number_of_lights_in_scene = 0;

var s_number_of_lights_in_program = 0;
var s_light_parameter_array = new Array();

// Call this before any UpdateLightsInProgram.
function InitializeLightParameters(context, number_of_lights) {
  s_number_of_lights_in_program = number_of_lights;
  for (var l = 0; l < number_of_lights; ++l) {
    s_light_parameter_array[l] = {};
    s_light_parameter_array[l].location = context.createParam(
        'light' + l + '_location', 'ParamFloat3');
    s_light_parameter_array[l].location.value = [0, 0, 0];

    // For the light, the final color parameter is the attentuation.
    s_light_parameter_array[l].color = context.createParam(
        'light' + l + '_color', 'ParamFloat4');
    s_light_parameter_array[l].color.value = [0, 0, 0, 1];
  }
}

// Returns the index of the new light for future manipulation.
// World location and color should be arrays.
// These will be assigned to Param objects.
function AddLight(world_location, color) {
  var light_index = s_number_of_lights_in_scene++;
  s_light_array[light_index] = {};
  s_light_array[light_index].location = world_location;
  s_light_array[light_index].color = color;
  return light_index;
}

// Sets the shader parameters to render using the closest lights
// to the 'focus' (whatever's passed in).
// |focus_location| must provide .x, .y, and .z for determining the
//                  distance to each light.
function UpdateLightsInProgram(focus_location) {
  // TODO: Modulate the lights' color / add behaviors for the lights.
  var lights_to_render = new Array();
  for (var i = 0; i < s_number_of_lights_in_program; ++i) {
    lights_to_render[i] = {};
    lights_to_render[i].distance = 10000000000000.0; // Arbitrary large number.
    lights_to_render[i].index = -1;
  }
  var number_of_lights = s_number_of_lights_in_scene;
  for (var l = 0; l < number_of_lights; ++l) {
    var x_diff = s_light_array[l].location.x - focus_location.x;
    var y_diff = s_light_array[l].location.y - focus_location.y;
    // TODO: This should be focus_location.z, but the avatar's location that is
    // passed in is offset from the world coordinates.  For our relatively flat
    // levels, this works more predictably.
    var z_diff = s_light_array[l].location.z - 0;
    var distance_to_focus = x_diff * x_diff + y_diff * y_diff + z_diff * z_diff;
    // This may change if we assign to an entry early in the list.
    var index_to_assign = l;
    for (var i = 0; i < s_number_of_lights_in_program; ++i) {
      if (distance_to_focus < lights_to_render[i].distance) {
        // Swap the index into this entry.  We will percolate the
        // light up.
        var replaced_index = lights_to_render[i].index;
        lights_to_render[i].index = index_to_assign;
        index_to_assign = replaced_index;

        var replaced_distance = lights_to_render[i].distance;
        lights_to_render[i].distance = distance_to_focus;
        distance_to_focus = replaced_distance;
      }
      if (index_to_assign < 0) {
        break;
      }
    }
  }

  // Finally, assign to the actual shader parameters.
  for (var i = 0; i < s_number_of_lights_in_program; ++i) {
    var light_index = lights_to_render[i].index;
    if (light_index >= 0) {
      s_light_parameter_array[i].location.value =
          s_light_array[light_index].location;
      s_light_parameter_array[i].color.value =
          s_light_array[light_index].color;
    }
  }
}

