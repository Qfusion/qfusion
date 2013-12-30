project "angelwrap"

    kind         "SharedLib"
    language     "C++"
    qf_targetdir "libs"
    
    files    { 
        "*.h",
        "addon/*.h",
        "../gameshared/q_angeliface.h",
        "../gameshared/q_math.h",
        "../gameshared/q_shared.h",
        "*.cpp",
        "addon/*.cpp",
        "../gameshared/q_math.c",
        "../gameshared/q_shared.c",
    }

    qf_links { "angelscript" }