# Install script for directory: /Users/elena/Documents/qfusion/libsrcs/libfreetype

# Set the install prefix
IF(NOT DEFINED CMAKE_INSTALL_PREFIX)
  SET(CMAKE_INSTALL_PREFIX "/usr/local")
ENDIF(NOT DEFINED CMAKE_INSTALL_PREFIX)
STRING(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
IF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  IF(BUILD_TYPE)
    STRING(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  ELSE(BUILD_TYPE)
    SET(CMAKE_INSTALL_CONFIG_NAME "Release")
  ENDIF(BUILD_TYPE)
  MESSAGE(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
ENDIF(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)

# Set the component getting installed.
IF(NOT CMAKE_INSTALL_COMPONENT)
  IF(COMPONENT)
    MESSAGE(STATUS "Install component: \"${COMPONENT}\"")
    SET(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  ELSE(COMPONENT)
    SET(CMAKE_INSTALL_COMPONENT)
  ENDIF(COMPONENT)
ENDIF(NOT CMAKE_INSTALL_COMPONENT)

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/freetype2" TYPE DIRECTORY FILES "/Users/elena/Documents/qfusion/libsrcs/libfreetype/include/" REGEX "/internal$" EXCLUDE)
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

IF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  IF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/Library/Frameworks" TYPE DIRECTORY FILES "/Users/elena/Documents/qfusion/libsrcs/libfreetype/Debug/freetype.framework" USE_SOURCE_PERMISSIONS)
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "freetype.framework/Versions/A/freetype"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
    ENDIF()
  ELSEIF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/Library/Frameworks" TYPE DIRECTORY FILES "/Users/elena/Documents/qfusion/libsrcs/libfreetype/Release/freetype.framework" USE_SOURCE_PERMISSIONS)
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "freetype.framework/Versions/A/freetype"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
    ENDIF()
  ELSEIF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/Library/Frameworks" TYPE DIRECTORY FILES "/Users/elena/Documents/qfusion/libsrcs/libfreetype/MinSizeRel/freetype.framework" USE_SOURCE_PERMISSIONS)
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "freetype.framework/Versions/A/freetype"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
    ENDIF()
  ELSEIF("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    FILE(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/Library/Frameworks" TYPE DIRECTORY FILES "/Users/elena/Documents/qfusion/libsrcs/libfreetype/RelWithDebInfo/freetype.framework" USE_SOURCE_PERMISSIONS)
    IF(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
      EXECUTE_PROCESS(COMMAND "/usr/bin/install_name_tool"
        -id "freetype.framework/Versions/A/freetype"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Library/Frameworks/freetype.framework/Versions/A/freetype")
    ENDIF()
  ENDIF()
ENDIF(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")

IF(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
ELSE(CMAKE_INSTALL_COMPONENT)
  SET(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
ENDIF(CMAKE_INSTALL_COMPONENT)

FILE(WRITE "/Users/elena/Documents/qfusion/libsrcs/libfreetype/${CMAKE_INSTALL_MANIFEST}" "")
FOREACH(file ${CMAKE_INSTALL_MANIFEST_FILES})
  FILE(APPEND "/Users/elena/Documents/qfusion/libsrcs/libfreetype/${CMAKE_INSTALL_MANIFEST}" "${file}\n")
ENDFOREACH(file)
