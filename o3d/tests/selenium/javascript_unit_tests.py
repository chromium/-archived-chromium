#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Runs JavaScript unit tests in Selenium.

NOTE: If you manually write a test in python (vs using the generic test)
The name of the screenshots must match the name of the test not including
the suffix.  In otherwords, if your test is called TestSampleCustomCameraMedium
then your screenshots need to be named customcamera1, customcamera2 etc.

This is so when it comes time to compare screenshots we can figure out which
reference images require a corresponding screenshot. In other words if
TestSampleCustomCameraMedium is run then we know that any reference file
named customcamera1_reference.png requires that there be a corresponding
screenshot.
"""


import selenium_utilities

class JavaScriptUnitTests(selenium_utilities.SeleniumTestCase):
  """Runs the JavaScript unit tests for the sample utilities."""

  def __init__(self, name, session, browser, test_type=None, sample_path=None,
               options=None):
    selenium_utilities.SeleniumTestCase.__init__(
        self, name, session, browser, test_type, sample_path, options)

  def GenericTest(self):
    """Generically test a sample.

    Each sample is expected to have a global variable called g_testResult
    That starts undefined and is set to true or false when the test is finished.
    """
    self.RunGenericTest(
        self.session.browserURL +
            "/tests/selenium/tests/",
        "typeof(window.g_testResult) != 'undefined'",
        "window.g_testResult")

  def TestStressDrawShapes(self):
    """Tries to draw a large number of shapes for stress testing."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL +
        "/tests/selenium/tests/drawshapes.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 20000)

    # Sanity checks.
    self.assertEqual("Drawshape stress test for O3D", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Check assets for base o3d setup:
    # 2 nodes in scene graph
    # Root node and g_parent_transform node
    self.assertEqual(
        "2",
        s.get_eval("window.g_client.root.getTransformsInTree().length"))

    # There are 8 nodes in the render graph.
    # 1 root render node
    # 1 viewport
    # 1 clear buffer
    # 1 tree traversal
    # 2 draw passes
    # 2 StateSets
    self.assertEqual(
        "8",
        s.get_eval("window.g_client.renderGraphRoot."
                   "getRenderNodesInTree().length"))

    # Draw 5 triangles
    s.type("numShapes", "5")
    s.click("btnTri")

    # 5 more primitives should get created
    # (1 for the parent transform)
    s.wait_for_condition(
        "window.g_client."
        "getObjectsByClassName('o3d.Shape').length == 5",
        5000)
    # 5 more primitives nodes should get created
    self.assertEqual(
        "5",
        s.get_eval("window.g_client."
                   "getObjectsByClassName('o3d.Primitive').length"))

    # Draw more triangles
    s.type("numShapes", "8")
    for i in range(1, 10):
      s.click("btnTri")
      s.wait_for_condition(
          "window.g_client."
          "getObjectsByClassName('o3d.Shape').length == %d" % (5 + i * 8),
          5000)
      self.assertEqual(
          str(5 + i * 8),
          s.get_eval("window.g_client."
                     "getObjectsByClassName('o3d.Primitive').length"))

    # Clear triangles, reset pack.
    s.click("btnClear")

    # Check assets for base o3d setup again.
    self.assertEqual(
        "2",
        s.get_eval("window.g_client.root.getTransformsInTree().length"))
    self.assertEqual(
        "8",
        s.get_eval("window.g_client.renderGraphRoot."
                   "getRenderNodesInTree().length"))

    # Now draw lines
    s.type("numShapes", "5")
    s.click("btnLines")

    # 5 more shapes should get created
    # (1 for the parent transform)
    s.wait_for_condition(
        "window.g_client."
        "getObjectsByClassName('o3d.Shape').length == 5",
        5000)
    # 5 more primitives and drawelements should get created
    self.assertEqual(
        "5",
        s.get_eval("window.g_client."
                   "getObjectsByClassName('o3d.Primitive').length"))
    self.assertEqual(
        "5",
        s.get_eval("window.g_client."
                   "getObjectsByClassName('o3d.DrawElement').length"))

    # Draw more lines
    s.type("numShapes", "11")
    for i in range(1, 10):
      s.click("btnLines")
      s.wait_for_condition(
          "window.g_client."
          "getObjectsByClassName('o3d.Shape').length == %d" % (5 + i * 11),
          5000)
      self.assertEqual(
          str(5 + i * 11),
          s.get_eval("window.g_client."
                     "getObjectsByClassName('o3d.Primitive').length"))

    # Clear triangles, reset pack.
    s.click("btnClear")

    # Check assets for base o3d setup again.
    self.assertEqual(
        "2",
        s.get_eval("window.g_client.root.getTransformsInTree().length"))
    self.assertEqual(
        "8",
        s.get_eval("window.g_client.renderGraphRoot."
                   "getRenderNodesInTree().length"))

    # Now draw 1000 triangles
    s.type("numShapes", "1000")
    s.click("btnTri")

    # 30 seconds to draw 1000 triangle shapes
    s.wait_for_condition(
        "window.g_client."
        "getObjectsByClassName('o3d.Shape').length == 1000",
        30000)
    # Assert number of primitives
    self.assertEqual(
        "1000",
        s.get_eval("window.g_client."
                   "getObjectsByClassName('o3d.Primitive').length"))

    # Clear triangles, reset pack.
    s.click("btnClear")

  def TestStressMultiWindow(self):
    """Opens 5 windows of simpletexture.html."""

    # Alias for the selenium session
    s = self.session

    # Save the titles of windows so we can reset the current window.
    # Note: docs of selenium  are out of date. We should be able to use ids or
    # names in select_window below but all of those methods failed.
    old = s.get_all_window_titles()

    for i in range(1, 5):
      s.open_window(s.browserURL +
          "/samples/simpletexture.html",
          "o3dstress_window%d" % i)

    for i in range(1, 5):
      s.select_window("o3dstress_window%d" % i)
      # Allow a limited time for the plugin to initialize.
      s.wait_for_condition("typeof(window.g_client) != 'undefined';", 30000)

      # Sanity checks.
      self.assertEqual("Tutorial B3: Textures", s.get_title())
      self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    for i in range(1, 5):
      s.select_window("o3dstress_window%d" % i)
      s.close()

    # make the old window the current window so the next test will run.
    s.select_window(old[0])

  def TestStressCullingZSort(self):
    """Checks culling and zsorting work."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL +
        "/tests/selenium/tests/culling-zsort-test.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 20000)

    # Sanity checks.
    self.assertEqual("Culling and ZSorting Test.", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))
    s.wait_for_condition("window.g_client != null", 10000)

    # Wait for all instances to be created.
    s.wait_for_condition(
        "window.g_totalDrawElementsElement.innerHTML == '2'",
        40000)

    # Stop animation
    s.run_script("g_timeMult = 0")
    s.run_script("g_client.renderMode = g_o3d.Client.RENDERMODE_ON_DEMAND")
    s.run_script("var g_rendered = false")

    new_data = []
    culling_reference_data = [[1793, 4472, 3780, 2, 4844, 213, 4631],
                              [1793, 6011, 3854, 2, 7734, 337, 7397],
                              [1793, 3014, 2416, 2, 3400, 121, 3279],
                              [1793, 2501, 2408, 2, 2420, 162, 2258],
                              [1793, 2933, 2914, 2, 2746, 182, 2564],
                              [1793, 2825, 2848, 2, 2604, 171, 2433],
                              [1793, 2933, 2790, 2, 2870, 155, 2715],
                              [1793, 4337, 3004, 2, 5360, 237, 5123]]

    # Take screenshots
    for clock in range(0, 8):
      s.run_script("window.g_clock = " + str(clock * 3.14159 * 2.5 + 0.5))
      self.assertTrue(
          selenium_utilities.TakeScreenShot(s, self.browser, "window.g_client",
                                            "cullingzsort" + str(clock)))
      s.run_script("g_framesRendered = 0")
      while int(s.get_eval("window.g_framesRendered")) < 3:
        s.run_script("window.g_client.render()")
      data = [s.get_eval("window.g_totalTransformsElement.innerHTML"),
              s.get_eval("window.g_transformsProcessedElement.innerHTML"),
              s.get_eval("window.g_transformsCulledElement.innerHTML"),
              s.get_eval("window.g_totalDrawElementsElement.innerHTML"),
              s.get_eval("window.g_drawElementsProcessedElement.innerHTML"),
              s.get_eval("window.g_drawElementsCulledElement.innerHTML"),
              s.get_eval("window.g_drawElementsRenderedElement.innerHTML")]
      new_data.append(data)
      print ", ".join(data)

    # check the results
    for clock in range(0, 8):
      for ii in range(0, 7):
        # comment out the following line and add a "pass" line if you need new
        # culling reference data.
        self.assertEqual(int(new_data[clock][ii]),
                         culling_reference_data[clock][ii])


if __name__ == "__main__":
  pass
