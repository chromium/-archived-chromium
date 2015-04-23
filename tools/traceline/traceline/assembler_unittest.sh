#!/bin/sh

./assembler_unittest.exe | ndisasm -u - | cut -c 11-
