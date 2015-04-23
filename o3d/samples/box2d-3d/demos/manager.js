// This file comes from Box2D-JS, Copyright (c) 2008 ANDO Yasushi.
// The original version is available at http://box2d-js.sourceforge.net/ under the
// zlib/libpng license (see License.txt).
// This version has been modified to make it work with O3D.

/**
 * O3DManager manages o3d objects for this demo.
 * @constructor
 */
function O3DManager() {
  this.shapes = [];
}

/**
 * Gets or creates a cylinder shape. If a cylinder of the same radius already
 * exists that one will be returned, otherwise a new one will be created.
 * @param {number} radius Radius of cylinder to create.
 * @return {o3d.Shape} The shape.
 */
O3DManager.prototype.createCylinder = function(radius) {
  var id = 'cylinder-' + radius;
  var shape = this.shapes[id];
  if (!shape) {
    shape = o3djs.primitives.createCylinder(g.pack,
                                            g.materials[0],
                                            radius, 40, 20, 1,
                                            [[1, 0, 0, 0],
                                             [0, 0, 1, 0],
                                             [0, -1, 0, 0],
                                             [0, 0, 0, 1]]);
    this.shapes[id] = shape;
  }
  return new O3DShape({shape: shape});
};

/**
 * Gets or creates a compound cylinder shape. If a compound cylinder shape of
 * the same parameters already exists that one will be returned, otherwise a new
 * one will be created.
 * @param {number} radius1 Radius of first cylinder.
 * @param {number} offset1 X Offset for first cylinder.
 * @param {number} radius2 Radius of second cylinder.
 * @param {number} offset2 X Offset for second cylinder.
 * @return {o3d.Shape} The shape.
 */
O3DManager.prototype.createCompoundCylinder = function(radius1,
                                                       offset1,
                                                       radius2,
                                                       offset2) {
  var id = 'compoundCylinder-' + radius1 + '-' + offset1 +
           '-' + radius2 + '-' + offset2;
  var shape = this.shapes[id];
  if (!shape) {
    shape = o3djs.primitives.createCylinder(
        g.pack, g.materials[0], radius1, 40, 20, 1,
        [[1, 0, 0, 0],
         [0, 0, 1, 0],
         [0, -1, 0, 0],
         [offset1, 0, 0, 1]]);
    shape2 = o3djs.primitives.createCylinder(
        g.pack, g.materials[0], radius2, 40, 20, 1,
        [[1, 0, 0, 0],
         [0, 0, 1, 0],
         [0, -1, 0, 0],
         [offset2, 0, 0, 1]]);
    shape2.elements[0].owner = shape;
    g.pack.removeObject(shape2);
    this.shapes[id] = shape;
  }
  return new O3DShape({shape: shape});
};

/**
 * Gets or creates a box shape. If a box of the same width and height already
 * exists that one will be returned, otherwise a new one will be created.
 * @param {number} width Width of box.
 * @param {number} height Height of box.
 * @return {o3d.Shape} The shape.
 */
O3DManager.prototype.createBox = function(width, height) {
  var name = 'box-' + width + '-' + height;
  var shape = this.shapes[name];
  if (!shape) {
    shape = o3djs.primitives.createBox(g.pack,
                                       g.materials[0],
                                       width * 2, height * 2, 40);
    this.shapes[name] = shape;
  }
  return new O3DShape({shape: shape});
};

/**
 * Gets or creates a wedge shape. If a wedge of the same parametrs already
 * exists that one will be returned, otherwise a new one will be created.
 * @param {Array} points Array of points in the format
 *     [[x1, y1], [x2, y2], [x3, y3]] that describe a 2d triangle.
 * @return {o3d.Shape} The shape.
 */
O3DManager.prototype.createWedge = function(points) {
  var name = 'wedge';
  for (var pp = 0; pp < points.length; ++pp) {
    name += '-' + points[pp][0] + '-' + points[pp][1];
  }
  var shape = this.shapes[name];
  if (!shape) {
    shape = o3djs.primitives.createPrism(g.pack,
                                         g.materials[0],
                                         points, 40);
    this.shapes[name] = shape;
  }
  return new O3DShape({shape: shape});
};

/**
 * Gets or creates a compound wedge shape (2 wedges). If a compound wedge of the
 * same parametrs already exists that one will be returned, otherwise a new one
 * will be created.
 * @param {Array} points1 Array of points that describe a 2d triangle for the
 *     first wedge in the format [[x1, y1], [x2, y2], [x3, y3]] .
 * @param {Object} position1 An object with x and y properties used to offset
 *     the first wedge.
 * @param {number} rotation1 Rotation in radians to rotate the first wedge on
 *        the z axis.
 * @param {Array} points2 Array of points that describe a 2d triangle for the
 *     second wedge in the format [[x1, y1], [x2, y2], [x3, y3]] .
 * @param {Object} position2 An object with x and y properties used to offset
 *     the second wedge.
 * @param {number} rotation2 Rotation in radians to rotate the second wedge on
 *        the z axis.
 * @return {o3d.Shape} The shape.
 */
O3DManager.prototype.createCompoundWedge = function(points1,
                                                    position1,
                                                    rotation1,
                                                    points2,
                                                    position2,
                                                    rotation2) {
  var name = 'compoundWedge';
  for (var pp = 0; pp < points1.length; ++pp) {
    name += '-' + points1[pp][0] + '-' + points1[pp][1];
  }
  name += '-' + position1.x + '-' + position1.y + '-' + rotation1;
  for (var pp = 0; pp < points2.length; ++pp) {
    name += '-' + points2[pp][0] + '-' + points2[pp][1];
  }
  name += '-' + position2.x + '-' + position2.y + '-' + rotation2;
  var shape = this.shapes[name];
  if (!shape) {
    shape = o3djs.primitives.createPrism(
        g.pack,
        g.materials[0],
        points1,
        40,
        g.math.matrix4.mul(
          g.math.matrix4.rotationZ(rotation1),
          g.math.matrix4.translation([position1.x, position1.y, 0])));
    shape2 = o3djs.primitives.createPrism(
        g.pack,
        g.materials[0],
        points2,
        40,
        g.math.matrix4.mul(
            g.math.matrix4.rotationZ(rotation2),
            g.math.matrix4.translation([position2.x, position2.y, 0])));
    shape2.elements[0].owner = shape;
    g.pack.removeObject(shape2);
    this.shapes[name] = shape;
  }
  return new O3DShape({shape: shape});
};

/**
 * An O3DShape manages an O3D shape for the demo.
 * @constructor
 * @param {Object} spec An object that contains the fields needed to create the
 *   O3DShape. Currently only the field "shape" is needed.
 */
function O3DShape(spec) {
  this.init(spec);
}

/**
 * Initializes an O3DShape
 * @param {Object} spec An object that contains the fields needed to create the
 *   O3DShape. Currently only the field "shape" is needed.
 */
O3DShape.prototype.init = function(spec) {
    this.transform = g.pack.createObject('Transform');
    this.transform.parent = g.root;
    this.transform.addShape(spec.shape);
    this.transform.createParam('colorMult', 'ParamFloat4').value =
      [Math.random() * 0.8 + 0.2,
       Math.random() * 0.8 + 0.2,
       Math.random() * 0.8 + 0.2,
       1];
};

/**
 * Updates the position and orientation of an O3DShape.
 * @param {Object} body A B2Body object from the Box2djs library.
 */
O3DShape.prototype.updateTransform = function(body) {
  var transform = this.transform;
  var position = body.GetOriginPosition();
  transform.identity();
  transform.translate(position.x, position.y, 0);
  transform.rotateZ(body.GetRotation());
};


