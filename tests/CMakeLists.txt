if(WIN32)
  set(GTEST_LIBS "gtest gtest_main")
else()
  set(GTEST_LIBS "gtest gtest_main pthread")
endif()

project(BufferTests)
add_executable(BufferTests IOBufferingUnitTests.cpp)
target_include_directories(BufferTests PRIVATE ${CMAKE_SOURCE_DIR}/src $ENV{GTEST_ROOT}/googletest/include)

if(WIN32)
  target_link_directories(BufferTests PUBLIC $ENV{GTEST_ROOT}/lib/${CMAKE_BUILD_TYPE})
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
else()
  target_link_directories(BufferTests PUBLIC $ENV{GTEST_ROOT}/lib)
endif()

target_link_libraries(BufferTests gtest.lib gtest_main.lib)