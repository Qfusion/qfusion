lib( "stb_image", { "libs/stb/stb_image.cpp" } )
msvc_obj_cxxflags( "libs/stb/stb_image", "/wd4244 /wd4456" )
gcc_obj_cxxflags( "libs/stb/stb_image", "-Wno-shadow -Wno-implicit-fallthrough" )

lib( "stb_image_write", { "libs/stb/stb_image_write.cpp" } )

lib( "stb_vorbis", { "libs/stb/stb_vorbis.cpp" } )
gcc_obj_cxxflags( "libs/stb/stb_image", "-Wno-shadow -Wno-unused-value" )
