solution "Cake"

-- common project settings
	configurations { "Debug", "Release" }
	platforms { "Native", "Universal" }
	flags { "FloatFast" }
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
	   links { "user32", "gdi32", "winmm", "ws2_32" }
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
		
-- hmap2 project
	project "hmap2"
		kind "ConsoleApp"
		language "C"
		targetdir = "../hmap2"
		includedirs { "hmap2" }
		files { "hmap2/*.h", "hmap2/*.c" }

-- QE3 project
	project "QE3"
		kind "WindowedApp"
		language "C"
		targetdir = "../qe3"
		includedirs { "qe3" }

		configuration { "Windows" }
			flags { "WinMain" }
			links { "opengl32", "glu32", "comctl32" }
			files { "qe3/*.h", "qe3/*.c", "qe3/win_qe3.rc" }

-- Cake project
	project "Cake"
		kind "WindowedApp"
		language "C"
		includedirs { "quake" }
		files { "quake/*.h", "quake/*.c" }
		
		configuration { "Windows" }
			flags { "WinMain" }
			files { "quake/win/*.h", "quake/win/*.c", "quake/win/quake.rc" }
			
		configuration {"MacOSX"}
			files { "quake/osx/*.h", "quake/osx/*.c", "quake/osx/*.m" }
		
		configuration {"not Windows", "not MacOSX"}
			files { "quake/linux/*.h", "quake/linux/*.c" }