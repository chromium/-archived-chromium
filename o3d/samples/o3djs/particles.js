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
 * @fileoverview This file contains various functions and classes for rendering
 * gpu based particles.
 *
 * TODO: Add 3d oriented particles.
 */

o3djs.provide('o3djs.particles');

o3djs.require('o3djs.math');

/**
 * A Module with various GPU particle functions and classes.
 * Note: GPU particles have the issue that they are not sorted per particle
 * but rather per emitter.
 * @namespace
 */
o3djs.particles = o3djs.particles || {};

/**
 * Enum for pre-made particle states.
 * @enum
 */
o3djs.particles.ParticleStateIds = {
   BLEND: 0,
   ADD: 1,
   BLEND_PREMULTIPLY: 2,
   BLEND_NO_ALPHA: 3,
   SUBTRACT: 4,
   INVERSE: 5};

/**
 * Particle Effect strings
 * @type {!Array.<{name: string, fxString: string}>}
 */
o3djs.particles.FX_STRINGS = [
  { name: 'particle3d', fxString: '' +
    'float4x4 worldViewProjection : WORLDVIEWPROJECTION;\n' +
    'float4x4 world : WORLD;\n' +
    'float3 worldVelocity;\n' +
    'float3 worldAcceleration;\n' +
    'float timeRange;\n' +
    'float time;\n' +
    'float timeOffset;\n' +
    'float frameDuration;\n' +
    'float numFrames;\n' +
    '\n' +
    '// We need to implement 1D!\n' +
    'sampler rampSampler;\n' +
    'sampler colorSampler;\n' +
    '\n' +
    'struct VertexShaderInput {\n' +
    '  float4 uvLifeTimeFrameStart : POSITION; // uv, lifeTime, frameStart\n' +
    '  float4 positionStartTime : TEXCOORD0;    // position.xyz, startTime\n' +
    '  float4 velocityStartSize : TEXCOORD1;   // velocity.xyz, startSize\n' +
    '  float4 accelerationEndSize : TEXCOORD2; // acceleration.xyz, endSize\n' +
    '  float4 spinStartSpinSpeed : TEXCOORD3;  // spinStart.x, spinSpeed.y\n' +
    '  float4 orientation : TEXCOORD4;  // orientation\n' +
    '  float4 colorMult : COLOR; //\n' +
    '};\n' +
    '\n' +
    'struct PixelShaderInput {\n' +
    '  float4 position : POSITION;\n' +
    '  float2 texcoord : TEXCOORD0;\n' +
    '  float1 percentLife : TEXCOORD1;\n' +
    '  float4 colorMult: TEXCOORD2;\n' +
    '};\n' +
    '\n' +
    'PixelShaderInput vertexShaderFunction(VertexShaderInput input) {\n' +
    '  PixelShaderInput output;\n' +
    '\n' +
    '  float2 uv = input.uvLifeTimeFrameStart.xy;\n' +
    '  float lifeTime = input.uvLifeTimeFrameStart.z;\n' +
    '  float frameStart = input.uvLifeTimeFrameStart.w;\n' +
    '  float3 position = input.positionStartTime.xyz;\n' +
    '  float startTime = input.positionStartTime.w;\n' +
    '  float3 velocity = mul(float4(input.velocityStartSize.xyz, 0),\n' +
    '                        world).xyz + worldVelocity;\n' +
    '  float startSize = input.velocityStartSize.w;\n' +
    '  float3 acceleration = mul(float4(input.accelerationEndSize.xyz, 0),\n' +
    '                            world).xyz + worldAcceleration;\n' +
    '  float endSize = input.accelerationEndSize.w;\n' +
    '  float spinStart = input.spinStartSpinSpeed.x;\n' +
    '  float spinSpeed = input.spinStartSpinSpeed.y;\n' +
    '\n' +
    '  float localTime = fmod((time - timeOffset - startTime), timeRange);\n' +
    '  float percentLife = localTime / lifeTime;\n' +
    '\n' +
    '  float frame = fmod(floor(localTime / frameDuration + frameStart),\n' +
    '                     numFrames);\n' +
    '  float uOffset = frame / numFrames;\n' +
    '  float u = uOffset + (uv.x + 0.5) * (1 / numFrames);\n' +
    '\n' +
    '  output.texcoord = float2(u, uv.y + 0.5);\n' +
    '  output.colorMult = input.colorMult;\n' +
    '\n' +
    '  float size = lerp(startSize, endSize, percentLife);\n' +
    '  float s = sin(spinStart + spinSpeed * localTime);\n' +
    '  float c = cos(spinStart + spinSpeed * localTime);\n' +
    '\n' +
    '  float4 rotatedPoint = float4((uv.x * c + uv.y * s) * size, 0,\n' +
    '                               (uv.x * s - uv.y * c) * size, 1);\n' +
    '  float3 center = velocity * localTime +\n' +
    '                  acceleration * localTime * localTime + \n' +
    '                  position;\n' +
    '  \n' +
    '      float4 q2 = input.orientation + input.orientation;\n' +
    '      float4 qx = input.orientation.xxxw * q2.xyzx;\n' +
    '      float4 qy = input.orientation.xyyw * q2.xyzy;\n' +
    '      float4 qz = input.orientation.xxzw * q2.xxzz;\n' +
    '  \n' +
    '      float4x4 localMatrix = float4x4(\n' +
    '        (1.0f - qy.y) - qz.z, \n' +
    '        qx.y + qz.w, \n' +
    '        qx.z - qy.w,\n' +
    '        0,\n' +
    '  \n' +
    '        qx.y - qz.w, \n' +
    '        (1.0f - qx.x) - qz.z, \n' +
    '        qy.z + qx.w,\n' +
    '        0,\n' +
    '  \n' +
    '        qx.z + qy.w, \n' +
    '        qy.z - qx.w, \n' +
    '        (1.0f - qx.x) - qy.y,\n' +
    '        0,\n' +
    '  \n' +
    '        center.x, center.y, center.z, 1);\n' +
    '  rotatedPoint = mul(rotatedPoint, localMatrix);\n' +
    '  output.position = mul(rotatedPoint, worldViewProjection);\n' +
    '  output.percentLife = percentLife;\n' +
    '  return output;\n' +
    '}\n' +
    '\n' +
    'float4 pixelShaderFunction(PixelShaderInput input): COLOR {\n' +
    '  float4 colorMult = tex2D(rampSampler, \n' +
    '                           float2(input.percentLife, 0.5)) *\n' +
    '                     input.colorMult;\n' +
    '  float4 color = tex2D(colorSampler, input.texcoord) * colorMult;\n' +
    '  return color;\n' +
    '}\n' +
    '\n' +
    '// #o3d VertexShaderEntryPoint vertexShaderFunction\n' +
    '// #o3d PixelShaderEntryPoint pixelShaderFunction\n' +
    '// #o3d MatrixLoadOrder RowMajor\n'},
  { name: 'particle2d', fxString: '' +
    'float4x4 viewProjection : VIEWPROJECTION;\n' +
    'float4x4 world : WORLD;\n' +
    'float4x4 viewInverse : VIEWINVERSE;\n' +
    'float3 worldVelocity;\n' +
    'float3 worldAcceleration;\n' +
    'float timeRange;\n' +
    'float time;\n' +
    'float timeOffset;\n' +
    'float frameDuration;\n' +
    'float numFrames;\n' +
    '\n' +
    '// We need to implement 1D!\n' +
    'sampler rampSampler;\n' +
    'sampler colorSampler;\n' +
    '\n' +
    'struct VertexShaderInput {\n' +
    '  float4 uvLifeTimeFrameStart : POSITION; // uv, lifeTime, frameStart\n' +
    '  float4 positionStartTime : TEXCOORD0;    // position.xyz, startTime\n' +
    '  float4 velocityStartSize : TEXCOORD1;   // velocity.xyz, startSize\n' +
    '  float4 accelerationEndSize : TEXCOORD2; // acceleration.xyz, endSize\n' +
    '  float4 spinStartSpinSpeed : TEXCOORD3;  // spinStart.x, spinSpeed.y\n' +
    '  float4 colorMult : COLOR; //\n' +
    '};\n' +
    '\n' +
    'struct PixelShaderInput {\n' +
    '  float4 position : POSITION;\n' +
    '  float2 texcoord : TEXCOORD0;\n' +
    '  float1 percentLife : TEXCOORD1;\n' +
    '  float4 colorMult: TEXCOORD2;\n' +
    '};\n' +
    '\n' +
    'PixelShaderInput vertexShaderFunction(VertexShaderInput input) {\n' +
    '  PixelShaderInput output;\n' +
    '\n' +
    '  float2 uv = input.uvLifeTimeFrameStart.xy;\n' +
    '  float lifeTime = input.uvLifeTimeFrameStart.z;\n' +
    '  float frameStart = input.uvLifeTimeFrameStart.w;\n' +
    '  float3 position = input.positionStartTime.xyz;\n' +
    '  float startTime = input.positionStartTime.w;\n' +
    '  float3 velocity = mul(float4(input.velocityStartSize.xyz, 0),\n' +
    '                        world).xyz + worldVelocity;\n' +
    '  float startSize = input.velocityStartSize.w;\n' +
    '  float3 acceleration = mul(float4(input.accelerationEndSize.xyz, 0),\n' +
    '                            world).xyz + worldAcceleration;\n' +
    '  float endSize = input.accelerationEndSize.w;\n' +
    '  float spinStart = input.spinStartSpinSpeed.x;\n' +
    '  float spinSpeed = input.spinStartSpinSpeed.y;\n' +
    '\n' +
    '  float localTime = fmod((time - timeOffset - startTime), timeRange);\n' +
    '  float percentLife = localTime / lifeTime;\n' +
    '\n' +
    '  float frame = fmod(floor(localTime / frameDuration + frameStart),\n' +
    '                     numFrames);\n' +
    '  float uOffset = frame / numFrames;\n' +
    '  float u = uOffset + (uv.x + 0.5) * (1 / numFrames);\n' +
    '\n' +
    '  output.texcoord = float2(u, uv.y + 0.5);\n' +
    '  output.colorMult = input.colorMult;\n' +
    '\n' +
    '  float3 basisX = viewInverse[0].xyz;\n' +
    '  float3 basisZ = viewInverse[1].xyz;\n' +
    '\n' +
    '  float size = lerp(startSize, endSize, percentLife);\n' +
    '  float s = sin(spinStart + spinSpeed * localTime);\n' +
    '  float c = cos(spinStart + spinSpeed * localTime);\n' +
    '\n' +
    '  float2 rotatedPoint = float2(uv.x * c + uv.y * s, \n' +
    '                               -uv.x * s + uv.y * c);\n' +
    '  float3 localPosition = float3(basisX * rotatedPoint.x +\n' +
    '                                basisZ * rotatedPoint.y) * size +\n' +
    '                         velocity * localTime +\n' +
    '                         acceleration * localTime * localTime + \n' +
    '                         position;\n' +
    '\n' +
    '  output.position = mul(float4(localPosition + world[3].xyz, 1), \n' +
    '                        viewProjection);\n' +
    '  output.percentLife = percentLife;\n' +
    '  return output;\n' +
    '}\n' +
    '\n' +
    'float4 pixelShaderFunction(PixelShaderInput input): COLOR {\n' +
    '  float4 colorMult = tex2D(rampSampler, \n' +
    '                           float2(input.percentLife, 0.5)) *\n' +
    '                     input.colorMult;\n' +
    '  float4 color = tex2D(colorSampler, input.texcoord) * colorMult;\n' +
    '  return color;\n' +
    '}\n' +
    '\n' +
    '// #o3d VertexShaderEntryPoint vertexShaderFunction\n' +
    '// #o3d PixelShaderEntryPoint pixelShaderFunction\n' +
    '// #o3d MatrixLoadOrder RowMajor\n'}];

/**
 * Creates a particle system.
 * You only need one of these to run multiple emitters of different types
 * of particles.
 * @param {!o3d.Pack} pack The pack for the particle system to manage resources.
 * @param {!o3djs.rendergraph.viewInfo} viewInfo A viewInfo so the particle
 *     system can do the default setup. The only thing used from viewInfo
 *     is the zOrderedDrawList. If that is not where you want your particles,
 *     after you create the particleEmitter use
 *     particleEmitter.material.drawList = myDrawList to set it to something
 *     else.
 * @param {!o3d.ParamFloat} opt_clockParam A ParamFloat to be the default
 *     clock for emitters of this particle system.
 * @param {!function(): number} opt_randomFunction A function that returns
 *     a random number between 0.0 and 1.0. This allows you to pass in a
 *     pseudo random function if you need particles that are reproducable.
 * @return {!o3djs.particles.ParticleSystem} The created particle system.
 */
o3djs.particles.createParticleSystem = function(pack,
                                                viewInfo,
                                                opt_clockParam,
                                                opt_randomFunction) {
  return new o3djs.particles.ParticleSystem(pack,
                                            viewInfo,
                                            opt_clockParam,
                                            opt_randomFunction);
};

/**
 * An Object to manage Particles.
 * @constructor
 * @param {!o3d.Pack} pack The pack for the particle system to manage resources.
 * @param {!o3djs.rendergraph.ViewInfo} viewInfo A viewInfo so the particle
 *     system can do the default setup. The only thing used from viewInfo
 *     is the zOrderedDrawList. If that is not where you want your particles,
 *     after you create the particleEmitter use
 *     particleEmitter.material.drawList = myDrawList to set it to something
 *     else.
 * @param {!o3d.ParamFloat} opt_clockParam A ParamFloat to be the default
 *     clock for emitters of this particle system.
 */
o3djs.particles.ParticleSystem = function(pack,
                                          viewInfo,
                                          opt_clockParam,
                                          opt_randomFunction) {
  var o3d = o3djs.base.o3d;
  var particleStates = [];
  var effects = [];
  for (var ee = 0; ee < o3djs.particles.FX_STRINGS.length; ++ee) {
    var info = o3djs.particles.FX_STRINGS[ee];
    var effect = pack.createObject('Effect');
    effect.name = info.name;
    effect.loadFromFXString(info.fxString);
    effects.push(effect);
  }

  var stateInfos = {};
  stateInfos[o3djs.particles.ParticleStateIds.BLEND] = {
    'SourceBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_SOURCE_ALPHA,
    'DestinationBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_INVERSE_SOURCE_ALPHA};

  stateInfos[o3djs.particles.ParticleStateIds.ADD] = {
    'SourceBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_SOURCE_ALPHA,
    'DestinationBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_ONE};

  stateInfos[o3djs.particles.ParticleStateIds.BLEND_PREMULTIPLY] = {
    'SourceBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_ONE,
    'DestinationBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_INVERSE_SOURCE_ALPHA};

  stateInfos[o3djs.particles.ParticleStateIds.BLEND_NO_ALPHA] = {
    'SourceBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_SOURCE_COLOR,
    'DestinationBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_INVERSE_SOURCE_COLOR};

  stateInfos[o3djs.particles.ParticleStateIds.SUBTRACT] = {
    'SourceBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_SOURCE_ALPHA,
    'DestinationBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_INVERSE_SOURCE_ALPHA,
    'BlendEquation':
        o3djs.base.o3d.State.BLEND_REVERSE_SUBTRACT};

  stateInfos[o3djs.particles.ParticleStateIds.INVERSE] = {
    'SourceBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_INVERSE_DESTINATION_COLOR,
    'DestinationBlendFunction':
        o3djs.base.o3d.State.BLENDFUNC_INVERSE_SOURCE_COLOR};

  for (var key in o3djs.particles.ParticleStateIds) {
    var state = pack.createObject('State');
    var id = o3djs.particles.ParticleStateIds[key];
    particleStates[id] = state;
    state.getStateParam('ZWriteEnable').value = false;
    state.getStateParam('CullMode').value = o3d.State.CULL_NONE;

    var info = stateInfos[id];
    for (var stateName in info) {
      state.getStateParam(stateName).value = info[stateName];
    }
  }

  var colorTexture = pack.createTexture2D(8, 8, o3d.Texture.ARGB8, 1, false);
  var pixelBase = [0, 0.20, 0.70, 1, 0.70, 0.20, 0, 0];
  var pixels = [];
  for (var yy = 0; yy < 8; ++yy) {
    for (var xx = 0; xx < 8; ++xx) {
      var pixel = pixelBase[xx] * pixelBase[yy];
      pixels.push(pixel, pixel, pixel, pixel);
    }
  }
  colorTexture.set(0, pixels);
  var rampTexture = pack.createTexture2D(3, 1, o3d.Texture.ARGB8, 1, false);
  rampTexture.set(0, [1, 1, 1, 1,
                      1, 1, 1, 0.5,
                      1, 1, 1, 0]);

  if (!opt_clockParam) {
    this.counter_ = pack.createObject('SecondCounter');
    opt_clockParam = this.counter_.getParam('count');
  }

  this.randomFunction_ = opt_randomFunction || function() {
        return Math.random();
      };

  /**
   * The states for the various blend modes.
   * @type {!Array.<!o3d.State>}
   */
  this.particleStates = particleStates;

  /**
   * The default ParamFloat to use as the clock for emitters created by
   * this system.
   * @type {!o3d.ParamFloat}
   */
  this.clockParam = opt_clockParam;

  /**
   * The pack used to manage particle system resources.
   * @type {!o3d.Pack}
   */
  this.pack = pack;

  /**
   * The viewInfo that is used to get drawLists.
   * @type {!o3djs.rendergraph.ViewInfo}
   */
  this.viewInfo = viewInfo;

  /**
   * The effects for particles.
   * @type {!Array.<!o3d.Effect>}
   */
  this.effects = effects;


  /**
   * The default color texture for particles.
   * @type {!o3d.Texture2D}
   */
  this.defaultColorTexture = colorTexture;


  /**
   * The default ramp texture for particles.
   * @type {!o3d.Texture2D}
   */
  this.defaultRampTexture = rampTexture;
};

/**
 * A ParticleSpec specifies how to emit particles.
 *
 * NOTE: For all particle functions you can specific a ParticleSpec as a
 * Javascript object, only specifying the fields that you care about.
 *
 * <pre>
 * emitter.setParameters({
 *   numParticles: 40,
 *   lifeTime: 2,
 *   timeRange: 2,
 *   startSize: 50,
 *   endSize: 90,
 *   positionRange: [10, 10, 10],
 *   velocity:[0, 0, 60], velocityRange: [15, 15, 15],
 *   acceleration: [0, 0, -20],
 *   spinSpeedRange: 4}
 * );
 * </pre>
 *
 * Many of these parameters are in pairs. For paired paramters each particle
 * specfic value is set like this
 *
 * particle.field = value + Math.random() - 0.5 * valueRange * 2;
 *
 * or in English
 *
 * particle.field = value plus or minus valueRange.
 *
 * So for example, if you wanted a value from 10 to 20 you'd pass 15 for value
 * and 5 for valueRange because
 *
 * 15 + or - 5  = (10 to 20)
 *
 * @constructor
 */
o3djs.particles.ParticleSpec = function() {
  /**
   * The number of particles to emit.
   * @type {number}
   */
  this.numParticles = 1;

  /**
   * The number of frames in the particle texture.
   * @type {number}
   */
  this.numFrames = 1;

  /**
   * The frame duration at which to animate the particle texture in seconds per
   * frame.
   * @type {number}
   */
  this.frameDuration = 1;

  /**
   * The initial frame to display for a particular particle.
   * @type {number}
   */
  this.frameStart = 0;

  /**
   * The frame start range.
   * @type {number}
   */
  this.frameStartRange = 0;

  /**
   * The life time of the entire particle system.
   * To make a particle system be continuous set this to match the lifeTime.
   * @type {number}
   */
  this.timeRange = 99999999;

  /**
   * The startTime of a particle.
   * @type {number?}
   */
  this.startTime = null;
  // TODO: Describe what happens if this is not set. I still have some
  //     work to do there.

  /**
   * The lifeTime of a particle.
   * @type {number}
   */
  this.lifeTime = 1;

  /**
   * The lifeTime range.
   * @type {number}
   */
  this.lifeTimeRange = 0;

  /**
   * The starting size of a particle.
   * @type {number}
   */
  this.startSize = 1;

  /**
   * The starting size range.
   * @type {number}
   */
  this.startSizeRange = 0;

  /**
   * The ending size of a particle.
   * @type {number}
   */
  this.endSize = 1;

  /**
   * The ending size range.
   * @type {number}
   */
  this.endSizeRange = 0;

  /**
   * The starting position of a particle in local space.
   * @type {!o3djs.math.Vector3}
   */
  this.position = [0, 0, 0];

  /**
   * The starting position range.
   * @type {!o3djs.math.Vector3}
   */
  this.positionRange = [0, 0, 0];

  /**
   * The velocity of a paritcle in local space.
   * @type {!o3djs.math.Vector3}
   */
  this.velocity = [0, 0, 0];

  /**
   * The velocity range.
   * @type {!o3djs.math.Vector3}
   */
  this.velocityRange = [0, 0, 0];

  /**
   * The acceleration of a particle in local space.
   * @type {!o3djs.math.Vector3}
   */
  this.acceleration = [0, 0, 0];

  /**
   * The accleration range.
   * @type {!o3djs.math.Vector3}
   */
  this.accelerationRange = [0, 0, 0];

  /**
   * The starting spin value for a particle in radians.
   * @type {number}
   */
  this.spinStart = 0;

  /**
   * The spin start range.
   * @type {number}
   */
  this.spinStartRange = 0;

  /**
   * The spin speed of a particle in radians.
   * @type {number}
   */
  this.spinSpeed = 0;

  /**
   * The spin speed range.
   * @type {number}
   */
  this.spinSpeedRange = 0;

  /**
   * The color multiplier of a particle.
   * @type {!o3djs.math.Vector4}
   */
  this.colorMult = [1, 1, 1, 1];

  /**
   * The color multiplier range.
   * @type {!o3djs.math.Vector4} colorMultRange
   */
  this.colorMultRange = [0, 0, 0, 0];

  /**
   * The velocity of all paritcles in world space.
   * @type {!o3djs.math.Vector3}
   */
  this.worldVelocity = [0, 0, 0];

  /**
   * The acceleration of all paritcles in world space.
   * @type {!o3djs.math.Vector3}
   */
  this.worldAcceleration = [0, 0, 0];

  /**
   * Whether these particles are oriented in 2d or 3d. true = 2d, false = 3d.
   * @type {boolean}
   */
  this.billboard = true;

  /**
   * The orientation of a particle. This is only used if billboard is false.
   * @type {!o3djs.quaternions.Quaterinion}
   */
  this.orientation = [0, 0, 0, 1];
};

/**
 * Creates a particle emitter.
 * @param {!o3d.Texture} opt_texture The texture to use for the particles.
 *     If you don't supply a texture a default is provided.
 * @param {!o3d.ParamFloat} opt_clockParam A ParamFloat to be the clock for
 *     the emitter.
 * @return {!o3djs.particles.ParticleEmitter} The new emitter.
 */
o3djs.particles.ParticleSystem.prototype.createParticleEmitter =
    function(opt_texture, opt_clockParam) {
  return new o3djs.particles.ParticleEmitter(this, opt_texture, opt_clockParam);
};

/**
 * A ParticleEmitter
 * @constructor
 * @param {!o3djs.particles.ParticleSystem} particleSystem The particle system
 *     to manage this emitter.
 * @param {!o3d.Texture} opt_texture The texture to use for the particles.
 *     If you don't supply a texture a default is provided.
 * @param {!o3d.ParamFloat} opt_clockParam A ParamFloat to be the clock for
 *     the emitter.
 */
o3djs.particles.ParticleEmitter = function(particleSystem,
                                           opt_texture,
                                           opt_clockParam) {

  opt_clockParam = opt_clockParam || particleSystem.clockParam;

  var o3d = o3djs.base.o3d;
  var pack = particleSystem.pack;
  var viewInfo = particleSystem.viewInfo;
  var material = pack.createObject('Material');
  material.name = 'particles';
  material.drawList = viewInfo.zOrderedDrawList;
  material.effect = particleSystem.effects[1];
  particleSystem.effects[1].createUniformParameters(material);
  material.getParam('time').bind(opt_clockParam);

  var rampSampler = pack.createObject('Sampler');
  rampSampler.texture = particleSystem.defaultRampTexture;
  rampSampler.addressModeU = o3d.Sampler.CLAMP;

  var colorSampler = pack.createObject('Sampler');
  colorSampler.texture = opt_texture || particleSystem.defaultColorTexture;
  colorSampler.addressModeU = o3d.Sampler.CLAMP;
  colorSampler.addressModeV = o3d.Sampler.CLAMP;

  material.getParam('rampSampler').value = rampSampler;
  material.getParam('colorSampler').value = colorSampler;

  var vertexBuffer = pack.createObject('VertexBuffer');
  var uvLifeTimeFrameStartField = vertexBuffer.createField('FloatField', 4);
  var positionStartTimeField = vertexBuffer.createField('FloatField', 4);
  var velocityStartSizeField = vertexBuffer.createField('FloatField', 4);
  var accelerationEndSizeField = vertexBuffer.createField('FloatField', 4);
  var spinStartSpinSpeedField = vertexBuffer.createField('FloatField', 4);
  var orientationField = vertexBuffer.createField('FloatField', 4);
  var colorMultField = vertexBuffer.createField('FloatField', 4);

  var indexBuffer = pack.createObject('IndexBuffer');

  var streamBank = pack.createObject('StreamBank');
  streamBank.setVertexStream(o3d.Stream.POSITION, 0,
                             uvLifeTimeFrameStartField, 0);
  streamBank.setVertexStream(o3d.Stream.TEXCOORD, 0,
                             positionStartTimeField, 0);
  streamBank.setVertexStream(o3d.Stream.TEXCOORD, 1,
                             velocityStartSizeField, 0);
  streamBank.setVertexStream(o3d.Stream.TEXCOORD, 2,
                             accelerationEndSizeField, 0);
  streamBank.setVertexStream(o3d.Stream.TEXCOORD, 3,
                             spinStartSpinSpeedField, 0);
  streamBank.setVertexStream(o3d.Stream.TEXCOORD, 4,
                             orientationField, 0);
  streamBank.setVertexStream(o3d.Stream.COLOR, 0,
                             colorMultField, 0);

  var shape = pack.createObject('Shape');
  var primitive = pack.createObject('Primitive');
  primitive.material = material;
  primitive.owner = shape;
  primitive.streamBank = streamBank;
  primitive.indexBuffer = indexBuffer;
  primitive.primitiveType = o3d.Primitive.TRIANGLELIST;
  primitive.createDrawElement(pack, null);

  this.vertexBuffer_ = vertexBuffer;
  this.uvLifeTimeFrameStartField_ = uvLifeTimeFrameStartField;
  this.positionStartTimeField_ = positionStartTimeField;
  this.velocityStartSizeField_ = velocityStartSizeField;
  this.accelerationEndSizeField_ = accelerationEndSizeField;
  this.spinStartSpinSpeedField_ = spinStartSpinSpeedField;
  this.orientationField_ = orientationField;
  this.colorMultField_ = colorMultField;
  this.indexBuffer_ = indexBuffer;
  this.streamBank_ = streamBank;
  this.primitive_ = primitive;
  this.rampSampler_ = rampSampler;
  this.rampTexture_ = particleSystem.defaultRampTexture;
  this.colorSampler_ = colorSampler;

  /**
   * The particle system managing this emitter.
   * @type {!o3djs.particles.ParticleSystem}
   */
  this.particleSystem = particleSystem;

  /**
   * The Shape used to render these particles.
   * @type {!o3d.Shape}
   */
  this.shape = shape;

  /**
   * The material used by this emitter.
   * @type {!o3d.Matrerial}
   */
  this.material = material;

  /**
   * The param that is the source for the time for this emitter.
   * @type {!o3d.ParamFloat}
   */
  this.clockParam = opt_clockParam;
};

/**
 * Sets the blend state for the particles.
 * You can use this to set the emitter to draw with BLEND, ADD, SUBTRACT, etc.
 * @param {o3djs.particles.ParticleStateIds} stateId The state you want.
 */
o3djs.particles.ParticleEmitter.prototype.setState = function(stateId) {
  this.material.state = this.particleSystem.particleStates[stateId];
};

/**
 * Sets the colorRamp for the particles.
 * The colorRamp is used as a multiplier for the texture. When a particle
 * starts it is multiplied by the first color, as it ages to progressed
 * through the colors in the ramp.
 *
 * <pre>
 * particleEmitter.setColorRamp([
 *   1, 0, 0, 1,    // red
 *   0, 1, 0, 1,    // green
 *   1, 0, 1, 0]);  // purple but with zero alpha
 * </pre>
 *
 * The code above sets the particle to start red, change to green then
 * fade out while changing to purple.
 *
 * @param {!Array.<number>} colorRamp An array of color values in
 *     the form RGBA.
 */
o3djs.particles.ParticleEmitter.prototype.setColorRamp = function(colorRamp) {
  var width = colorRamp.length / 4;
  if (width % 1 != 0) {
    throw 'colorRamp must have multiple of 4 entries';
  }

  if (this.rampTexture_ == this.particleSystem.defaultRampTexture) {
    this.rampTexture_ = null;
  }

  if (this.rampTexture_ && this.rampTexture_.width != width) {
    this.particleSystem.pack.removeObject(this.rampTexture_);
    this.rampTexture_ = null;
  }

  if (!this.rampTexture_) {
    this.rampTexture_ = this.particleSystem.pack.createTexture2D(
        width, 1, o3djs.base.o3d.Texture.ARGB8, 1, false);
  }

  this.rampTexture_.set(0, colorRamp);
  this.rampSampler_.texture = this.rampTexture_;
};

/**
 * Sets the parameters of the particle emitter.
 *
 * Each of these parameters are in pairs. The used to create a table
 * of particle parameters. For each particle a specfic value is
 * set like this
 *
 * particle.field = value + Math.random() - 0.5 * valueRange * 2;
 *
 * or in English
 *
 * particle.field = value plus or minus valueRange.
 *
 * So for example, if you wanted a value from 10 to 20 you'd pass 15 for value
 * and 5 for valueRange because
 *
 * 15 + or - 5  = (10 to 20)
 *
 * @param {!o3djs.particles.ParticleSpec} parameters The parameters for the
 *     emitters.
 * @param {!function(number, !o3djs.particles.ParticleSpec): void}
 *     opt_perParticleParamSetter A function that is called for each particle to
 *     allow it's parameters to be adjusted per particle. The number is the
 *     index of the particle being created, in other words, if numParticles is
 *     20 this value will be 0 to 19. The ParticleSpec is a spec for this
 *     particular particle. You can set any per particle value before returning.
 */
o3djs.particles.ParticleEmitter.prototype.setParameters = function(
    parameters,
    opt_perParticleParamSetter) {
  var defaults = new o3djs.particles.ParticleSpec();
  for (var key in parameters) {
    if (typeof defaults[key] === 'undefined') {
      throw 'unknown particle parameter "' + key + '"';
    }
    defaults[key] = parameters[key];
  }

  var numParticles = defaults.numParticles;

  var uvLifeTimeFrameStart = [];
  var positionStartTime = [];
  var velocityStartSize = [];
  var accelerationEndSize = [];
  var spinStartSpinSpeed = [];
  var orientation = [];
  var colorMults = [];

  // Set the globals.
  this.material.effect =
      this.particleSystem.effects[defaults.billboard ? 1 : 0];
  this.material.getParam('timeRange').value = defaults.timeRange;
  this.material.getParam('numFrames').value = defaults.numFrames;
  this.material.getParam('frameDuration').value = defaults.frameDuration;
  this.material.getParam('worldVelocity').value = defaults.worldVelocity;
  this.material.getParam('worldAcceleration').value =
      defaults.worldAcceleration;

  var corners = [
      [-0.5, -0.5],
      [+0.5, -0.5],
      [+0.5, +0.5],
      [-0.5, +0.5]];

  var random = this.particleSystem.randomFunction_;

  var plusMinus = function(range) {
    return (random() - 0.5) * range * 2;
  };

  var plusMinusVector = function(range) {
    var v = [];
    for (var ii = 0; ii < range.length; ++ii) {
      v.push(plusMinus(range[ii]));
    }
    return v;
  };

  for (var ii = 0; ii < numParticles; ++ii) {
    if (opt_perParticleParamSetter) {
      opt_perParticleParamSetter(ii, defaults);
    }
    var pLifeTime = defaults.lifeTime;
    var pStartTime = (defaults.startTime === null) ?
        (ii * defaults.lifeTime / numParticles) : defaults.startTime;
    var pFrameStart = defaults.frameStart + plusMinus(defaults.frameStartRange);
    var pPosition = o3djs.math.addVector(
        defaults.position, plusMinusVector(defaults.positionRange));
    var pVelocity = o3djs.math.addVector(
        defaults.velocity, plusMinusVector(defaults.velocityRange));
    var pAcceleration = o3djs.math.addVector(
        defaults.acceleration, plusMinusVector(defaults.accelerationRange));
    var pColorMult = o3djs.math.addVector(
        defaults.colorMult, plusMinusVector(defaults.colorMultRange));
    var pSpinStart = defaults.spinStart + plusMinus(defaults.spinStartRange);
    var pSpinSpeed = defaults.spinSpeed + plusMinus(defaults.spinSpeedRange);
    var pStartSize = defaults.startSize + plusMinus(defaults.startSizeRange);
    var pEndSize = defaults.endSize + plusMinus(defaults.endSizeRange);
    var pOrientation = defaults.orientation;

    // make each corner of the particle.
    for (var jj = 0; jj < 4; ++jj) {
      uvLifeTimeFrameStart.push(corners[jj][0], corners[jj][1], pLifeTime,
                                pFrameStart);
      positionStartTime.push(
          pPosition[0], pPosition[1], pPosition[2], pStartTime);
      velocityStartSize.push(
          pVelocity[0], pVelocity[1], pVelocity[2], pStartSize);
      accelerationEndSize.push(
          pAcceleration[0], pAcceleration[1], pAcceleration[2], pEndSize);
      spinStartSpinSpeed.push(pSpinStart, pSpinSpeed, 0, 0);
      orientation.push(
          pOrientation[0], pOrientation[1], pOrientation[2], pOrientation[3]);
      colorMults.push(
          pColorMult[0], pColorMult[1], pColorMult[2], pColorMult[3]);
    }
  }

  if (this.vertexBuffer_.numElements != numParticles * 4) {
    this.vertexBuffer_.allocateElements(numParticles * 4);

    var indices = [];
    for (var ii = 0; ii < numParticles; ++ii) {
      // Make 2 triangles for the quad.
      var startIndex = ii * 4
      indices.push(startIndex + 0, startIndex + 1, startIndex + 2);
      indices.push(startIndex + 0, startIndex + 2, startIndex + 3);
    }
    this.indexBuffer_.set(indices);
  }

  this.uvLifeTimeFrameStartField_.setAt(0, uvLifeTimeFrameStart);
  this.positionStartTimeField_.setAt(0, positionStartTime);
  this.velocityStartSizeField_.setAt(0, velocityStartSize);
  this.accelerationEndSizeField_.setAt(0, accelerationEndSize);
  this.spinStartSpinSpeedField_.setAt(0, spinStartSpinSpeed);
  this.orientationField_.setAt(0, orientation);
  this.colorMultField_.setAt(0, colorMults);

  this.primitive_.numberPrimitives = numParticles * 2;
  this.primitive_.numberVertices = numParticles * 4;
};

/**
 * Creates a OneShot particle emitter instance.
 * You can use this for dust puffs, explosions, fireworks, etc...
 * @param {!o3d.Transform} opt_parent The parent for the oneshot.
 * @return {!o3djs.particles.OneShot} A OneShot object.
 */
o3djs.particles.ParticleEmitter.prototype.createOneShot = function(opt_parent) {
  return new o3djs.particles.OneShot(this, opt_parent);
};

/**
 * An object to manage a particle emitter instance as a one shot. Examples of
 * one shot effects are things like an explosion, some fireworks.
 * @constructor
 * @param {!o3djs.particles.ParticleEmitter} emitter The emitter to use for the
 *     one shot.
 * @param {!o3d.Transform} opt_parent The parent for this one shot.
 */
o3djs.particles.OneShot = function(emitter, opt_parent) {
  var pack = emitter.particleSystem.pack;
  this.emitter_ = emitter;

  /**
   * Transform for OneShot.
   * @type {!o3d.Transform}
   */
  this.transform = pack.createObject('Transform');
  this.transform.visible = false;
  this.transform.addShape(emitter.shape);
  this.timeOffsetParam_ =
      this.transform.createParam('timeOffset', 'ParamFloat');
  if (opt_parent) {
    this.setParent(opt_parent);
  }
};

/**
 * Sets the parent transform for this OneShot.
 * @param {!o3d.Transform} parent The parent for this one shot.
 */
o3djs.particles.OneShot.prototype.setParent = function(parent) {
  this.transform.parent = parent;
};

/**
 * Triggers the oneshot.
 *
 * Note: You must have set the parent either at creation, with setParent, or by
 * passing in a parent here.
 *
 * @param {!o3djs.math.Vector3} opt_position The position of the one shot
 *     relative to its parent.
 * @param {!o3d.Transform} opt_parent The parent for this one shot.
 */
o3djs.particles.OneShot.prototype.trigger = function(opt_position, opt_parent) {
  if (opt_parent) {
    this.setParent(opt_parent);
  }
  if (opt_position) {
    this.transform.identity();
    this.transform.translate(opt_position);
  }
  this.transform.visible = true;
  this.timeOffsetParam_.value = this.emitter_.clockParam.value;
};
