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
 * The Pan tool allows the user to pan around the view by clicking and dragging.
 */
function PanTool(camera) {
  this.camera = camera;
  this.dragging = false;
  this.lastOffset = null;
}

PanTool.prototype.handleMouseDown = function(e) {
  var offset = { x: e.x, y: e.y };
  this.lastOffset = offset;
  this.dragging = true;
};

PanTool.prototype.handleMouseMove = function(e) {
  if (this.dragging && e.x !== undefined) {
    var offset = { x: e.x, y: e.y };

    dY = (offset.y - this.lastOffset.y);
    dX = -(offset.x - this.lastOffset.x);
    this.lastOffset = offset;

    this.camera.target.x -= (dY * Math.cos(this.camera.eye.rotZ) +
        dX * Math.sin(this.camera.eye.rotZ)) /
        (700 / this.camera.eye.distanceFromTarget);
    this.camera.target.y += (-dY * Math.sin(this.camera.eye.rotZ) +
        dX * Math.cos(this.camera.eye.rotZ)) /
        (700 / this.camera.eye.distanceFromTarget);

    this.camera.update();
  }
};

PanTool.prototype.handleMouseUp = function(e) {
  this.dragging = false;
};

PanTool.prototype.handleKeyDown = function(e) {
  return false;
};

PanTool.prototype.handleKeyUp = function(e) {
  return false;
};
