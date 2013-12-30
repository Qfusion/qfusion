project "irc"

    kind     "SharedLib"
    language "C"
    
    files    { 
        "*.h",
        "../gameshared/q_shared.h",
        "*.c",
        "../gameshared/q_shared.c",
    }

    configuration "windows"
        links { "ws2_32" }

    configuration "macosx"