workspace "GTA3Archipelago"
    configurations { "Debug", "Release" }
    platforms { "x86" }
    location "build"
    startproject "GTA3Archipelago"

project "GTA3Archipelago"
    kind "SharedLib"
    language "C++"
    architecture "x86"
    characterset "MBCS"
    cppdialect "C++17"
    staticruntime "on"
    targetname "GTA3Archipelago"
    targetextension ".asi"
    flags { "NoPCH", "NoImportLib" }
    largeaddressaware "on"
    linkoptions "/SAFESEH:NO"
    links { "Ws2_32" }

    files { "src/**.h", "src/**.cpp", "third_party/minhook/src/**.c" }
    removefiles { "third_party/minhook/src/hde/hde64.c" }
    includedirs { "src" }
    defines { "_CRT_SECURE_NO_WARNINGS", "WIN32_LEAN_AND_MEAN", "NOMINMAX" }

    targetdir "output/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"

    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        symbols "On"
        runtime "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        symbols "On"
        optimize "Speed"
        runtime "Release"

-- Test projects exist only in local checkouts; the published repository builds the ASI alone.
if os.isdir("tests") then
-- Test engine: fake GTA III and fake GTA3 Client around the src/ code, without dllmain.
project "AsiHost"
    kind "ConsoleApp"
    language "C++"
    architecture "x86"
    cppdialect "C++17"
    staticruntime "on"
    files { "tests/AsiHost.cpp" }
    targetdir "output/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}/AsiHost"

project "GTA3ArchipelagoTests"
    kind "ConsoleApp"
    language "C++"
    architecture "x86"
    characterset "MBCS"
    cppdialect "C++17"
    staticruntime "on"
    largeaddressaware "on"
    links { "Ws2_32" }

    files { "src/**.h", "src/**.cpp", "tests/unit/**.h", "tests/unit/**.cpp" }
    removefiles { "src/dllmain.cpp" }
    removefiles { "src/GameRuntime.cpp" }
    removefiles { "src/UIHooks.cpp" }
    removefiles { "src/opcode/NativeExecutor.cpp" }
    includedirs { "src", "tests/unit" }
    defines { "_CRT_SECURE_NO_WARNINGS", "WIN32_LEAN_AND_MEAN", "NOMINMAX" }

    targetdir "output/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}/Tests"

    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        symbols "On"
        runtime "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        symbols "On"
        optimize "Speed"
        runtime "Release"
end
