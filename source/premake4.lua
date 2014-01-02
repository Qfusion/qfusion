solution "qfusion"
    configurations { "Debug", "Release" }
    platforms { "x32", "x64" }
    
    qf_libs = {
        angelscript = {
            windows = { "angelscript" },
            linux   = { "angelscript" }
        },
        ogg = {
            windows = { "libogg_static" },
            linux   = { "ogg" }
        },
        vorbis = {
            windows = { "libvorbis_static", "libvorbisfile_static" },
            linux   = { "vorbis" }
        },
        theora = {
            windows = { "libtheora_static" },
            linux   = { "theora" } 
        },
        freetype = {
            windows = { "libfreetypestat" },
            linux   = { "freetype" } 
        },
        png = {
            windows = { "libpngstat", "zlibstat" },
            linux   = { "png", "z" } 
        },
        jpeg = {
            windows = { "libjpegstat" },
            linux   = { "jpeg" } 
        },
        curl = {
            windows = { "libcurlstat" },
            linux   = { "curl" }
        },
        z = {
            windows = { "zlibstat" },
            linux   = { "z" }
        }
    }

    function qf_links(libs) 
        local platform_libs = {}

        for i, lib in ipairs(libs) do
            if qf_libs[lib] then
                for j, platform_lib in ipairs(qf_libs[lib][os.get()]) do
                    table.insert(platform_libs, platform_lib)
                end
            else
                table.insert(platform_libs, lib)
            end
        end

        links(platform_libs)
    end

    function qf_targetdir(dir)
        cfg = configuration()
        
        configuration { cfg.terms, "Debug" }
        targetdir ("../build/debug/" .. dir)

        configuration { cfg.terms, "Release" }
        targetdir ("../build/release/" .. dir)

        configuration(cfg.terms)
    end

    configuration "Debug"
        targetdir "build/debug"
        objdir    "build/debug/obj"
        defines   { "_DEBUG", "DEBUG" }
        flags     { "Symbols" }
        
    configuration "Release"
        targetdir "build/release"
        objdir    "build/release/obj"
        defines   { "NDEBUG" }
        flags     { "Optimize" }

    configuration "vs*"
        flags        { "StaticRuntime" }
        targetsuffix "_$(PlatformShortName)"
        defines      { "WIN32", "_WINDOWS", "_CRT_SECURE_NO_WARNINGS", "_SECURE_SCL=0", "CURL_STATICLIB" }

        includedirs {
            "../libsrcs/libogg/include",
            "../libsrcs/libvorbis/include",
            "../libsrcs/libtheora/include",
            "../libsrcs/libcurl/include",
            "../libsrcs/angelscript/sdk/angelscript/include",
            "../libsrcs/zlib",
            "../libsrcs/libfreetype/include",
            "../libsrcs/libpng",
            "../libsrcs/libjpeg",
            "win32/include/msvc",
            "win32/include",
        }

    configuration {"vs*", "Debug"}
        libdirs { "win32/x86/lib/debug" }

    configuration {"vs*", "Release"}
        libdirs { "win32/x86/lib/release" }

    configuration "linux"
        targetprefix ""
        targetsuffix "_x86_64"
        
        includedirs {
            "/usr/include/SDL", -- #include "SDL.h" instead of #include "SDL/SDL.h" in unix_snd.c
            "/usr/include/freetype2",
            "../libsrcs/angelscript/sdk/angelscript/include",
        }

        libdirs {
            "../libsrcs/angelscript/sdk/angelscript/lib" 
        }

    dofile "angelwrap/angelwrap.premake"
    dofile "cgame/cgame.premake"
    dofile "cin/cin.premake"
    dofile "ftlib/ftlib.premake"
    dofile "game/game.premake"
    dofile "irc/irc.premake"
    dofile "ref_gl/ref_gl.premake"
    dofile "snd_openal/snd_openal.premake"
    dofile "snd_qf/snd_qf.premake"
    dofile "tv_server/tv_server.premake"
    dofile "ui/ui.premake"
    dofile "qfusion.premake"
    dofile "qfusion_server.premake"
