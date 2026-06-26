workspace "MissionSelector"
    configurations { "Release", "Debug" }
    platforms { "x86" }
    location ( "build" )
    startproject "MissionSelector"

project "MissionSelector"
    kind "SharedLib"
    language "C++"
    targetextension ".asi"
    targetname "III.MissionSelector"
    architecture "x86"
    characterset "MBCS"
    cppdialect "C++17"
    flags { "NoPCH", "NoImportLib" }
    staticruntime "on"
    largeaddressaware "on"
    linkoptions "/SAFESEH:NO"

    files {
        "src/**.h",
        "src/**.cpp",
        "lib/minhook/include/MinHook.h",
        "lib/minhook/src/**.h",
        "lib/minhook/src/**.c",
    }

    includedirs {
        "src/",
        "lib/minhook/include/",
    }

    defines {
        "_CRT_SECURE_NO_WARNINGS",
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX",
    }

    targetdir "output/%{cfg.buildcfg}/"
    objdir    "build/obj/%{cfg.buildcfg}/"

    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        symbols "On"
        runtime "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        symbols "On"
        optimize "Speed"
        runtime "Release"
