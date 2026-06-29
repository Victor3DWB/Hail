#pragma once

namespace Hail
{
	// The mapping of hotkeyes to commands
	struct InputMapping;

	namespace AngelScript
	{
		// Manager for all angelscript registering of C++ classes to be used in the script language
		class TypeRegistry;
		// Manages the current running instance of scripts as well as hot-reloading and debugging
		class Handler; 
	}

	// Class containing all the classes and data that the using application need to be able to run and call engine functionality
	struct ApplcationInitData
	{
		AngelScript::TypeRegistry* m_pAsTypeRegistry;
		AngelScript::Handler* m_pAsHandler;

		InputMapping* m_pInputMapping;
	};
}

