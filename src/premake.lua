project.name = "Quake"

-- Options

	addoption("with-qcc", "Include the QuakeC Compiler")

-- Project Settings

	project.bindir = "bin"

	dopackage("quake")
	if (options["with-qcc"]) then
		dopackage("qcc")
	end

-- A little extra cleanup

	function doclean(cmd, arg)
		docommand(cmd, arg)
		os.rmdir("bin")
	end
