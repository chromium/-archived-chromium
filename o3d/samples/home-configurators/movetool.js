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
function MoveTool(context, root) {
  this.lastMove = null;
  this.context = context;
  this.root = root;
  this.transformInfo = o3djs.picking.createTransformInfo(root, null);
  this.movePlane = [0.0, 0.0, 1.0];
  this.downPoint = null;
  this.selectedObject = null;
}

/*
 * transform_node : The shape's root node that should be moving.
 *                  (the same node as would be found by our picking code)
 */
MoveTool.prototype.initializeWithShape = function(transform_node) {
  this.transformInfo.update();
  translation = g_math.matrix4.getTranslation(
      transform_node.getUpdatedWorldMatrix());
  this.downPoint = translation;
  this.selectedObject = transform_node;
  this.lastMove = null;
};

MoveTool.prototype.handleMouseDown = function(e) {
  var worldRay = o3djs.picking.clientPositionToWorldRay(e.x,
                                                        e.y,
                                                        this.context,
                                                        g_client.width,
                                                        g_client.height);
  this.transformInfo.update();
  this.selectedObject = null;
  var pickInfo = this.transformInfo.pick(worldRay);
  if (pickInfo) {
    pickedObject = pickInfo.shapeInfo.parent;
    while (pickedObject.parent != null &&
           pickedObject.parent.transform != this.root) {
      pickedObject = pickedObject.parent;
    }
    this.selectedObject = pickedObject.transform;
  }
  if (this.selectedObject) {
    this.downPoint = pickInfo.worldIntersectionPosition.slice(0, 3);
    this.lastMove = null;
  }
};

MoveTool.prototype.handleMouseUp = function(e) {
  this.downPoint = null;
};

MoveTool.prototype.handleMouseMove = function(e) {
  if (this.downPoint == null)
    return;

  if (this.selectedObject === null)
    return;

  var offset;

  if (e.x !== undefined) {
    // An O3D event
    offset = { x: e.x, y: e.y };
  } else {
    // A JS event
    e = e ? e : window.event;
    elem = e.target ? e.target : e.srcElement;

    // Check if the cursor is in the O3D area.
    if (elem !== g_o3dElement) {
      return;
    }
    offset = getRelativeCoordinates(e, elem);
  }

  var worldRay = o3djs.picking.clientPositionToWorldRay(offset.x,
                                                        offset.y,
                                                        this.context,
                                                        g_client.width,
                                                        g_client.height);
  // Compute the offset from the start point.
  var ray_dir = g_math.subVector(worldRay.far, worldRay.near);
  ray_dir = g_math.normalize(ray_dir);
  var ray_dot_plane = g_math.dot(ray_dir, this.movePlane);
  if (Math.abs(ray_dot_plane) < 0.001) {
    return [0.0, 0.0, 0.0];
  }
  var plane_distance =
      g_math.dot(this.downPoint, this.movePlane) -
      g_math.dot(worldRay.near, this.movePlane);
  t = plane_distance / ray_dot_plane;
  move_position = g_math.addVector(
      g_math.mulVectorScalar(ray_dir, t), worldRay.near);
  var offset = g_math.subVector(move_position, this.downPoint);
  translation = offset;
  if (this.lastMove != null) {
    translation = g_math.subVector(offset, this.lastMove);
  }
  this.lastMove = offset;

  if (this.selectedObject) {
    matrix = this.selectedObject.localMatrix;
    transformation = g_math.matrix4.translation(translation);
    matrix = g_math.matrix4.mul(matrix, transformation);
    this.selectedObject.localMatrix = matrix;
  }
};

MoveTool.prototype.handleKeyDown = function(key) {
  return false;
};

MoveTool.prototype.handleKeyUp = function(key) {
  // TODO: We should really have a selection instead of this
  //             dual-use of the tool's last selected object.
  if (key == DELETE || key == BACKSPACE) {
    if (this.downPoint == null && this.selectedObject != null) {
      this.selectedObject.setParent(null);
      this.selectedObject = null;
      return true;
    }
  }
  return false;
};
