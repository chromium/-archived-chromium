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
 * @fileoverview The beachdemo javascript.
 */


o3djs.require('o3djs.util');
o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.pack');
o3djs.require('o3djs.math');
o3djs.require('o3djs.quaternions');
o3djs.require('o3djs.dump');
o3djs.require('o3djs.camera');
o3djs.require('o3djs.primitives');
o3djs.require('o3djs.loader');
o3djs.require('o3djs.picking');
o3djs.require('o3djs.canvas');
o3djs.require('o3djs.fps');
o3djs.require('o3djs.debug');
o3djs.require('o3djs.particles');

var RENDER_TARGET_WIDTH = 256;
var RENDER_TARGET_HEIGHT = 256;
var PROXY_HEIGHT = 5150;

//        client.root
//            |
//        g_mainRoot
//            |
//      +-----+--------+----------------+
//      |              |                |
//  g_baseRoot   g_waterTransform    g_skyDomeTransform
//      |
//  g_sceneRoot

var g_re;
var g_animateCamera = false;
var g_cameraDuration = 1;
var g_cameraPoint;
var g_cameraPointIndex = 0;
var g_cameraTimer = 0;
var g_demoTimer = 0;
var g_runDemo = true;
var g_oldCameraClock = 0;
var g_speedTransforms = [[], [], [], []];
var g_sceneRoot;
var g_baseRoot;
var g_reflectionClipHeight = 100;
var g_mainClipHeight = 100000000;
var g_o3d;
var g_hudFadeTime;
var g_helpVisible = false;
var g_math;
var g_key;
var g_paint;
var g_sceneUrl;
var g_quaternions;
var g_waterMode = 0;
var g_updateRenderTargets = true;
var g_compileEffect;
var g_reflectRefract = false;
var g_environmentSampler;
var g_materialPanelElement;
var g_propPanelElement;
var g_effectPanelElement;
var g_upperPanelElement;
var g_effectTabsElement;
var g_effectTextAreaElement;
var g_editableEffects = [];
var g_editableEffectsSource = [];
var g_currentEditEffect;
var g_faderColorParam;
var g_faderTransform;
var g_renderTargetDisplayRoot;
var g_sceneElement;
var g_client;
var g_scenePack;
var g_proxyPack;
var g_mainPack;
var g_fadeParams = [];
var g_mainViewInfo;  // main view
var g_hudRoot;  // root transform for hud.
var g_mainRoot;
var g_proxyRoot;
var g_waterLevel = 500;
var g_reflectionViewInfo;
var g_refractionViewInfo;
var g_hudViewInfo;
var g_loader;
var g_loadInfo;
var g_reflectionClipState;
var g_refractionClipState;
var g_mainRenderGraphRoot;
var g_reflectionSurfaceSet;
var g_refractionSurfaceSet;
var g_skyDomeTransform;
var g_waterTransform;
var g_reflectionTexture;
var g_refractionTexture;
var g_globalParams;
var g_globalClockParam;
var g_clipHeightParam;
var g_lightPositionParam;
var g_lightDirectionParam;
var g_lightColorParam;
var g_proxyOffsetParam;
var g_originalLightColor;
var g_viewPositionParam;
var g_underwaterMaterials;
var g_whiteTexture;
var g_whiteSampler;
var g_waterMaterial;
var g_waterEffect;
var g_waterColorAndSkyEffect;
var g_waterStyle2Effect;
var g_torchMaterial;
var g_torchEmitter;
var g_torchTexture;
var g_mistTexture;
var g_topMistEmitter;
var g_bottomMistEmitter;
var g_rippleEmitter;
var g_skyDomeMaterial;
var g_o3dWidth = -1;
var g_o3dHeight = -1;
var g_o3dElement;
var g_cameraInfos = [];
var g_cameraMoveSpeedMultiplier = 50;
var g_keyCurrentlyDown = 0;  // If any key is currently down this is true.
var g_keyDown = [];  // which keys are down by key code.
var g_keyDownKeyCodeFunctions = {};
var g_keyUpKeyCodeFunctions = {};
var g_materialSwapTable = [];
var g_showingSimpleMaterialsMode = 0;
var g_simpleEffects = [];
var g_originalSampler = { };
var g_dragStartContext;
var g_dragging = false;
var g_imageShape;
var g_imageMaterial;
var g_imageEffect;
var g_waterColor = [0.13, 0.19, 0.22, 1];
var g_hudQuad;
var g_fpsManager;
var g_fpsVisible = false;
var g_particleSystem;
var g_particleLoader;
var g_downloadPercent = -1;
var g_showError = false;
var g_sceneEffects = [];
var g_sceneTexturesByURI;

var g_camera = {
  farPlane: 80000,
  nearPlane: 10,
  up: [0, 0, 1],
  fieldOfView: Math.PI / 4,  // 45 degrees
  eye: [-9662, -10927, 1920],
  targetVector: [0.43, 0.90, 0.02],
  xAxis: [0.8335, -0.5522, -0.0157],
  minFieldOfView: 5 * Math.PI / 180,
  maxFieldOfView: 70 * Math.PI / 180
};

var g_cameraPoints = [
  { duration: 15,
    start: {eye: [-7952.043, -3027.629, 1782.494],
            targetVector: [0.054, 0.997, -0.059],
            fieldOfView: 45},
    end: {eye: [4579.533, -3707.403, 1782.494],
          targetVector: [0.221, 0.963, 0.156],
          fieldOfView: 45}},
  { duration: 5,
    start: {eye: [-9862.542, -11676.196, 1888.062],
            targetVector: [0.552, 0.834, -0.007],
            fieldOfView: 45},
    end: {eye: [-4809.674, -4048.170, 1822.536],
          targetVector: [0.552, 0.834, -0.007],
          fieldOfView: 45}},
  { duration: 5,
    start: {eye: [2728.344, -6393.682, 2241.729],
            targetVector: [-0.312, 0.949, 0.039],
            fieldOfView: 45},
    end: {eye: [-1683.553, 3379.889, 3616.049],
          targetVector: [-0.118, 0.796, 0.594],
          fieldOfView: 45}},
  { duration: 5,
    start: {eye: [1499.756, -2208.060, 380.914],
            targetVector: [-0.537, 0.788, 0.303],
            fieldOfView: 45},
    end: {eye: [7333.003, -6937.257, 4163.998],
          targetVector: [-0.811, 0.509, -0.290],
          fieldOfView: 45}},
  { duration: 5,
    start: {eye: [4746.377, 1086.006, 3433.715],
            targetVector: [-0.982, 0.188, -0.036],
            fieldOfView: 45},
    end: {eye: [4746.377, 1086.006, 3433.715],
          targetVector: [-0.996, 0.072, 0.044],
          fieldOfView: 6.49667876045379}},
  { duration: 5,
    start: {eye: [-4173.890, -4212.830, 398.267],
            targetVector: [-0.339, 0.900, 0.272],
            fieldOfView: 45},
    end: {eye: [-4149.606, -4391.048, 2110.549],
          targetVector: [0.007, 0.998, 0.065],
          fieldOfView: 45}},
  { duration: 5,
    start: {eye: [-4809.674, -4048.170, 1822.536],
            targetVector: [0.294, 0.956, -0.022],
            fieldOfView: 45},
    end: {eye: [-4535.282, -187.079, 2537.158],
          targetVector: [0.146, 0.971, 0.190],
          fieldOfView: 45}}];

// The artists followed no rules. The assumption by the o3djs libraries
// is that textures with non-one alpha should be drawn with alpha
// blending on in the zOrderedDrawPass, otherwise they should be drawn
// with alpha blending off in the performanceDrawPass but the artists gave
// us textures that have non-one alpha even though they are meant to be
// drawn opaque.
//
// The next most common way to decide whether to use opaque or
// transparent blending is a naming convention but the arists
// didn't do that either.
//
// For some cases it doesn't really matter but, (1) drawing with alpha
// blending on is much slower than off and (2) drawing in the
// zOrderedDrawPass has to sort which is slow and sometimes things
// can get sorted wrong if they are too large relative to each other.
//
// So, here's a hard coded list to set the materials to the correct
// drawList :-(
function makeInfo(list, reflect, refract, main, type, effect) {
  return {
      list: list,
      reflect: reflect,
      refract: refract,
      main: main,
      type: type,
      effect: effect};
}
var g_materialLists = {
    // ---------------------------- list reflect refract main adj
    '_6_-_Default': makeInfo(0, true, false, true, 1,
                                      'just_color'),
    'default': makeInfo(1, false, false, true, 1,  // palmTreeB
                                      'diffuse_bump'),
    'Folg_BushA_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse_bump'),
    'Folg_BushB_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse_bump'),
    'Folg_BushC_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse_bump'),
    'Folg_coralD_mat': makeInfo(1, false, true, false, 1,
                                      'diffuse'),
    'Folg_coralG_mat': makeInfo(1, false, true, false, 1,
                                      'diffuse'),
    'Folg_coralRockA_mat': makeInfo(0, false, true, false, 2,
                                      'diffuse_bump_2textures'),
    'Folg_coralRockB_mat': makeInfo(0, false, true, false, 2,
                                      'diffuse_bump_2textures'),
    'Folg_FernA_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse'),
    'Folg_hangingFerns_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse_bump'),
    'Folg_largeFernA_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse'),
    'Folg_LeafyPlantA_mat': makeInfo(1, true, false, true, 1,
                                      'diffuse_bump'),
    'Folg_palmTreeA': makeInfo(1, false, false, true, 1,
                                      'diffuse'),
    'Prop_brokenShip_mat': makeInfo(0, true, true, true, 0,
                                      'diffuse'),
    'Prop_pillarA_mat': makeInfo(0, false, false, true, 0,
                                      'diffuse_bump_specular'),
    'prop_tikiMaskA': makeInfo(0, false, false, true, 0,
                                      'diffuse_bump_specular'),
    'Prop_TorchA_mat': makeInfo(0, false, false, true, 0,
                                      'diffuse'),
    'Prop_wallA_mat': makeInfo(0, false, false, true, 0,
                                      'diffuse_bump'),
    'Props_Bridge_mat': makeInfo(0, true, false, true, 0,
                                      'diffuse'),
    'Rock_Dark': makeInfo(0, true, true, true, 2,
                                      'diffuse_bump'),
    'Sand_Dark': makeInfo(0, false, true, false, 0,
                                      'diffuse_bump_2textures'),
    'Standard_2': makeInfo(0, true, true, false, 0,  // palmfrawns
                                      'diffuse'),
    'Standard_3': makeInfo(1, false, true, true, 0,  // waterfall
                                      ''),
    'Rock_Dark_Island': makeInfo(0, true, true, true, 2,  // Island
                                      'diffuse_bump_blend')};

var g_randSeed = 0;
var g_randRange = Math.pow(2, 32);

/**
 * Dumps a vector with a name label in a format useful for javascript.
 * @param {string} name The name.
 * @param {!o3d.math.Vector3} v The vector.
 */
function dumpVector(name, v) {
  o3djs.dump.dump(
      ' ' + name + ': [' +
      v[0].toFixed(3) + ', ' +
      v[1].toFixed(3) + ', ' +
      v[2].toFixed(3) + '],\n');
}

/**
 * Dump the camera info in a format useful for javascript.
 */
function dumpCameraInfo() {
  o3djs.dump.dump('{');
  dumpVector('eye', g_camera.eye);
  dumpVector('targetVector', g_camera.targetVector);
  o3djs.dump.dump('  fieldOfView: ' +
                  g_math.radToDeg(g_camera.fieldOfView) + '};\n');
}

/**
 * Returns a deterministic pseudorandom number bewteen 0 and 1.
 * @return {number} a random number between 0 and 1.
 */
function pseudoRandom() {
  return (g_randSeed = (134775813 * g_randSeed + 1) % g_randRange) /
         g_randRange;
}

// ***************************** Mouse functions *******************************

/**
 * Handler for onmousedown.
 * @param {event} e A mouse event.
 */
function onMouseDown(e) {
  if (!g_keyCurrentlyDown) {
    g_dragging = true;
    g_dragStartContext = {
      view: o3djs.math.copyMatrix(g_mainViewInfo.drawContext.view),
      projection: o3djs.math.copyMatrix(g_mainViewInfo.drawContext.projection),
      offsetX: g_client.width * 0.5 - e.x,
      offsetY: g_client.height * 0.5 - e.y
    };
    stopAnimatedCamera();
  }
}

/**
 * Handler for onmousemove.
 * @param {event} e A mouse event.
 */
function onMouseMove(e) {
  if (g_dragging) {
    // Compute the world ray based on the view we had when we started dragging.
    var worldRay = o3djs.picking.clientPositionToWorldRayEx(
        g_o3dWidth - (e.x + g_dragStartContext.offsetX),
        g_o3dHeight - (e.y + g_dragStartContext.offsetY),
        g_dragStartContext.view,
        g_dragStartContext.projection,
        g_o3dWidth,
        g_o3dHeight);

    g_camera.targetVector = g_math.normalize(g_math.subVector(worldRay.near,
                                                              g_camera.eye));
    updateCamera();
    stopAnimatedCamera();
  }
}

/**
 * Handler for onmouseup.
 * @param {event} e A mouse event.
 */
function onMouseUp(e) {
  g_dragging = false;
}

/**
 * Hander for the scroll wheel.
 * @param {Event} e Mouse event.
 */
function onWheel(e) {
  if (e.deltaY) {
    var target = g_camera.minFieldOfView;
    if (e.deltaY < 0) {
      target = g_camera.maxFieldOfView;
    }

    g_camera.fieldOfView = g_math.lerpVector(target, g_camera.fieldOfView, 0.9);

    updateProjection();
    stopAnimatedCamera();
  }
}

// *************************** Keyboard functions ******************************

/**
 * Tracks key down events.
 * @param {Event} e keyboard event.
 */
function onKeyDown(e) {
  if (!g_dragging && !g_keyDown[e.keyCode]) {
    ++g_keyCurrentlyDown;
    g_keyDown[e.keyCode] = true;

    var keyFunction = g_keyDownKeyCodeFunctions[e.keyCode];
    if (keyFunction) {
      keyFunction(e);
    }
  }
}

/**
 * Tracks key up events.
 * @param {Event} e keyboard event.
 */
function onKeyUp(e) {
  if (g_keyDown[e.keyCode]) {
    --g_keyCurrentlyDown;
    g_keyDown[e.keyCode] = false;

    var keyFunction = g_keyUpKeyCodeFunctions[e.keyCode];
    if (keyFunction) {
      keyFunction(e);
    }
  }
}

/**
 * Converts a keyCode or charCode to a keyCode.
 * @param {number|string} code The key code or char code.
 * @return {number} the key code.
 */
function convertToKeyCode(code) {
  if (typeof(code) == 'string') {
    code = code.charCodeAt(0);
    if (code >= 'a'.charCodeAt(0)) {
      code += 65 - 'a'.charCodeAt(0);
    }
  }
  return code;
}

/**
 * Registers a key code with a key up function.
 * @param {number|string} keyCode The key code to register a function with.
 * @param {!function(!event): void} keyFunction A function that will be passed
 *     the event for the key.
 */
function registerKeyDownFunction(keyCode, keyFunction) {
  g_keyDownKeyCodeFunctions[convertToKeyCode(keyCode)] = keyFunction;
}

/**
 * Registers a key code with a key down function.
 * @param {number|string} keyCode The key code to register a function with.
 * @param {!function(!event): void} keyFunction A function that will be passed
 *     the event for the key.
 */
function registerKeyUpFunction(keyCode, keyFunction) {
  g_keyUpKeyCodeFunctions[convertToKeyCode(keyCode)] = keyFunction;
}

/**
 * Registers a key code with a both a key down and key up function.
 * @param {number|string} keyCode The key code to register a function with.
 * @param {!function(!event): void} keyUpFunction A function that will be passed
 *     the event for the key being released.
 * @param {!function(!event): void} keyDownFunction A function that will be
 *     passed the event for the key being down..
 */
function registerKeyUpDownFunction(keyCode, keyUpFunction, keyDownFunction) {
  registerKeyUpFunction(keyCode, keyUpFunction);
  registerKeyUpFunction(keyCode, keyDownFunction);
}

/**
 * Registers key handlers.
 */
function registerKeyHandlers() {
  registerKeyDownFunction('0', keySetCamera);
  registerKeyDownFunction('1', keySetCamera);
  registerKeyDownFunction('2', keySetCamera);
  registerKeyDownFunction('3', keySetCamera);
  registerKeyDownFunction('4', keySetCamera);
  registerKeyDownFunction('5', keySetCamera);
  registerKeyDownFunction('6', keySetCamera);
  registerKeyDownFunction('7', keySetCamera);
  registerKeyDownFunction('8', keySetCamera);
  registerKeyDownFunction('9', keySetCamera);

  registerKeyDownFunction('h', toggleHelp);
  registerKeyDownFunction('p', togglePropsPanel);
  registerKeyDownFunction('m', toggleMaterialPanel);
  registerKeyDownFunction('e', toggleEffectPanel);
  registerKeyDownFunction('r', toggleRenderTargets);
  registerKeyDownFunction('f', toggleFps);
  registerKeyDownFunction('c', toggleSimpleMaterials);
  registerKeyDownFunction('o', toggleWaterEffect);
  registerKeyDownFunction('q', toggleDemoCamera);

  // Comment the line below in to enable dumping camera info.
  // This can be used to generate camera points for the animated
  // camera but is only compatible with Firefox.
  //registerKeyDownFunction('z', dumpCameraInfo);
}

// **************************** Camera Functions *******************************

/**
 * Updates the camera (the view matrix of the drawContext) with the current
 * camera settings.
 */
function updateCamera() {
  var target = g_math.addVector(g_camera.eye, g_camera.targetVector);
  var view = g_math.matrix4.lookAt(g_camera.eye,
                                   target,
                                   g_camera.up);
  g_viewPositionParam.value = g_camera.eye;
  g_mainViewInfo.drawContext.view = view;
  g_reflectionViewInfo.drawContext.view = view;
  g_refractionViewInfo.drawContext.view = view;
  var cameraMatrix = g_math.inverse4(view);
  g_camera.xAxis = cameraMatrix[0].splice(0, 3);
  g_updateRenderTargets = true;
}

/**
 * Updates the projection matrix of the drawContext with the current camera
 * settings.
 */
function updateProjection() {
  // Create a perspective projection matrix.
  g_mainViewInfo.drawContext.projection = g_math.matrix4.perspective(
    g_camera.fieldOfView, g_o3dWidth / g_o3dHeight, g_camera.nearPlane,
    g_camera.farPlane);

  g_reflectionViewInfo.drawContext.projection = g_math.matrix4.perspective(
    g_camera.fieldOfView, g_o3dWidth / g_o3dHeight,
    g_camera.nearPlane, g_camera.farPlane);

  g_refractionViewInfo.drawContext.projection = g_math.matrix4.perspective(
    g_camera.fieldOfView, g_o3dWidth / g_o3dHeight,
    g_camera.nearPlane, g_camera.farPlane);

  g_hudViewInfo.drawContext.projection = g_math.matrix4.orthographic(
      0 + 0.5,
      g_o3dWidth + 0.5,
      g_o3dHeight + 0.5,
      0 + 0.5,
      0.001,
      1000);
  g_updateRenderTargets = true;
}

/**
 * Update the fader plane for the current client area size.
 */
function updateFaderPlane() {
  // Scale fader plane to cover screen.
  // If we made a custom shader for this this wouldn't be needed.
  g_faderTransform.identity();
  g_faderTransform.translate(0, 0, -10);
  g_faderTransform.scale(g_client.width, g_client.height, 1);
}

/**
 * Sets the camera to a preset.
 * @param {number} cameraIndex Index of camera preset.
 */
function setCamera(cameraIndex) {
  var cameraInfo = g_cameraInfos[cameraIndex];
  // pull out camera info from view matrix.
  var cameraMatrix = g_math.inverse4(cameraInfo.view);
  g_camera.eye = cameraMatrix[3].splice(0, 3);
  g_camera.targetVector = g_math.negativeVector(cameraMatrix[2].splice(0, 3));
  //g_camera.fieldOfView = cameraInfo.fieldOfViewRadians;
  g_camera.fieldOfView = o3djs.math.degToRad(45);

  updateCamera();
  updateProjection();
  stopAnimatedCamera();
}

/**
 * Moves the camera in its local X axis.
 * @param {number} direction Position or negative amount to move.
 */
function moveCameraLeftRight(direction) {
  direction *= g_cameraMoveSpeedMultiplier;
  g_camera.eye = g_math.addVector(
      g_camera.eye,
      g_mathVectorScalar.mul(g_camera.xAxis, direction));
  updateCamera();
  stopAnimatedCamera();
}

/**
 * Moves the camera in its local Z axis.
 * @param {number} direction Position or negative amount to move.
 */
function moveCameraForwardBack(direction) {
  direction *= g_cameraMoveSpeedMultiplier;
  g_camera.eye = g_math.addVector(
      g_camera.eye,
      g_math.mulVectorScalar(g_camera.targetVector, direction));
  updateCamera();
  stopAnimatedCamera();
}

// ************************ Effect Editor Functions ****************************

/**
 * Starts editing an effect.
 * @param {number} effectId The clientId of the effect.
 */
function editEffect(effectId) {
  if (g_currentEditEffect) {
    // Save the current edit.
    // TODO: would it be better to have a textarea per effect and
    //     hide / unhide them?
    g_editableEffectsSource[g_currentEditEffect.clientId] =
        g_effectTextAreaElement.value;
  }

  var effect = g_client.getObjectById(effectId);
  g_effectTextAreaElement.value = g_editableEffectsSource[effectId];

  g_currentEditEffect = effect;
}

/**
 * Edits an effect from the value on an option element
 */
function editEffectFromElement() {
  var element = o3djs.util.getElementById('effectselect');
  editEffect(parseInt(element.value));
}

/**
 * Compiles the current effect.
 */
function compileEffect() {
  if (g_currentEditEffect) {
    var source = g_effectTextAreaElement.value;

    // Turn off the default error callback so we can get the error ourselves.
    g_client.clearErrorCallback();
    g_client.clearLastError();
    g_compileEffect.loadFromFXString(source);
    var error = g_client.lastError;
    o3djs.base.setErrorHandler(g_client);
    if (error) {
      alert(error);
    } else {
      g_currentEditEffect.loadFromFXString(source);
      // TODO: call createUniformParameters for all materials
      //     using this effect then call setupMaterialEditor so it will
      //     display new parameters.

      // Tell the render targets to update.
      g_updateRenderTargets = true;
    }
  }
}

/**
 * Setup effect editor.
 */
function setupEffectEditor() {
  // create an effect for testing.
  g_compileEffect = g_mainPack.createObject('Effect');

  var compileButton = o3djs.util.getElementById('compileButton');
  compileButton.onclick = compileEffect;

  // create pseudo tabs.
  // TODO: Make it look prettier.
  var html = '<select id="effectselect">';
  for (var ii = 0; ii < g_editableEffects.length; ++ii) {
    var effect = g_editableEffects[ii];
    g_editableEffectsSource[effect.clientId] = effect.source;
    html += '' +
      '<option value="' + effect.clientId + '">' + effect.name + '</option>';
  }
  g_effectTabsElement.innerHTML = html + '</select>';
  var element = o3djs.util.getElementById('effectselect');
  element.onchange = editEffectFromElement;
  element.onblur = editEffectFromElement;

  // Setup the first effect.
  editEffect(g_editableEffects[0].clientId);
}

// ************************* Prop Editor Functions *****************************

/**
 * Setups the prop editor.
 */
function setupPropEditor() {
  var propPrefixes = {watersurface: true};
  var transforms = g_scenePack.getObjectsByClassName('o3d.Transform');
  for (var tt = 0; tt < transforms.length; ++tt) {
    var transform = transforms[tt];
    if (transform.shapes.length > 0) {
      var name = transform.name;
      //if (!isNaN(name.substring(name.length -1))) {
      //  var prefix = name.replace(/\d*$/, '');
      //  if (prefix.length > 0) {
      //    propPrefixes[prefix] = true;
      //  }
      //}
      propPrefixes[name] = true;
    }
  }

  var html = '<table>';
  var count = 0;
  for (var prefix in propPrefixes) {
    html += '' +
        '<tr class="' + ((count % 2 == 0) ? 'even' : 'odd') + '"><td>' +
        '<input id="prop_' + prefix + '" ' +
        'type="checkbox" CHECKED />' +
        prefix +
        '</td></tr>';
    ++count;
  }
  g_propPanelElement.innerHTML = html + '</table>';
  for (var prefix in propPrefixes) {
    var input = o3djs.util.getElementById('prop_' + prefix);
    input.onclick = o3djs.util.curry(toggleProp, prefix);
  }
}

/**
 * Toggles props.
 * Goes through all transforms in the client and if their name starts with
 * prefix sets their visibility to true or false.
 * @param {string} prefix Prefix of props to toggle.
 */
function toggleProp(prefix) {
  var element = o3djs.util.getElementById('prop_' + prefix);
  var visible = element.checked;
  // We should probably cache all the transforms since this is an expensive
  // operation.
  var transforms = g_client.getObjectsByClassName('o3d.Transform');
  for (var tt = 0; tt < transforms.length; ++tt) {
    var transform = transforms[tt];
    if (transform.name.substring(0, prefix.length) === prefix) {
      transform.visible = visible;
    }
  }
  // Tell the render targets to update.
  g_updateRenderTargets = true;
}

// *********************** Material Editor Functions ***************************

/**
 * Escapes a string, changing < to &lt;
 * @param {string} str to escape.
 * @return {string} escaped string.
 */
function escapeHTML(str) {
  return str.replace(/</g, '&lt;');
}

/**
 * Gets a param value as a string
 * @param {!o3d.Param} param Param to get value from.
 * @return {string} value of param as a string.
 */
function getParamAsString(param) {
  if (param.isAClassName('o3d.ParamFloat')) {
    return param.value.toFixed(5);
  } else if (param.isAClassName('o3d.ParamFloat4')) {
    var value = param.value;
    for (var ii = 0; ii < value.length; ++ii) {
      value[ii] = value[ii].toFixed(2);
    }
    return value.toString();
  } else {
    return '--na--';
  }
}

/**
 * Reads the current value of the input and sets the matching param to that
 * value.
 * @param {number} paramId Id of param and input.
 */
function updateParam(paramId) {
  var param = g_client.getObjectById(paramId);
  var element = o3djs.util.getElementById('param_' + paramId);
  var value = element.value;
  var error = false;
  var v;
  if (param.isAClassName('o3d.ParamFloat')) {
    if (isNaN(value)) {
      error = true;
    }
    v = parseFloat(value);
  } else if (param.isAClassName('o3d.ParamFloat4')) {
    var values = value.split(/ *, *| +/);
    if (values.length != 4) {
      error = true;
    } else {
      v = [];
      for (var ii = 0; ii < values.length; ++ii) {
        if (isNaN(values[ii])) {
          error = true;
          break;
        }
        v[ii] = parseFloat(values[ii]);
      }
    }
  }

  if (!error) {
    param.value = v;
    // Tell the render targets to update.
    g_updateRenderTargets = true;
  }

  element.style.backgroundColor = error ? '#fcc' : '';
}

/**
 * Creates the html to edit the given param object.
 * @param {!o3d.ParamObject} paramObject The param object to create html for.
 * @param {string} rowClass name of class for row.
 * @return {string} the generated HTML.
 */
function createHTMLForParamObject(paramObject, rowClass) {
  var html = '' +
      '<tr class="' + rowClass + '">' +
      '<td class="name" colspan="2">' + escapeHTML(paramObject.name) + '</td>' +
      '</tr>';
  var params = paramObject.params;
  for (var pp = 0; pp < params.length; ++pp) {
    var param = params[pp];
    // Skip builtins and ones with an input connection.
    if (param.name.substring(0, 4) !== 'o3d.' &&
        param.inputConnection == null &&
        (param.isAClassName('o3d.ParamFloat') ||
         param.isAClassName('o3d.ParamFloat4'))) {
      html += '' +
          '<tr>' +
          '<td class="field">' +
          '<label>' + escapeHTML(param.name) + '</label>' +
          '</td>' +
          '<td class="value">' +
          '<input type="text" id="param_' + param.clientId + '" ' +
          'value="' + getParamAsString(param) + '"></input>' +
          '</td>' +
          '</tr>';
    }
  }
  return html;
}

/**
 * Sets the onblur and onchange handlers in the html for a given param object.
 * @param {!o3d.ParamObject} paramObject The param object to create html for.
 */
function setHTMLHandlersForParamObject(paramObject) {
  var params = paramObject.params;
  for (var pp = 0; pp < params.length; ++pp) {
    var param = params[pp];
    // Skip builtins and ones with an input connection.
    if (param.name.substring(0, 4) !== 'o3d.' &&
        param.inputConnection == null &&
        (param.isAClassName('o3d.ParamFloat') ||
         param.isAClassName('o3d.ParamFloat4'))) {
      var input = o3djs.util.getElementById('param_' + param.clientId);
      input.onblur = o3djs.util.curry(updateParam, param.clientId);
      input.onchange = o3djs.util.curry(updateParam, param.clientId);
    }
  }
}

/**
 * Sets up html with event handers to edit the material parameters.
 */
function setupMaterialEditor() {
  var html = '<table>';
  var materials = g_scenePack.getObjectsByClassName('o3d.Material');
  var count = 0;
  materials.unshift(g_globalParams);
  materials.unshift(g_waterMaterial);
  materials.unshift(g_underwaterMaterials[0]);
  materials.unshift(g_underwaterMaterials[1]);
  for (var mm = 0; mm < materials.length; ++mm) {
    var material = materials[mm];
    html += createHTMLForParamObject(material, count % 2 == 0 ? 'even' : 'odd');
    ++count;
  }
  g_materialPanelElement.innerHTML = html + '</table>';

  for (var mm = 0; mm < materials.length; ++mm) {
    var material = materials[mm];
    setHTMLHandlersForParamObject(material)
  }
}

// ************************* Specific Key Handlers *****************************

function setupWaterHeavyUpdateOnlyOnViewChange() {
  g_waterMaterial.effect = g_waterEffect;
}

function setupWaterHeavyUpdateAlways() {
  g_waterMaterial.effect = g_waterEffect;
}

function setupWaterJustSkyAndColor() {
  g_waterMaterial.effect = g_waterColorAndSkyEffect;
}

function setupWaterStyle2() {
  g_waterMaterial.effect = g_waterStyle2Effect;
}

/**
 * Toggles the water effect.
 * @param {Event} e Event for key that was pressed.
 */
function toggleWaterEffect(e) {
  ++g_waterMode;
  if (g_waterMode == 4) {
    g_waterMode = 0;
  }

  switch (g_waterMode) {
    case 0:
      setupWaterHeavyUpdateOnlyOnViewChange();
      break;
    case 1:
      setupWaterHeavyUpdateAlways();
      break;
    case 2:
      setupWaterJustSkyAndColor();
      break;
    case 3:
      setupWaterStyle2();
      break;
  }
}

/**
 * Toggles the animted camera.
 * @param {Event} e Event for key that was pressed.
 */
function toggleDemoCamera(e) {
  g_runDemo = !g_runDemo;
  if (g_runDemo) {
    g_animateCamera = true;
  } else {
    stopAnimatedCamera();
  }
}

/**
 * Restores and original sampler on params of a certain name.
 * @param {!o3d.ParamObject} paramObject Object to restore samplers on.
 * @param {string} samplerName Name of sampler parameter to restore.
 */
function restoreOriginalSampler(paramObject, samplerName) {
  var param = paramObject.getParam(samplerName);
  if (param) {
    param.value = g_originalSampler[param.clientId];
  }
}

/**
 * Replaces samplers on params of a certain name with a white sampler.
 * @param {!o3d.ParamObject} paramObject Object to replace samplers on.
 * @param {string} samplerName Name of sampler parameter to replace samplers on.
 */
function replaceSamplerWithWhiteSampler(paramObject, samplerName) {
  var param = paramObject.getParam(samplerName);
  if (param) {
    g_originalSampler[param.clientId] = param.value;
    param.value = g_whiteSampler;
  }
}

/**
 * Toggles the materials to simple effects.
 * @param {Event} e Event for key that was pressed.
 */
function toggleSimpleMaterials(e) {
  g_updateRenderTargets = true;

  var materials = g_scenePack.getObjectsByClassName('o3d.Material');
  materials.unshift(g_waterMaterial);
  materials.unshift(g_underwaterMaterials[0]);
  materials.unshift(g_underwaterMaterials[1]);

  ++g_showingSimpleMaterialsMode;
  g_showingSimpleMaterialsMode = g_showingSimpleMaterialsMode % 4;

  switch (g_showingSimpleMaterialsMode) {
    case 1: {
      g_originalLightColor = g_lightColorParam.value;
      g_lightColorParam.value = [1, 1, 1, 1];
      var drawElements = g_scenePack.getObjectsByClassName('o3d.DrawElement');
      for (var ii = 0; ii < drawElements.length; ++ii) {
        replaceSamplerWithWhiteSampler(drawElements[ii], 'diffuseSampler');
      }
      break;
    }
    case 2: {
      g_lightColorParam.value = g_originalLightColor;
      var drawElements = g_scenePack.getObjectsByClassName('o3d.DrawElement');
      for (var ii = 0; ii < drawElements.length; ++ii) {
        restoreOriginalSampler(drawElements[ii], 'diffuseSampler');
      }
      break;
    }
  }

  for (var mm = 0; mm < materials.length; ++mm) {
    var material = materials[mm];
    switch (g_showingSimpleMaterialsMode) {
      case 0: {
        material.effect = g_materialSwapTable[material.clientId];
        break;
      }
      case 1: {
        replaceSamplerWithWhiteSampler(material, 'diffuseSampler');
        replaceSamplerWithWhiteSampler(material, 'diffuse2Sampler');
        break;
      }
      case 2: {
        restoreOriginalSampler(material, 'diffuseSampler');
        restoreOriginalSampler(material, 'diffuse2Sampler');

        var effect = material.effect;
        g_materialSwapTable[material.clientId] = effect;
        if (!g_simpleEffects[effect.clientId]) {
          pseudoRandom();  // eat a random number to get pleasing colors.
          pseudoRandom();  // eat a random number to get pleasing colors.
          var newEffect = g_mainPack.createObject('Effect');
          newEffect.loadFromFXString(
              o3djs.util.getElementContentById('simpleshader'));
          newEffect.createUniformParameters(newEffect);
          newEffect.getParam('simpleColor').value = [
              pseudoRandom(),
              pseudoRandom(),
              pseudoRandom(),
              1];
          g_simpleEffects[effect.clientId] = newEffect;
        }
        material.effect = g_simpleEffects[effect.clientId];
        break;
      }
      case 3: {
        material.effect = g_imageEffect;
        break;
      }
    }
  }
}

/**
 * Toggles the render target display.
 * @param {Event} e Event for key that was pressed.
 */
function toggleRenderTargets(e) {
  g_renderTargetDisplayRoot.visible = !g_renderTargetDisplayRoot.visible;
}

/**
 * Toggles the fps display.
 * @param {Event} e Event for key that was pressed.
 */
function toggleFps(e) {
  g_fpsVisible = !g_fpsVisible;
  g_fpsManager.setVisible(g_fpsVisible);
}

function togglePropsPanel(e) {
  if (g_propPanelElement.style.display === '') {
    g_propPanelElement.style.display = 'none';
    g_sceneElement.style.width = '100%';
  } else {
    g_materialPanelElement.style.display = 'none';
    g_propPanelElement.style.display = '';
    g_sceneElement.style.width = '80%';
  }
}

/**
 * Toggles the material panel.
 * @param {Event} e Event for key that was pressed.
 */
function toggleMaterialPanel(e) {
  if (g_materialPanelElement.style.display === '') {
    g_materialPanelElement.style.display = 'none';
    g_sceneElement.style.width = '100%';
  } else {
    g_propPanelElement.style.display = 'none';
    g_materialPanelElement.style.display = '';
    g_sceneElement.style.width = '80%';
  }
}

/**
 * Toggles the effect panel.
 * @param {Event} e Event for key that was pressed.
 */
function toggleEffectPanel(e) {
  if (g_effectPanelElement.style.display === '') {
    g_effectPanelElement.style.display = 'none';
    g_upperPanelElement.style.height = '100%';
  } else {
    g_effectPanelElement.style.display = '';
    g_upperPanelElement.style.height = '70%';
  }
}

/**
 * Sets the camera to a camera preset from a key press.
 * @param {Event} e Event for key that was pressed. Expects 0-9.
 */
function keySetCamera(e) {
  var index = e.keyCode - 49;
  if (index < 0) {
    index = 9;
  }
  var cameraInfo = g_cameraInfos[index];
  if (cameraInfo) {
    setCamera(index);
  }
}

// ***************************** Scene Functions *******************************

/**
 * Sets the position of the sun, updating shader parameters.
 * @param {!o3djs.math.Vector3} position The position of the sun.
 */
function setSunPosition(position) {
  g_lightPositionParam.value = position;
  g_lightDirectionParam.value = g_math.negativeVector(
      g_math.normalize(position));
  g_lightDirectionParam.value = g_math.normalize(position);
}

// ********************************** Misc *************************************

/**
 * Sets a param if it exists.
 * @param {!o3d.ParamObject} paramObject The object that has the param.
 * @param {string} paramName name of param.
 * @param {*} value the value for the param.
 */
function setParam(paramObject, paramName, value) {
  var param = paramObject.getParam(paramName);
  if (param) {
    param.value = value;
  }
}

/**
 * Binds a param if it exists.
 * @param {!o3d.ParamObject} paramObject The object that has the param.
 * @param {string} paramName name of param.
 * @param {!o3d.Param} sourceParam The param to bind to.
 */
function bindParam(paramObject, paramName, sourceParam) {
  var param = paramObject.getParam(paramName);
  if (param) {
    param.bind(sourceParam);
  }
}

/**
 * Prints out a transform tree.
 * @param {!o3d.Transform} transform transform to print.
 * @param {string} prefix Prefix to print.
 */
function dumpTransforms(transform, prefix) {
  var materialName = '';
  var shapes = transform.shapes;
  if (shapes.length > 0) {
    materialName = ' (' + shapes[0].elements[0].material.name + ')';
  }
  o3djs.dump.dump(prefix + transform.name + materialName + '\n');
  var children = transform.children;
  for (var cc = 0; cc < children.length; ++cc) {
    dumpTransforms(children[cc], prefix + '    ');
  }
}

/**
 * Adds transforms at each level of the scene to group things by where they
 * need to be rendered, refraction, main, both.
 * @param {!o3d.Transform} transform Transform to scan.
 */
function getSpeedTransforms(transform) {
  // 0 : neither, 1 : main, 2 : reflect, 3 : both
  var speedTransforms = [];
  var children = transform.children;
  for (var cc = 0; cc < children.length; ++cc) {
    var child = children[cc];
    var check = child;

    // If a child  has a single child of the same but with the suffix
    // '_PIVOT' use that as the check node.
    var checkChildren = child.children;
    if (checkChildren.length == 1 &&
        checkChildren[0].name == child.name + '_PIVOT') {
      check = checkChildren[0];
    }
    // If check has a shape that has a primitive that uses one of the
    // materials on the list then attach it to a speed transform.
    var grouped = false;
    var shapes = check.shapes;
    if (shapes.length > 0) {
      // gets assume 1 shape, 1 element
      var material = shapes[0].elements[0].material;
      var materialInfo = g_materialLists[material.name];
      if (materialInfo) {
        grouped = true;
        var index = (materialInfo.main ? 1 : 0) +
                    (materialInfo.reflect ? 2 : 0);
        var speedTransform = speedTransforms[index];
        if (!speedTransform) {
          speedTransform = g_mainPack.createObject('Transform');
          speedTransform.name = 'speed_' + index;
          speedTransform.parent = transform;
          speedTransforms[index] = speedTransform;
        }
        child.parent = speedTransform;
      }
    }

    if (!grouped) {
      getSpeedTransforms(child);
    }
  }

  // Now add speed transforms to global list.
  for (var ii = 0; ii < 4; ++ii) {
    if (speedTransforms[ii]) {
      g_speedTransforms[ii].push(speedTransforms[ii]);
    }
  }
}

/**
 * Gets a texture from g_scenePack.
 * @param {string} textureName Name of texture.
 * @return {!o3d.Texture} The requested texture.
 */
function getTexture(textureName) {
  // I'm searching by URI because the old conditioner sadly renamed all the
  // textures making it next to impossible to find things :-(
  if (!g_sceneTexturesByURI) {
    g_sceneTexturesByURI = { };
    var textures = g_scenePack.getObjectsByClassName('o3d.Texture');
    for (var tt = 0; tt < textures.length; ++tt) {
      var texture = textures[tt];
      var uri = texture.getParam('uri').value;
      g_sceneTexturesByURI[uri] = texture;
    }
  }

  return g_sceneTexturesByURI['images/' + textureName];
}

/**
 * Adds a texture to a material.
 * @param {!o3d.Material} material Material to add texture to.
 * @param {string} samplerName Name of sampler parameter to attach texture to.
 * @param {string} textureName Name of texture.
 */
function addTexture(material, samplerName, textureName) {
  var param = material.createParam(samplerName, 'ParamSampler');
  var sampler = g_scenePack.createObject('Sampler');
  param.value = sampler;
  sampler.texture = getTexture(textureName);
}

/**
 * Sets up the materials in the scene.
 */
function setupSceneMaterials() {
  var drawLists = [g_mainViewInfo.performanceDrawList,
                   g_mainViewInfo.zOrderedDrawList];

  var adjust = [
      {shininess: 50, specular: [0.5, 0.5, 0.5, 1]},
      {shininess: 100, specular: [0.3, 0.5, 0.3, 1]},
      {shininess: 80, specular: [0.3, 0.3, 0.3, 1]}];

  // Setup the materials. Because Collada can't really handle
  // the materials needed we pretty much have to do this manaually. It would
  // have been good to make a rule for it but I have no time.
  for (var name in g_materialLists) {
    var info = g_materialLists[name];
    var materials = g_scenePack.getObjects(name, 'o3d.Material');
    for (var mm = 0; mm < materials.length; ++mm) {
      var material = materials[mm];
      if (info.effect) {
        var effect = g_sceneEffects[info.effect];
        if (!effect) {
          effect = g_scenePack.createObject('Effect');
          effect.name = info.effect;
          var fxString = o3djs.util.getElementContentById(info.effect);
          effect.loadFromFXString(fxString);
          g_sceneEffects[info.effect] = effect;
          g_editableEffects.push(effect);
        }
        material.effect = effect;
        material.createParam('lightWorldPos', 'ParamFloat3');
        material.createParam('lightColor', 'ParamFloat4');
        material.createParam('clipHeight', 'ParamFloat');

        // special handling for island and seafloor materials.
        if (info.effect == 'diffuse_bump_blend') {
          addTexture(material, 'diffuse2Sampler', 'image1.dds');
        }
      }
      material.drawList = drawLists[info.list];

      // Manually connect all the materials' lightWorldPos params or a global
      // light param.
      bindParam(material, 'lightWorldPos', g_lightPositionParam);
      bindParam(material, 'lightColor', g_lightColorParam);
      bindParam(material, 'clipHeight', g_clipHeightParam);
      setParam(material, 'ambient', [0.2, 0.2, 0.2, 1]);

      var type = info.type;
      setParam(material, 'shininess', adjust[type].shininess);
      setParam(material, 'specular', adjust[type].specular);
    }
  }
}

/**
 * Loads the proxy.
 */
function loadProxy() {
  function callback(pack, parent, exception) {
    g_loadInfo = null;
    if (exception) {
      showError(exception);
    } else {
      loadMainScene();

      o3djs.pack.preparePack(pack, g_mainViewInfo);

      var material = pack.getObjectsByClassName('o3d.Material')[0];
      var effect = g_mainPack.createObject('Effect');
      effect.loadFromFXString(o3djs.util.getElementContentById('proxy'));
      effect.createUniformParameters(material);
      setParam(material, 'lightWorldPos', [0, -100000, 200000]);
      setParam(material, 'ambient', [0, 0, 0, 0]);
      setParam(material, 'diffuse', [0.7, 0.7, 0.7, 0.5]);
      setParam(material, 'specular', [0, 0, 0, 0]);
      bindParam(material, 'offset', g_proxyOffsetParam);
      material.effect = effect;
      material.drawList = g_mainViewInfo.zOrderedDrawList;
      var state = pack.createObject('State');
      material.state = state;
      state.getStateParam('AlphaReference').value = 0.0;
      state.getStateParam('CullMode').value = g_o3d.State.CULL_CCW;
      var material2 = pack.createObject('Material');
      effect.createUniformParameters(material2);
      material2.copyParams(material);
      bindParam(material2, 'offset', g_proxyOffsetParam);
      material2.effect = effect;
      material2.drawList = g_mainViewInfo.zOrderedDrawList;

      state = pack.createObject('State');
      material2.state = state;
      state.getStateParam('AlphaReference').value = 0.0;
      state.getStateParam('CullMode').value = g_o3d.State.CULL_CW;

      parent.createDrawElements(pack, material2);
    }
  }

  g_proxyPack = g_client.createPack();
  g_proxyRoot = g_proxyPack.createObject('Transform');
  g_proxyRoot.parent = g_baseRoot;

  try {
    var url = o3djs.util.getAbsoluteURI('assets/beach-low-poly.o3dtgz');
    g_loadInfo = o3djs.scene.loadScene(g_client, g_proxyPack, g_proxyRoot,
                                       url, callback, {opt_async: true});
  } catch (e) {
    showError(e);
  }
}

/**
 * Loads the main scene.
 */
function loadMainScene() {
  function callback(pack, parent, exception) {
    g_loadInfo = null;
    if (exception) {
      showError(exception);
    } else {
      g_proxyRoot.visible = false;

      setupWaterfall();

      // Generate draw elements and setup material draw lists.
      parent.createDrawElements(pack, null);

      setupSceneMaterials();

      // Turn off culling since we can see the entire world checking culling
      // is a waste of CPU time.
      var elements = g_scenePack.getObjectsByClassName('o3d.Element');
      for (var ee = 0; ee < elements.length; ++ee) {
        elements[ee].cull = false;
        o3djs.element.setBoundingBoxAndZSortPoint(elements[ee]);
      }

      // Add missing streams to terrain.
      var terrainNames = [
          'terrainSpireA_002',
          'terrainSpireA_003',
          'terrainLargeRock',
          'terrainSpireA_001'];
      var semantics = [
          g_o3d.Stream.TEXCOORD,
          g_o3d.Stream.BINORMAL,
          g_o3d.Stream.TANGENT];
      for (var tt = 0; tt < terrainNames.length; ++tt) {
        var streamBank = g_scenePack.getObjects(terrainNames[tt],
                                                'o3d.StreamBank')[0];
        for (var ii = 0; ii < semantics.length; ++ii) {
          var stream = streamBank.getVertexStream(semantics[ii], 0);
          streamBank.setVertexStream(semantics[ii], 1, stream.field, 0)
        }
      }

      g_cameraInfos = o3djs.camera.getCameraInfos(parent,
                                                  g_o3dWidth,
                                                  g_o3dHeight);
      setCamera(1);
      setupUnderwater();

      getSpeedTransforms(g_sceneRoot);

      //o3djs.dump.dump("--------\n");
      //dumpTransforms(g_sceneRoot, '');

      setupMaterialEditor();
      setupEffectEditor();
      setupPropEditor();

      registerKeyHandlers();

      if (false) {
        o3djs.dump.dump('---dump g_scenePack shapes---\n');
        var shapes = g_scenePack.getObjectsByClassName('o3d.Shape');
        for (var t = 0; t < shapes.length; t++) {
          var shape = shapes[t];
          o3djs.dump.dump('shape ' + t + ': ' + shape.name + '\n');
          //o3djs.dump.dumpShape(shape);
        }
      }

      if (false) {
        o3djs.dump.dump('---dump g_scenePack materials---\n');
        var materials = g_scenePack.getObjectsByClassName('o3d.Material');
        for (var t = 0; t < materials.length; t++) {
          var material = materials[t];
          o3djs.dump.dump (
              '  ' + t + ' : ' + material.className +
              ' : "' + material.name + '"\n');
          var params = material.params;
          for (var p = 0; p < params.length; ++p) {
            var param = params[p];
            if (param.className == 'o3d.ParamSampler') {
              o3djs.dump.dump('     ' + p + ': ' +
                              param.value.texture.name + '\n');
            }
          }
          //o3djs.dump.dumpParams(materials[t], '    ');
        }
      }

      if (false) {
        o3djs.dump.dump('---dump g_scenePack textures---\n');
        var textures = g_scenePack.getObjectsByClassName('o3d.Texture');
        for (var t = 0; t < textures.length; t++) {
          o3djs.dump.dump(t + ': ');
          o3djs.dump.dumpTexture(textures[t]);
        }

        o3djs.dump.dump('---dump g_scenePack effects---\n');
        var effects = g_scenePack.getObjectsByClassName('o3d.Effect');
        for (var t = 0; t < effects.length; t++) {
          o3djs.dump.dump ('  ' + t + ' : ' + effects[t].className +
                  ' : "' + effects[t].name + '"\n');
          o3djs.dump.dumpParams(effects[t], '    ');
        }
      }
    }
  }

  try {
    // We need to make a subloader because we can't make the particles
    // until both the scene and the particle textures are loaded.
    g_loadInfo = g_loader.loadInfo;
    g_particleLoader = g_loader.createLoader(setupParticles);
    g_particleLoader.loadTexture(
        g_mainPack,
        o3djs.util.getAbsoluteURI('assets/pe_fire.jpg'),
        function(texture, success) {
          g_torchTexture = texture;
        });
    g_particleLoader.loadTexture(
        g_mainPack,
        o3djs.util.getAbsoluteURI('assets/pe_mist.png'),
        function(texture, success) {
          g_mistTexture = texture;
        });

    var url = o3djs.util.getAbsoluteURI('assets/beachdemo.o3dtgz');
    g_particleLoader.loadScene(
        g_client, g_scenePack, g_sceneRoot, url, callback, {opt_async: true});
    g_particleLoader.finish()
    g_loader.finish();
  } catch (e) {
    showError(e);
  }
}

/**
 * Records the client's size if it's changed.
 */
function setClientSize() {
  var newWidth = parseInt(g_client.width);
  var newHeight = parseInt(g_client.height);

  if (newWidth != g_o3dWidth || newHeight != g_o3dHeight) {
    g_o3dWidth = newWidth;
    g_o3dHeight = newHeight;

    updateProjection();
    g_fpsManager.resize(g_o3dWidth, g_o3dHeight);
    g_fpsManager.setPosition(g_o3dWidth - 80, 10);
    updateFaderPlane();
  }
}

/**
 * Moves the camera based on key state.
 */
function handleCameraKeys() {
  var moveX = 0;
  var moveY = 0;

  if (g_keyDown[37] || g_keyDown[65]) {
    moveX = -1;
  }
  if (g_keyDown[39] || g_keyDown[68]) {
    moveX = 1;
  }
  if (g_keyDown[38] || g_keyDown[87]) {
    moveY = 1;
  }
  if (g_keyDown[40] || g_keyDown[83]) {
    moveY = -1;
  }

  if (moveX) {
    moveCameraLeftRight(moveX);
    stopAnimatedCamera();
  }

  if (moveY) {
    moveCameraForwardBack(moveY);
    stopAnimatedCamera();
  }
}

/**
 * Sets the speed transforms visible or invisible to turn on/off whole groups of
 * shapes not needed for certain rendering.
 * @param {boolean} main Turn on stuff marked for main.
 * @param {boolean} reflect Turn on stuff marked for reflect.
 * @param {boolean} force Force visible to true.
 */
function setSpeedTransforms(main, reflect, force) {
  var mask = (main ? 1 : 0) + (reflect ? 2 : 0);
  for (var ii = 0; ii < 4; ++ii) {
    var visible = ((ii & mask) != 0) || force;
    var speedTransforms = g_speedTransforms[ii];
    for (var jj = 0; jj < speedTransforms.length; ++jj) {
      speedTransforms[jj].visible = visible;
    }
  }
}

/**
 * Eases in a number.
 * @param {number} value Value to ease in. Must be 0 to 1.
 * @return {number} Ease in version of value.
 */
function easeIn(value) {
  return 1 - Math.cos(value * Math.PI * 0.5);
}

/**
 * Eases out a number.
 * @param {number} value Value to ease out. Must be 0 to 1.
 * @return {number} Ease out version of value.
 */
function easeOut(value) {
  return Math.sin(value * Math.PI * 0.5);
}

/**
 * Ease in and out a number.
 * @param {number} value Value to ease in out. Must be 0 to 1.
 * @return {number} Ease in out version of value.
 */
function easeInOut(value) {
  if (value < 0.5) {
    return easeIn(value * 2) * 0.5;
  } else {
    return easeOut(value * 2 - 1) * 0.5 + 0.5;
  }
}

/**
 * Stops the animated camera.
 */
function stopAnimatedCamera() {
  g_animateCamera = false;
  g_demoTimer = 30;
  g_cameraTimer = 0;
  g_faderTransform.visible = false;
}

/**
 * Animates the camera.
 * @param {number} elapsedTime Elapsed time in seconds.
 */
function animateCamera(elapsedTime) {
  if (g_animateCamera && window.g_finished) {
    g_cameraTimer -= elapsedTime;

    if (g_cameraTimer <= 0) {
      ++g_cameraPointIndex;
      if (g_cameraPointIndex >= g_cameraPoints.length) {
        g_cameraPointIndex = 0;
      }
      g_cameraPoint = g_cameraPoints[g_cameraPointIndex];
      g_cameraDuration = g_cameraPoint.duration * 3;
      g_cameraTimer = g_cameraDuration;
    }

    var lerp = 1;
    if (g_cameraTimer > 1) {
      var moveDuration = g_cameraDuration - 1;
      var timer = g_cameraTimer - 1;
      lerp = easeInOut(1 - timer / moveDuration);

      if (g_cameraTimer > g_cameraDuration - 1) {
        var fade = g_cameraTimer - (g_cameraDuration - 1);
        g_faderTransform.visible = true;
        g_faderColorParam.value = [0, 0, 0, fade];
      } else {
        g_faderTransform.visible = false;
      }
    } else {
      g_faderTransform.visible = true;
      g_faderColorParam.value = [0, 0, 0, 1 - g_cameraTimer];
    }

    g_camera.eye = g_math.lerpVector(g_cameraPoint.start.eye,
                                     g_cameraPoint.end.eye,
                                     lerp);
    g_camera.targetVector = g_math.lerpVector(g_cameraPoint.start.targetVector,
                                              g_cameraPoint.end.targetVector,
                                              lerp);
    g_camera.fieldOfView = g_math.degToRad(
        g_math.lerpScalar(g_cameraPoint.start.fieldOfView,
                          g_cameraPoint.end.fieldOfView,
                          lerp));

    updateCamera();
    updateProjection();
  } else {
    if (g_runDemo) {
      g_demoTimer -= elapsedTime;
      if (g_demoTimer <= 0) {
        g_animateCamera = true;
      }
    }
  }
}

/**
 * Called every frame.
 * @param {!o3d.RenderEvent} renderEvent Info about this frame.
 */
function onRender(renderEvent) {
  // save off the render event so look at it from the debugger.
  g_re = renderEvent;

  var elapsedTime = renderEvent.elapsedTime * window.g_timeMult;
  if (g_hudFadeTime > 0) {
    g_hudFadeTime -= elapsedTime;
    if (g_hudFadeTime <= 0) {
      g_hudQuad.transform.visible = false;
    }
  }

  // This is for selenium so that the hud is predictable.
  if (g_hudFadeTime > 0 && window.g_timeMult == 0) {
    g_hudFadeTime = 0;
    g_hudQuad.transform.visible = false;
  }

  // Normally I'd have used a SecondCounter but so we can run this in
  // selenium I set it up this way to be easy.
  window.g_clock += elapsedTime;
  g_globalClockParam.value = window.g_clock;

  if (g_loadInfo) {
    var progressInfo = g_loadInfo.getKnownProgressInfoSoFar();
    g_proxyOffsetParam.value = progressInfo.percent / 100 * PROXY_HEIGHT;
    if (progressInfo.percent != g_downloadPercent) {
      g_downloadPercent = progressInfo.percent;
      setHudText('Loading... ' + progressInfo.percent + '%' +
                 ' (' + progressInfo.downloaded +
                 ' of ' + progressInfo.totalBytes + progressInfo.suffix + ')');
    }
  }

  // This if is for selenium to make the camera predictable.
  if (window.g_timeMult) {
    animateCamera(elapsedTime);
  } else {
    setCamera(1);
  }

  handleCameraKeys();
  setClientSize();
  g_fpsManager.update(renderEvent);

  if (g_updateRenderTargets || g_waterMode == 1) {
    g_updateRenderTargets = false;

    // Render the reflection texture.
    setSpeedTransforms(false, true, false);
    g_clipHeightParam.value = g_reflectionClipHeight;
    g_client.root.identity();
    g_client.root.scale(1, 1, -1);  // flip the scene
    g_client.renderTree(g_reflectionSurfaceSet);

    // Render the refraction texture.
    setSpeedTransforms(true, true, true);
    g_client.root.identity();
    g_client.root.scale(1, 1, 1 /* 0.75 */);  // squish the scene.
    g_client.renderTree(g_refractionSurfaceSet);
  }

  // Render the main scene.
  setSpeedTransforms(true, false, false);
  g_clipHeightParam.value = g_mainClipHeight;
  g_client.root.identity();
  g_client.renderTree(g_mainViewInfo.root);

  // Render the HUD.
  g_client.renderTree(g_hudViewInfo.root);

  // Render the FPS display.
  g_client.renderTree(g_fpsManager.viewInfo.root);
}

function onAllLoadingFinished() {
  g_loader = null;
  g_animateCamera = true;

  showHint();

  window.g_finished = true;  // for selenium testing.
}

/**
 * Creates the client area.
 */
function init() {
  // These are here so they are shared by both V8 and the browser.
  window.g_finished = false;  // for selenium
  window.g_timeMult = 1;
  window.g_clock = 0;

  // Comment out the line below to run the sample in the browser JavaScript
  // engine. This may be helpful for debugging.
  o3djs.util.setMainEngine(o3djs.util.Engine.V8);

  o3djs.util.addScriptUri('');
  o3djs.util.makeClients(initStep2, 'LargeGeometry');
}

/**
 * Initializes O3D and loads the scene into the transform graph.
 * @param {Array} clientElements Array of o3d object elements.
 */
function initStep2(clientElements) {
  g_materialPanelElement = o3djs.util.getElementById('materialpanel');
  g_propPanelElement = o3djs.util.getElementById('proppanel');
  g_effectPanelElement = o3djs.util.getElementById('effectpanel');
  g_upperPanelElement = o3djs.util.getElementById('upperpanel');
  g_effectTabsElement = o3djs.util.getElementById('effecttabs');
  g_effectTextAreaElement = o3djs.util.getElementById('effecttextarea');
  g_sceneElement = o3djs.util.getElementById('o3d');

  g_o3dElement = clientElements[0];
  g_o3d = g_o3dElement.o3d;
  g_math = o3djs.math;
  g_quaternions = o3djs.quaternions;
  window.g_client = g_client = g_o3dElement.client;

  g_mainPack = g_client.createPack();
  g_scenePack = g_client.createPack();

  // Create Render Targets for the reflection and refraction.
  g_reflectionTexture = g_mainPack.createTexture2D(RENDER_TARGET_WIDTH,
                                                   RENDER_TARGET_HEIGHT,
                                                   g_o3d.Texture.ARGB8, 1,
                                                   true);
  var reflectionSurface = g_reflectionTexture.getRenderSurface(0, g_mainPack);
  g_refractionTexture = g_mainPack.createTexture2D(RENDER_TARGET_WIDTH,
                                                   RENDER_TARGET_HEIGHT,
                                                   g_o3d.Texture.XRGB8, 1,
                                                   true);
  var refractionSurface = g_refractionTexture.getRenderSurface(0, g_mainPack);
  var depthSurface = g_mainPack.createDepthStencilSurface(RENDER_TARGET_WIDTH,
                                                          RENDER_TARGET_HEIGHT);

  g_mainRoot = g_mainPack.createObject('Transform');
  g_baseRoot = g_scenePack.createObject('Transform');
  g_baseRoot.parent = g_mainRoot;
  g_sceneRoot = g_scenePack.createObject('Transform');
  g_sceneRoot.parent = g_baseRoot;
  g_mainRoot.parent = g_client.root;
  g_sceneRoot.translate(0, 0, -g_waterLevel);

  // Setup the render graph to generate them.
  g_reflectionSurfaceSet = g_mainPack.createObject('RenderSurfaceSet');
  g_reflectionSurfaceSet.renderSurface = reflectionSurface;
  g_reflectionSurfaceSet.renderDepthStencilSurface = depthSurface;

  g_refractionSurfaceSet = g_mainPack.createObject('RenderSurfaceSet');
  g_refractionSurfaceSet.renderSurface = refractionSurface;
  g_refractionSurfaceSet.renderDepthStencilSurface = depthSurface;

  // Create states to set clipping.
  g_reflectionClipState = g_mainPack.createObject('State');
  g_reflectionClipState.getStateParam('AlphaTestEnable').value = true;
  g_reflectionClipState.getStateParam('AlphaComparisonFunction').value =
      g_o3d.State.CMP_GREATER;
  var reflectionStateSet = g_mainPack.createObject('StateSet');
  reflectionStateSet.state = g_reflectionClipState;
  reflectionStateSet.parent = g_reflectionSurfaceSet;

  var fStrength = 4.0;
  g_refractionClipState = g_mainPack.createObject('State');
  g_refractionClipState.getStateParam('AlphaTestEnable').value = true;
  g_refractionClipState.getStateParam('AlphaComparisonFunction').value =
      g_o3d.State.CMP_GREATER;

  var refractionStateSet = g_mainPack.createObject('StateSet');
  refractionStateSet.state = g_refractionClipState;
  refractionStateSet.parent = g_refractionSurfaceSet;

  // Create the render graph for the main view.
  g_mainViewInfo = o3djs.rendergraph.createBasicView(
      g_mainPack,
      g_mainRoot);

  // Create a render graph for the reflection map
  g_reflectionViewInfo = o3djs.rendergraph.createExtraView(g_mainViewInfo);
  g_reflectionViewInfo.root.parent = reflectionStateSet;
  g_reflectionViewInfo.treeTraversal.transform = g_baseRoot;
  g_reflectionViewInfo.performanceState.getStateParam('CullMode').value =
      g_o3d.State.CULL_CCW;
  g_reflectionViewInfo.performanceState.getStateParam(
      'ColorWriteEnable').value = 15;
  g_reflectionViewInfo.zOrderedState.getStateParam('CullMode').value =
      g_o3d.State.CULL_CCW;
  g_reflectionViewInfo.zOrderedState.getStateParam(
      'ColorWriteEnable').value = 15;

  // Create a render graph for the refraction map
  g_refractionViewInfo = o3djs.rendergraph.createBasicView(
      g_mainPack,
      g_baseRoot,
      refractionStateSet);

  // Create a render graph for the HUD
  g_hudRoot = g_mainPack.createObject('Transform');
  g_hudViewInfo = o3djs.rendergraph.createBasicView(
      g_mainPack,
      g_hudRoot);
  g_hudViewInfo.clearBuffer.clearColorFlag = false;

  g_hudViewInfo.zOrderedState.getStateParam('CullMode').value =
      g_o3d.State.CULL_NONE;

  g_hudViewInfo.drawContext.view = g_math.matrix4.lookAt(
      [0, 0, 1],  // eye
      [0, 0, 0],  // target
      [0, 1, 0]); // up

  //g_reflectionViewInfo.clearBuffer.clearColor = [0.5, 1, 0.5, 1];
  //g_refractionViewInfo.clearBuffer.clearColor = [0.5, 0.5, 1, 1];
  g_reflectionViewInfo.clearBuffer.clearColor = [0, 0, 0, 0];
  g_refractionViewInfo.clearBuffer.clearColor = g_waterColor;

  // Set some names so it's easier to debug.
  g_mainViewInfo.performanceDrawList.name = 'performanceDrawList';
  g_mainViewInfo.zOrderedDrawList.name = 'zOrderedDrawList';

  // Turn off culling for transparent stuff so we can see the backs of leaves.
  g_mainViewInfo.zOrderedState.getStateParam('CullMode').value =
      g_o3d.State.CULL_NONE;
  g_mainViewInfo.zOrderedState.getStateParam('AlphaReference').value = 0.7;

  // Turn on alpha test in the performance list for our clipping plane.
  g_mainViewInfo.performanceState.getStateParam('AlphaTestEnable').value = true;
  g_mainViewInfo.performanceState.getStateParam(
      'AlphaComparisonFunction').value = g_o3d.State.CMP_GREATER;

  g_fpsManager = o3djs.fps.createFPSManager(g_mainPack,
                                            g_client.width,
                                            g_client.height);
  g_fpsManager.setVisible(false);

  // Create a param object to hold a few params to drive things globally.
  g_globalParams = g_mainPack.createObject('ParamObject');
  g_globalParams.name = 'global params';
  g_globalClockParam = g_globalParams.createParam('clock', 'ParamFloat');
  g_lightPositionParam = g_globalParams.createParam('lightWorldPos',
                                                    'ParamFloat3');
  g_lightDirectionParam = g_globalParams.createParam('lightDirection',
                                                     'ParamFloat3');
  g_lightColorParam = g_globalParams.createParam('lightColor',
                                                 'ParamFloat4');
  g_lightColorParam.value = [2.0, 1.8, 1.4, 1];
  setSunPosition([0, -100000, 200000]);

  g_clipHeightParam = g_globalParams.createParam('clipHeight', 'ParamFloat');
  g_proxyOffsetParam = g_globalParams.createParam('offset', 'ParamFloat');

  g_particleSystem = o3djs.particles.createParticleSystem(g_mainPack,
                                                          g_mainViewInfo,
                                                          g_globalClockParam,
                                                          pseudoRandom);

  // Since we set the state for the draw pass to 'AlphaReference' = 0.7
  // We need to set it back to 0.0 for the particles.
  for (var ii = 0; ii < g_particleSystem.particleStates.length; ++ii) {
    g_particleSystem.particleStates[ii].getStateParam(
        'AlphaReference').value = 0.0;
  }

  g_editableEffects.push(g_particleSystem.effects[0]);
  g_editableEffects.push(g_particleSystem.effects[1]);

  g_loader = o3djs.loader.createLoader(onAllLoadingFinished);

  setupWater();
  setupHud();

  loadProxy();

  // It's important to create stuff in g_mainPack and not g_scenePack because
  // g_scenePack will be scanned and modified after loading.

  setClientSize();
  updateCamera();
  updateProjection();

  o3djs.event.addEventListener(g_o3dElement, 'mousedown', onMouseDown);
  o3djs.event.addEventListener(g_o3dElement, 'mousemove', onMouseMove);
  o3djs.event.addEventListener(g_o3dElement, 'mouseup', onMouseUp);
  o3djs.event.addEventListener(g_o3dElement, 'wheel', onWheel);
  o3djs.event.addEventListener(g_o3dElement, 'keydown', onKeyDown);
  o3djs.event.addEventListener(g_o3dElement, 'keyup', onKeyUp);

  // If we don't check the size of the client area every frame we don't get a
  // chance to adjust the perspective matrix fast enough to keep up with the
  // browser resizing us.
  g_client.setRenderCallback(onRender);

  // Because we don't render the render targets every frame of the OS has
  // to reset them their contents will get lost. In that case O3D will notify
  // us through this callback so we can re-render our render targets.
  g_client.setLostResourcesCallback(function() {
        g_updateRenderTargets = true;
      });
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

  var url = o3djs.util.getAbsoluteURI('assets/' + textureName);
  loader.loadTexture(pack, url, function(texture, success) {
        sampler.texture = texture;
      });

  return sampler;
}

/**
 * Create the waterfall effect.
 */
function setupWaterfall() {
  // A prefix for waterfall materials would have been better.
  var material = g_scenePack.getObjects('Standard_3', 'o3d.Material')[0];

  // Create an effect with a v offset parameter so we can scroll the
  // UVs.
  var effect = g_mainPack.createObject('Effect');
  effect.name = 'waterfall';
  effect.loadFromFXString(o3djs.util.getElementContentById('waterfallshader'));
  effect.createUniformParameters(material);

  g_editableEffects.push(effect);

  // Set the waterfall to use additive blending.
  var state = g_mainPack.createObject('State');
  state.getStateParam('SourceBlendFunction').value =
      g_o3d.State.BLENDFUNC_SOURCE_ALPHA;
  state.getStateParam('DestinationBlendFunction').value =
      g_o3d.State.BLENDFUNC_ONE;
  state.getStateParam('AlphaReference').value = 0.0;
  //state.getStateParam('ZWriteEnable').value = false;

  material.state = state;
  material.drawList = g_mainViewInfo.zOrderedDrawList;
  material.effect = effect;

  // Create a counter to scroll the Vs.
  // var counter = g_mainPack.createObject('SecondCounter');
  // material.getParam('vOffset').bind(counter.getParam('count'));
  //
  // For selenium testing we need a global clock.
  material.getParam('vOffset').bind(g_globalClockParam);

}

/**
 * Setup underwater.
 * Must be called after the scene has loaded.
 * NOTE: The coral needs a new shader that supports normal maps
 *    but it's a low priority to fix right now.
 */
function setupUnderwater() {
  var effect = g_mainPack.createObject('Effect');
  effect.name = 'underwater';
  effect.loadFromFXString(o3djs.util.getElementContentById('underwatershader'));
  g_editableEffects.push(effect);

  // make 2 materials, one for zOrdered, one for performance.
  var materials = [];
  for (var ii = 0; ii < 2; ++ii) {
    var material = g_mainPack.createObject('Material');
    // Copy the water params so this material gets access to the noise samplers.
    // and the clock regardless of whether or not it uses them. That way you
    // can access them as you edit the shader live.
    material.copyParams(g_waterMaterial);
    material.effect = effect;
    effect.createUniformParameters(material);

    material.getParam('sunVector').bind(g_lightDirectionParam);
    material.getParam('waterColor').value = g_waterColor;
    material.getParam('fadeFudge').value = -1 / 1800;
    material.getParam('clock').bind(g_globalClockParam);

    g_fadeParams[ii] = material.getParam('fadeFudge');
    materials[ii] = material;
  }

  materials[0].drawList = g_refractionViewInfo.performanceDrawList;
  materials[0].name = 'underwaterOpaque';
  materials[1].drawList = g_refractionViewInfo.zOrderedDrawList;
  materials[1].name = 'underwaterTransparent';

  g_underwaterMaterials = materials;

  // put a draw element on each element in the scene to draw it with the
  // underwater shader.
  var elements = g_scenePack.getObjectsByClassName('o3d.Element');
  for (var ee = 0; ee < elements.length; ++ee) {
    var element = elements[ee];
    var originalMaterial = element.material;
    var materialInfo = g_materialLists[originalMaterial.name];
    if ((!materialInfo || materialInfo.refract) &&
        element.name != 'Seafloor|Sand_Dark' &&
        (!materialInfo || materialInfo.effect != 'diffuse_bump_2textures')) {
      // Use the sampler from the original material.
      var originalSamplerParam = originalMaterial.getParam('diffuseSampler');
      if (originalSamplerParam) {
        var drawElement = element.createDrawElement(
            g_scenePack,
            originalMaterial.drawList == g_mainViewInfo.performanceDrawList ?
              materials[0] : materials[1]);
        // create a Sampler Param on this draw element to use instead of the
        // material's.
        var samplerParam = drawElement.createParam('diffuseSampler',
                                                   'ParamSampler');
        samplerParam.value = originalSamplerParam.value;
      }
    }
  }

  // Special case the sand and coral rocks.
  var materialNames = {
      'Sand_Dark': {texture: 'image3.dds'},
      'Folg_coralRockA_mat': {texture: 'image30.dds'},
      'Folg_coralRockB_mat': {texture: 'image30.dds'}};
  for (var name in materialNames) {
    var info = materialNames[name];
    var material = g_scenePack.getObjects(name, 'o3d.Material')[0];
    material.drawList = g_refractionViewInfo.performanceDrawList;
    addTexture(material, 'diffuse2Sampler', info.texture);
  }
}

/**
 * Create the water effect.
 */
function setupWater() {
  var waterEffects = ['watershader', 'watercolorandskyshader', 'waterstyle2'];
  var effects = [];
  for (var ee = 0; ee < waterEffects.length; ++ee) {
    var name = waterEffects[ee]
    var effect = g_mainPack.createObject('Effect');
    effect.name = name;
    effect.loadFromFXString(o3djs.util.getElementContentById(name));
    effects[ee] = effect;
    g_editableEffects.push(effect);
  }
  g_waterEffect = effects[0];
  g_waterColorAndSkyEffect = effects[1];
  g_waterStyle2Effect = effects[2];

  var effect = g_waterEffect;

  var material = g_mainPack.createObject('Material');
  g_waterMaterial = material;
  material.name = 'water';
  material.drawList = g_mainViewInfo.performanceDrawList;
  material.effect = effect;
  effect.createUniformParameters(material);

  // We could reuse the one from the waterfall but let's make 2 anyway.
  // var counter = g_mainPack.createObject('SecondCounter');
  // For selenium testing we need a global clock.

  material.getParam('waterColor').value = g_waterColor;
  material.getParam('reflectionRefractionOffset').value = 0.1;
  //material.getParam('clock').bind(counter.getParam('count'));
  material.getParam('clock').bind(g_globalClockParam);
  g_viewPositionParam = material.getParam('viewPosition');

  var sampler = g_mainPack.createObject('Sampler');
  sampler.texture = g_refractionTexture;
  sampler.addressModeU = g_o3d.Sampler.MIRROR;
  sampler.addressModeV = g_o3d.Sampler.MIRROR;
  material.getParam('refractionSampler').value = sampler;
  sampler = g_mainPack.createObject('Sampler');
  sampler.texture = g_reflectionTexture;
  sampler.addressModeU = g_o3d.Sampler.MIRROR;
  sampler.addressModeV = g_o3d.Sampler.MIRROR;
  material.getParam('reflectionSampler').value = sampler;

  var shape = o3djs.primitives.createPlane(g_mainPack, material,
                                           100000, 100000, 100, 100,
                                           [[1, 0, 0, 0],
                                            [0, 0, 1, 0],
                                            [0, -1, 0, 0],
                                            [0, 0, 0, 1]]);

  g_waterTransform = g_mainPack.createObject('Transform');
  g_waterTransform.name = 'watersurface';
  g_waterTransform.addShape(shape);

  function waterAssetsLoaded() {
    g_waterTransform.parent = g_mainRoot;
    setupSkyDome();
  }

  // Create a loader for the water so we can know when all its assets have
  // loaded.
  var loader = g_loader.createLoader(waterAssetsLoaded);

  g_environmentSampler = loadTexture(loader, g_mainPack, material,
              'environmentSampler',
              'sky-cubemap.dds');

  // Create some textures.
  var textureInfo = [
    {width: 128, height: 128, type: 0, name: 'noiseSampler'},
    {width: 64, height: 64, type: 0, name: 'noiseSampler2'},
    {width: 32, height: 32, type: 0, name: 'noiseSampler3'},
    {width: 32, height: 1, type: 1, name: 'fresnelSampler'}
  ];

  for (var tt = 0; tt < textureInfo.length; ++tt) {
    var info = textureInfo[tt];
    var pixels = [];

    switch (info.type) {
    case 0:
      // Create a noise texture.
      for (var yy = 0; yy < info.height; ++yy) {
        for (var xx = 0; xx < info.width; ++xx) {
          for (var cc = 0; cc < 3; ++cc) {
            pixels.push(pseudoRandom());
          }
        }
      }
      break;
    case 1:
      // Create a ramp texture. (this needs to be a fresnel ramp?)
      for (var yy = 0; yy < info.height; ++yy) {
        for (var xx = 0; xx < info.width; ++xx) {
          // TODO: figure this out.
          var color = Math.pow(1 - xx / info.width, 10);
          for (var cc = 0; cc < 3; ++cc) {
            pixels.push(color);
          }
        }
      }
      break;
    }
    var texture = g_mainPack.createTexture2D(
       info.width, info.height, g_o3d.Texture.XRGB8, 1, false);
    texture.set(0, pixels);
    var sampler = g_mainPack.createObject('Sampler');
    sampler.texture = texture;
    material.getParam(info.name).value = sampler;
  }

  loader.finish();
}

/**
 * Create particles.
 */
function setupParticles() {
  setupTorches();
  setupMist();
}

/**
 * Create the torches.
 */
function setupTorches() {
  g_torchEmitter = g_particleSystem.createParticleEmitter(g_torchTexture);
  g_torchEmitter.setState(o3djs.particles.ParticleStateIds.ADD);
  g_torchEmitter.setColorRamp(
      [1, 1, 0, 1,
       1, 0, 0, 1,
       0, 0, 0, 1,
       0, 0, 0, 0.5,
       0, 0, 0, 0]);
  g_torchEmitter.setParameters({
    numParticles: 40,
    lifeTime: 2,
    timeRange: 2,
    startSize: 50,
    endSize: 90,
    positionRange: [10, 10, 10],
    velocity: [0, 0, 60], velocityRange: [15, 15, 15],
    acceleration: [0, 0, -20],
    spinSpeedRange: 4}
  );

  g_torchMaterial = g_torchEmitter.material;

  // Add one to each torch.
  var shape = g_torchEmitter.shape;
  g_scenePack.getObjects('particle_torch01',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_torch02',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_torch03',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_torch04',
                         'o3d.Transform')[0].addShape(shape);
}

/**
 * Create the mist.
 */
function setupMist() {
  g_topMistEmitter = g_particleSystem.createParticleEmitter(g_mistTexture);
  g_topMistEmitter.setState(o3djs.particles.ParticleStateIds.ADD);
  g_topMistEmitter.setColorRamp(
      [1, 1, 1, 2,
       1, 1, 1, 0]);
  g_topMistEmitter.setParameters({
    numParticles: 20,
    timeRange: 3,
    lifeTime: 3, lifeTimeRange: 1,
    startSize: 400,
    endSize: 600,
    position: [-100, -100, 0], positionRange: [25, 25, 0],
    velocity: [0, 0, 150], velocityRange: [15, 15, 15],
    worldAcceleration: [0, 0, -500],
    spinSpeedRange: 8}
  );

  // Add one to each top.
  var shape = g_topMistEmitter.shape;
  g_scenePack.getObjects('particle_falltop01',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_falltop02',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_falltop03',
                         'o3d.Transform')[0].addShape(shape);

  g_bottomMistEmitter = g_particleSystem.createParticleEmitter(g_mistTexture);
  g_bottomMistEmitter.setState(o3djs.particles.ParticleStateIds.ADD);
  g_bottomMistEmitter.setColorRamp(
      [1, 1, 1, 1,
       1, 1, 1, 0]);
  g_bottomMistEmitter.setParameters({
    numParticles: 40,
    lifeTime: 2,
    timeRange: 2,
    startSize: 800,
    endSize: 1500,
    position: [0, 0, 100], positionRange: [200, 200, 10],
    velocityRange: [200, 200, 0],
    acceleration: [0, 0, -20],
    spinSpeedRange: 4}
  );

  // Add one to each bottom.
  shape = g_bottomMistEmitter.shape;

  g_scenePack.getObjects('particle_fallbottom01',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_fallbottom02',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_fallbottom03',
                         'o3d.Transform')[0].addShape(shape);

  g_rippleEmitter = g_particleSystem.createParticleEmitter(g_mistTexture);
  g_rippleEmitter.setState(o3djs.particles.ParticleStateIds.ADD);
  g_rippleEmitter.setColorRamp(
      [0.7, 0.8, 1, 0.5,
       1, 1, 1, 0]);
  g_rippleEmitter.setParameters({
    numParticles: 20,
    lifeTime: 2,
    timeRange: 2,
    startSize: 50,
    endSize: 10000,
    position: [0, 0, 10], positionRange: [250, 250, 0],
    orientation: o3djs.quaternions.rotationX(Math.PI / 2),
    billboard: false});

  // Add one to each bottom.
  shape = g_rippleEmitter.shape;

  g_scenePack.getObjects('particle_fallbottom01',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_fallbottom02',
                         'o3d.Transform')[0].addShape(shape);
  g_scenePack.getObjects('particle_fallbottom03',
                         'o3d.Transform')[0].addShape(shape);
}

function setupSkyDome() {
  // Create the skydome effect.
  var effect = g_mainPack.createObject('Effect');
  effect.name = 'skydome';
  effect.loadFromFXString(o3djs.util.getElementContentById('skydomeshader'));
  g_editableEffects.push(effect);

  var material = g_mainPack.createObject('Material');
  g_skyDomeMaterial = material;
  material.name = 'skydome';
  material.drawList = g_mainViewInfo.performanceDrawList;
  material.effect = effect;
  effect.createUniformParameters(material);

  material.getParam('environmentSampler').value = g_environmentSampler;

  // Create a special quad to draw the sky. We won't transform this quad
  // at all. It's already in clip-space.
  var shape = o3djs.primitives.createPlane(g_mainPack, material,
                                           2, 2, 1, 1,
                                           [[1, 0, 0, 0],
                                            [0, 0, 1, 0],
                                            [0, -1, 0, 0],
                                            [0, 0, 0.99999, 1]]);

  g_skyDomeTransform = g_mainPack.createObject('Transform');
  g_skyDomeTransform.parent = g_mainRoot;
  g_skyDomeTransform.addShape(shape);
}

/**
 * Creates an Image object which is a transform and a child scaleTransform
 * scaled to match the texture
 *
 * @constructor
 * @param {!o3d.Transform} parent Transform to parent image too.
 * @param {!o3d.Texture} texture The texture.
 * @param {boolean} opt_topLeft If true the origin of the image will be it's
 *    topleft corner, the default is the center of the image.
 */
function Image(parent, texture, opt_topLeft) {
  // create a transform for positioning
  this.transform = g_mainPack.createObject('Transform');
  this.transform.parent = parent;

  // create a transform for scaling to the size of the image just so
  // we don't have to manage that manually in the transform above.
  this.scaleTransform = g_mainPack.createObject('Transform');
  this.scaleTransform.parent = this.transform;

  // setup the sampler for the texture
  this.sampler = g_mainPack.createObject('Sampler');
  this.sampler.addressModeU = g_o3d.Sampler.CLAMP;
  this.sampler.addressModeV = g_o3d.Sampler.CLAMP;
  this.paramSampler = this.scaleTransform.createParam('diffuseSampler',
                                                      'ParamSampler');
  this.paramSampler.value = this.sampler;

  this.sampler.texture = texture;
  this.scaleTransform.addShape(g_imageShape);
  if (opt_topLeft) {
    this.scaleTransform.translate(texture.width / 2, texture.height / 2, 0);
  }
  this.scaleTransform.scale(texture.width, -texture.height, 1);
  this.colorParam = this.scaleTransform.createParam('colorMult', 'ParamFloat4');
  this.colorParam.value = [1, 1, 1, 1];
}

/**
 * Sets up the hud.
 */
function setupHud() {
  var effect = g_mainPack.createObject('Effect');
  effect.name = 'hud';
  effect.loadFromFXString(o3djs.util.getElementContentById('imageshader'));
  g_editableEffects.push(effect);
  // Make the default colorMult 1, 1, 1, 1 uncase it is not supplied by the
  // material.
  effect.createParam('colorMult', 'ParamFloat4').value = [1, 1, 1, 1];

  g_imageEffect = effect;

  var g_imageMaterial = g_mainPack.createObject('Material');
  g_imageMaterial.drawList = g_hudViewInfo.zOrderedDrawList;
  g_imageMaterial.effect = effect;
  effect.createUniformParameters(g_imageMaterial);
  g_imageMaterial.getParam('colorMult').value = [1, 1, 1, 1];

  g_renderTargetDisplayRoot = g_mainPack.createObject('Transform');
  g_renderTargetDisplayRoot.parent = g_hudRoot;
  g_renderTargetDisplayRoot.visible = false;

  g_imageShape = o3djs.primitives.createPlane(g_mainPack, g_imageMaterial,
                                              1, 1, 1, 1,
                                              [[1, 0, 0, 0],
                                               [0, 0, 1, 0],
                                               [0, 1, 0, 0],
                                               [0, 0, 0, 1]]);

  // Because it's easier to make a texture here than manage another effect
  var backTexture = g_mainPack.createTexture2D(
      1, 1, g_o3d.Texture.XRGB8, 1, false);
  backTexture.set(0, [1, 1, 1]);

  g_whiteTexture = backTexture;
  g_whiteSampler = g_mainPack.createObject('Sampler');
  g_whiteSampler.texture = g_whiteTexture;

  // Make images to show the render targets.
  for (var ii = 0; ii < 2; ++ii) {
    var renderTargetTexture = (ii == 0) ? g_reflectionTexture :
                                          g_refractionTexture;
    var scale = 1;
    var x = 10;
    var y = 10 + ii * (g_reflectionTexture.height * scale + 10);
    var borderSize = 2;
    var image;
    // make a back image to create a border around render target.
    image = new Image(g_renderTargetDisplayRoot, backTexture, true);
    image.transform.translate(x - borderSize, y - borderSize, -3);
    image.transform.scale(renderTargetTexture.width * scale + borderSize * 2,
                          renderTargetTexture.height * scale + borderSize * 2,
                          1);
    image = new Image(g_renderTargetDisplayRoot, renderTargetTexture, true);
    image.transform.translate(x, y, -2);
    image.transform.scale(scale, scale, 1);
  }

  // Make a fader plane.
  {
    var image = new Image(g_hudRoot, backTexture, true);
    g_faderTransform = image.transform;
    g_faderTransform.visible = false;
    g_faderColorParam = image.colorParam;
    updateFaderPlane();
  }

  // Make a canvas for text.
  var canvasLib = o3djs.canvas.create(g_mainPack,
                                      g_hudRoot,
                                      g_hudViewInfo);

  g_hudQuad = canvasLib.createXYQuad(20, 20, -1, 512, 512, true);
  g_paint = g_mainPack.createObject('CanvasPaint');

  g_paint.setOutline(3, [1, 1, 1, 1]);
  g_paint.textAlign = g_o3d.CanvasPaint.LEFT;
  g_paint.textSize = 16;
  g_paint.textTypeface = 'Arial';
  g_paint.color = [0, 0, 0, 1];

  setHudText('Loading...');
}

/**
 * Sets the text on the hud.
 * @param {string} text The text to display.
 */
function setHudText(text) {
  if (g_showError) {
    return;
  }
  var canvas = g_hudQuad.canvas;
  canvas.clear([0, 0, 0, 0]);
  canvas.saveMatrix();
  var lines = text.split('\n');
  for (var ll = 0; ll < lines.length; ++ll) {
    var tabs = lines[ll].split('\t');
    for (var tt = 0; tt < tabs.length; ++tt) {
      canvas.drawText(tabs[tt], 10 + tt * 120, 30 + 20 * ll, g_paint);
    }
  }
  canvas.restoreMatrix();

  g_hudQuad.updateTexture();
}

/**
 * Show a hint message.
 */
function showHint() {
  g_hudQuad.transform.visible = true;
  g_hudFadeTime = 0.0;
  setHudText('press H for help.');
}

/**
 * Show a help message.
 */
function toggleHelp() {
  g_hudFadeTime = 0.0;
  g_helpVisible = !g_helpVisible;
  g_hudQuad.transform.visible = true;
  if (g_helpVisible) {
  setHudText('1 - 4\t: Camera Preset\n' +
             'Mouse\t: Look Around\n' +
             'Wheel\t: Field of View\n' +
             'Arrows\t: Move Camera\n' +
             'p\t: Toggle Props\n' +
             'm\t: Edit Materials\n' +
             'e\t: Edit Effects\n' +
             'r\t: Show Render Targets\n' +
             'c\t: Use Simple Shaders\n' +
             'f\t: Show FPS\n' +
             'h\t: Show Help\n' +
             'o\t: Change Water Effect\n' +
             'q\t: Toggle demo camera\n');
  } else {
    showHint();
  }
}

/**
 * Show error.
 * @param {string} msg Msg to display.
 */
function showError(msg) {
  g_hudQuad.transform.visible = true;
  setHudText('Error: Could not load scene.\n' + msg);
  g_showError = true;
}

/**
 * Removes any callbacks so they don't get called after the page has unloaded.
 */
function uninit() {
  if (g_client) {
    g_client.cleanup();
  }
}

