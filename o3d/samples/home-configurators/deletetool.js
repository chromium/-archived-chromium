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
function DeleteTool(context, root) {
  this.context = context;
  this.root = root;
  this.transformInfo = o3djs.picking.createTransformInfo(root, null);
  this.selectedObject = null;
}

DeleteTool.prototype.getPickedObject = function(e) {
  var worldRay = o3djs.picking.clientPositionToWorldRay(e.x,
                                                        e.y,
                                                        this.context,
                                                        g_client.width,
                                                        g_client.height);
  this.transformInfo.update();
  var pickInfo = this.transformInfo.pick(worldRay);
  var picked_object_root = null;
  if (pickInfo) {
    pickedObject = pickInfo.shapeInfo.parent;
    while (pickedObject.parent != null &&
           pickedObject.parent.transform != this.root) {
      pickedObject = pickedObject.parent;
    }
    picked_object_root = pickedObject.transform;
  }
  return picked_object_root;
};

DeleteTool.prototype.handleMouseDown = function(e) {
  this.selectedObject = this.getPickedObject(e);
};

DeleteTool.prototype.handleMouseUp = function(e) {
  if (this.selectedObject != null &&
      this.selectedObject == this.getPickedObject(e)) {
    // Delete the object if the user clicked down and up on the same object.
    this.selectedObject.parent = null;
  }
  this.selectedObject = null;
};

DeleteTool.prototype.handleMouseMove = function(e) {
};

DeleteTool.prototype.handleKeyDown = function(e) {
  return false;
};

DeleteTool.prototype.handleKeyUp = function(e) {
  return false;
};

