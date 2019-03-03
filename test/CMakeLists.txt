#FetchContent is only available for version 3.14 and upwards and we need it to get googletest
if(${CMAKE_VERSION} VERSION_LESS "3.14.0") 
    include(FetchContent)
endif()

set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll" FORCE)
# Download and unpack googletest at configure time
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        master
)
# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
FetchContent_MakeAvailable(googletest)

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(CppProperties_test)
target_sources(CppProperties_test PRIVATE src/PropertyContainerHierarchy.cpp src/PropertyContainerTests.cpp src/PropertyTests.cpp src/ProxyPropertyTests.cpp)
target_link_libraries(CppProperties_test gtest gtest_main CppProperties::cppproperties)

# add our tests automatically to ctest which makes them discoverable by IDE's like MSVS
include(GoogleTest)
gtest_discover_tests(CppProperties_test)