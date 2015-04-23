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
 * This file contains definitions for the common functions used by all the home
 * configurator pages.
 */
o3djs.require('o3djs.util');
o3djs.require('o3djs.arcball');
o3djs.require('o3djs.dump');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.shape');
o3djs.require('o3djs.effect');
o3djs.require('o3djs.material');
o3djs.require('o3djs.pack');
o3djs.require('o3djs.picking');
o3djs.require('o3djs.scene');

var g_root;
var g_o3d;
var g_math;
var g_client;
var g_thisRot;
var g_lastRot;
var g_pack = null;
var g_mainPack;
var g_viewInfo;
var g_lightPosParam;
var g_currentTool = null;
var g_floorplanRoot = null;
var g_placedModesRoot = null;
var TOOL_ORBIT = 3;
var TOOL_MOVE = 1;
var TOOL_ZOOM_EXTENTS = 6;
var g_urlToInsert;
var g_isDraggingItem = false;
var g_o3dElement = null;
var g_lastMouseButtonState = 0;

/**
 * Retrieve the absolute position of an element on the screen.
 */
function getAbsolutePosition(element) {
  var r = { x: element.offsetLeft, y: element.offsetTop };
  if (element.offsetParent) {
    var tmp = getAbsolutePosition(element.offsetParent);
    r.x += tmp.x;
    r.y += tmp.y;
  }
  return r;
}

/**
 * Retrieve the coordinates of the given event relative to the center
 * of the widget.
 *
 * @param event
 *  A mouse-related DOM event.
 * @param reference
 *  A DOM element whose position we want to transform the mouse coordinates to.
 * @return
 *    An object containing keys 'x' and 'y'.
 */
function getRelativeCoordinates(event, reference) {
  var x, y;
  event = event || window.event;
  var el = event.target || event.srcElement;
  if (!window.opera && typeof event.offsetX != 'undefined') {
    // Use offset coordinates and find common offsetParent
    var pos = { x: event.offsetX, y: event.offsetY };
    // Send the coordinates upwards through the offsetParent chain.
    var e = el;
    while (e) {
      e.mouseX = pos.x;
      e.mouseY = pos.y;
      pos.x += e.offsetLeft;
      pos.y += e.offsetTop;
      e = e.offsetParent;
    }
    // Look for the coordinates starting from the reference element.
    var e = reference;
    var offset = { x: 0, y: 0 }
    while (e) {
      if (typeof e.mouseX != 'undefined') {
        x = e.mouseX - offset.x;
        y = e.mouseY - offset.y;
        break;
      }
      offset.x += e.offsetLeft;
      offset.y += e.offsetTop;
      e = e.offsetParent;
    }
    // Reset stored coordinates
    e = el;
    while (e) {
      delete e.mouseX;
      delete e.mouseY;
      e = e.offsetParent;
    }
  }
  else {
    // Use absolute coordinates
    var pos = getAbsolutePosition(reference);
    x = event.pageX - pos.x;
    y = event.pageY - pos.y;
  }

  return { x: x, y: y };
}


// The target camera has its z and y flipped because that's the way Scott
// Lininger thinks.
function TargetCamera() {
  this.eye = {
      rotZ: -Math.PI / 3,
      rotH: Math.PI / 3,
      distanceFromTarget: 700 };
  this.target = { x: 0, y: 0, z: 0 };
}

TargetCamera.prototype.update = function() {
  var target = [this.target.x, this.target.y, this.target.z];

  this.eye.x = this.target.x + Math.cos(this.eye.rotZ) *
      this.eye.distanceFromTarget * Math.sin(this.eye.rotH);
  this.eye.y = this.target.y + Math.sin(this.eye.rotZ) *
      this.eye.distanceFromTarget * Math.sin(this.eye.rotH);
  this.eye.z = this.target.z + Math.cos(this.eye.rotH) *
      this.eye.distanceFromTarget;

  var eye = [this.eye.x, this.eye.y, this.eye.z];
  var up = [0, 0, 1];
  g_viewInfo.drawContext.view = g_math.matrix4.lookAt(eye, target, up);
  g_lightPosParam.value = eye;
};

var g_camera = new TargetCamera();

function peg(value, lower, upper) {
  if (value < lower) {
    return lower;
  } else if (value > upper) {
    return upper;
  } else {
    return value;
  }
}

/**
 * Keyboard constants.
 */
var BACKSPACE = 8;
var TAB = 9;
var ENTER = 13;
var SHIFT = 16;
var CTRL = 17;
var ALT = 18;
var ESCAPE = 27;
var PAGEUP = 33;
var PAGEDOWN = 34;
var END = 35;
var HOME = 36;
var LEFT = 37;
var UP = 38;
var RIGHT = 39;
var DOWN = 40;
var DELETE = 46;
var SPACE = 32;

/**
 * Create some global key capturing. Keys that are pressed will be stored in
 * this global array.
 */
g_keyIsDown = [];

document.onkeydown = function(e) {
  var keycode;
  if (window.event) {
    keycode = window.event.keyCode;
  } else if (e) {
    keycode = e.which;
  }
  g_keyIsDown[keycode] = true;
  if (g_currentTool != null) {
    g_currentTool.handleKeyDown(keycode);
  }
};

document.onkeyup = function(e) {
  var keycode;
  if (window.event) {
    keycode = window.event.keyCode;
  } else if (e) {
    keycode = e.which;
  }
  g_keyIsDown[keycode] = false;
  if (g_currentTool != null) {
    g_currentTool.handleKeyUp(keycode);
  }
};

document.onmouseup = function(e) {
  if (g_currentTool != null) {
    g_currentTool.handleMouseUp(e);
  } else {
    cancelInsertDrag();
  }
};

// NOTE: mouseDown, mouseMove and mouseUp are mouse event handlers for events
// taking place inside the o3d area.  They typically pass the events down
// to the currently selected tool (e.g. Orbit, Move, etc).  Tool and item
// selection mouse events are registered seperately on their respective DOM
// elements.

// This function handles the mousedown events that happen inside the o3d
// area.  If a tool is currently selected (e.g. Orbit, Move, etc.) the event
// is forwarded over to it.  If the middle mouse button is pressed then we
// temporarily switch over to the orbit tool to emulate the SketchUp behavior.
function mouseDown(e) {
  // If the middle mouse button is used, then switch into the orbit tool,
  // Sketchup-style.
  if (e.button == g_o3d.Event.BUTTON_MIDDLE) {
    g_lastTool = g_currentTool;
    g_lastSelectorLeft = $('toolselector').style.left;
    selectTool(null, TOOL_ORBIT);
  }

  if (g_currentTool != null) {
    g_currentTool.handleMouseDown(e);
  }
}

// This function handles mouse move events inside the o3d area.  It simply
// forwards them down to the currently selected tool.
function mouseMove(e) {
  if (g_currentTool != null) {
    g_currentTool.handleMouseMove(e);
  }
}

// This function handles mouse up events that take place in the o3d area.
// If the middle mouse button is lifted then we switch out of the temporary
// orbit tool mode.
function mouseUp(e) {
  // If the middle mouse button was used, then switch out of the orbit tool
  // and reset to their last tool.
  if (e.button == g_o3d.Event.BUTTON_MIDDLE) {
    $('toolselector').style.left = g_lastSelectorLeft;
    g_currentTool = g_lastTool
  }
  if (g_currentTool != null) {
    g_currentTool.handleMouseUp(e);
  }
}

function scrollMe(e) {
  e = e ? e : window.event;
  var raw = e.detail ? e.detail : -e.wheelDelta;
  if (raw < 0) {
    g_camera.eye.distanceFromTarget *= 11 / 12;

  } else {
    g_camera.eye.distanceFromTarget *= (1 + 1 / 12);
  }
  g_camera.update();
}

function $(name) {
  return document.getElementById(name);
}


// An array of tool objects that will get populated when our base model loads.
var g_tools = [];

function selectTool(e, opt_toolNumber) {
  var ICON_WIDTH = 32;
  var toolNumber = opt_toolNumber;

  if (toolNumber == undefined) {
    // Where you click determines your tool. But since our underlying toolbar
    // graphic isn't perfectly uniform, perform some acrobatics to get the best
    // toolNumber match.
    var pt = getRelativeCoordinates(e, $('toolpanel'));
    if (pt.x < 120) {
      toolNumber = Math.floor((pt.x - 8) / ICON_WIDTH)
    } else {
      toolNumber = Math.floor((pt.x - 26) / ICON_WIDTH)
    }
    toolNumber = peg(toolNumber, 0, 9);
  }

  // Now place the selector graphic over the tool we clicked.
  if (toolNumber < 3) {
    $('toolselector').style.left = toolNumber * ICON_WIDTH + 8;
  } else {
    $('toolselector').style.left = toolNumber * ICON_WIDTH + 26;
  }

  // Finally, activate the appropriate tool.
  // TODO: Replace this hacky zoom extents tool detection with
  // tools that have an onActivate callback.
  if (toolNumber == TOOL_ZOOM_EXTENTS) {
    // Reset our camera. (Zooming to extents would be better, obviously ;)
    g_camera.eye.distanceFromTarget = 900;
    g_camera.target = { x: -100, y: 0, z: 0 };
    g_camera.update();

    // Then reset to the orbit tool, after pausing for a bit so the user
    // sees the zoom extents button depress.
    setTimeout(function() { selectTool(null, TOOL_ORBIT)}, 200);
  } else {
    g_currentTool = g_tools[toolNumber];
  }

}

function loadFile(context, path) {
  function callback(pack, start_move_tool_root, exception) {
    if (exception) {
      alert('Could not load: ' + path + '\n' + exception);
    } else {
      // Generate draw elements and setup material draw lists.
      o3djs.pack.preparePack(g_pack, g_viewInfo);

      // Manually connect all the materials' lightWorldPos params to the context
      var materials = g_pack.getObjectsByClassName('o3d.Material');
      for (var m = 0; m < materials.length; ++m) {
        var material = materials[m];
        var param = material.getParam('lightWorldPos');
        if (param) {
          param.bind(g_lightPosParam);
        }
      }

      /*
      o3djs.dump.dump('---dumping context---\n');
      o3djs.dump.dumpParamObject(context);

      o3djs.dump.dump('---dumping root---\n');
      o3djs.dump.dumpTransformTree(g_client.root);

      o3djs.dump.dump('---dumping render root---\n');
      o3djs.dump.dumpRenderNodeTree(g_client.renderGraphRoot);

      o3djs.dump.dump('---dump g_pack shapes---\n');
      var shapes = g_pack.getObjectsByClassName('o3d.Shape');
      for (var t = 0; t < shapes.length; t++) {
        o3djs.dump.dumpShape(shapes[t]);
      }

      o3djs.dump.dump('---dump g_pack materials---\n');
      var materials = g_pack.getObjectsByClassName('o3d.Material');
      for (var t = 0; t < materials.length; t++) {
        o3djs.dump.dump (
            '  ' + t + ' : ' + materials[t].className +
            ' : "' + materials[t].name + '"\n');
        o3djs.dump.dumpParams(materials[t], '    ');
      }

      o3djs.dump.dump('---dump g_pack textures---\n');
      var textures = g_pack.getObjectsByClassName('o3d.Texture');
      for (var t = 0; t < textures.length; t++) {
        o3djs.dump.dumpTexture(textures[t]);
      }

      o3djs.dump.dump('---dump g_pack effects---\n');
      var effects = g_pack.getObjectsByClassName('o3d.Effect');
      for (var t = 0; t < effects.length; t++) {
        o3djs.dump.dump ('  ' + t + ' : ' + effects[t].className +
                ' : "' + effects[t].name + '"\n');
        o3djs.dump.dumpParams(effects[t], '    ');
      }
    */
    }
    if (start_move_tool_root != g_floorplanRoot) {
      selectTool(null, TOOL_MOVE);
      g_tools[TOOL_MOVE].initializeWithShape(start_move_tool_root);
    }
    g_camera.update();
  }

  g_pack = g_client.createPack();
  g_pack.name = 'load pack';

  new_object_root = null;
  if (g_floorplanRoot == null) {
    // Assign as the floorplan
    g_floorplanRoot = g_pack.createObject('o3d.Transform');
    g_floorplanRoot.name = 'floorplan';
    g_floorplanRoot.parent = g_client.root;

    g_placedModelsRoot = g_pack.createObject('o3d.Transform');
    g_placedModelsRoot.name = 'placedModels';
    g_placedModelsRoot.parent = g_floorplanRoot;

    // Put the object we're loading on the floorplan.
    new_object_root = g_floorplanRoot;

    // Create our set of tools that can be activated.
    // Note: Currently only the Delete, Move, Rotate, Orbit, Pan and Zoom
    // tools are implemented.  The last four icons in the toolbar are unused.
    g_tools = [
      new DeleteTool(g_viewInfo.drawContext, g_placedModelsRoot),
      new MoveTool(g_viewInfo.drawContext, g_placedModelsRoot),
      new RotateTool(g_viewInfo.drawContext, g_placedModelsRoot),
      new OrbitTool(g_camera),
      new PanTool(g_camera),
      new ZoomTool(g_camera),
      null,
      null,
      null,
      null
    ]

    selectTool(null, TOOL_ORBIT);
  } else {
    // Create a new transform for the loaded file
    new_object_root = g_pack.createObject('o3d.Transform');
    new_object_root.name = 'loaded object';
    new_object_root.parent = g_placedModelsRoot;

  }

  if (path != null) {
    o3djs.scene.loadScene(g_client, g_pack, new_object_root, path, callback);
  }

  return new_object_root;
}

function setClientSize() {
  // Create a perspective projection matrix
  g_viewInfo.drawContext.projection = g_math.matrix4.perspective(
    3.14 * 45 / 180, g_client.width / g_client.height, 0.1, 5000);
}

function resize() {
  setClientSize();
}

function init() {
  o3djs.util.makeClients(initStep2);
}

function initStep2(clientElements) {
  g_o3dElement = clientElements[0];

  var path = window.location.href;
  var index = path.lastIndexOf('/');
  path = path.substring(0, index + 1) + g_buildingAsset;
  var url = document.getElementById('url').value = path;

  g_o3d = g_o3dElement.o3d;
  g_math = o3djs.math;
  g_client = g_o3dElement.client;

  g_mainPack = g_client.createPack();
  g_mainPack.name = 'simple viewer pack';

  // Create the render graph for a view.
  g_viewInfo = o3djs.rendergraph.createBasicView(
      g_mainPack,
      g_client.root,
      g_client.renderGraphRoot,
      [1, 1, 1, 1]);

  g_lastRot = g_math.identity(3);
  g_thisRot = g_math.identity(3);

  var root = g_client.root;

  var target = [0, 0, 0];
  var eye = [0, 0, 5];
  var up = [0, 1, 0];
  g_viewInfo.drawContext.view = g_math.matrix4.lookAt(eye, target, up);

  setClientSize();

  var paramObject = g_mainPack.createObject('ParamObject');
  // Set the light at the same position as the camera to create a headlight
  // that illuminates the object straight on.
  g_lightPosParam = paramObject.createParam('lightWorldPos', 'ParamFloat3');
  g_lightPosParam.value = eye;

  doload();

  o3djs.event.addEventListener(g_o3dElement, 'mousedown', mouseDown);
  o3djs.event.addEventListener(g_o3dElement, 'mousemove', mouseMove);
  o3djs.event.addEventListener(g_o3dElement, 'mouseup', mouseUp);

  g_o3dElement.addEventListener('mouseover', dragOver, false);
  // for Firefox
  g_o3dElement.addEventListener('DOMMouseScroll', scrollMe, false);
  // for Safari
  g_o3dElement.onmousewheel = scrollMe;

  document.getElementById('toolpanel').onmousedown = selectTool;

  // Create our catalog list from the global list of items (g_items).
  html = [];
  for (var i = 0; i < g_items.length; i++) {
    html.push('<div class="catalogItem" onmousedown="startInsertDrag(\'',
        g_items[i].url, '\')" style="background-position:-', (i * 62) ,
        'px 0px" title="', g_items[i].title, '"></div>');
  }
  $('itemlist').innerHTML = html.join('');

  // Register a mouse-move event listener to the entire window so that we can
  // catch the click-and-drag events that originate from the list of items
  // and end up in the o3d element.
  document.addEventListener('mousemove', mouseMove, false);
}

function dragOver(e) {
  if (g_urlToInsert != null) {
    doload(g_urlToInsert);
  }
  g_urlToInsert = null;
}

function doload(opt_url) {
  var url;
  if (opt_url != null) {
    url = opt_url
  } else if ($('url')) {
    url = $('url').value;
  }
  g_root = loadFile(g_viewInfo.drawContext, url);
}

function startInsertDrag(url) {
  // If no absolute web path was passed, assume it's a local file
  // coming from the assets directory.
  if (url.indexOf('http') != 0) {
    var path = window.location.href;
    var index = path.lastIndexOf('/');
    g_urlToInsert = path.substring(0, index + 1) + g_assetPath + url;
  } else {
    g_urlToInsert = url;
  }
}

function cancelInsertDrag() {
  g_urlToInsert = null;
}

