auto_ui_test_input_generator.py takes in a list of possible actions separated  by new lines,
the number of commands per file, and the number of actions per command, and generate either
a single random file or a number of files containing all possible commands. This file is then
used as input to automated_ui_tests.exe (see chrome/test/automated_ui_tests/automated_ui_tests.cc/h)
which will run the commands, reporting on the success or failure of each.

An example of typical use:

$ python auto_ui_test_input_generator.py --action-list-file="possible_actions.txt" --output="automated_ui_tests.txt" --commands-per-file 100 --actions-per-command 5

$ automated_ui_tests.exe --input="automated_ui_tests.txt" --output="automated_ui_test_report.txt"


This will generate a random sequence of 100 commands containing 5 actions each and write it, formatted in XML,
to automated_ui_tests.txt. Then automated_ui_tests.exe reads that file, runs the commands, and outputs an XML
file to automated_ui_test_report.txt.

In the future we can write a script to parse automated_ui_test_report for failures and warnings.
