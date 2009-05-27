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
 * @fileoverview This file contains functions used for creating a beach scene.
 *
 */
o3djs.require('o3djs.util');
o3djs.require('o3djs.math');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.pack');
o3djs.require('o3djs.effect');
o3djs.require('o3djs.dump');
o3djs.require('o3djs.primitives');
o3djs.require('o3djs.loader');

// Events

/**
 * Run the init() function once the page has finished loading.
 */
window.onload = init;

/**
 * Run the unload() function when the page is unloaded.
 */
window.onunload = unload;

// O3D variables
var g_root;
var g_o3dElement;
var g_o3d;
var g_math;
var g_client;
var g_pack = null;
var g_viewInfo;
var g_clientWidth;
var g_clientHeight;
var g_loader;
var g_finished = false;  // for selenium
var g_globalParams;

// Model variables
var g_modelCounter = 0;
var g_modelPaths = [];
var g_modelPacks = [];
var g_modelRoots = [];
var g_modelIndices = [];

// Pack variables
var g_rockPack;
var g_waterPack;
var g_bridgePack;
var g_woodPack;
var g_domePack;
var g_treePack;
var g_sunPack;

// Time variables
var g_frame;
var g_clock = 0.0;
var g_clockParam;
var g_timeMult = 1.0;

// General shader variables
var g_lightDirectionParam;

// Water variables
var g_samplerParam;
var g_swizzle1Param;
var g_deepWaterTextures = [];

// Cameras
var g_cameraOriginal = {
 eye: { x: -2.751, y: 3.529, z: -8.563 },
 target: { x: 0, y: 4, z: -10 },
 up: { x: 0, y: 1, z: 0 }
};
var g_cameraOverhead = {
 eye: { x: 0, y: 120, z: -20 },
 target: { x: 0, y: 2, z: -20 },
 up: { x: 0, y: 0, z: -1 }
};
var g_camera = {
 eye: { x: 0, y: 0, z: 0 },
 target: { x: 0, y: 0, z: 0 },
 up: { x: 0, y: 0, z: 0 }
};

var g_clientZoom = 1.0;

// Initialize the models we'll be using
g_modelPaths[0] = 'assets/rocks.9.o3dtgz';
g_modelPaths[1] = 'assets/lazy_bridge.o3dtgz';

g_modelIndices[0] = 0;
g_modelIndices[1] = 2;

/**
 * Performs the initial creation of the packs and their roots.
 */
function initializePacks() {
  var packNames = [
      'Rock pack',    // 0
      'Tree pack',    // 1
      'Brdige pack',  // 2
      'Wood pack',    // 3
      'Dome pack',    // 4
      'Water pack',   // 5
      'Sun pack'];    // 6

  for (var mp = 0; mp < packNames.length; mp++) {
    g_modelPacks[mp] = g_client.createPack();
    g_modelRoots[mp] = g_modelPacks[mp].createObject('Transform');
    g_modelRoots[mp].parent = g_root;
  }

  // Set some useful variables
  g_rockPack = g_modelPacks[0];
  g_treePack = g_modelPacks[1];
  g_bridgePack = g_modelPacks[2];
  g_woodPack = g_modelPacks[3];
  g_domePack = g_modelPacks[4];
  g_waterPack = g_modelPacks[5];
  g_sunPack = g_modelPacks[6];

}

/**
 * Swaps the properties of two cameras.
 * @param {!Object} oldCamera The old camera.
 * @param {!Object} newCamera The new camera to switch to.
 */
function switchCamera(oldCamera, newCamera) {
  oldCamera.eye.x = newCamera.eye.x;
  oldCamera.eye.y = newCamera.eye.y;
  oldCamera.eye.z = newCamera.eye.z;

  oldCamera.target.x = newCamera.target.x;
  oldCamera.target.y = newCamera.target.y;
  oldCamera.target.z = newCamera.target.z;

  oldCamera.up.x = newCamera.up.x;
  oldCamera.up.y = newCamera.up.y;
  oldCamera.up.z = newCamera.up.z;
}


/**
 * Generates the projection matrix based on the size of the o3d plugin and the
 * camera zoom.
 */
function resize() {
  var newWidth = g_client.width;
  var newHeight = g_client.height;

  if (g_clientZoom != g_cameraZoom ||
      newWidth != g_clientWidth ||
      newHeight != g_clientHeight) {
    g_clientZoom = g_cameraZoom;
    g_clientWidth = newWidth;
    g_clientHeight = newHeight;

    // Set the projection matrix, with a vertical field of view of 30 degrees
    // a near clipping plane of 0.1 and far clipping plane of 10000.
    g_viewInfo.drawContext.projection = g_math.matrix4.perspective(
        g_math.degToRad(g_clientZoom * 30),
        g_clientWidth / g_clientHeight,
        0.1,
        10000);
  }
}
/**
 * Performs functions (usually related to animation) every time the frame is
 * rendered. Also updates properities such as time.
 * @param {!o3d.RenderEvent} renderEvent Render event.
 */
function onrender(renderEvent) {

  var elapsedTime = renderEvent.elapsedTime;
  g_clock += g_timeMult * elapsedTime;

  g_clockParam.value = g_clock;

  g_frame = Math.floor(g_clock * 18);
  g_frame = g_frame % 90;

  var frame = g_frame - 22.5;
  var twoPiOver90 = 2.0 * Math.PI / 90.0;
  var s1 = Math.sin(frame * twoPiOver90) + 1;
  var s2 = Math.sin((frame - 30) * twoPiOver90) + 1;
  var s3 = Math.sin((frame - 60) * twoPiOver90) + 1;

  var denominator = s1 + s2 + s3;
  s1 /= denominator;
  s2 /= denominator;
  s3 /= denominator;

  if (g_frame >= 0 && g_frame < 30) {
    g_swizzle1Param.value = new Array(s1, s3, s2);
  } else if (g_frame >= 30 && g_frame < 60) {
    g_swizzle1Param.value = new Array(s2, s1, s3);
  } else {
    g_swizzle1Param.value = new Array(s3, s2, s1);
  }

  var i = g_frame % 30;
  var m = Math.floor(g_frame / 3);
  if (g_deepWaterTextures[i]){
    g_samplerParam.value.texture = g_deepWaterTextures[i];
  }
  resize();
}

/**
 * Creates the client area.
 */
function init() {
  o3djs.util.makeClients(initStep2);
}

/**
 * Initializes the scene.
 * @param {Array} clientElements Array of o3d object elements.
 */
function initStep2(clientElements) {
  g_o3dElement = clientElements[0];
  g_o3d = g_o3dElement.o3d;
  g_math = o3djs.math;
  g_client = g_o3dElement.client;

  // Currently hardcoding the size.
  g_clientWidth = 700;
  g_clientHeight = 500;

  g_loader = o3djs.loader.createLoader(function() {
      g_finished = true;  // for selenuim
  });

  g_pack = g_client.createPack();
  g_root = g_pack.createObject('Transform');
  g_root.parent = g_client.root;

  // Initialize the camera
  switchCamera(g_camera, g_cameraOriginal);

  // Initialize all the other packs
  initializePacks();

  // Create the render graph for a view.
  g_viewInfo = o3djs.rendergraph.createBasicView(
      g_pack,
      g_client.root,
      g_client.renderGraphRoot);

  // Initialize camera
  resize();
  setViewFromRotation();

  // Create global params.
  g_globalParams = g_pack.createObject('ParamObject');
  g_lightDirectionParam = g_globalParams.createParam('lightWorldPos',
                                                     'ParamFloat3');
  g_clockParam = g_globalParams.createParam('timeParam',
                                            'ParamFloat');
  g_swizzle1Param = g_globalParams.createParam('swizzle1Param',
                                               'ParamFloat3');

  g_lightDirectionParam.value = [0, .15, -1];

  // Add our models
  water(g_waterPack, g_modelRoots[5]);
  dome(g_domePack, g_modelRoots[4]);
  sun(g_sunPack, g_modelRoots[6]);
  rocks(g_rockPack, g_modelRoots[0]);
  bridge(g_bridgePack, g_modelRoots[2]);

  // Add mouse control
  o3djs.event.addEventListener(g_o3dElement, 'mousedown', startDragging);
  o3djs.event.addEventListener(g_o3dElement, 'mousemove', drag);
  o3djs.event.addEventListener(g_o3dElement, 'mouseup', stopDragging);
  o3djs.event.addEventListener(g_o3dElement, 'wheel', scrollMe);

  g_client.setRenderCallback(onrender);

  g_loader.finish();
}

/**
 * Returns the path of where the file is located
 * with the trailing slash
 * @return {string} The current path.
 */
function getCurrentPath() {
  var path = window.location.href;
  var index = path.lastIndexOf('/');
  return path.substring(0, index + 1);
}

/**
 * Loads a texture.
 *
 * @param {!o3djs.loader.Loader} loader Loader to use to load texture.
 * @param {!o3d.Pack} pack Pack to load texture in.
 * @param {!o3d.Material} material Material to attach sampler to.
 * @param {string} samplerName Name of sampler.
 * @param {string} textureName filename of texture.
 * @return {!o3d.Sampler} Sampler attached to material.
 */
function loadTexture(loader, pack, material, samplerName, textureName) {
  var samplerParam = material.getParam(samplerName);
  var sampler = pack.createObject('Sampler');
  samplerParam.value = sampler;

  var url = getCurrentPath() + 'assets/' + textureName;
  loader.loadTexture(pack, url, function(texture, exception) {
        if (exception) {
          alert(exception);
        } else {
          sampler.texture = texture;
        }
      });

  return sampler;
}

/**
 * Creates the rocks and its shaders.
 * @param {!o3d.Pack} pack Pack to load the rocks into.
 * @param {o3d.Transform} root Parent transform for the rocks shape.
 */
function rocks(pack, root) {
  var loader = g_loader.createLoader(function() {
      o3djs.pack.preparePack(pack, g_viewInfo);});
  loader.loadScene(g_client, pack, root, getCurrentPath() + g_modelPaths[0],
                   rockCallback);

  function rockCallback(pack, root, exception) {
    var rockTextures = ['rock_tile_rgb.jpg', 'main_rock_normal.dds'];
    var bumpSamplers = ['DiffuseSampler', 'BumpSampler'];

    var materials = pack.getObjects('lambert2', 'o3d.Material');
    for (var m = 0; m < materials.length; m++) {
      var material = materials[m];
      var fxString = document.getElementById('rocks').value;
      var effect = pack.createObject('Effect');
      effect.loadFromFXString(fxString);

      effect.createUniformParameters(material);
      material.effect = effect;

      var lightDiffuseParam = material.getParam('diffuse_Color');
      if (lightDiffuseParam) {
        lightDiffuseParam.value = [.5, .5, .5, 1];
      }
      for (var t = 0; t < rockTextures.length; t++) {
        loadTexture(loader, pack, material, bumpSamplers[t], rockTextures[t]);
      }
    }
    connectParams(pack);
  }
  loader.finish();
}

/**
 * Creates the bridge and its shaders.
 * @param {!o3d.Pack} pack Pack to load the bridge into.
 * @param {!o3d.Transform} root Parent transform for the bridge shape.
 */
function bridge(pack, root) {
  var loader = g_loader.createLoader(function() {
      o3djs.pack.preparePack(pack, g_viewInfo);});
  loader.loadScene(
      g_client, pack, root, getCurrentPath() + g_modelPaths[1], callback);

  function callback(pack, root, exception) {
    var materials = pack.getObjects('bridgeMaterial', 'o3d.Material');
    for (var m = 0; m < materials.length; m++) {
      var material = materials[m];

      var fxString = document.getElementById('bridge').value;
      var effect = pack.createObject('Effect');
      effect.loadFromFXString(fxString);

      effect.createUniformParameters(material);
      material.effect = effect;

      // White ambient light
      var lightAmbientParam = material.getParam('lightAmbient');
      if (lightAmbientParam) {
        lightAmbientParam.value = [0.04, 0.04, 0.04, 1];
      }
      // Reddish diffuse light
      var lightDiffuseParam = material.getParam('lightDiffuse');
      if (lightDiffuseParam) {
        lightDiffuseParam.value = [.45, .27, .07, 1];
      }
    }
    connectParams(pack);
  }

  loader.finish();
}

/**
 * Creates the water shapes, applies correct shaders and connects any special
 * water params.
 * @param {!o3d.Pack} pack Pack to load the water into.
 * @param {!o3d.Transform} root Parent transform for the water shape.
 */
function water(pack, root) {
  // Create the material
  o3djs.dump.dump('-- start water\n');

  var material = pack.createObject('Material');
  o3djs.dump.dump('-- water: created material\n');
  material.drawList = g_viewInfo.performanceDrawList;
  var effect = pack.createObject('Effect');
  var effectString = document.getElementById('water').value;
  effect.loadFromFXString(effectString);
  effect.createUniformParameters(material);
  material.effect = effect;

  o3djs.dump.dump('-- water: created material & effect\n');
  var swizzle1Param = material.getParam('swizzle1');
  swizzle1Param.bind(g_swizzle1Param);
  o3djs.dump.dump('-- water: bound swizzle param\n');

  var water = o3djs.primitives.createDisc(pack, material,
                                          155, 300, 500, 300, 10)
  var loader = g_loader.createLoader(
      function() {
        root.addShape(water);
      });

  // The texture that this sampler references changes every 3 frames
  g_samplerParam = material.getParam('HeightSampler');
  if (g_samplerParam) {
    o3djs.dump.dump('bind sampler param\n');
    g_samplerParam.value = pack.createObject('Sampler');
  }

  // This is so "index" is unique.
  // @param {number} index Index of texture to register.
  // @return {!function(!o3d.Texture): void} A function that takes a
  //     texture.
  function registerDeepWaterTexture(index) {
    return function(texture) {
      g_deepWaterTextures[index] = texture;
    }
  }

  o3djs.dump.dump('-- water: start loading deep water textures\n');
  for (var i = 0; i < 30; i++) {
    var textureName = 'deepwater.' + i + '.png';
    var url = getCurrentPath() + 'assets/deepwater/' + textureName;
    loader.loadTexture(pack, url, registerDeepWaterTexture(i));
  }

  var sampler = loadTexture(loader, pack, material, 'Reflectivity',
                            'reflectivity_map.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;
  sampler.magFilter = g_o3d.Sampler.LINEAR;
  sampler.minFilter = g_o3d.Sampler.LINEAR;

  sampler = loadTexture(loader, pack, material, 'ReflectionSampler',
                        'rock_reflection_new.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;

  sampler = loadTexture(loader, pack, material, 'HorizonRamp',
                        'horizon_ramp_1.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;

  sampler = loadTexture(loader, pack, material, 'SunRamp', 'sun_ramp_1.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;

  sampler = loadTexture(loader, pack, material, 'RockTexture',
                        'rock_tile_rgb.jpg');

  connectParams(pack);
  o3djs.dump.dump('Done applying Shader\n');

  root.localMatrix = g_math.matrix4.translation([-2.751, 0, -8.563]);
  o3djs.dump.dump('-- Finished water\n');
  loader.finish();
}


/**
 * Creates the sun shape, applies correct shaders and connects any special
 * sun params.
 * @param {!o3d.Pack} pack Pack to load the sun into.
 * @param {!o3d.Transform} root Parent transform for the sun shape.
 */
function sun(pack, root) {
  // Create the material
  o3djs.dump.dump('-- start sun\n');

  var material = pack.createObject('Material');
  var effect = pack.createObject('Effect');
  var fxString = document.getElementById('sun').value;
  effect.loadFromFXString(fxString);
  material.drawList = g_viewInfo.zOrderedDrawList;
  material.effect = effect;
  effect.createUniformParameters(material);
  o3djs.dump.dump('-- created sun material\n');

  // Probably not a necessary state, but available for the future
  var state = pack.createObject('State');
  state.getStateParam('CullMode').value = g_o3d.State.CULL_NONE;
  material.state = state;

  var sun = o3djs.primitives.createPlane(pack, material, 20, 20, 2, 2);
  var loader = g_loader.createLoader(
      function() {
        root.addShape(sun);
      });

  var sampler = loadTexture(loader, pack, material, 'DiffuseSampler',
                            'sun_compat.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;

  connectParams(pack);
  root.localMatrix = g_math.matrix4.mul(
      g_math.matrix4.rotationX(90 * 0.0174),
      g_math.matrix4.translation([0, 15, 140]));
  loader.finish();
  o3djs.dump.dump('-- Finished sun\n');
}

/**
 * Creates the dume sphere shape, applies correct shaders and connects any
 * special dome params.
 * @param {!o3d.Pack} pack Pack to load the dome into.
 * @param {!o3d.Transform} root Parent transform for the dome sphere shape.
 */
function dome(pack, root) {
  // Create the material
  o3djs.dump.dump('-- start dome\n');

  var material = pack.createObject('Material');
  var effect = pack.createObject('Effect');
  var fxString = document.getElementById('dome').value;
  effect.loadFromFXString(fxString);
  material.effect = effect;
  material.drawList = g_viewInfo.performanceDrawList;
  effect.createUniformParameters(material);
  o3djs.dump.dump('-- created dome material\n');

  // Turn off culling since we are inside the sphere
  var state = pack.createObject('State');
  state.getStateParam('CullMode').value = g_o3d.State.CULL_NONE;
  material.state = state;

  var dome = o3djs.primitives.createSphere(pack, material, 150, 25, 25);

  var loader = g_loader.createLoader(
      function() {
        root.addShape(dome);
      });

  var sampler = loadTexture(loader, pack, material, 'DiffuseSampler',
                            'sky_compat.png');

  sampler = loadTexture(loader, pack, material, 'HorizonRamp',
                        'horizon_ramp.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;

  sampler = loadTexture(loader, pack, material, 'SunRamp', 'sun_ramp.png');
  sampler.addressModeU = g_o3d.Sampler.CLAMP;
  sampler.addressModeV = g_o3d.Sampler.CLAMP;

  connectParams(pack);
  loader.finish();
  o3djs.dump.dump('-- Finished dome\n');
}

/**
 * Binds params to the global params so they update automatically.
 * @param {!o3d.Pack} pack The pack containing the materials with params.
 */
function connectParams(pack) {
  // Manually connect all the materials' lightWorldPos params to the global
  // params.
  var materials = pack.getObjectsByClassName('o3d.Material');
  for (var m = 0; m < materials.length; ++m) {
    var material = materials[m];
    var param = material.getParam('lightDirection');
    if (param) {
      param.bind(g_lightDirectionParam);
    }
    param = material.getParam('inputTime');
    if (param) {
      param.bind(g_clockParam);
    }
    param = material.getParam('cameraEye');
    if (param) {
     param.value = [-2.751, 3.529, -8.563];
    }
  }
}


/**
 * Remove any callbacks so they don't get called after the page has unloaded.
 */
function unload() {
  if (g_client) {
    g_client.cleanup();
  }
}
