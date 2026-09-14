project "Shared"
	location "%{dirs.srcdir}/Shared"
	
	print ("Building Shared...")
		
	language "C++"
	cppdialect "C++17"
	kind "StaticLib"

	targetdir ("%{dirs.libdir}")
	targetname("%{prj.name}_%{cfg.buildcfg}")
	objdir ("%{dirs.intdir}")

	pchheader "Shared_PCH.h"
	pchsource "Shared_PCH.cpp"

	includedirs {
		"%{dirs.srcdir}/Shared"
	}

	files {
		"%{dirs.srcdir}/Shared/**.h",
		"%{dirs.srcdir}/Shared/**.hpp",
		"%{dirs.srcdir}/Shared/**.cpp",
	}

	defines {
		'PROJECT_DIR="' .. (dirs.projectdir):gsub("%\\", "/") .. '/"',
	 	'SOURCE_DIR="' .. (dirs.sourcedir):gsub("%\\", "/") .. '/"',
	 	'RESOURCE_DIR="' .. (dirs.resourcesindir):gsub("%\\", "/") .. '/"',
	 	'RESOURCE_DIR_OUT="' .. (dirs.resourcesoutdir):gsub("%\\", "/") .. '/"',
	 	'SHADER_DIR_IN="' .. (dirs.shadersindir):gsub("%\\", "/") .. '/"',
	 	'SHADER_DIR_OUT="' .. (dirs.shadersoutdir):gsub("%\\", "/") .. '/"',
	 	'TEXTURES_DIR_IN="' .. (dirs.texturesindir):gsub("%\\", "/") .. '/"',
	 	'TEXTURES_DIR_OUT="' .. (dirs.texturesoutdir):gsub("%\\", "/") .. '/"',
		'ANGELSCRIPT_DIR="' .. (dirs.angelscriptdir):gsub("%\\", "/") .. '/"'
	}

	libdirs { "%{dirs.libdir}" }	

