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


"""Selenium tests on the o3d samples.

Checks for javascript errors and compares screenshots
taken with reference images.

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


import os
import time
import selenium_utilities


class SampleTests(selenium_utilities.SeleniumTestCase):
  """Tests a couple of samples in the o3d samples directory."""

  # TODO: Change to correct object class when NPAPI class is exposed.
  SELENIUM_OBJ_TYPE = "[object HTMLObjectElement]"

  def __init__(self, name, session, browser, test_type=None, sample_path=None,
               options=None):
    selenium_utilities.SeleniumTestCase.__init__(
        self, name, session, browser, test_type, sample_path, options)

  def GenericTest(self):
    """Generically test a sample.

    Each sample is expected to have a global variable called g_finished
    which is set to true when it is finished initializing and is in a state
    ready for a screenshot.
    """
    self.RunGenericTest(
        self.session.browserURL + "/samples/",
        "(typeof(window.g_finished) != 'undefined') && "
        "window.g_finished == true;",
        None)

  def AssertEqualMatrix4(self, matrix1, matrix2):
    """Compares two 4-by-4 matrices.

       It asserts that their entries are each very close.
    Args:
      matrix1: Full DOM path to first matrix in javascript.
      matrix2: Full DOM path to second matrix in javascript.
    """
    # Alias for the selenium session
    s = self.session
    for i in range(0, 4):
      for j in range(0, 4):
        string1 = "%s[%d][%d]" % (matrix1, i, j)
        string2 = "%s[%d][%d]" % (matrix2, i, j)
        a = float(s.get_eval(string1))
        b = float(s.get_eval(string2))
        self.assertTrue(abs(a - b) < 0.001)

  def TestSampleRotateModel(self):
    """Tests rotatemodel.html."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/rotatemodel.html")

    # wait for sample to be ready.
    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;", 50000)

    # Capture screenshot
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "rotatemodel1"))

    # Check that local matrix is the identity
    self.AssertEqualMatrix4("window.g_math.matrix4.identity()",
                            "window.g_sceneRoot.localMatrix")

    # TODO:  s.key_press is not functioning on Chrome, so we revert
    #  to button-based testing of the rotation functions on this platform.
    #  Remove this test when chrome keyboard input works in selenium.
    browser_is_chrome = self.browser == "*googlechrome"

    # Rotate slightly
    if browser_is_chrome:
      s.click("A")
    else:
      s.key_press("document.getElementById('o3d')", "a")

    # Check that rotation matrix is correct
    self.AssertEqualMatrix4(
        "window.g_math.matrix4.rotationZYX([0, -0.05, 0])",
        "window.g_sceneRoot.localMatrix")

    # Rotate model
    for unused_i in range(1, 10):
      if browser_is_chrome:
        s.click("W")
      else:
        s.key_press("document.getElementById('o3d')", "w")

    for unused_i in range(1, 5):
      if browser_is_chrome:
        s.click("S")
      else:
        s.key_press("document.getElementById('o3d')", "s")

    for unused_i in range(1, 5):
      if browser_is_chrome:
        s.click("A")
      else:
        s.key_press("document.getElementById('o3d')", "a")

    for unused_i in range(1, 10):
      if browser_is_chrome:
        s.click("D")
      else:
        s.key_press("document.getElementById('o3d')", "d")

    # Capture screenshot
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "rotatemodel2"))

    # Reset view
    s.click("//input[@value='Reset view']")

  def TestSampleCustomCamera(self):
    """Tests customcamera.html."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/customcamera.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 10000)

    # Sanity checks.
    self.assertEqual("Tutorial B4: Cameras and events", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Try different views of the camera
    time.sleep(1)
    s.type("eyeX", "5")
    time.sleep(1)
    s.type("eyeY", "5")
    time.sleep(1)
    s.type("eyeZ", "5")
    time.sleep(1)
    s.click("btnSet")

    time.sleep(1)
    s.type("eyeX", "2")
    time.sleep(1)
    s.type("eyeY", "3")
    time.sleep(1)
    s.type("eyeZ", "2")
    time.sleep(1)
    s.type("upX", "1")
    time.sleep(1)
    s.type("upY", "0")
    time.sleep(1)
    s.click("btnSet")
    time.sleep(2)

    # Capture screenshot
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "customcamera1"))

    # Reset view
    s.click("btnDefault")

  def TestSampleRenderMode(self):
    """Tests render-mode.html."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/render-mode.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 10000)

    # Sanity checks.
    self.assertEqual("Render Mode Example.", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))
    s.wait_for_condition("window.g_client != null", 5000)

    # Wait for it to render a few frames.
    s.wait_for_condition(
        "window.g_framesRendered > 10",
        5000)

    # stop rendering
    s.click("//input[@value='ondemand']")

    # reset frame count to 0
    s.run_script("window.g_framesRendered = 0")

    # wait 1 second
    time.sleep(1)

    # check that we didn't render much. Note: This is a kind of hacky
    # check because if something else (the window being covered/unconvered)
    # causes the window to be refreshed this number is unpredictable. Also,
    # if the window is covered we don't render so that can effect this test
    # as well.  The hope is since we waiting 1 second it should render 30-60
    # frames if render mode is not working.
    self.assertTrue(
        s.get_eval("window.g_framesRendered < 10"))

  def TestSamplePicking(self):
    """Tests picking.html."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/picking.html")

    # wait for sample to be ready.
    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;", 50000)

    # Sanity checks.
    self.assertEqual("O3D Picking Example.", s.get_title())

    pick_info = [{"x": 189, "y": 174, "shape": "pConeShape1"},
                 {"x": 191, "y": 388, "shape": "pTorusShape1"},
                 {"x": 459, "y": 365, "shape": "pPipeShape1"},
                 {"x": 466, "y": 406, "shape": "pCubeShape1"}]

    # if it's not working these will timeout.
    for record in pick_info:
      # Selenium can't really click the mouse; it can only tell JavaScript that
      # the mouse has been clicked, which isn't enough to get an OS-level event
      # into the plugin.  So we go around the plugin and just call the event
      # handler that it would have called.  This means we're testing most of the
      # code in the sample, but not the actual event path.
      s.get_eval("window.pick({x:%d,y:%d});" % (record["x"], record["y"]))
      s.wait_for_condition(
          "window.g_pickInfoElem.innerHTML == '" + record["shape"] + "'",
          10000)

  def TestSampleShader_Test(self):
    """Tests shader-test.html."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/shader-test.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 20000)

    # Sanity checks.
    self.assertEqual("Shader Test", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # wait for it to initialize.
    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;",
                         20000)

    # if they are animated we need to stop the animation and set the clock
    # to some time so we get a known state.
    s.run_script("g_timeMult = 0")
    s.run_script("g_clock = 27.5")

    # Figure out how many options there are.
    num_shaders = s.get_eval(
        "window.document.getElementById('shaderSelect').length")

    # try each shader
    for shader in range(0, int(num_shaders)):
      # select shader
      s.select("//select[@id='shaderSelect']", ("index=%d" % shader))
      # Take screenshot
      self.assertTrue(selenium_utilities.TakeScreenShot(
          s, self.browser, "g_client", ("shader-test%d" % shader)))

  def TestSampleErrorTextureSmall(self):
    """Tests error-texture.html."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/error-texture.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 30000)

    # Sanity checks.
    self.assertEqual("Error Texture", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Take screenshots
    time.sleep(2)   # helps with Vista FF screencapture
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "errortexture1"))
    s.click("//input[@value='User Texture']")
    s.wait_for_condition("(window.g_errorMsgElement.innerHTML=='-');",
                         1000)
    time.sleep(2)   # helps with Vista FF screencapture
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "errortexture2"))
    s.click("//input[@value='No Texture']")
    s.wait_for_condition("(window.g_errorMsgElement.innerHTML=="
                         "'Missing texture for sampler s2d');",
                         1000)
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "errortexture3"))
    s.click("//input[@value='hide 0']")
    s.wait_for_condition("(window.g_errorMsgElement.innerHTML=="
                         "'Missing Sampler for ParamSampler texSampler0');",
                         1000)
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "errortexture4"))
    s.click("//input[@value='hide 1']")
    s.wait_for_condition("(window.g_errorMsgElement.innerHTML=="
                         "'Missing ParamSampler');",
                         1000)
    self.assertTrue(selenium_utilities.TakeScreenShot(s, self.browser,
                                                      "g_client",
                                                      "errortexture5"))

  def TestSampleMultipleClientsLarge(self):
    """Tries to draw a simple example in a large number of clients."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/multiple-clients.html")

    # Allow a limited time for the plugin to initialize.  We spot-check the
    # first and last here to make sure the page has basically loaded.  Before we
    # access any plugin data, we'll check each individual client.
    s.wait_for_condition("typeof(window.g_clients[49]) != 'undefined';", 40000)

    # Sanity checks.
    self.assertEqual("Multiple Clients", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Wait until the entire setup is finished.
    s.wait_for_condition("window.g_setupDone == true;", 5000)

    # Spot-check assets for base o3d setup:
    for index in range(0, 50, 12):
      client_string = (
          "window.document.getElementById('o3d%d').client" % index)

      # Make sure this client is ready for us.
      s.wait_for_condition(client_string + " != null;", 1000)
      s.wait_for_condition(client_string + ".root != null;", 1000)

      # There are 1 draw elements:
      self.assertEqual(
          "1",
          s.get_eval(client_string +
                     ".getObjectsByClassName('o3d.DrawElement').length"))

      # There are 8 nodes in the render graph.
      # 1 root render node
      # 1 viewport
      # 1 clear buffer
      # 1 tree traversal
      # 2 draw passes
      # 2 StateSets
      self.assertEqual(
          "8",
          s.get_eval(client_string + ".renderGraphRoot."
                     "getRenderNodesInTree().length"))

  def TestSamplePingPongLarge(self):
    """Validates the start-up logic of the ping-pong sample."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/pingpong/o3dPingPong.html")

    # Sanity checks.
    self.assertEqual("o3dPingPong", s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Start the game
    s.click("clientBanner")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;", 10000)

  def TestSampleHelloCube_TexturesSmall(self):
    """Validates loading of files from an external source."""

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/hellocube-textures.html")

    # Sanity checks.
    self.assertEqual(
        "Hello Square Textures: Getting started with O3D, take 3.",
        s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;", 10000)

    # if they are animated we need to stop the animation and set the clock
    # to some time so we get a known state.
    s.run_script("g_timeMult = 0")
    s.run_script("g_clock = 27.5")

    # Take screenshot
    self.assertTrue(selenium_utilities.TakeScreenShot(
        s,
        self.browser,
        "g_client",
        "hellocube-textures1"))

  def TestLoadTextureFromFileSmall(self):
    """Checks for improper access to local files."""

    # Alias for the selenium session.
    s = self.session
    s.open(s.browserURL + "/samples/hellocube-textures.html")

    # Sanity checks.
    self.assertEqual(
        "Hello Square Textures: Getting started with O3D, take 3.",
        s.get_title())
    self.assertEqual("null", s.get_eval("window.undefined_symbol_xxxyyy"))

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;", 10000)

    # Check that loading from a local file fails, when the page is served over
    # http.
    existing_image = os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     "..",
                     "..",
                     "samples",
                     "assets",
                     "google-square.png"))

    self.assertTrue(os.path.exists(existing_image))
    s.type("url", "file://%s" % existing_image)
    s.click("updateButton")

    s.wait_for_condition(
        "(typeof(window.g_textureLoadDenied) != 'undefined') && "
        "window.g_textureLoadDenied == true;", 10000)

  def TestSampleRefreshPageLoad_Small(self):
    """Tests the behaviour of the plug-in and browser when the page is
       refreshed before all of the O3D streams have loaded.
    """

    # Alias for the selenium session
    s = self.session
    s.open(s.browserURL + "/samples/archive-textures.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 10000)

    # Instruct the page to continually download content.
    s.run_script("window.g_repeatDownload = true")
    s.click("startLoad")

    # Wait for the browser to load the page.
    s.wait_for_condition(
        "(typeof(window.g_streamingStarted) != 'undefined')", 10000)

    # Refresh the page, before waiting for all of the textures to be loaded.
    # This tests that the browser won't hang while processing the
    # no-longer-needed streams.
    s.open(s.browserURL + "/samples/archive-textures.html")

    # Allow a limited time for the plugin to initialize.
    s.wait_for_condition("typeof(window.g_client) != 'undefined';", 10000)

    # Instruct the page to download the tgz file once.
    s.click("startLoad")

    s.wait_for_condition("(typeof(window.g_finished) != 'undefined') && "
                         "window.g_finished == true;", 20000)

if __name__ == "__main__":
  pass
