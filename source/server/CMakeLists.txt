project(${QFUSION_SERVER_NAME})

file(GLOB SERVER_HEADERS
	"*.h"
	"../gameshared/q_*.h"
	"../gameshared/anorms.h"
	"../gameshared/config.h"	
	"../qcommon/*.h"
	"../qalgo/*.h"
	"../angelwrap/qas_public.h"
	"../game/g_public.h"
	"../server/*.h"
	)

file(GLOB SERVER_SOURCES
	"*.cpp"
	"../qcommon/*.cpp"
	"../null/cl_null.cpp"
	"../gameshared/q_*.cpp"
	"../qalgo/*.cpp"
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	file(GLOB SERVER_PLATFORM_SOURCES 
		"../win32/win_fs.cpp"
		"../win32/win_net.cpp"
		"../win32/win_sys.cpp"
		"../win32/win_console.cpp"
		"../win32/win_time.cpp"
		"../win32/win_lib.cpp"
		"../win32/win_threads.cpp"
		"../null/sys_vfs_null.cpp"
		"../win32/conproc.cpp"
		)

	set(SERVER_PLATFORM_LIBRARIES "ws2_32.lib" "winmm.lib")
	set(SERVER_BINARY_TYPE WIN32)
else()
	file(GLOB SERVER_PLATFORM_SOURCES 
		"../unix/unix_fs.cpp"
		"../unix/unix_net.cpp"
		"../unix/unix_sys.cpp"
		"../unix/unix_console.cpp"
		"../unix/unix_time.cpp"
		"../unix/unix_lib.cpp"
		"../unix/unix_threads.cpp"
		"../null/sys_vfs_null.cpp"
		)

	set(SERVER_PLATFORM_LIBRARIES "pthread" "dl" "m")
	set(SERVER_BINARY_TYPE "")
endif()

add_executable(${QFUSION_SERVER_NAME} ${SERVER_BINARY_TYPE} ${SERVER_HEADERS} ${SERVER_SOURCES} ${SERVER_PLATFORM_SOURCES})
add_dependencies(${QFUSION_SERVER_NAME} angelwrap game)
target_link_libraries(${QFUSION_SERVER_NAME} PRIVATE ${CURL_LIBRARY} ${ZLIB_LIBRARY} ${SERVER_PLATFORM_LIBRARIES})
qf_set_output_dir(${QFUSION_SERVER_NAME} "")

set_target_properties(${QFUSION_SERVER_NAME} PROPERTIES COMPILE_DEFINITIONS "DEDICATED_ONLY")
