cmake_minimum_required(VERSION 3.5)
project(libcppproperties VERSION 1.0.0 LANGUAGES CXX)

#Make sure that custom modules like FindXYZ are found
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

##############################################
# Declare dependencies


##############################################
# Create target and set properties
add_library(cppproperties INTERFACE)

# target_sources(cppproperties INTERFACE 
	# include/cppproperties/PropertyContainer.h 
# )
if(MSVC)
    add_custom_target(cppproperties.header SOURCES include/cppproperties/PropertyContainer.h include/cppproperties/PropertyDescriptor.h)
endif()
  # add_library(cppproperties
      # include/cppproperties/PropertyContainer.h
	  # include/cppproperties/PropertyDescriptor.h
  # )



#Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(CppProperties::cppproperties ALIAS cppproperties)

#Set target properties
target_include_directories(cppproperties INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cppproperties INTERFACE cxx_std_17)

 # target_link_libraries(cppproperties
      # PUBLIC
       
      # PRIVATE

 # )

##############################################
# Installation instructions
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/CppProperties)
install(TARGETS cppproperties
    EXPORT cppproperties-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
 )

#This is required so that the exported target has the name CppProperties and not cppproperties
set_target_properties(cppproperties PROPERTIES 
	EXPORT_NAME CppProperties
)

#Export the targets to a script
install(EXPORT cppproperties-targets
  FILE
    CppPropertiesTargets.cmake
  NAMESPACE
    CppProperties::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

#Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CppPropertiesConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/CppPropertiesConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CppPropertiesConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

#Install the config, configversion and custom find modules
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CppPropertiesConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CppPropertiesConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

##############################################
## Exporting from the build tree

export(EXPORT cppproperties-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/CppPropertiesTargets.cmake NAMESPACE CppProperties::)

#Register package in user's package registry
export(PACKAGE CppProperties)

add_subdirectory(test)
