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
 * @fileoverview This file contains various functions for helping setup
 * elements for o3d
 */

o3djs.provide('o3djs.element');

o3djs.require('o3djs.math');

/**
 * A Module for element functions.
 * @namespace
 */
o3djs.element = o3djs.element || {};

/**
 * Sets the bounding box and z sort point of an element.
 * @param {!o3d.Element} element Element to set bounding box and z sort point
 *     on.
 */
o3djs.element.setBoundingBoxAndZSortPoint = function(element) {
  var boundingBox = element.getBoundingBox(0);
  var minExtent = boundingBox.minExtent;
  var maxExtent = boundingBox.maxExtent;
  element.boundingBox = boundingBox;
  element.cull = true;
  element.zSortPoint = o3djs.math.divVectorScalar(
      o3djs.math.addVector(minExtent, maxExtent), 2);
};

/**
 * Adds missing texture coordinate streams to a primitive.
 *
 * This is very application specific but if it's a primitive
 * and if it uses a collada material the material builder
 * assumes 1 TEXCOORD stream per texture. In other words if you have
 * both a specular texture AND a diffuse texture the builder assumes
 * you have 2 TEXCOORD streams. This assumption is often false.
 *
 * To work around this we check how many streams the material
 * expects and if there are not enough UVs streams we duplicate the
 * last TEXCOORD stream until there are, making a BIG assumption that
 * that will work.
 *
 * The problem is maybe you have 4 textures and each of them share
 * texture coordinates. There is information in the collada file about
 * what stream to connect each texture to.
 *
 * @param {!o3d.Element} element Element to add streams to.
 */
o3djs.element.addMissingTexCoordStreams = function(element) {
  // TODO: We should store that info. The conditioner should either
  // make streams that way or pass on the info so we can do it here.
  if (element.isAClassName('o3d.Primitive')) {
    var material = /** @type {!o3d.Material} */ (element.material);
    var streamBank = element.streamBank;
    var lightingType = o3djs.effect.getColladaLightingType(material);
    if (lightingType) {
      var numTexCoordStreamsNeeded =
          o3djs.effect.getNumTexCoordStreamsNeeded(material);
      // Count the number of TEXCOORD streams the streamBank has.
      var streams = streamBank.vertexStreams;
      var lastTexCoordStream = null;
      var numTexCoordStreams = 0;
      for (var ii = 0; ii < streams.length; ++ii) {
        var stream = streams[ii];
        if (stream.semantic == o3djs.base.o3d.Stream.TEXCOORD) {
          lastTexCoordStream = stream;
          ++numTexCoordStreams;
        }
      }
      // Add any missing TEXCOORD streams. It might be more efficient for
      // the GPU to create an effect that doesn't need the extra streams
      // but this is a more generic solution because it means we can reuse
      // the same effect.
      for (var ii = numTexCoordStreams;
           ii < numTexCoordStreamsNeeded;
           ++ii) {
        streamBank.setVertexStream(
            lastTexCoordStream.semantic,
            lastTexCoordStream.semanticIndex + ii - numTexCoordStreams + 1,
            lastTexCoordStream.field,
            lastTexCoordStream.startIndex);
      }
    }
  }
};

/**
 * Copies an element and streambank or buffers so the two will share
 * streambanks, vertex and index buffers.
 * @param {!o3d.Pack} pack Pack to manage created objects.
 * @param {!o3d.Element} sourceElement The element to copy.
 * @return {!o3d.Element} the new copy of sourceElement.
 */
o3djs.element.duplicateElement = function(pack, sourceElement) {
  var newElement = pack.createObject(sourceElement.className);
  newElement.copyParams(sourceElement);
  // TODO: If we get the chance to parameterize the primitive's settings
  //     we can delete this code since copyParams will handle it.
  //     For now it only handles primitives by doing it manually.
  if (sourceElement.isAClassName('o3d.Primitive')) {
    newElement.indexBuffer = sourceElement.indexBuffer;
    newElement.startIndex = sourceElement.startIndex;
    newElement.primitiveType = sourceElement.primitiveType;
    newElement.numberVertices = sourceElement.numberVertices;
    newElement.numberPrimitives = sourceElement.numberPrimitives;
  }
  return newElement;
};


