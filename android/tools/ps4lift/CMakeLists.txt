cmake_minimum_required(VERSION 3.21)
project(ps4lift C CXX)
# cmake -S tools/ps4lift -B ~/src/ps4lift-build -G Ninja \
#   -DCMAKE_PREFIX_PATH="$HOME/src/remill/install;$HOME/src/remill/dependencies/install" \
#   -DCMAKE_CXX_COMPILER=clang++-22 -DCMAKE_BUILD_TYPE=Release
find_package(remill CONFIG REQUIRED)
add_executable(ps4lift ps4lift.cpp)
target_link_libraries(ps4lift PRIVATE remill)
set_target_properties(ps4lift PROPERTIES CXX_STANDARD 17)
find_package(LLVM CONFIG REQUIRED)
target_include_directories(ps4lift SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
