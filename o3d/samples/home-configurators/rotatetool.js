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
 * A tool to move components around in the scene.  Will constrain movement to
 * the ground plane.
 *
 * Requires the picking Client 3d Utilities API.
 */
function RotateTool(context, root) {
  this.downPoint = null;
  this.lastRotation = 0;
  this.context = context;
  this.selectedObject = null;
  this.root = root;
  this.transformInfo = o3djs.picking.createTransformInfo(root, null);
  this.rotateAxis = [0.0, 0.0, 1.0];
  this.rotateCenter = null;
}

RotateTool.prototype.handleMouseDown = function(e) {
  this.downPoint = { x: e.x, y: e.y };
  var window_width = g_client.width;
  var window_height = g_client.height;
  var worldRay = o3djs.picking.clientPositionToWorldRay(this.downPoint.x,
                                                        this.downPoint.y,
                                                        this.context,
                                                        window_width,
                                                        window_height);
  this.transformInfo.update();
  var pickInfo = this.transformInfo.pick(worldRay);
  if (pickInfo) {
    pickedObject = pickInfo.shapeInfo.parent;
    while (pickedObject.parent != null &&
           pickedObject.parent.transform != this.root) {
      dump(pickedObject.transform.name + '\n');
      pickedObject = pickedObject.parent;
    }
    this.selectedObject = pickedObject.transform;
  } else {
    this.selectedObject = null;
  }
  if (this.selectedObject) {
    dump(this.selectedObject.name + '\n');
    this.lastRotation = 0.0;
  } else {
    this.downPoint = null;
  }
};

RotateTool.prototype.handleMouseUp = function(e) {
  this.downPoint = null;
  this.selectedObject = null;
};

RotateTool.prototype.handleMouseMove = function(e) {
  if (this.downPoint == null)
    return;

  if (e.x === undefined)
    return;

  offset = this.downPoint.x - e.x;
  radian_rotation = -10.0 * Math.round(offset / 10.0) * (Math.PI / 180.0);

  relative_rotation = radian_rotation - this.lastRotation;
  this.lastRotation = radian_rotation;

  if (relative_rotation != 0 && this.selectedObject) {
    matrix = this.selectedObject.localMatrix;
    transformation = g_math.matrix4.axisRotation(this.rotateAxis,
                                                 relative_rotation);
    matrix = g_math.matrix4.mul(transformation, matrix);
    this.selectedObject.localMatrix = matrix;
  }
};

RotateTool.prototype.handleKeyDown = function(e) {
  return false;
};

RotateTool.prototype.handleKeyUp = function(e) {
  return false;
};
