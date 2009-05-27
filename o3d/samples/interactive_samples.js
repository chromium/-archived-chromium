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

var codeArray = [
  {
    'category': 'Basic',
     'samples': [
       {'files':
         ['simple.html', 'simple.js'],
        'sampleName': 'Simple'},
       {'files':
         ['helloworld.html', 'helloworld.js'],
        'sampleName': 'Hello World'},
       {'files':
        ['customcamera.html', 'customcamera.js'],
        'sampleName': 'Custom Camera',
        'ownWindow': true},
       {'files':
        ['simpletexture.html', 'simpletexture.js'],
        'sampleName': 'Simple Texture'},
       {'files':
        ['hellocube.html', 'hellocube.js'],
        'sampleName': 'Simple Cube'},
       {'files':
        ['hellocube-colors.html', 'hellocube-colors.js'],
        'sampleName': 'Simple Cube With Colors',
        'ownWindow': true},
       {'files':
        ['hellocube-textures.html', 'hellocube-textures.js'],
        'sampleName': 'Simple Cube With Textures'}
     ]
  },
  {
    'category': 'Advanced',
    'samples': [
       {'files':
        ['2d.html', '2d.js'],
        'sampleName': '2d'},
       {'files':
        ['culling.html', 'culling.js'],
        'sampleName': 'Culling',
        'ownWindow': true},
       {'files':
        ['displayfps.html', 'displayfps.js'],
        'sampleName': 'Display FPS',
        'ownWindow': true},
       {'files':
        ['hud-2d-overlay.html', 'hud-2d-overlay.js'],
        'sampleName': 'HUD 2D Overlay'},
       {'files':
        ['instancing.html', 'instancing.js'],
        'sampleName': 'Instancing'},
       {'files':
        ['instance-override.html', 'instance-override.js'],
        'sampleName': 'Instance Override'},
       {'files':
        ['juggler.html', 'juggler.js'],
        'sampleName': 'Juggler'},
       {'files':
        ['julia.html', 'julia.js'],
        'sampleName': 'Julia'},
       {'files':
        ['multiple-clients.html', 'multiple-clients.js'],
        'sampleName': 'Multiple Clients',
        'ownWindow': true},
       {'files':
        ['multiple-views.html', 'multiple-views.js'],
        'sampleName': 'Multiple Views'},
       {'files':
        ['phongshading.html', 'phongshading.js'],
        'sampleName': 'Phong Shading'},
       {'files':
        ['picking.html', 'picking.js'],
        'sampleName': 'Picking'},
       {'files':
        ['render-mode.html', 'render-mode.js'],
        'sampleName': 'Render Mode',
        'ownWindow': true},
       {'files':
        ['render-targets.html', 'render-targets.js'],
        'sampleName': 'Render Targets'},
       {'files':
        ['convolution.html', 'convolution.js'],
        'sampleName': 'Convolution Shader'},
       {'files':
        ['sobel.html', 'sobel.js'],
        'sampleName': 'Sobel Edge Detection'},
       {'files':
        ['rotatemodel.html', 'rotatemodel.js'],
        'sampleName': 'Rotate Model'},
       {'files':
        ['scatter-chart.html', 'scatter-chart.js'],
        'sampleName': 'Scatter Chart',
        'ownWindow': true},
       {'files':
        ['shader-test.html', 'shader-test.js'],
        'sampleName': 'Shader Test',
        'ownWindow': true},
       {'files':
        ['stencil_example.html', 'stencil_example.js'],
        'sampleName': 'Stencil Example'},
       {'files':
        ['error-texture.html', 'error-texture.js'],
        'sampleName': 'Error Texture',
        'ownWindow': true},
       {'files':
        ['texturesamplers.html', 'texturesamplers.js'],
        'sampleName': 'Texture Samplers'},
       {'files':
        ['generate-texture.html', 'generate-texture.js'],
        'sampleName': 'Generating A Texture'},
       {'files':
        ['procedural-texture.html', 'procedural-texture.js'],
        'sampleName': 'Procedural Texture'},
       {'files':
        ['zsorting.html', 'zsorting.js'],
        'sampleName': 'Z-Sorting'},
       {'files':
        ['canvas.html', 'canvas.js'],
        'sampleName': 'Canvas' },
       {'files':
        ['canvas-texturedraw.html', 'canvas-texturedraw.js'],
        'sampleName': 'Canvas: Drawing With Bitmaps',
        'ownWindow': true},
       {'files':
        ['vertex-shader.html', 'vertex-shader.js'],
        'sampleName': 'Vertex Shader' },
       {'files':
        ['primitives.html', 'primitives.js'],
        'sampleName': 'Primitives',
        'ownWindow': true },
       {'files':
        ['tutorial-primitive.html', 'tutorial-primitive.js'],
        'sampleName': 'Tutorial Primitive',
        'ownWindow': true },
       {'files':
        ['skinning.html', 'skinning.js'],
        'sampleName': 'Skinning' },
       {'files':
        ['animation.html', 'animation.js'],
        'sampleName': 'Animation' },
       {'files':
        ['animated-scene.html', 'animated-scene.js'],
        'sampleName': 'Animated Scene',
        'ownWindow': true },
       {'files':
        ['particles.html', 'particles.js'],
        'sampleName': 'Particles' },
       {'files':
        ['vertex-shader-animation.html', 'vertex-shader-animation.js'],
        'sampleName': 'Vertex Shader Animation' }
    ]
  }
];

