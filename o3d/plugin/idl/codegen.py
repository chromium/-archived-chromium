#!/usr/bin/python2.4
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file is just here so that we can modify the python path before
# invoking nixysa.

import subprocess
import sys
import os
import os.path

third_party = os.path.join('..', '..', '..', 'third_party')
pythonpath = os.pathsep.join([os.path.join(third_party, 'gflags', 'python'),
                              os.path.join(third_party, 'ply', 'files')])

orig_pythonpath = os.environ.get('PYTHONPATH')
if orig_pythonpath:
  pythonpath = os.pathsep.join([pythonpath, orig_pythonpath])

os.environ['PYTHONPATH'] = pythonpath

nixysa = os.path.join(third_party, 'nixysa', 'files', 'codegen.py')
status = subprocess.call([sys.executable, nixysa] + sys.argv[1:])
sys.exit(status)
