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
 * @fileoverview This file contains various functions for helping create
 * render graphs for o3d.  It puts them in the "rendergraph" module on
 * the o3djs object.
 *
 *     Note: This library is only a sample. It is not meant to be some official
 *     library. It is provided only as example code.
 *
 */

o3djs.provide('o3djs.rendergraph');

/**
 * A Module for creating render graphs.
 * @namespace
 */
o3djs.rendergraph = o3djs.rendergraph || {};

/**
 * Creates a basic render graph setup to draw opaque and transparent
 * 3d objects.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.Transform} treeRoot root Transform of tree to render.
 * @param {!o3d.RenderNode} opt_parent RenderNode to build this view under.
 * @param {!o3djs.math.Vector4} opt_clearColor color to clear view.
 * @param {number} opt_priority Optional base priority for created objects.
 * @param {!o3djs.math.Vector4} opt_viewport viewport settings for view.
 * @param {!o3d.DrawList} opt_performanceDrawList Optional DrawList to
 *     use for performanceDrawPass.
 * @param {!o3d.DrawList} opt_zOrderedDrawList Optional DrawList to
 *     use for zOrderedDrawPass.
 * @return {!o3djs.rendergraph.ViewInfo} A ViewInfo object with info about
 *         everything created.
 */
o3djs.rendergraph.createView = function(pack,
                                        treeRoot,
                                        opt_parent,
                                        opt_clearColor,
                                        opt_priority,
                                        opt_viewport,
                                        opt_performanceDrawList,
                                        opt_zOrderedDrawList) {
  return new o3djs.rendergraph.ViewInfo(pack,
                                        treeRoot,
                                        opt_parent,
                                        opt_clearColor,
                                        opt_priority,
                                        opt_viewport,
                                        opt_performanceDrawList,
                                        opt_zOrderedDrawList);
};

/**
 * Creates a basic render graph setup to draw opaque and transparent
 * 3d objects.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.Transform} treeRoot root Transform of tree to render.
 * @param {!o3d.RenderNode} opt_parent RenderNode to build this view under.
 * @param {!o3djs.math.Vector4} opt_clearColor color to clear view.
 * @param {number} opt_priority Optional base priority for created objects.
 * @param {!o3djs.math.Vector4} opt_viewport viewport settings for view.
 * @return {!o3djs.rendergraph.ViewInfo} A ViewInfo object with info about
 *     everything created.
 */
o3djs.rendergraph.createBasicView = function(pack,
                                             treeRoot,
                                             opt_parent,
                                             opt_clearColor,
                                             opt_priority,
                                             opt_viewport) {
   return o3djs.rendergraph.createView(pack,
                                       treeRoot,
                                       opt_parent,
                                       opt_clearColor,
                                       opt_priority,
                                       opt_viewport);
};

/**
 * Creates an extra view render graph setup to draw opaque and transparent
 * 3d objects based on a previously created view. It uses the previous view
 * to share draw lists and to set the priority.
 * @param {!o3djs.rendergraph.ViewInfo} viewInfo ViewInfo returned from
 *     createBasicView.
 * @param {!o3djs.math.Vector4} opt_viewport viewport settings for view.
 * @param {!o3djs.math.Vector4} opt_clearColor color to clear view.
 * @param {number} opt_priority base priority for created objects.
 * @return {!o3djs.rendergraph.ViewInfo} A ViewInfo object with info about
 *     everything created.
 */
o3djs.rendergraph.createExtraView = function(viewInfo,
                                             opt_viewport,
                                             opt_clearColor,
                                             opt_priority) {
  return o3djs.rendergraph.createView(viewInfo.pack,
                                      viewInfo.treeRoot,
                                      viewInfo.renderGraphRoot,
                                      opt_clearColor,
                                      opt_priority,
                                      opt_viewport,
                                      viewInfo.performanceDrawList,
                                      viewInfo.zOrderedDrawList);
};

/**
 * A ViewInfo object creates the standard o3d objects needed for
 * a single 3d view. Those include a ClearBuffer followed by a TreeTraveral
 * followed by 2 DrawPasses all of which are children of a Viewport. On top of
 * those a DrawContext and optionally 2 DrawLists although you can pass in your
 * own DrawLists if there is a reason to reuse the same DrawLists such was with
 * mulitple views of the same scene.
 *
 * The render graph created is something like:
 * <pre>
 *        [Viewport]
 *            |
 *     +------+--------+------------------+---------------------+
 *     |               |                  |                     |
 * [ClearBuffer] [TreeTraversal] [Performance StateSet] [ZOrdered StateSet]
 *                                        |                     |
 *                               [Performance DrawPass] [ZOrdered DrawPass]
 * </pre>
 *
 * @constructor
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.Transform} treeRoot root Transform of tree to render.
 * @param {!o3d.RenderNode} opt_parent RenderNode to build this view under.
 * @param {!o3djs.math.Vector4} opt_clearColor color to clear view.
 * @param {number} opt_priority Optional base priority for created objects.
 * @param {!o3djs.math.Vector4} opt_viewport viewport settings for view.
 * @param {!o3d.DrawList} opt_performanceDrawList DrawList to use for
 *     performanceDrawPass.
 * @param {!o3d.DrawList} opt_zOrderedDrawList DrawList to use for
 *     zOrderedDrawPass.
 */
o3djs.rendergraph.ViewInfo = function(pack,
                                      treeRoot,
                                      opt_parent,
                                      opt_clearColor,
                                      opt_priority,
                                      opt_viewport,
                                      opt_performanceDrawList,
                                      opt_zOrderedDrawList) {
  var clearColor = opt_clearColor || [0.5, 0.5, 0.5, 1.0];

  var viewPriority = opt_priority || 0;
  var priority = 0;

  // Create Viewport.
  var viewport = pack.createObject('Viewport');
  if (opt_viewport) {
    viewport.viewport = opt_viewport;
  }
  viewport.priority = viewPriority;

  // Create a clear buffer.
  var clearBuffer = pack.createObject('ClearBuffer');
  clearBuffer.clearColor = clearColor;
  clearBuffer.priority = priority++;
  clearBuffer.parent = viewport;

  // Create DrawLists
  var performanceDrawList;
  if (opt_performanceDrawList) {
    performanceDrawList = opt_performanceDrawList;
    this.ownPerformanceDrawList_ = false;
  } else {
    performanceDrawList = pack.createObject('DrawList');
    this.ownPerformanceDrawList_ = true;
  }
  var zOrderedDrawList;
  if (opt_zOrderedDrawList) {
    zOrderedDrawList = opt_zOrderedDrawList;
    this.ownZOrderedDrawList_ = false;
  } else {
    zOrderedDrawList = pack.createObject('DrawList');
    this.ownZOrderedDrawList_ = true;
  }

  // Create DrawContext.
  var drawContext = pack.createObject('DrawContext');

  // Creates a TreeTraversal and parents it to the root.
  var treeTraversal = pack.createObject('TreeTraversal');
  treeTraversal.priority = priority++;
  treeTraversal.parent = viewport;

  // Creates an empty stateSet to parent the performanceDrawPass to
  // just to make it easier to do certain effects.
  var performanceStateSet = pack.createObject('StateSet');
  var performanceState = pack.createObject('State');
  performanceStateSet.state = performanceState;
  performanceStateSet.priority = priority++;
  performanceStateSet.parent = viewport;
  performanceState.getStateParam('ColorWriteEnable').value = 7;

  // Creates a DrawPass for performance shapes.
  var performanceDrawPass = pack.createObject('DrawPass');
  performanceDrawPass.drawList = performanceDrawList;
  performanceDrawPass.parent = performanceStateSet;

  // Creates a StateSet so everything on the zOrderedDrawPass is assumed
  // to need alpha blending with the typical settings.
  var zOrderedStateSet = pack.createObject('StateSet');
  var zOrderedState = pack.createObject('State');
  zOrderedState.getStateParam('AlphaBlendEnable').value = true;
  zOrderedState.getStateParam('SourceBlendFunction').value =
      o3djs.base.o3d.State.BLENDFUNC_SOURCE_ALPHA;
  zOrderedState.getStateParam('DestinationBlendFunction').value =
      o3djs.base.o3d.State.BLENDFUNC_INVERSE_SOURCE_ALPHA;
  zOrderedState.getStateParam('AlphaTestEnable').value = true;
  zOrderedState.getStateParam('AlphaComparisonFunction').value =
      o3djs.base.o3d.State.CMP_GREATER;
  zOrderedState.getStateParam('ColorWriteEnable').value = 7;

  zOrderedStateSet.state = zOrderedState;
  zOrderedStateSet.priority = priority++;
  zOrderedStateSet.parent = viewport;

  // Creates a DrawPass for zOrdered shapes.
  var zOrderedDrawPass = pack.createObject('DrawPass');
  zOrderedDrawPass.drawList = zOrderedDrawList;
  zOrderedDrawPass.sortMethod = o3djs.base.o3d.DrawList.BY_Z_ORDER;
  zOrderedDrawPass.parent = zOrderedStateSet;

  // Register the passlists and drawcontext with the TreeTraversal
  treeTraversal.registerDrawList(performanceDrawList, drawContext, true);
  treeTraversal.registerDrawList(zOrderedDrawList, drawContext, true);
  treeTraversal.transform = treeRoot;

  /**
   * Pack that manages the objects created for this ViewInfo.
   * @type {!o3d.Pack}
   */
  this.pack = pack;

  /**
   * The RenderNode this ViewInfo render graph subtree is parented under.
   * @type {(!o3d.RenderNode|undefined)}
   */
  this.renderGraphRoot = opt_parent;

  /**
   * The root node of the transform graph this ViewInfo renders.
   * @type {!o3d.Transform}
   */
  this.treeRoot = treeRoot;

  /**
   * The root of the subtree of the render graph this ViewInfo is managing.
   * If you want to set the priority of a ViewInfo's rendergraph subtree use
   * <pre>
   * viewInfo.root.priority = desiredPriority;
   * </pre>
   * @type {!o3d.RenderGraph}
   */
  this.root = viewport;

  /**
   * The Viewport RenderNode created for this ViewInfo.
   * @type {!o3d.Viewport}
   */
  this.viewport = viewport;

  /**
   * The ClearBuffer RenderNode created for this ViewInfo.
   * @type {!o3d.ClearBuffer}
   */
  this.clearBuffer = clearBuffer;

  /**
   * The StateSet RenderNode above the performance DrawPass in this ViewInfo
   * @type {!o3d.StateSet}
   */
  this.performanceStateSet = performanceStateSet;

  /**
   * The State object used by the performanceStateSet object in this ViewInfo.
   * By default, no states are set here.
   * @type {!o3d.State}
   */
  this.performanceState = performanceState;

  /**
   * The DrawList used for the performance draw pass. Generally for opaque
   * materials.
   * @type {!o3d.DrawList}
   */
  this.performanceDrawList = performanceDrawList;

  /**
   * The StateSet RenderNode above the ZOrdered DrawPass in this ViewInfo
   * @type {!o3d.StateSet}
   */
  this.zOrderedStateSet = zOrderedStateSet;

  /**
   * The State object used by the zOrderedStateSet object in this ViewInfo.
   * By default AlphaBlendEnable is set to true, SourceBlendFucntion is set to
   * State.BLENDFUNC_SOURCE_ALPHA and DestinationBlendFunction is set to
   * State.BLENDFUNC_INVERSE_SOURCE_ALPHA
   * @type {!o3d.State}
   */
  this.zOrderedState = zOrderedState;

  /**
   * The DrawList used for the zOrdered draw pass. Generally for transparent
   * materials.
   * @type {!o3d.DrawList}
   */
  this.zOrderedDrawList = zOrderedDrawList;

  /**
   * The DrawContext used by this ViewInfo.
   * @type {!o3d.DrawContext}
   */
  this.drawContext = drawContext;

  /**
   * The TreeTraversal used by this ViewInfo.
   * @type {!o3d.TreeTraversal}
   */
  this.treeTraversal = treeTraversal;

  /**
   * The DrawPass used with the performance DrawList created by this ViewInfo.
   * @type {!o3d.DrawPass}
   */
  this.performanceDrawPass = performanceDrawPass;

  /**
   * The DrawPass used with the zOrdered DrawList created by this ViewInfo.
   * @type {!o3d.DrawPass}
   */
  this.zOrderedDrawPass = zOrderedDrawPass;

  /**
   * The highest priority used for objects under the Viewport RenderNode created
   * by this ViewInfo.
   * @type {number}
   */
  this.priority = priority;

  // Parent whatever the root is to the parent passed in.
  if (opt_parent) {
    this.root.parent = opt_parent;
  }
};

/**
 * Destroys the various objects created for the view.
 *
 * @param {Boolean} opt_destroyDrawContext True if you want view's DrawContext
 *     destroyed. Default = true.
 * @param {Boolean} opt_destroyDrawList True if you want view's DrawLists
 *     destroyed. Default = true.
 */
o3djs.rendergraph.ViewInfo.prototype.destroy = function(
    opt_destroyDrawContext,
    opt_destroyDrawList) {
  if (opt_destroyDrawContext === undefined) {
    opt_destroyDrawContext = true;
  }
  if (opt_destroyDrawList === undefined) {
    opt_destroyDrawList = true;
  }
  // Remove everything we created from the pack.
  this.pack.removeObject(this.viewport);
  this.pack.removeObject(this.clearBuffer);
  if (this.ownPerformanceDrawList_ && opt_destroyDrawList) {
    this.pack.removeObject(this.performanceDrawList);
  }
  if (this.ownZOrderedDrawList_ && opt_destroyDrawList) {
    this.pack.removeObject(this.zOrderedDrawList);
  }
  if (opt_destroyDrawContext) {
    this.pack.removeObject(this.drawContext);
  }
  this.pack.removeObject(this.treeTraversal);
  this.pack.removeObject(this.performanceDrawPass);
  this.pack.removeObject(this.zOrderedDrawPass);
  // Remove our substree from its parent.
  this.viewport.parent = null;

  // At this point, IF nothing else is referencing any of these objects
  // they should get removed.
};

