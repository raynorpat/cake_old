project.name = "Quake"

-- Options

	addoption("with-qcc", "Include the QuakeC Compiler")

-- Project Settings

	project.bindir = "../"

	dopackage("quake")
	if (options["with-qcc"]) then
		dopackage("qcc")
	end