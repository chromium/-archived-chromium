{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'installer_util_prebuild',
      'type': 'none',
      'actions': [
        {
          'action_name': 'installer_util_strings',
          'inputs': [
            'create_string_rc.bat',
            'create_string_rc.py',
            '../../../app/generated_resoruces.grd',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/installer_util_prebuild/installer_util_strings.rc',
            '<(SHARED_INTERMEDIATE_DIR)/installer_util_prebuild/installer_util_strings.h',
          ],
          'action': [
            './create_string_rc.bat',
            '<(SHARED_INTERMEDIATE_DIR)/installer_util_prebuild'
          ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
           '<(SHARED_INTERMEDIATE_DIR)/installer_util_prebuild',
        ],
      },
    },
  ],
}
