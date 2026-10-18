cmake_minimum_required(VERSION 3.16.0)

# The one place the firmware's version is written. It becomes the application
# description's version, which the boot log and the changelog both follow. scripts/bump-version.sh is what changes it.
set(PROJECT_VER "2.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tiger_touch)
