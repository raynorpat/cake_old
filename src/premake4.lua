solution "Quake"

-- common project settings
	configurations { "Debug", "Release" }
	platforms { "Native", "Universal" }
	flags { "ExtraWarnings", "FloatFast" }
	location "build"
	targetdir = "../.."
	
-- setup the build configs
    configuration "Debug"
        defines { "DEBUG" }
        flags { "Symbols" }
 
    configuration "Release"
        defines { "NDEBUG" }
        flags { "Optimize" }
		
	configuration "windows"
	   links { "user32", "gdi32", "winmm", "ws2_32", "dxguid" }
	configuration "linux"
	   links { "m" }	 
	configuration "macosx"
	   links { "Cocoa.framework" }		

-------------------------------------------

-- QCC project
	project "QCC"
		kind "ConsoleApp"
		language "C++"
		targetdir = "../qc"
		includedirs { "qcc" }
		files { "qcc/*.h", "qcc/*.cpp" }
	
-- Quake project
	project "Quake"
		kind "WindowedApp"
		language "C"
		includedirs { "quake" }
		files { "quake/*.h", "quake/*.c" }
		
		configuration { "Windows" }
			flags { "WinMain" }
			files { "quake/win/*.h", "quake/win/*.c", "quake/win/quake.rc" }
			
		configuration {"MacOSX"}
			files { "quake/osx/*.h", "quake/osx/*.c" }
		
		configuration {"not Windows", "not MacOSX"}
			files { "quake/linux/*.h", "quake/linux/*.c" }
