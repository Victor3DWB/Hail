project "Game"
	location "%{dirs.srcdir}/Game"

	print ("Building Game...")
		
	language "C++"
	cppdialect "C++17"
	kind "StaticLib"

	targetdir ("%{dirs.libdir}")
	targetname("%{prj.name}_%{cfg.buildcfg}")
	objdir ("%{dirs.intdir}")

	pchheader "Game_PCH.h"
	pchsource "Game_PCH.cpp"

	debugdir ("%{dirs.outdir}")

	files {
		"%{dirs.srcdir}/Game/**.h",
		"%{dirs.srcdir}/Game/**.cpp",
	}


	includedirs {
		".",
		"./**",
		"%{dirs.srcdir}",
		"%{dirs.srcdir}/Shared/",
		"%{dirs.extdir}/AngelScript/include/"
	}

	dependson { "ReflectionCodeGenerator" }

	-- Libs
	libdirs { "%{dirs.libdir}",
		"%{dirs.extdir}/AngelScript/"
	 }	
	links { 
		"Engine",
		"ReflectionCodeGenerator",
		"Shared"
		 }
 	filter { "configurations:Debug" }
	 	links { 
			"angelscript64d"
			 }
	filter { "configurations:Release" }
		links { 
			"angelscript64"
			 }
	filter { "configurations:Production" }
		links { 
			"angelscript64"
			 }

 	-- Defines
 	defines {
	 	'SOURCE_DIR="' .. (dirs.sourcedir):gsub("%\\", "/") .. '/"',
	 	'RESOURCE_DIR="' .. (dirs.resourcesindir):gsub("%\\", "/") .. '/"',
	 	'SHADER_DIR_IN="' .. (dirs.shadersindir):gsub("%\\", "/") .. '/"',
	 	'SHADER_DIR_OUT="' .. (dirs.shadersoutdir):gsub("%\\", "/") .. '/"',
	 	'TEXTURES_DIR_IN="' .. (dirs.texturesindir):gsub("%\\", "/") .. '/"',
	 	'TEXTURES_DIR_OUT="' .. (dirs.texturesoutdir):gsub("%\\", "/") .. '/"',
		'ANGELSCRIPT_DIR="' .. (dirs.angelscriptdir):gsub("%\\", "/") .. '/"'
	}

	-- Running prebuild commands, might comment out and rework

	filter { "system:windows" }
		prebuildcommands { "start %{dirs.outdir}/ReflectionCodeGenerator_%{cfg.buildcfg}.exe Game" }

		
