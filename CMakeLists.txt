#To configure CMake, use a command like:
# config: cmake -DCMAKE_PREFIX_PATH=/path/to/prefix ..
# or to customize a little more: cmake -DWt_DIR=/path/to/wt/prefix/lib/cmake/wt/ -DBoost_INCLUDE_DIR=/path/to/boost/prefix/include ..


cmake_minimum_required( VERSION 3.10 )
project( FullSpectrumId )


option( STATICALLY_LINK_TO_GADRAS "Select weather to statically or dynamically load GADRAS" OFF )
option( FOR_WEB_DEPLOYMENT "Whether for web-development, or local development" OFF )
option( ENABLE_SESSION_DETAIL_LOGGING "Enable creating a separate log directory for each user session" OFF )
option( USE_MINIFIED_JS_CSS "Whether to use the minified JS/CSS from this project" OFF )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

if( WIN32 )
  set(MSVC_RUNTIME "static")
  include(cmake/ConfigureMsvc.txt)
  configure_msvc_runtime()
  set( Boost_USE_STATIC_RUNTIME ON )
else( WIN32 )
  if( NOT DEFINED Boost_USE_STATIC_RUNTIME )
    set( Boost_USE_STATIC_RUNTIME OFF )
  endif( NOT DEFINED Boost_USE_STATIC_RUNTIME )
endif( WIN32 )

set( Boost_USE_STATIC_LIBS ON )
set( Boost_USE_MULTITHREADED ON )
find_package( Boost REQUIRED COMPONENTS thread filesystem program_options )

find_package( Wt REQUIRED COMPONENTS Wt HTTP )

set( SpecUtils_REBIN_FILES_TO_SINGLE_BINNING OFF CACHE BOOL "" )
set( SpecUtils_NO_BOOST_LIB OFF CACHE BOOL "" )
set( SpecUtils_USE_WT_THREADPOOL OFF CACHE BOOL "")  #TODO: Enable this without requiring FindWt.cmake
set( SpecUtils_EXTERNALLY_DEFINED_LOG_MESSAGE ON CACHE BOOL "")
set( SpecUtils_BUILD_UNIT_TESTS OFF CACHE BOOL "")
set( SpecUtils_BUILD_REGRESSION_TEST OFF CACHE BOOL "")
set( SpecUtils_PYTHON_BINDINGS OFF CACHE BOOL "")
set( SpecUtils_JAVA_SWIG OFF CACHE BOOL "")
set( SpecUtils_D3_SUPPORT_FILE_STATIC OFF CACHE BOOL "")
set( SpecUtils_D3_SCRIPTS_RUNTIME_DIR "web_assets" CACHE STRING "" )
add_subdirectory( 3rd_party/SpecUtils )


add_executable( full-spec
  main.cpp
  FullSpectrumId/FullSpectrumApp.h
  src/FullSpectrumApp.cpp
  FullSpectrumId/Analysis.h
  src/Analysis.cpp
  FullSpectrumId/AnalysisGui.h
  src/AnalysisGui.cpp
  FullSpectrumId/D3SpectrumDisplayDiv.h
  src/D3SpectrumDisplayDiv.cpp
  FullSpectrumId/D3TimeChart.h
  src/D3TimeChart.cpp
  web_assets/D3TimeChart.js
  FullSpectrumId/SampleSelect.h
  src/SampleSelect.cpp
  FullSpectrumId/EnergyCal.h
  src/EnergyCal.cpp
  src/RestResources.cpp
  FullSpectrumId/RestResources.h
  src/AnalysisFromFiles.cpp
  FullSpectrumId/AnalysisFromFiles.h
  src/AppUtils.cpp
  FullSpectrumId/AppUtils.h
  src/CommandLineAna.cpp
  FullSpectrumId/CommandLineAna.h
  src/SimpleDialog.cpp
  FullSpectrumId/SimpleDialog.h
  FullSpectrumId/FullSpectrumId_config.h.in
)


 target_link_libraries( full-spec PUBLIC
   SpecUtils
   Wt::Wt
   Wt::HTTP
   Boost::filesystem
   Boost::program_options
   ${CMAKE_DL_LIBS}
)


#set( GADRAS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/Gadras/v19.1.1/GadrasIsotopeID" )
#set( GADRAS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/Gadras/v19.2.3/GadrasIsotopeID" )
set( GADRAS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/Gadras/v18.8.11/GadrasIsotopeID" )

if( WIN32 )
  target_link_libraries( full-spec PRIVATE Bcrypt.lib )
endif( WIN32 )

# Right now:
#  - On linux we can dynamically or statically use the GADRAS library.
#  - On Windows we dont have a .lib file, so will dynamically load "libgadrasiid.dll" at run
#    time and resolve functions then
#  - On macOS we will also dynamically load "libgadrasiid_x86_64.dylib" and resolve functions
if( STATICALLY_LINK_TO_GADRAS )
  add_library( gadraslib STATIC IMPORTED )
  
  if( ("${CMAKE_SYSTEM}" MATCHES "Linux") )
    #set( GADRAS_LIB_LOCATION "${GADRAS_DIR}/libs/ubuntu/x86_64/staticfpic/5.4.0/libgadrasiid.a" )
    set( GADRAS_LIB_LOCATION "${GADRAS_DIR}/libs/ubuntu/staticfpic/libgadrasiid_x86_64.a" )
  else( ("${CMAKE_SYSTEM}" MATCHES "Linux") )
    message( FATAL "Only Linux supports static linking to the GADRAS library" )
  endif( ("${CMAKE_SYSTEM}" MATCHES "Linux") )
  
  set_target_properties( gadraslib PROPERTIES
    IMPORTED_LOCATION ${GADRAS_LIB_LOCATION}
    INTERFACE_INCLUDE_DIRECTORIES  "${GADRAS_DIR}/docs"
  )
  
  #if( WIN32 )
  #  target_link_libraries( full-spec PRIVATE gadraslib )
  #else( WIN32 )
    # TODO: I'm not yet totally set on how to best link to gfortran or whatever
    # set( GCC_LIB_DIR "/usr/lib/gcc/x86_64-linux-gnu/7/" )
    # target_link_libraries( full-spec PRIVATE ${GCC_LIB_DIR}/libgfortran.a ${GCC_LIB_DIR}/libquadmath.a )
  
    target_link_libraries( full-spec PRIVATE gadraslib
      -static-libgcc -Wl,-Bstatic -lstdc++ -lgfortran -lquadmath -Wl,-Bdynamic
    )
  #endif( WIN32 )
else( STATICALLY_LINK_TO_GADRAS )
  # Windows or macOS: add Gadras header in so will be available in the IDE
  list( APPEND OTHER_SUPPORT_FILES "${GADRAS_DIR}/docs/GadrasIsotopeID.h" )
  
  # Allow including the GADRAS header (we need the IsotopeIDResult struct at least)
  target_include_directories( full-spec PUBLIC "${GADRAS_DIR}/docs/" )
  
  if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
  # TODO: maybe check adding '-static-libstdc++', but maybe not necassary
    target_link_libraries( full-spec PRIVATE -static-libgcc -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic )
  endif( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
  
  if( ("${CMAKE_SYSTEM}" MATCHES "Linux") )
    list( APPEND GADRAS_LIB_LOCATION "${GADRAS_DIR}GadrasIsotopeID/libs/ubuntu/x86_64/shared/7.5.0/libgadrasiid.so" )
    list( APPEND GADRAS_LIB_LOCATION "${GADRAS_DIR}GadrasIsotopeID/libs/ubuntu/x86_64/shared/7.5.0/libfileio.so" )
    list( APPEND GADRAS_LIB_LOCATION "${GADRAS_DIR}GadrasIsotopeID/libs/ubuntu/x86_64/shared/7.5.0/libphotonxsec.so" )
  elseif( WIN32 )
    set( GADRAS_LIB_LOCATION "${GADRAS_DIR}/libs/win64/shared/libgadrasiid.dll" )
  else()
    list( APPEND GADRAS_LIB_LOCATION "${GADRAS_DIR}/libs/macOS/gcc10.2/libgadrasiid.dylib" )
    list( APPEND GADRAS_LIB_LOCATION "${GADRAS_DIR}/libs/macOS/gcc10.2/libfileio.dylib" )
    list( APPEND GADRAS_LIB_LOCATION "${GADRAS_DIR}/libs/macOS/gcc10.2/libphotonxsec.dylib" )
  endif()
  
endif( STATICALLY_LINK_TO_GADRAS )


configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/FullSpectrumId/FullSpectrumId_config.h.in
    ${CMAKE_BINARY_DIR}/FullSpectrumId_config.h
)

target_include_directories( full-spec PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
)


# Set the Xcode working directory to the build directory - requires CMake 3.17
if( CMAKE_GENERATOR STREQUAL "Xcode" )
  set_target_properties( full-spec PROPERTIES
    XCODE_GENERATE_SCHEME TRUE
    XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif( CMAKE_GENERATOR STREQUAL "Xcode" )


if( NOT Wt_VERSION STREQUAL "4.4.0" )
  message( WARNING "The web_assets/resources folder is from Wt 4.4.0, and you are using ${Wt_VERSION}.  You should consider upgrading resources." )
endif( NOT Wt_VERSION STREQUAL "4.4.0" )

# Copy the Wt JS, css and image resources over to build directory
#if( NOT WT_RESOURCES_DIRECTORY )
#  find_path( WT_RESOURCES_DIRECTORY form.css ${Boost_INCLUDE_DIRS}/../share/Wt/resources )
#endif( NOT WT_RESOURCES_DIRECTORY )
#if( WT_RESOURCES_DIRECTORY )
#   file( COPY ${WT_RESOURCES_DIRECTORY} DESTINATION ${PROJECT_BINARY_DIR}/ )
#endif( WT_RESOURCES_DIRECTORY )

list( APPEND OTHER_SUPPORT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${CMAKE_CURRENT_SOURCE_DIR}/TODO.md )

file( GLOB CSS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/web_assets/*" )
file( GLOB_RECURSE CONFIG_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/*" )

list( APPEND OTHER_SUPPORT_FILES "${CSS_FILES}" "${CONFIG_FILES}" )

if( WIN32 )
#        file(COPY ${PROJECT_SOURCE_DIR}/${_dir} DESTINATION ${PROJECT_BINARY_DIR}/ )
else( WIN32 )
  execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink
         ${PROJECT_SOURCE_DIR}/web_assets
         ${PROJECT_BINARY_DIR}/web_assets
      )
endif( WIN32 )


add_custom_target( support_files SOURCES ${OTHER_SUPPORT_FILES} )

if( USE_MINIFIED_JS_CSS )
  include(cmake/MinimizeJsAndCss.cmake)
  minimize_js_resource("${PROJECT_SOURCE_DIR}/web_assets/D3TimeChart.js" "${PROJECT_SOURCE_DIR}/web_assets/D3TimeChart.min.js" )
  minimize_js_resource("${PROJECT_SOURCE_DIR}/web_assets/SpectrumChartD3.js" "${PROJECT_SOURCE_DIR}/web_assets/SpectrumChartD3.min.js" )
  minimize_css_resource("${PROJECT_SOURCE_DIR}/web_assets/AnalysisGui.css" "${PROJECT_SOURCE_DIR}/web_assets/AnalysisGui.min.css" )
  minimize_css_resource("${PROJECT_SOURCE_DIR}/web_assets/SpectrumChartD3.css" "${PROJECT_SOURCE_DIR}/web_assets/SpectrumChartD3.min.css" )
  minimize_css_resource("${PROJECT_SOURCE_DIR}/web_assets/D3TimeChart.css" "${PROJECT_SOURCE_DIR}/web_assets/D3TimeChart.min.css" )
  minimize_css_resource("${PROJECT_SOURCE_DIR}/web_assets/FullSpectrumApp.css" "${PROJECT_SOURCE_DIR}/web_assets/FullSpectrumApp.min.css" )
  minimize_css_resource("${PROJECT_SOURCE_DIR}/web_assets/SampleSelect.css" "${PROJECT_SOURCE_DIR}/web_assets/SampleSelect.min.css" )
  minimize_css_resource("${PROJECT_SOURCE_DIR}/web_assets/SimpleDialog.css" "${PROJECT_SOURCE_DIR}/web_assets/SimpleDialog.min.css" )
endif( USE_MINIFIED_JS_CSS )

# Install the EXE
install( TARGETS full-spec DESTINATION . )

# Install the web assets
install( DIRECTORY web_assets DESTINATION . )

# We will use DRFs from v19.1.1, since these are most complete and are actually all compatible with v18.11
install( DIRECTORY "${GADRAS_DIR}/gadras_isotope_id_run_directory" DESTINATION . )

# Config files; but take out local_dev so we dont actually use that in production
install( DIRECTORY config DESTINATION .
  PATTERN "wt_config_local_dev.xml" EXCLUDE
  PATTERN "app_config_local.ini" EXCLUDE
  PERMISSIONS WORLD_READ
)

# And if we arent statically linking, copy over the GADRAS shared library to load at run time.
if( NOT STATICALLY_LINK_TO_GADRAS )
  install( FILES "${GADRAS_LIB_LOCATION}" DESTINATION . )
endif( NOT STATICALLY_LINK_TO_GADRAS )
