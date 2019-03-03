get_filename_component(CppProperties_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

list(APPEND CMAKE_MODULE_PATH ${CppProperties_CMAKE_DIR})

# NOTE Had to use find_package because find_dependency does not support COMPONENTS or MODULE until 3.8.0

list(REMOVE_AT CMAKE_MODULE_PATH -1)

if(NOT TARGET CppProperties::cppproperties)
    include("${CppProperties_CMAKE_DIR}/CppPropertiesTargets.cmake")
endif()

set(CppProperties_LIBRARIES CppProperties::cppproperties)
