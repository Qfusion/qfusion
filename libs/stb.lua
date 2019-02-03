lib( "stb_image", { "libs/stb/stb_image.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_image.cpp", "/wd4244 /wd4456" )
gcc_obj_cxxflags( "libs/stb/stb_image.cpp", "-Wno-shadow -Wno-implicit-fallthrough" )

lib( "stb_image_write", { "libs/stb/stb_image_write.cpp" } )

lib( "stb_vorbis", { "libs/stb/stb_vorbis.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_vorbis.cpp", "/wd4244 /wd4245 /wd4456 /wd4457 /wd4701" )
gcc_obj_cxxflags( "libs/stb/stb_vorbis.cpp", "-Wno-shadow -Wno-unused-value" )
