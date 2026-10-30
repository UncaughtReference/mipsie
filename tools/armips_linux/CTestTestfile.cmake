# CMake generated Testfile for 
# Source directory: /mnt/d/armips/armips
# Build directory: /mnt/d/armips/armips/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(armipstests "/mnt/d/armips/armips/build/armipstests" "/mnt/d/armips/armips/Tests")
set_tests_properties(armipstests PROPERTIES  _BACKTRACE_TRIPLES "/mnt/d/armips/armips/CMakeLists.txt;261;add_test;/mnt/d/armips/armips/CMakeLists.txt;0;")
subdirs("ext/filesystem")
