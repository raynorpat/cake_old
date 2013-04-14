package.name     = "Quake"
package.language = "c"
package.kind     = "exe"
package.target   = "quake"

-- Build Flags / Defines

	package.buildflags = 
	{ 
		"no-64bit-checks",
		"static-runtime",
		"extra-warnings",
	}

	package.config["Release"].buildflags = 
	{ 
		"no-symbols", 
		"optimize-size",
		"no-frame-pointers",
	}

	if (OS == "windows") then
		package.kind = "winexe"

		package.buildflags = { 
			"no-main",
		}
		
		package.defines = { 
			"_CRT_SECURE_NO_DEPRECATE",
			"WIN32",
		}
	end

-- Libraries

	if (OS == "linux") then
		package.links = { "m" }
	elseif (OS == "windows") then
		package.links = { "user32", "gdi32", "winmm", "ws2_32", "dxguid" }
	end


-- Files

	package.files = {
		matchfiles("*.h", "*.c")
	}

	if (OS == "linux") then
		package.excludes = {
			"cd_win.c",
			"net_wins.c",
			"snd_osx.c",
			"snd_win.c",
			"sys_win.c",
			"vid_win.c"
		}
	elseif (OS == "windows") then
		package.excludes = {
			"cd_linux.c",
			"net_udp.c",
			"snd_alsa.c",
			"snd_oss.c",
			"snd_linux.c",
			"snd_osx.c",
			"sys_linux.c",
			"vid_glx.c"
		}
	else
		package.files =	{
			matchrecursive("null/*.c")
		}
	end