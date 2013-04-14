package.name     = "Quake"
package.language = "c"
package.kind     = "exe"
package.target   = "quake"

-- Build Flags

	package.buildflags = 
	{ 
		"no-64bit-checks",
		"static-runtime",
		"extra-warnings"
	}

	package.config["Release"].buildflags = 
	{ 
		"no-symbols", 
		"optimize-size",
		"no-frame-pointers"
	}


-- Defines

	package.defines = {
		"_CRT_SECURE_NO_DEPRECATE",     -- to avoid VS2005 stdlib warnings
	}

-- Libraries

	if (OS == "linux") then
		package.links = { "m" }
	end


-- Files

	package.files =
	{
		matchrecursive("*.h", "*.c")
	}
