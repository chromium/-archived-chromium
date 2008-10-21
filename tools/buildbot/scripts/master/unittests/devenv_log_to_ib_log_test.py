import os
import unittest

import cl_command

class DevenvLogToIbLogTest(unittest.TestCase):

  def testConvert(self):
    source_log = open(os.path.join('data', 'error-log-compile-stdio-devenv'))
    expected_result_log = open(
        os.path.join('data', 'error-log-compile-stdio-devenv-converted'))
    converted_content = cl_command.DevenvLogToIbLog(source_log.read()).Convert()
    self.assertEqual(expected_result_log.read(), converted_content)

    source_log.close()
    expected_result_log.close()

if __name__ == '__main__':
  unittest.main()