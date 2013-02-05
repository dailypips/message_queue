cmake_minimum_required (VERSION 2.8)

project (message_queue)

add_executable (mq_blk_test
                "${CMAKE_SOURCE_DIR}/test/mq_blk_test.cpp")

include_directories ("${CMAKE_SOURCE_DIR}/include")

target_link_libraries (mq_blk_test pthread)

set_target_properties (mq_blk_test PROPERTIES
                       RUNTIME_OUTPUT_DIRECTORY
                       "${CMAKE_BINARY_DIR}/bin"
                       )

add_executable (mq_nblk_test
                "${CMAKE_SOURCE_DIR}/test/mq_nblk_test.cpp")

include_directories ("${CMAKE_SOURCE_DIR}/include")

target_link_libraries (mq_nblk_test pthread)

set_target_properties (mq_nblk_test PROPERTIES
                       RUNTIME_OUTPUT_DIRECTORY
                       "${CMAKE_BINARY_DIR}/bin"
                       )

