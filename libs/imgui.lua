lib( "imgui", { "libs/imgui/*.cpp" } )

obj_cxxflags( "libs/imgui/imgui_freetype.cpp", "-I libs/freetype" )
obj_cxxflags( "libs/imgui/imgui_impl_opengl3.cpp", "-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\\\"client/renderer/glad.h\\\"" )
