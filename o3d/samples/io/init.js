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
 * @fileoverview  This file creates all of the global o3d stuff that
 * establishes the plugin window, sets its size, creates necessary light
 * and camera parameters, and stuff like that.
 */
o3djs.require('o3djs.util');
o3djs.require('O3D.math');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.pack');
o3djs.require('o3djs.scene');

// Create some globals that store our main pointers to o3d objects.
var o3d;
var math;
var math;
var client;
var pack;
var globalParams;
var g_viewInfo;
var drawContext;
var blackSampler;
var g_loadInfo;

// This variable keeps track of whether a collada file is correctly loaded.
var isLoaded = false;

// This variable, surprisingly, contains the time since the last frame
// (in seconds)
var timeSinceLastFrame = 0;
var frameCount = 0;

// This is only a handy data structure for storing camera information. It's not
// actually a core O3D object.
var camera = {
  eye: [0, 0, 500],
  target: [0, 0, 0]
};

/**
 * This is a top-level switch that tells us whether we're running inside a
 * SketchUp web dialog. (If not, then we're in O3D.)
 * TODO : Re-enable the SketchUp web-dialog functionality, but through
 * a check different than simply testing if we are running in IE.
 */
//var isSU = (navigator.appVersion.indexOf('MSIE') != -1) ? true : false;
var isSU = false;

// Some parameters to pass to the shaders as the camera moves around.
var camera_eye_param;
var camera_target_param;

// Keep track of the lights in the level.
var light_array;
var max_number_of_lights = 4;  // Determined from the GPU effect.

/**
 * Creates the client area.
 */
function init() {
  o3djs.util.makeClients(initStep2);
}

/**
 * Gets called at the page's onLoad event. This function sets up our plugin and
 * level selection UI.
 * @param {Array} clientElements Array of o3d object elements.
 */
function initStep2(clientElements) {
  // Walk across all levels loaded by includes.js and show a list for the user
  // to choose from.
  $('content').innerHTML = 'Choose a level...<br>';
  for (var i = 0; i < levels.length; i++) {
    var level = levels[i];
    var str = '<span class="levellink" onclick="loadLevel(' + i + ')">' +
        level.name + '</span><br>'
    $('content').innerHTML += str;
  }

  soundPlayer.play('sound/music.mp3', 100, 999);

  // If we're NOT running inside SketchUp, then we need to set up our o3d
  // window.
  if (!isSU) {
    var o3dElement = clientElements[0];
    o3dElement.id = 'o3dObj';
    o3d = o3dElement.o3d;
    math = o3djs.math;
    client = o3dElement.client;
    pack = client.createPack();
    var blackTexture = pack.createTexture2D(1, 1, o3d.Texture.XRGB8, 1, false);
    blackTexture.set(0, [0, 0, 0]);
    blackSampler = pack.createObject('Sampler');
    blackSampler.texture = blackTexture;

    // Create the render graph for a view.
    g_viewInfo = o3djs.rendergraph.createBasicView(
        pack,
        client.root,
        client.renderGraphRoot);

    drawContext = g_viewInfo.drawContext;

    var target = [0, 0, 0];
    var eye = [0, 0, 5];
    var up = [0, 1, 0];
    var view_matrix = math.matrix4.lookAt(eye, target, up);

    drawContext.view = view_matrix;

    globalParams = pack.createObject('ParamObject');

    // Initialize all the effect's required parameters.
    var ambient_light_color_param = globalParams.createParam(
        'ambientLightColor',
        'ParamFloat4');
    ambient_light_color_param.value = [0.27, 0.2, 0.2, 1];
    var sunlight_direction_param = globalParams.createParam('sunlightDirection',
                                                            'ParamFloat3');

    // 20 degrees off.
    sunlight_direction_param.value = [-0.34202, 0.93969262, 0.0];
    var sunlight_color_param = globalParams.createParam('sunlightColor',
                                                        'ParamFloat4');

    sunlight_color_param.value = [0.55, 0.6, 0.7, 1.0];
    // global parameter, this will change as the character moves.
    camera_eye_param = globalParams.createParam('cameraEye', 'ParamFloat3');
    camera_eye_param.value = eye;
    camera_target_param = globalParams.createParam('cameraTarget',
                                                   'ParamFloat3');
    camera_target_param.value = target;

    InitializeLightParameters(globalParams, max_number_of_lights);

    var fog_color_param = globalParams.createParam('fog_color', 'ParamFloat4');
    fog_color_param.value = [0.5, 0.5, 0.5, 1.0];

    // Create a perspective projection matrix.
    var o3d_width = 845;
    var o3d_height = 549;

    var proj_matrix = math.matrix4.perspective(
       45 * Math.PI / 180, o3d_width / o3d_height, 0.1, 5000);

    drawContext.projection = proj_matrix;

    o3dElement.onkeydown = document.onkeydown;
    o3dElement.onkeyup = document.onkeyup;

    o3djs.event.startKeyboardEventSynthesis(o3dElement);
  }
}

/**
 * Remove callbacks on the o3d client.
 */
function uninit() {
  if (client) {
    client.cleanup();
  }
}

/**
 * Helper function to shorten document.getElementById()
 * @param {string} value  The elementID to find.
 * @return {string} The document element with our ID.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Helper function to tell us if a value is set to something useful.
 * @param {Object} value  The thing to check out.
 * @return {boolean} Whether the thing is empty.
 * TODO: 'Tis silly to have both an isEmpty and exists function
 * when they're probably doing the same thing.
 */
function isEmpty(value) {
  return (value == undefined) || (value == '');
}

/**
 * Returns the 2nd value if the first is rmpty.
 * @param {Object} val1  The thing to check for emptyness.
 * @param {Object} val2  The thing to maybe pass back.
 * @return {Object} One of the above.
 */
function ifEmpty(val1, val2) {
  if (val1 == undefined || val1 == '') {
    return val2;
  }
  return val1;
}

/**
 * Helper function to tell us if a value exists.
 * @param {Object} item  The thing to check out.
 * @return {boolean} Whether the thing exists.
 */
function exists(item) {
  if (item == undefined || item == null) {
    return false;
  } else {
    return true;
  }
}

/**
 * Takes an index number into our global levels[] array, and it loads up that
 * level.
 * @param {number} levelID  The number of the level to load in.
 */
function loadLevel(levelID) {
  if (isLoaded) {
    return;
  }
  world = levels[levelID];

  // If we're running in 'SketchUp mode' inside the level editor, tell SU to
  // do the loading, then bail out of here.
  if (isSU) {
    clearTimeout(timerID)
    url = 'skp:do_open@';
    url += 'level_name=' + world.colladaFile;
    url += '&friendly_name=' + world.name;
    window.location.href = url;
    $('export-button').disabled = false;
    $('play-button').disabled = false;
    $('pause-button').disabled = true;
    isLoaded = true;
    return;
  }

  var path = window.location.href;
  var index = path.lastIndexOf('/');
  path = path.substring(0, index + 1) + 'levels/' + world.colladaFile;
  $('o3d').style.width = '845px';
  $('o3d').style.height = '549px';

  // Create a callback function to prepare the transform graph once its loaded.
  function callback(exception) {
    if (exception) {
      alert('Could not load: ' + path + '\n' + exception);
    } else {
      var current_light = 0;
      var my_effect = pack.createObject('Effect');
      my_effect.name = 'global_effect';
      var effect_string = $('global_effect').value;
      my_effect.loadFromFXString(effect_string);

      var xform_nodes = client.root.getTransformsInTree();
      for (var i = 0; i < xform_nodes.length; ++i) {
        var shape_name = xform_nodes[i].name;
        if (shape_name.indexOf('Light') == 0) {
          var world_transform = xform_nodes[i].getUpdatedWorldMatrix();
          var world_loc = world_transform[3];
          var world_location = world_loc;
          var color = [1.0, 0.7, 0.10, 150.0];

          var light_index = AddLight(world_location, color);
          xform_nodes[i].parent = null;
          ++current_light;
        }
      }

      {
        var materials = pack.getObjectsByClassName('o3d.Material');
        for (var m = 0; m < materials.length; ++m) {
          var material = materials[m];
          if (!material.getParam('diffuseSampler')) {
            diffuseParam = material.createParam('diffuseSampler',
                                                'ParamSampler');
            diffuseParam.value = blackSampler;
          }
          material.effect = my_effect;
          my_effect.createUniformParameters(material);
          // go through each param on the material
          var params = material.params;
          for (var p = 0; p < params.length; ++p) {
            var dst_param = params[p];
            // see if there is one of the same name on the paramObject we're
            // using for global parameters.
            var src_param = globalParams.getParam(dst_param.name);
            if (src_param) {
              // see if they are compatible
              if (src_param.isAClassName(dst_param.className)) {
                // bind them
                dst_param.bind(src_param);
              }
            }
          }
        }
      }

      // Remove all line primitives as we don't really want them to render.
      {
        var primitives = pack.getObjectsByClassName('o3d.Primitive');
        for (var p = 0; p < primitives.length; ++p) {
          var primitive = primitives[p];
          if (primitive.primitiveType == o3d.Primitive.LINELIST) {
            primitive.owner = null;
            pack.removeObject(primitive);
          }
        }
      }

      o3djs.pack.preparePack(pack, g_viewInfo);
      avatar = world.actors[0];
      isLoaded = true;
      client.setRenderCallback(onRender);
      $('container').style.visibility = 'visible';
    }
  }

  // Create a new transform node to attach our world to.
  var world_parent = pack.createObject('Transform');
  world_parent.parent = client.root;

  function loadSceneCallback(pack, parent, exception) {
    g_loadInfo = null;
    callback(exception);
  }
  g_loadInfo = o3djs.scene.loadScene(client, pack, world_parent, path,
                                     loadSceneCallback);
}

/**
 * This is our little callback handler that o3d calls after each frame.
 * @param {Object} renderEvent  An event object.
 */
function onRender(renderEvent) {
  if (g_loadInfo) {
    $('fps').innerHTML = 'Loading: ' +
         g_loadInfo.getKnownProgressInfoSoFar().percent + '%';
  }
  if (isLoaded == false) {
    return;
  }

  var elapsedTime = Math.min(renderEvent.elapsedTime, 1 / 20);

  nextFrame(elapsedTime);
  UpdateLightsInProgram(avatar);

  // Every 20 frames, update our frame rate display
  frameCount++;
  if (frameCount % 20 == 0) {
    $('fps').innerHTML = Math.floor(1.0 / elapsedTime + 0.5);
  }
}

/**
 * This cancels scrolling and keeps focus on a hidden element so spacebar
 * doesn't page down.
 */
function cancelScroll() {
  $('focusHolder').focus();
  var pageWidth = document.body.clientWidth;
  var pageHeight = document.body.clientHeight;
  document.body.scrollTop = 0;
  document.body.scrollLeft = 0;
}
