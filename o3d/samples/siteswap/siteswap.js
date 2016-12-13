// @@REWRITE(insert js-copyright)
// @@REWRITE(delete-start)
// Copyright 2009 Google Inc.  All Rights Reserved
// @@REWRITE(delete-end)

/**
 * This file contains the top-level logic and o3d-related code for the siteswap
 * animator.
 */

o3djs.require('o3djs.rendergraph');
o3djs.require('o3djs.math');
o3djs.require('o3djs.primitives');
o3djs.require('o3djs.dump');

// Global variables are all referenced via g, so that either interpreter can
// find them easily.
var g = {};

/**
 * Creates a color based on an input index as a seed.
 * @param {!number} index the seed value to select the color.
 * @return {!Array.number} an [r g b a] color.
 */
function createColor(index) {
  var N = 12; // Number of distinct colors.
  var root3 = Math.sqrt(3);
  var theta = 2 * Math.PI * index / N;
  var sin = Math.sin(theta);
  var cos = Math.cos(theta);
  return [(1 / 3 + 2 / 3 * cos) + (1 / 3 - cos / 3 - sin / root3),
          (1 / 3 - cos / 3 + sin / root3) + (1 / 3 + 2 / 3 * cos),
          (1 / 3 - cos / 3 - sin / root3) + (1 / 3 - cos / 3 + sin / root3),
          1];
}

/**
 * Creates a material, given the index as a seed to make it distinguishable.
 * @param {number} index an integer used to create a distinctive color.
 * @return {!o3d.Material} the material.
 */
function createMaterial(index) {
  var material = g.pack.createObject('Material');

  // Apply our effect to this material. The effect tells the 3D hardware
  // which shader to use.
  material.effect = g.effect;

  // Set the material's drawList
  material.drawList = g.viewInfo.performanceDrawList;

  // This will create our quadColor parameter on the material.
  g.effect.createUniformParameters(material);

  // Set up the individual parameters in our effect file.

  // Light position
  var light_pos_param = material.getParam('light_pos');
  light_pos_param.value = [10, 10, 20];

  // Phong components of the light source
  var light_ambient_param = material.getParam('light_ambient');
  var light_diffuse_param = material.getParam('light_diffuse');
  var light_specular_param = material.getParam('light_specular');

  // White ambient light
  light_ambient_param.value = [0.04, 0.04, 0.04, 1];

  light_diffuse_param.value = createColor(index);
  // White specular light
  light_specular_param.value = [0.5, 0.5, 0.5, 1];

  // Shininess of the material (for specular lighting)
  var shininess_param = material.getParam('shininess');
  shininess_param.value = 30.0;

  // Bind the counter's count to the input of the FunctionEval.
  var paramTime = material.getParam('time');
  paramTime.bind(g.counter.getParam('count'));

  material.getParam('camera_pos').value = g.eye;

  return material;
}

/**
 * Gets a material from our cache, creating it if it's not yet been made.
 * Uses index as a seed to make the material distinguishable.
 * @param {number} index an integer used to create/fetch a distinctive color.
 * @return {!o3d.Material} the material.
 */
function getMaterial(index) {
  g.materials = g.materials || [];  // See initStep2 for a comment.
  if (!g.materials[index]) {
    g.materials[index] = createMaterial(index);
  }
  return g.materials[index];
}

/**
 * Initializes g.o3d.
 * @param {Array} clientElements Array of o3d object elements.
 */
function initStep2(clientElements) {
  // Initializes global variables and libraries.
  window.g = g;

  // Used to tell whether we need to recompute our view on resize.
  g.o3dWidth = -1;
  g.o3dHeight = -1;

  // We create a different material for each color of object.
  //g.materials = [];  // TODO(ericu): If this is executed, we fail.  Why?

  // We hold on to all the shapes here so that we can clean them up when we want
  // to change patterns.
  g.ballShapes = [];
  g.handShapes = [];

  g.o3dElement = clientElements[0];
  g.o3d = g.o3dElement.o3d;
  g.math = o3djs.math;
  g.client = g.o3dElement.client;

  // Initialize client sample libraries.
  o3djs.base.init(g.o3dElement);

  // Create a g.pack to manage our resources/assets
  g.pack = g.client.createPack();

  // Create the render graph for a view.
  g.viewInfo = o3djs.rendergraph.createBasicView(
      g.pack,
      g.client.root,
      g.client.renderGraphRoot);

  // Get the default context to hold view/projection matrices.
  g.context = g.viewInfo.drawContext;

  // Load a simple effect from a textarea.
  g.effect = g.pack.createObject('Effect');
  g.effect.loadFromFXString(document.getElementById('shader').value);

  // Eye-position: this is where our camera is located.
  // Global because each material we create must also know where it is, so that
  // the shader works properly.
  g.eye = [1, 6, 10];

  // Target, this is the point at which our camera is pointed.
  var target = [0, 2, 0];

  // Up-vector, this tells the camera which direction is 'up'.
  // We define the positive y-direction to be up in this example.
  var up = [0, 1, 0];

  g.context.view = g.math.matrix4.lookAt(g.eye, target, up);

  // Make a SecondCounter to provide the time for our animation.
  g.counter = g.pack.createObject('SecondCounter');
  g.counter.multiplier = 3;  // Speed up time; this is in throws per second.

  // Generate the projection and viewProjection matrices based
  // on the g.o3d plugin size by calling onResize().
  onResize();

  // If we don't check the size of the client area every frame we don't get a
  // chance to adjust the perspective matrix fast enough to keep up with the
  // browser resizing us.
  // TODO(ericu): Switch to using the resize event once it's checked in.
  g.client.setRenderCallback(onResize);
}

/**
 * Stops or starts the animation based on the state of an html checkbox.
 */
function updateAnimating() {
  var box = document.the_form.check_box;
  g.counter.running = box.checked;
}

/**
 * Generates the projection matrix based on the size of the g.o3d plugin
 * and calculates the view-projection matrix.
 */
function onResize() {
  var newWidth = g.client.width;
  var newHeight = g.client.height;

  if (newWidth != g.o3dWidth || newHeight != g.o3dHeight) {
    debug('resizing');
    g.o3dWidth = newWidth;
    g.o3dHeight = newHeight;

    // Create our projection matrix, with a vertical field of view of 45 degrees
    // a near clipping plane of 0.1 and far clipping plane of 100.
    g.context.projection = g.math.matrix4.perspective(
        45 * Math.PI / 180,
        g.o3dWidth / g.o3dHeight,
        0.1,
        100);
  }
}

/**
 * Computes and prepares animation of the pattern input via the html form.  If
 * the box is checked, this will immediately begin animation as well.
 */
function onComputePattern() {
  try {
    g.counter.removeAllCallbacks();
    var group = document.the_form.radio_group_hands;
    var numHands = -1;
    for (var i = 0; i < group.length; ++i) {
      if (group[i].checked) {
        numHands = parseInt(group[i].value);
      }
    }
    var style = 'even';
    if (document.the_form.pair_hands.checked) {
      style = 'pairs';
    }
    var patternString = document.getElementById('input_pattern').value;
    var patternData =
      computeFullPatternFromString(patternString, numHands, style);
    startAnimation(
        patternData.numBalls,
        patternData.numHands,
        patternData.duration,
        patternData.ballCurveSets,
        patternData.handCurveSets);
  } catch (ex) {
    popup(stringifyObj(ex));
    throw ex;
  }
  setUpSelection();
}

/**
 * Removes any callbacks so they don't get called after the page has unloaded.
 */
function cleanup() {
  g.client.cleanup();
}


/**
 * Dump out a newline-terminated string to the debug console, if available.
 * @param {!string} s the string to output.
 */
function debug(s) {
  o3djs.dump.dump(s + '\n');
}

/**
 * Dump out a newline-terminated string to the debug console, if available,
 * then display it via an alert.
 * @param {!string} s the string to output.
 */
function popup(s) {
  debug(s);
  window.alert(s);
}

/**
 * If t, throw an exception.
 * @param {!bool} t the value to test.
 */
function assert(t) {
  if (!t) {
    throw new Error('Assertion failed!');
  }
}

/**
 * Convert an object to a string containing a full one-level-deep property
 * listing, with values.
 * @param {!Object} o the object to convert.
 * @return {!string} the converted object.
 */
function stringifyObj(o) {
  var s = '';
  for (var i in o) {
    s += i + ':' + o[i] + '\n';
  }
  return s;
}

/**
 * Add the information in a curve to the params on a shape, such that the vertex
 * shader will move the shape along the curve at times after timeBase.
 * @param {!Curve} curve the curve the shape should follow.
 * @param {!o3d.Shape} shape the shape being moved.
 * @param {!number} timeBase the base to subtract from the current time when
 *                  giving the curve calculation its time input.
 */
function setParamCurveInfo(curve, shape, timeBase) {
  assert(curve);
  assert(shape);
  try {
    shape.elements[0].getParam('time_base').value = timeBase;
    shape.elements[0].getParam('coeff_a').value =
        [curve.xEqn.a, curve.yEqn.a, curve.zEqn.a];
    shape.elements[0].getParam('coeff_b').value =
        [curve.xEqn.b, curve.yEqn.b, curve.zEqn.b];
    shape.elements[0].getParam('coeff_c').value =
        [curve.xEqn.c, curve.yEqn.c, curve.zEqn.c];
    shape.elements[0].getParam('coeff_d').value =
        [curve.xEqn.d, curve.yEqn.d, curve.zEqn.d];
    shape.elements[0].getParam('coeff_e').value =
        [curve.xEqn.e, curve.yEqn.e, curve.zEqn.e];
    shape.elements[0].getParam('coeff_f').value =
        [curve.xEqn.f, curve.yEqn.f, curve.zEqn.f];

    assert(curve.xEqn.lerpRate == curve.yEqn.lerpRate);
    assert(curve.xEqn.lerpRate == curve.zEqn.lerpRate);
    shape.elements[0].getParam('coeff_lerp').value = curve.xEqn.lerpRate;
    if (curve.xEqn.lerpRate) {
      shape.elements[0].getParam('coeff_l_a').value =
          [curve.xEqn.lA, curve.yEqn.lA, curve.zEqn.lA];
      shape.elements[0].getParam('coeff_l_b').value =
          [curve.xEqn.lB, curve.yEqn.lB, curve.zEqn.lB];
      shape.elements[0].getParam('coeff_l_c').value =
          [curve.xEqn.lC, curve.yEqn.lC, curve.zEqn.lC];
    }
  } catch (ex) {
    debug(ex);
    throw ex;
  }
}

/**
 * Create the params that the shader expects on the supplied shape's first
 * element.
 * @param {!o3d.Shape} shape the shape on whose first element to create params.
 */
function createParams(shape) {
  shape.elements[0].createParam('coeff_a', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_b', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_c', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_d', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_e', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_f', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_l_a', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_l_b', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_l_c', 'ParamFloat3').value = [0, 0, 0];
  shape.elements[0].createParam('coeff_lerp', 'ParamFloat').value = 0;
  shape.elements[0].createParam('time_base', 'ParamFloat').value = 0;
}

/**
 * Adjust the number of ball shapes in g.pack.
 * @param {!number} numBalls the number of balls desired.
 */
function setNumBalls(numBalls) {
  for (var i = 0; i < g.ballShapes.length; ++i) {
    g.pack.removeObject(g.ballShapes[i]);
    g.client.root.removeShape(g.ballShapes[i]);
  }
  g.ballShapes = [];

  for (var i = 0; i < numBalls; ++i) {
    var shape = o3djs.primitives.createSphere(g.pack,
                                              getMaterial(5 * i),
                                              0.10,
                                              70,
                                              70);
    shape.name = 'Ball ' + i;

    // generate the draw elements.
    shape.createDrawElements(g.pack, null);

    // Now attach the sphere to the root of the scene graph.
    g.client.root.addShape(shape);

    // Create the material params for the shader.
    createParams(shape);

    g.ballShapes[i] = shape;
  }
}

/**
 * Adjust the number of hand shapes in g.pack.
 * @param {!number} numHands the number of hands desired.
 */
function setNumHands(numHands) {
  g.counter.removeAllCallbacks();

  for (var i = 0; i < g.handShapes.length; ++i) {
    g.pack.removeObject(g.handShapes[i]);
    g.client.root.removeShape(g.handShapes[i]);
  }
  g.handShapes = [];

  for (var i = 0; i < numHands; ++i) {
    var shape = o3djs.primitives.createBox(g.pack,
                                           getMaterial(3 * (i + 1)),
                                           0.25,
                                           0.05,
                                           0.25);
    shape.name = 'Hand ' + i;

    // generate the draw elements.
    shape.createDrawElements(g.pack, null);

    // Now attach the sphere to the root of the scene graph.
    g.client.root.addShape(shape);

    // Create the material params for the shader.
    createParams(shape);

    g.handShapes[i] = shape;
  }
}

