cmake_minimum_required(VERSION 2.6)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")
#include(FindBoost)

# following causes warning in 2.6 - ignore as per
# http://www.linuxquestions.org/questions/linux-software-2/findpkgconfig-vs.-pkgconfig-716440/
#include(UsePkgConfig)
#PKGCONFIG(libxml++-2.6 LIBXML_INCLUDE_DIRS LIBXML_LIB_DIRS LIBXML_LINK_FLAGS LIBXML_CFLAGS)

find_package(Boost REQUIRED)
find_package(Glib REQUIRED)
find_package(LibXML2 REQUIRED)
find_package(LibXML++ REQUIRED)
find_package(Jack REQUIRED)
# TODO could do with a proper check for liblo here
include_directories(${LibXML++_INCLUDE_DIRS} ${JACK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LibXML++_LIBRARIES} ${JACK_LIBRARIES} lo boost_program_options sndfile)



# use dbuggin flags
IF(UNIX)
	SET(CMAKE_CXX_FLAGS "-g -Wall")
ELSEIF(APPLE)
	SET(CMAKE_CXX_FLAGS "-g -Wall")
ELSE(UNIX)
ENDIF(UNIX)

add_executable(resoundnv-server core.cpp jackengine.cpp oscmanager.cpp dsp.cpp)
target_link_libraries(resoundnv-server ${LIBS})

add_executable(resoundnv-calibrate resoundnv_cal.cpp)
target_link_libraries(resoundnv-calibrate ${LIBS})
