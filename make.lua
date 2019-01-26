require( "ggbuild.gen_ninja" )
require( "ggbuild.git_version" )

require( "libs.imgui" )

obj_cxxflags( ".*", "-I source -I libs" )

msvc_obj_cxxflags( ".*", "/W4 /wd4100 /wd4146 /wd4189 /wd4201 /wd4324 /wd4351 /wd4127 /wd4505 /wd4530 /wd4702 /D_CRT_SECURE_NO_WARNINGS" )
gcc_obj_cxxflags( ".*", "-std=c++11 -static-libstdc++ -msse2 -ffast-math -fno-strict-aliasing -fno-strict-overflow -fvisibility=hidden" )
gcc_obj_cxxflags( ".*", "-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wshadow -Wcast-align -Wvla -Wformat-security" ) -- -Wconversion

obj_cxxflags( ".*", "-DCURL_STATICLIB -D_LIBCPP_TYPE_TRAITS" )

if config == "release" then
	obj_cxxflags( ".*", "-DMICROPROFILE_ENABLED=0" )
end

do
	local platform_srcs
	local platform_libs

	if OS == "windows" then
		platform_srcs = {
			"source/win32/win_client.cpp",
			"source/win32/win_console.cpp",
			"source/win32/win_fs.cpp",
			"source/win32/win_net.cpp",
			"source/win32/win_threads.cpp",
		}
		platform_libs = { }
	else
		platform_srcs = {
			"source/unix/unix_console.cpp",
			"source/unix/unix_fs.cpp",
			"source/unix/unix_net.cpp",
			"source/unix/unix_threads.cpp",
		}
		platform_libs = { "mbedtls" }
	end

	bin( "client", {
		srcs = {
			"source/cgame/*.cpp",
			"source/client/**.cpp",
			"source/gameshared/**.cpp",
			"source/qalgo/*.cpp",
			"source/qcommon/*.cpp",
			"source/server/sv_*.cpp",
			platform_srcs
		},

		libs = { "imgui" },

		prebuilt_libs = {
			"angelscript",
			"curl",
			"freetype",
			"openal",
			"sdl",
			"zlib",
			platform_libs
		},

		rc = "source/win32/client",

		gcc_extra_ldflags = "-lm -lpthread -ldl -no-pie -static-libstdc++",
		msvc_extra_ldflags = "gdi32.lib ole32.lib oleaut32.lib ws2_32.lib crypt32.lib winmm.lib version.lib imm32.lib /link /SUBSYSTEM:WINDOWS",
	} )

	obj_cxxflags( "source/client/ftlib/.+", "-I libs/freetype" )
	obj_cxxflags( "source/client/sound/.+", "-DAL_LIBTYPE_STATIC" )
end

do
	local platform_srcs
	local platform_libs

	if OS == "windows" then
		platform_srcs = {
			"source/win32/win_console.cpp",
			"source/win32/win_fs.cpp",
			"source/win32/win_lib.cpp",
			"source/win32/win_net.cpp",
			"source/win32/win_server.cpp",
			"source/win32/win_threads.cpp",
			"source/win32/win_time.cpp",
		}
		platform_libs = { }
	else
		platform_srcs = {
			"source/unix/unix_console.cpp",
			"source/unix/unix_fs.cpp",
			"source/unix/unix_lib.cpp",
			"source/unix/unix_net.cpp",
			"source/unix/unix_server.cpp",
			"source/unix/unix_threads.cpp",
			"source/unix/unix_time.cpp",
		}
		platform_libs = { "mbedtls" }
	end

	bin( "server", {
		srcs = {
			"source/gameshared/angelwrap/**.cpp",
			"source/gameshared/q_*.cpp",
			"source/qalgo/*.cpp",
			"source/qcommon/*.cpp",
			"source/server/*.cpp",
			platform_srcs
		},

		prebuilt_libs = {
			"angelscript",
			"curl",
			"zlib",
			platform_libs
		},

		gcc_extra_ldflags = "-lm -lpthread -ldl -no-pie -static-libstdc++",
		msvc_extra_ldflags = "ws2_32.lib crypt32.lib",
	} )
end

dll( "game", {
	"source/game/*.cpp",
	"source/gameshared/*.cpp",
	"source/qalgo/hash.cpp",
	"source/qalgo/rng.cpp",
} )

obj_cxxflags( "source/gameshared/angelwrap/.+", "-I third-party/angelscript/sdk/angelscript/include" )
obj_cxxflags( "source/.+_as_.+", "-I third-party/angelscript/sdk/angelscript/include" )
obj_cxxflags( "source/.+_ascript.cpp", "-I third-party/angelscript/sdk/angelscript/include" )
