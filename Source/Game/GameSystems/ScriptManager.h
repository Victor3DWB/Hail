#pragma once
namespace Hail
{
	namespace AngelScript
	{
		class TypeRegistry;
		class Handler;
		class Runner;
	}
	class ScriptManager
	{
	public:

		bool Init(AngelScript::Handler* pHandler, AngelScript::TypeRegistry* pTypeRegistry);
		void Cleanup();

		void Update();

		AngelScript::Runner* m_pRunner;

	private:
		bool RegisterGameScriptClasses(AngelScript::TypeRegistry* pTypeRegistry);

	};
	
}



