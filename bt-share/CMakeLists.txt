CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(bluetooth-share C)

SET(SRCS src/bt-share-main.c
src/bt-share-notification.c
src/bt-share-common.c
src/obex-event-handler.c
src/bt-share-syspopup.c
src/bt-share-ipc.c
src/bt-share-noti-handler.c
)


INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/lib)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED glib-2.0 bluetooth-api
				aul dbus-glib-1 appcore-common
				dlog vconf syspopup-caller pmapi sysman
				notification calendar-service2
				appsvc appcore-efl libprivilege-control
				capi-appfw-application
				capi-content-media-content)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
#SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -finstrument-functions")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -pie")
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(BINDIR "/usr/bin")
SET(EXEC_PREFIX "\${prefix}")
#SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "/usr/include")
SET(VERSION 1.0)

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")
ADD_DEFINITIONS("-DTARGET")
ADD_DEFINITIONS("-DSLP_DEBUG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} -L${CMAKE_CURRENT_SOURCE_DIR}/../lib -lbluetooth-share-api)


INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BINDIR})
INSTALL(FILES ${CMAKE_SOURCE_DIR}/bluetooth-share.rule DESTINATION /opt/etc/smack/accesses.d)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/org.bluetooth.share.service DESTINATION share/dbus-1/services)
