CMAKE_MINIMUM_REQUIRED(VERSION 3.17.0)

PROJECT(Clock)

# Checks for packages
FIND_PACKAGE(GTK2 2.24 REQUIRED gtk)

# Checks for library functions.

# Check for ROX-CLib and version
SET(PLATFORM $ENV{PLATFORM})
FIND_PROGRAM(ROX_RUN rox_run PATHS ENV APP_DIR)
FIND_PROGRAM(LIBDIR libdir PATHS ENV APP_DIR)
EXECUTE_PROCESS(COMMAND ${LIBDIR} --version 2.1.10 ROX-CLib
			OUTPUT_VARIABLE ROXCLib_DIR
			OUTPUT_STRIP_TRAILING_WHITESPACE
)
SET(ROXCLib_DIR "${ROXCLib_DIR}/${PLATFORM}")
SET(ROXCLib_INCLUDE_DIRS "${ROXCLib_DIR}/include")
FIND_LIBRARY(ROXCLib_LIBRARIES
    NAMES librox-clib rox-clib
    PATHS "${ROXCLib_DIR}/${PLATFORM}/lib" "${ROXCLib_DIR}/lib"
)

# Is altzone defined in <time.h> ?

# Additional libraries
FIND_PACKAGE(LibXml2 REQUIRED)

# Generated config.h
CONFIGURE_FILE(config.h.in config.h)

# Header files
SET(HEADERS 
    alarm.h
    extra.h
)

# Source files 
SET(SOURCES
    clock.c
    alarm.c
)

ADD_EXECUTABLE(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    ${ROXCLib_INCLUDE_DIRS}
    ${GTK2_INCLUDE_DIRS}
    LibXml2::LibXml2
)

TARGET_LINK_LIBRARIES(${PROJECT_NAME}
    ${ROXCLib_LIBRARIES}
    ${GTK2_LIBRARIES}
    LibXml2::LibXml2
    m
)

INSTALL(TARGETS ${PROJECT_NAME}
)