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
 * @fileoverview This file contains functions used controlling the camera.
 *
 */

 /**
  * Clamps a value between bounds.
  *
  * @param {number} inputValue Value to be clamped.
  * @param {number} lowerBound Lower limit.
  * @param {number} upperBound Upper limit.
  * @return {number} The clamped value.
  */
function clamp(inputValue, lowerBound, upperBound) {
  var resultValue = inputValue;
  if (inputValue < lowerBound) resultValue = lowerBound;
  if (inputValue > upperBound) resultValue = upperBound;
  return resultValue;
}

/**
 * Functions for control based on the mouse
 */
var dragging = false;
var g_rotX = 2.85;
var g_rotY = 0;
var g_thisClick;
var g_lastClick;
var g_cameraZoom = 1.0;

function startDragging(e) {
  g_thisClick = e;

  dragging = true;
}

/**
 * Scroll wheel handler. Adjusts g_cameraZoom global according to wheel
 * movement then causes a recalculation of the projection matrix.
 * The goal is to simulate a zoom lens rather than to move the viewpoint.
 * @param {Event} e Wheel event to handle.
 */
function scrollMe(e) {
  if (e.deltaY) {
    g_cameraZoom = clamp(g_cameraZoom * ((e.deltaY < 0 ? 13 : 11) / 12),
                         0.1,
                         4.0);
    resize();
  }
}

function drag(e) {
  if (dragging) {
    var scale = -.001 * g_cameraZoom;

    g_lastClick = g_thisClick;
    g_thisClick = e;

    var xLowerLimit = 2.3;
    var xUpperLimit = 4.2;
    var yLowerLimit = -0.6;
    var yUpperLimit = 0.25;

    g_rotX = clamp(g_rotX - scale * (g_thisClick.x - g_lastClick.x),
                   xLowerLimit, xUpperLimit);
    g_rotY = clamp(g_rotY + scale * (g_thisClick.y - g_lastClick.y),
                   yLowerLimit, yUpperLimit);

    setViewFromRotation();
  }
}

function setViewFromRotation() {
  var rotMatX = g_math.matrix4.rotationY(g_rotX);
  var right = g_math.matrix4.transformDirection(rotMatX, [0, 0, 1]);
  var camera_X = g_math.normalize(g_math.cross([0, 1, 0], right));
  var rotMatY = g_math.matrix4.axisRotation(camera_X, g_rotY);
  var rot_mat = g_math.matrix4.mul(rotMatX, rotMatY);
  g_root.identity();

  // TODO: eye position should be a variable
  var eye = [-2.751, 3.529, -8.563];
  g_viewInfo.drawContext.view = g_math.matrix4.lookAt(
      eye,
      g_math.addVector(eye, g_math.mulVectorMatrix([0, 0, 1, 0], rot_mat)),
      g_math.mulVectorMatrix([0, 1, 0, 0], rot_mat));
}

function stopDragging(e) {
  dragging = false;
}
