#/*=========================================================================
#Date: Ag 2015
#Authors include:
#- Eugenio Marinetto [*][ç] emarinetto@hggm.es
#- Laura Sanz [*] lsanz@hggm.es
#- Javier Pascau [*][ç] jpascau@hggm.es
#[*] Laboratorio de Imagen Medica, Hospital Gregorio Maranon - http://image.hggm.es/
#[ç] Departamento de Bioingeniería e Ingeniería Aeroespacial. Universidad Carlos III de Madrid
#=========================================================================*/

IF(NPTrackingTools_DIR)

  # OpenIGTLink has been built already
  FIND_PACKAGE(NPTrackingTools REQUIRED PATHS ${NPTrackingTools_DIR} NO_DEFAULT_PATH)

  message("NPTrackingTools_LIBRARY_DIRS: " ${NPTrackingTools_LIBRARY_DIRS})

  MESSAGE(STATUS "Using NPTrackingTools available at: ${NPTrackingTools_DIR}")

  # Copy libraries to PLUS_EXECUTABLE_OUTPUT_PATH
  IF ( ${CMAKE_GENERATOR} MATCHES "Visual Studio" )
    FILE(COPY
      ${NPTrackingTools_LIBRARY_DIRS}/Release/
      DESTINATION ${PLUS_EXECUTABLE_OUTPUT_PATH}/Release
      FILES_MATCHING REGEX .*${CMAKE_SHARED_LIBRARY_SUFFIX}
      )
    FILE(COPY
      ${NPTrackingTools_LIBRARY_DIRS}/Debug/
      DESTINATION ${PLUS_EXECUTABLE_OUTPUT_PATH}/Debug
      FILES_MATCHING REGEX .*${CMAKE_SHARED_LIBRARY_SUFFIX}
      )
  ELSE()
      FILE(COPY
        ${NPTrackingTools_LIBRARY_DIRS}/
        DESTINATION ${PLUS_EXECUTABLE_OUTPUT_PATH}
        FILES_MATCHING REGEX .*${CMAKE_SHARED_LIBRARY_SUFFIX}
        )
  ENDIF()
  SET (PLUS_NPTrackingTools_DIR "${NPTrackingTools_DIR}" CACHE INTERNAL "Path to store NPTrackingTools binaries")
ELSE(NPTrackingTools_DIR)

  # NPTrackingTools has not been built yet, so download and build it as an external project
  SET (PLUS_NPTrackingTools_SRC_DIR "${CMAKE_BINARY_DIR}/NPTrackingTools")
  SET (PLUS_NPTrackingTools_DIR "${CMAKE_BINARY_DIR}/NPTrackingTools-bin" CACHE INTERNAL "Path to store NPTrackingTools binaries")
  ExternalProject_Add( NPTrackingTools
    SOURCE_DIR "${PLUS_NPTrackingTools_SRC_DIR}"
    BINARY_DIR "${PLUS_NPTrackingTools_DIR}"
    #--Download step--------------
    GIT_REPOSITORY "${GIT_PROTOCOL}://github.com/nenetto/NPTrackingTools"
    GIT_TAG "PLUS-v1.0"
    #--Configure step-------------
    CMAKE_ARGS
        ${ep_common_args}
        -DLIBRARY_OUTPUT_PATH:STRING=${PLUS_EXECUTABLE_OUTPUT_PATH}
        -DBUILD_SHARED_LIBS:BOOL=${PLUSBUILD_BUILD_SHARED_LIBS}
        -DBUILD_EXAMPLES:BOOL=OFF
        -DBUILD_TESTING:BOOL=OFF
        -DNPTrackingTools_USE_FAKE:BOOL=OFF
        -DUSE_CalibrationValidation:BOOL=OFF
        -DUSE_IGTLinkPython:BOOL=OFF
        -DUSE_Python:BOOL=OFF
        -DUSE_OptitrackClient:BOOL=OFF
        -DUSE_OptitrackServer:BOOL=OFF
        -DUSE_TestOptitrack:BOOL=OFF
        -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
        -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
        -DNPTrackingTools_OpenIGTLink_EXTERNAL_DIR:PATH=${PLUS_OpenIGTLink_DIR}
    #--Build step-----------------
    #--Install step-----------------
    INSTALL_COMMAND ""
    DEPENDS ${NPTrackingTools_DEPENDENCIES}
    )

ENDIF(NPTrackingTools_DIR)