#include "Engine_PCH.h"
#include <new>

#include <stdlib.h>
#include <string>
#include <algorithm> // std::sort

#include "Array.h"
#include "TypeRegistry.h"
#include "Debugger.h" // Type registry to variable callback

using namespace Hail;
using namespace AngelScript;

// This macro is used to avoid warnings about unused variables.
// Usually where the variables are only used in debug mode.
#define UNUSED_VAR(x) (void)(x)

// Set the default memory routines
// Use the angelscript engine's memory routines by default
static asALLOCFUNC_t userAlloc = asAllocMem;
static asFREEFUNC_t  userFree  = asFreeMem;

// Allows the application to set which memory routines should be used by the array object
void ScriptArray::SetMemoryFunctions(asALLOCFUNC_t allocFunc, asFREEFUNC_t freeFunc)
{
	userAlloc = allocFunc;
	userFree = freeFunc;
}

static void RegisterScriptArray_Native(TypeRegistry* pTypeRegistry);
static void RegisterScriptArray_Generic(TypeRegistry* pTypeRegistry);
namespace Hail
{
	namespace AngelScript
	{
		struct ArrayBuffer
		{
			asDWORD maxElements;
			asDWORD numElements;
			asBYTE  data[1];
		};

		struct ArrayCache
		{
			asIScriptFunction* cmpFunc;
			asIScriptFunction* eqFunc;
			int cmpFuncReturnCode; // To allow better error message in case of multiple matches
			int eqFuncReturnCode;
		};
	}
}


// We just define a number here that we assume nobody else is using for
// object type user data. The add-ons have reserved the numbers 1000
// through 1999 for this purpose, so we should be fine.
const asPWORD ARRAY_CACHE = 1000;

static void CleanupTypeInfoArrayCache(asITypeInfo *type)
{
	ArrayCache *cache = reinterpret_cast<ArrayCache*>(type->GetUserData(ARRAY_CACHE));
	if( cache )
	{
		cache->~ArrayCache();
		userFree(cache);
	}
}

ScriptArray* ScriptArray::Create(asITypeInfo *ti, asUINT length)
{
	// Allocate the memory
	void *mem = userAlloc(sizeof(ScriptArray));
	if( mem == 0 )
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Out of memory");

		return 0;
	}

	// Initialize the object
	ScriptArray *a = new(mem) ScriptArray(length, ti);

	return a;
}

ScriptArray* ScriptArray::Create(asITypeInfo *ti, void *initList)
{
	// Allocate the memory
	void *mem = userAlloc(sizeof(ScriptArray));
	if( mem == 0 )
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Out of memory");

		return 0;
	}

	// Initialize the object
	ScriptArray *a = new(mem) ScriptArray(ti, initList);

	return a;
}

ScriptArray* ScriptArray::Create(asITypeInfo *ti, asUINT length, void *defVal)
{
	// Allocate the memory
	void *mem = userAlloc(sizeof(ScriptArray));
	if( mem == 0 )
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Out of memory");

		return 0;
	}

	// Initialize the object
	ScriptArray *a = new(mem) ScriptArray(length, defVal, ti);

	return a;
}

ScriptArray* ScriptArray::Create(asITypeInfo *ti)
{
	return ScriptArray::Create(ti, asUINT(0));
}

// This optional callback is called when the template type is first used by the compiler.
// It allows the application to validate if the template can be instantiated for the requested
// subtype at compile time, instead of at runtime. The output argument dontGarbageCollect
// allow the callback to tell the engine if the template instance type shouldn't be garbage collected,
// i.e. no asOBJ_GC flag.
static bool ScriptArrayTemplateCallback(asITypeInfo *ti, bool &dontGarbageCollect)
{
	// Make sure the subtype can be instantiated with a default factory/constructor,
	// otherwise we won't be able to instantiate the elements.
	int typeId = ti->GetSubTypeId();
	if( typeId == asTYPEID_VOID )
		return false;
	if( (typeId & asTYPEID_MASK_OBJECT) && !(typeId & asTYPEID_OBJHANDLE) )
	{
		asITypeInfo *subtype = ti->GetEngine()->GetTypeInfoById(typeId);
		asDWORD flags = subtype->GetFlags();
		if( (flags & asOBJ_VALUE) && !(flags & asOBJ_POD) )
		{
			// Verify that there is a default constructor
			bool found = false;
			for( asUINT n = 0; n < subtype->GetBehaviourCount(); n++ )
			{
				asEBehaviours beh;
				asIScriptFunction *func = subtype->GetBehaviourByIndex(n, &beh);
				if( beh != asBEHAVE_CONSTRUCT ) continue;

				if( func->GetParamCount() == 0 )
				{
					// Found the default constructor
					found = true;
					break;
				}
			}

			if( !found )
			{
				H_ERROR("Array, The subtype has no default constructor");
				return false;
			}
		}
		else if( (flags & asOBJ_REF) )
		{
			bool found = false;

			// If value assignment for ref type has been disabled then the array
			// can be created if the type has a default factory function
			if( !ti->GetEngine()->GetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE) )
			{
				// Verify that there is a default factory
				for( asUINT n = 0; n < subtype->GetFactoryCount(); n++ )
				{
					asIScriptFunction *func = subtype->GetFactoryByIndex(n);
					if( func->GetParamCount() == 0 )
					{
						// Found the default factory
						found = true;
						break;
					}
				}
			}

			if( !found )
			{
				// No default factory
				H_ERROR(StringL::Format("Array, The subtype '%s' has no default factory", subtype ? subtype->GetEngine()->GetTypeDeclaration(subtype->GetTypeId()) : "UNKNOWN"));
				return false;
			}
		}

		// If the object type is not garbage collected then the array also doesn't need to be
		if( !(flags & asOBJ_GC) )
			dontGarbageCollect = true;
	}
	else if( !(typeId & asTYPEID_OBJHANDLE) )
	{
		// Arrays with primitives cannot form circular references,
		// thus there is no need to garbage collect them
		dontGarbageCollect = true;
	}
	else
	{
		H_ASSERT( typeId & asTYPEID_OBJHANDLE );

		// It is not necessary to set the array as garbage collected for all handle types.
		// If it is possible to determine that the handle cannot refer to an object type
		// that can potentially form a circular reference with the array then it is not
		// necessary to make the array garbage collected.
		asITypeInfo *subtype = ti->GetEngine()->GetTypeInfoById(typeId);
		asDWORD flags = subtype->GetFlags();
		if( !(flags & asOBJ_GC) )
		{
			if( (flags & asOBJ_SCRIPT_OBJECT) )
			{
				// Even if a script class is by itself not garbage collected, it is possible
				// that classes that derive from it may be, so it is not possible to know
				// that no circular reference can occur.
				if( (flags & asOBJ_NOINHERIT) )
				{
					// A script class declared as final cannot be inherited from, thus
					// we can be certain that the object cannot be garbage collected.
					dontGarbageCollect = true;
				}
			}
			else
			{
				// For application registered classes we assume the application knows
				// what it is doing and don't mark the array as garbage collected unless
				// the type is also garbage collected.
				dontGarbageCollect = true;
			}
		}
	}

	// The type is ok
	return true;
}

// TODO: Make type registry be a singleton
TypeRegistry* g_pTypeRegistry = nullptr;

namespace Hail
{
	namespace AngelScript
	{
		// Registers the template array type
		void RegisterScriptArray(TypeRegistry* pTypeRegistry, bool defaultArray)
		{
			g_pTypeRegistry = pTypeRegistry;
			if (strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") == 0)
				RegisterScriptArray_Native(pTypeRegistry);
			else
				RegisterScriptArray_Generic(pTypeRegistry);

			if (defaultArray)
			{
				int r = pTypeRegistry->GetEngine()->RegisterDefaultArrayType("Array<T>"); H_ASSERT(r >= 0);
				UNUSED_VAR(r);
			}
		}
	}
}

static Variable GetScriptArrayVariableData(void* pObj)
{
	ScriptArray* pArray = (ScriptArray*)pObj;

	asITypeInfo* pTypeInfo = pArray->GetArrayObjectType();
	Variable variableToReturn;
	variableToReturn.m_type = StringL::Format("Array %s", pTypeInfo->GetEngine()->GetTypeInfoById(pArray->GetElementTypeId())->GetName());
	uint32 count = pArray->GetCount();
	variableToReturn.m_value = StringL::Format("Count : %u", count);

	int subTypeID = pArray->GetElementTypeId();
	for (uint32 i = 0; i < count; i++)
	{
		Variable& member = variableToReturn.m_members.Add();
		member = ASTypeToVariable(pArray->At(i), subTypeID, 1, pTypeInfo->GetEngine(), g_pTypeRegistry);
		member.m_name = StringL::Format("[%u]", i);
	}
	return variableToReturn;
}

static void RegisterScriptArray_Native(TypeRegistry* pTypeRegistry)
{
	int r = 0;
	UNUSED_VAR(r);

	// Register the object type user data clean up
	pTypeRegistry->GetEngine()->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoArrayCache, ARRAY_CACHE);

	// Register the array type as a template
	r = pTypeRegistry->RegisterType("Array<class T>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE, H_FILE_LINE, "Array<T>"); H_ASSERT( r );
	r = pTypeRegistry->RegisterVariableFunction("Array<class T>", &GetScriptArrayVariableData); H_ASSERT(r);

	asIScriptEngine* engine = pTypeRegistry->GetEngine();

	bool bResult;

	//refs
	// Register a callback for validating the subtype before it is used
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "bool f(%s)", { { "in", "int", false, true }, { "out", "bool", false, true } }, asBEHAVE_TEMPLATE_CALLBACK, asCALL_CDECL, asFUNCTION(ScriptArrayTemplateCallback), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Templates receive the object type as the first parameter. To the script writer this is hidden
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s)", { { "in", "int", false, true } }, asBEHAVE_FACTORY, asCALL_CDECL, asFUNCTIONPR(ScriptArray::Create, (asITypeInfo*), ScriptArray*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s) explicit", { { "in", "int", false, true }, { "length", "uint" } }, asBEHAVE_FACTORY, asCALL_CDECL, asFUNCTIONPR(ScriptArray::Create, (asITypeInfo*), ScriptArray*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s)", { { "in", "int", false, true }, { "length", "uint" }, { "in", "T", EVariableTypeDataState::ConstRef, "value" }}, asBEHAVE_FACTORY, asCALL_CDECL, asFUNCTIONPR(ScriptArray::Create, (asITypeInfo*), ScriptArray*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	
	// Register the factory that will be used for initialization lists
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s) {repeat T}", { { "in", "int", EVariableTypeDataState::Ref, "type"}, {"in", "int", EVariableTypeDataState::Ref, "list"}}, asBEHAVE_LIST_FACTORY, asCALL_CDECL, asFUNCTIONPR(ScriptArray::Create, (asITypeInfo*, void*), ScriptArray*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// The memory management methods
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f()", { }, asBEHAVE_ADDREF, asCALL_THISCALL, asMETHOD(ScriptArray, AddRef), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f()", { }, asBEHAVE_RELEASE, asCALL_THISCALL, asMETHOD(ScriptArray, Release), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// The index operator returns the template subtype
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "opIndex", "T", EVariableTypeDataState::Ref }, { {"index", "uint"}}, asMETHODPR(ScriptArray, At, (asUINT), void*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "opIndex", "T", EVariableTypeDataState::ConstRef }, { {"index", "uint"} }, asMETHODPR(ScriptArray, At, (asUINT) const, const void*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// The assignment operator
	bResult = pTypeRegistry->RegisterClassOperatorOverload("Array<T>", { "opAssign", "Array<T>", EVariableTypeDataState::Ref }, asMETHOD(ScriptArray, operator=), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Other methods
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "AddAt", "void" }, { {"index", "uint"}, {"in", "T", EVariableTypeDataState::ConstRef, "value"}}, asMETHODPR(ScriptArray, At, (asUINT) const, const void*), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "AddAt", "void" }, { {"index", "uint"}, {"arr", "Array<T>", EVariableTypeDataState::ConstRef} }, asMETHODPR(ScriptArray, AddAt, (asUINT, const ScriptArray&), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Add", "void" }, { {"in", "T", EVariableTypeDataState::ConstRef, "value"}}, asMETHOD(ScriptArray, Add), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "RemoveAt", "void" }, { {"index", "uint"} }, asMETHOD(ScriptArray, RemoveAt), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "RemoveLast", "void" }, { }, asMETHOD(ScriptArray, RemoveLast), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "RemoveRange", "void" }, { {"start", "uint"}, {"count", "uint"} }, asMETHOD(ScriptArray, RemoveRange), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
#if AS_USE_ACCESSORS != 1
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Size", "uint", EVariableTypeDataState::Const }, { }, asMETHOD(ScriptArray, GetCount), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
#endif
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Reserve", "void" }, { {"length", "uint"} }, asMETHOD(ScriptArray, Reserve), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Resize", "void" }, { {"length", "uint"} }, asMETHODPR(ScriptArray, Resize, (asUINT), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "SortAsc", "void" }, { }, asMETHODPR(ScriptArray, SortAsc, (), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "SortAsc", "void" }, { {"startAt", "uint"}, {"count", "uint"} }, asMETHODPR(ScriptArray, SortAsc, (asUINT, asUINT), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "SortDesc", "void" }, { }, asMETHODPR(ScriptArray, SortDesc, (), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "SortDesc", "void" }, { {"startAt", "uint"}, {"count", "uint"} }, asMETHODPR(ScriptArray, SortDesc, (asUINT, asUINT), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Reverse", "void" }, { }, asMETHOD(ScriptArray, Reverse), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// The token 'if_handle_then_const' tells the engine that if the type T is a handle, then it should refer to a read-only object
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Find", "int", EVariableTypeDataState::Const }, { {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asMETHODPR(ScriptArray, Find, (void*) const, int), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// TODO: It should be "int find(const T&in value, uint startAt = 0) const"
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "Find", "int", EVariableTypeDataState::Const }, { {"startAt", "uint"}, {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asMETHODPR(ScriptArray, Find, (void*) const, int), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "FindByRef", "int", EVariableTypeDataState::Const }, { {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asMETHODPR(ScriptArray, FindByRef, (void*) const, int), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// TODO: It should be "int findByRef(const T&in value, uint startAt = 0) const"
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "FindByRef", "int", EVariableTypeDataState::Const }, { {"startAt", "uint"}, {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asMETHODPR(ScriptArray, FindByRef, (asUINT, void*) const, int), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "opEquals", "bool", EVariableTypeDataState::Const }, { {"in", "Array<T>", EVariableTypeDataState::ConstRef} }, asMETHOD(ScriptArray, operator==), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "IsEmpty", "bool", EVariableTypeDataState::Const }, { }, asMETHOD(ScriptArray, IsEmpty), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Sort with callback for comparison
	r = engine->RegisterFuncdef("bool Array<T>::less(const T&in if_handle_then_const a, const T&in if_handle_then_const b)");
	bResult = pTypeRegistry->RegisterClassMethod("Array<T>", { "sort", "void" }, { {"in", "less", EVariableTypeDataState::ConstRef }, {"startAt", "uint", EVariableTypeDataState::None, " = 0"}, {"count", "uint", EVariableTypeDataState::None, " =  uint(-1)"} }, asMETHODPR(ScriptArray, Sort, (asIScriptFunction*, asUINT, asUINT), void), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Register GC behaviours in case the array needs to be garbage collected
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "int f()", { }, asBEHAVE_GETREFCOUNT, asCALL_THISCALL, asMETHOD(ScriptArray, GetRefCount), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f()", { }, asBEHAVE_SETGCFLAG, asCALL_THISCALL, asMETHOD(ScriptArray, SetFlag), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "bool f()", { }, asBEHAVE_GETGCFLAG, asCALL_THISCALL, asMETHOD(ScriptArray, GetFlag), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f(%s)", { { "in", "int", EVariableTypeDataState::Ref } }, asBEHAVE_ENUMREFS, asCALL_THISCALL, asMETHOD(ScriptArray, EnumReferences), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f(%s)", { { "in", "int", EVariableTypeDataState::Ref } }, asBEHAVE_RELEASEREFS, asCALL_THISCALL, asMETHOD(ScriptArray, ReleaseAllHandles), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

}

ScriptArray &ScriptArray::operator=(const ScriptArray &other)
{
	// Only perform the copy if the array types are the same
	if( &other != this &&
		other.GetArrayObjectType() == GetArrayObjectType() )
	{
		// Make sure the arrays are of the same size
		Resize(other.buffer->numElements);

		// Copy the value of each element
		CopyBuffer(buffer, other.buffer);
	}

	return *this;
}

ScriptArray::ScriptArray(asITypeInfo *ti, void *buf)
{
	// The object type should be the template instance of the array
	H_ASSERT( ti && StringCompare(ti->GetName(), "Array") );

	refCount = 1;
	gcFlag = false;
	objType = ti;
	objType->AddRef();
	buffer = 0;

	Precache();

	asIScriptEngine *engine = ti->GetEngine();

	// Determine element size
	if( subTypeId & asTYPEID_MASK_OBJECT )
		elementSize = sizeof(asPWORD);
	else
		elementSize = engine->GetSizeOfPrimitiveType(subTypeId);

	// Determine the initial size from the buffer
	asUINT length = *(asUINT*)buf;

	// Make sure the array size isn't too large for us to handle
	if( !CheckMaxSize(length) )
	{
		// Don't continue with the initialization
		return;
	}

	// Copy the values of the array elements from the buffer
	if( (ti->GetSubTypeId() & asTYPEID_MASK_OBJECT) == 0 )
	{
		CreateBuffer(&buffer, length);

		// Copy the values of the primitive type into the internal buffer
		if( length > 0 )
			memcpy(At(0), (((asUINT*)buf)+1), length * elementSize);
	}
	else if( ti->GetSubTypeId() & asTYPEID_OBJHANDLE )
	{
		CreateBuffer(&buffer, length);

		// Copy the handles into the internal buffer
		if( length > 0 )
			memcpy(At(0), (((asUINT*)buf)+1), length * elementSize);

		// With object handles it is safe to clear the memory in the received buffer
		// instead of increasing the ref count. It will save time both by avoiding the
		// call the increase ref, and also relieve the engine from having to release
		// its references too
		memset((((asUINT*)buf)+1), 0, length * elementSize);
	}
	else if( ti->GetSubType()->GetFlags() & asOBJ_REF )
	{
		// Only allocate the buffer, but not the objects
		subTypeId |= asTYPEID_OBJHANDLE;
		CreateBuffer(&buffer, length);
		subTypeId &= ~asTYPEID_OBJHANDLE;

		// Copy the handles into the internal buffer
		if( length > 0 )
			memcpy(buffer->data, (((asUINT*)buf)+1), length * elementSize);

		// For ref types we can do the same as for handles, as they are
		// implicitly stored as handles.
		memset((((asUINT*)buf)+1), 0, length * elementSize);
	}
	else
	{
		// TODO: Optimize by calling the copy constructor of the object instead of
		//       constructing with the default constructor and then assigning the value
		// TODO: With C++11 ideally we should be calling the move constructor, instead
		//       of the copy constructor as the engine will just discard the objects in the
		//       buffer afterwards.
		CreateBuffer(&buffer, length);

		// For value types we need to call the opAssign for each individual object
		for( asUINT n = 0; n < length; n++ )
		{
			void *obj = At(n);
			asBYTE *srcObj = (asBYTE*)buf;
			srcObj += 4 + n*ti->GetSubType()->GetSize();
			engine->AssignScriptObject(obj, srcObj, ti->GetSubType());
		}
	}

	// Notify the GC of the successful creation
	if( objType->GetFlags() & asOBJ_GC )
		objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);
}

ScriptArray::ScriptArray(asUINT length, asITypeInfo *ti)
{
	// The object type should be the template instance of the array
	H_ASSERT( ti && StringCompare(ti->GetName(),"Array") );

	refCount = 1;
	gcFlag = false;
	objType = ti;
	objType->AddRef();
	buffer = 0;

	Precache();

	// Determine element size
	if( subTypeId & asTYPEID_MASK_OBJECT )
		elementSize = sizeof(asPWORD);
	else
		elementSize = objType->GetEngine()->GetSizeOfPrimitiveType(subTypeId);

	// Make sure the array size isn't too large for us to handle
	if( !CheckMaxSize(length) )
	{
		// Don't continue with the initialization
		return;
	}

	CreateBuffer(&buffer, length);

	// Notify the GC of the successful creation
	if( objType->GetFlags() & asOBJ_GC )
		objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);
}

ScriptArray::ScriptArray(const ScriptArray &other)
{
	refCount = 1;
	gcFlag = false;
	objType = other.objType;
	objType->AddRef();
	buffer = 0;

	Precache();

	elementSize = other.elementSize;

	if( objType->GetFlags() & asOBJ_GC )
		objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);

	CreateBuffer(&buffer, 0);

	// Copy the content
	*this = other;
}

ScriptArray::ScriptArray(asUINT length, void *defVal, asITypeInfo *ti)
{
	// The object type should be the template instance of the array
	H_ASSERT( ti && StringCompare(ti->GetName(),"Array") );

	refCount = 1;
	gcFlag = false;
	objType = ti;
	objType->AddRef();
	buffer = 0;

	Precache();

	// Determine element size
	if( subTypeId & asTYPEID_MASK_OBJECT )
		elementSize = sizeof(asPWORD);
	else
		elementSize = objType->GetEngine()->GetSizeOfPrimitiveType(subTypeId);

	// Make sure the array size isn't too large for us to handle
	if( !CheckMaxSize(length) )
	{
		// Don't continue with the initialization
		return;
	}

	CreateBuffer(&buffer, length);

	// Notify the GC of the successful creation
	if( objType->GetFlags() & asOBJ_GC )
		objType->GetEngine()->NotifyGarbageCollectorOfNewObject(this, objType);

	// Initialize the elements with the default value
	for( asUINT n = 0; n < GetCount(); n++ )
		SetValue(n, defVal);
}

void ScriptArray::SetValue(asUINT index, void *value)
{
	// At() will take care of the out-of-bounds checking, though
	// if called from the application then nothing will be done
	void *ptr = At(index);
	if( ptr == 0 ) return;

	if ((subTypeId & ~asTYPEID_MASK_SEQNBR) && !(subTypeId & asTYPEID_OBJHANDLE))
	{
		asITypeInfo *subType = objType->GetSubType();
		if (subType->GetFlags() & asOBJ_ASHANDLE)
		{
			// For objects that should work as handles we must use the opHndlAssign method
			// TODO: Must support alternative syntaxes as well
			// TODO: Move the lookup of the opHndlAssign method to Precache() so it is only done once
			StringL decl = StringL::Format("%s%s%s%s",subType->GetName(), "& opHndlAssign(const ", + subType->GetName(), "&in)");
			asIScriptFunction* func = subType->GetMethodByDecl(decl.Data());
			if (func)
			{
				// TODO: Reuse active context if existing
				asIScriptEngine* engine = objType->GetEngine();
				asIScriptContext* ctx = engine->RequestContext();
				ctx->Prepare(func);
				ctx->SetObject(ptr);
				ctx->SetArgAddress(0, value);
				// TODO: Handle errors
				ctx->Execute();
				engine->ReturnContext(ctx);
			}
			else
			{
				// opHndlAssign doesn't exist, so try ordinary value assign instead
				objType->GetEngine()->AssignScriptObject(ptr, value, subType);
			}
		}
		else
			objType->GetEngine()->AssignScriptObject(ptr, value, subType);
	}
	else if( subTypeId & asTYPEID_OBJHANDLE )
	{
		void *tmp = *(void**)ptr;
		*(void**)ptr = *(void**)value;
		objType->GetEngine()->AddRefScriptObject(*(void**)value, objType->GetSubType());
		if( tmp )
			objType->GetEngine()->ReleaseScriptObject(tmp, objType->GetSubType());
	}
	else if( subTypeId == asTYPEID_BOOL ||
			 subTypeId == asTYPEID_INT8 ||
			 subTypeId == asTYPEID_UINT8 )
		*(char*)ptr = *(char*)value;
	else if( subTypeId == asTYPEID_INT16 ||
			 subTypeId == asTYPEID_UINT16 )
		*(short*)ptr = *(short*)value;
	else if( subTypeId == asTYPEID_INT32 ||
			 subTypeId == asTYPEID_UINT32 ||
			 subTypeId == asTYPEID_FLOAT ||
			 subTypeId > asTYPEID_DOUBLE ) // enums have a type id larger than doubles
		*(int*)ptr = *(int*)value;
	else if( subTypeId == asTYPEID_INT64 ||
			 subTypeId == asTYPEID_UINT64 ||
			 subTypeId == asTYPEID_DOUBLE )
		*(double*)ptr = *(double*)value;
}

ScriptArray::~ScriptArray()
{
	if( buffer )
	{
		DeleteBuffer(buffer);
		buffer = 0;
	}
	if( objType ) objType->Release();
}

asUINT ScriptArray::GetCount() const
{
	return buffer->numElements;
}

bool ScriptArray::IsEmpty() const
{
	return buffer->numElements == 0;
}

void ScriptArray::Reserve(asUINT maxElements)
{
	if( maxElements <= buffer->maxElements )
		return;

	if( !CheckMaxSize(maxElements) )
		return;

	// Allocate memory for the buffer
	ArrayBuffer *newBuffer = reinterpret_cast<ArrayBuffer*>(userAlloc(sizeof(ArrayBuffer)-1 + elementSize*maxElements));
	if( newBuffer )
	{
		newBuffer->numElements = buffer->numElements;
		newBuffer->maxElements = maxElements;
	}
	else
	{
		// Out of memory
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Out of memory");
		return;
	}

	// As objects in arrays of objects are not stored inline, it is safe to use memcpy here
	// since we're just copying the pointers to objects and not the actual objects.
	memcpy(newBuffer->data, buffer->data, buffer->numElements*elementSize);

	// Release the old buffer
	userFree(buffer);

	buffer = newBuffer;
}

void ScriptArray::Resize(asUINT numElements)
{
	if( !CheckMaxSize(numElements) )
		return;

	Resize((int)numElements - (int)buffer->numElements, (asUINT)-1);
}

void ScriptArray::RemoveRange(asUINT start, asUINT count)
{
	if (count == 0)
		return;

	if( buffer == 0 || start > buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if (ctx)
			ctx->SetException("Index out of bounds");
		return;
	}

	// Cap count to the end of the array
	if (start + count > buffer->numElements)
		count = buffer->numElements - start;

	// Destroy the elements that are being removed
	Destruct(buffer, start, start + count);

	// Compact the elements
	// As objects in arrays of objects are not stored inline, it is safe to use memmove here
	// since we're just copying the pointers to objects and not the actual objects.
	memmove(buffer->data + start*elementSize, buffer->data + (start + count)*elementSize, (buffer->numElements - start - count)*elementSize);
	buffer->numElements -= count;
}

// Internal
void ScriptArray::Resize(int delta, asUINT at)
{
	if( delta < 0 )
	{
		if( -delta > (int)buffer->numElements )
			delta = -(int)buffer->numElements;
		if( at > buffer->numElements + delta )
			at = buffer->numElements + delta;
	}
	else if( delta > 0 )
	{
		// Make sure the array size isn't too large for us to handle
		if( delta > 0 && !CheckMaxSize(buffer->numElements + delta) )
			return;

		if( at > buffer->numElements )
			at = buffer->numElements;
	}

	if( delta == 0 ) return;

	if( buffer->maxElements < buffer->numElements + delta )
	{
		// Allocate memory for the buffer
		ArrayBuffer *newBuffer = reinterpret_cast<ArrayBuffer*>(userAlloc(sizeof(ArrayBuffer)-1 + elementSize*(buffer->numElements + delta)));
		if( newBuffer )
		{
			newBuffer->numElements = buffer->numElements + delta;
			newBuffer->maxElements = newBuffer->numElements;
		}
		else
		{
			// Out of memory
			asIScriptContext *ctx = asGetActiveContext();
			if( ctx )
				ctx->SetException("Out of memory");
			return;
		}

		// As objects in arrays of objects are not stored inline, it is safe to use memcpy here
		// since we're just copying the pointers to objects and not the actual objects.
		memcpy(newBuffer->data, buffer->data, at*elementSize);
		if( at < buffer->numElements )
			memcpy(newBuffer->data + (at+delta)*elementSize, buffer->data + at*elementSize, (buffer->numElements-at)*elementSize);

		// Initialize the new elements with default values
		Construct(newBuffer, at, at+delta);

		// Release the old buffer
		userFree(buffer);

		buffer = newBuffer;
	}
	else if( delta < 0 )
	{
		Destruct(buffer, at, at-delta);
		// As objects in arrays of objects are not stored inline, it is safe to use memmove here
		// since we're just copying the pointers to objects and not the actual objects.
		memmove(buffer->data + at*elementSize, buffer->data + (at-delta)*elementSize, (buffer->numElements - (at-delta))*elementSize);
		buffer->numElements += delta;
	}
	else
	{
		// As objects in arrays of objects are not stored inline, it is safe to use memmove here
		// since we're just copying the pointers to objects and not the actual objects.
		memmove(buffer->data + (at+delta)*elementSize, buffer->data + at*elementSize, (buffer->numElements - at)*elementSize);
		Construct(buffer, at, at+delta);
		buffer->numElements += delta;
	}
}

// internal
bool ScriptArray::CheckMaxSize(asUINT numElements)
{
	// This code makes sure the size of the buffer that is allocated
	// for the array doesn't overflow and becomes smaller than requested

	asUINT maxSize = 0xFFFFFFFFul - sizeof(ArrayBuffer) + 1;
	if( elementSize > 0 )
		maxSize /= elementSize;

	if( numElements > maxSize )
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Too large array size");

		return false;
	}

	// OK
	return true;
}

asITypeInfo *ScriptArray::GetArrayObjectType() const
{
	return objType;
}

int ScriptArray::GetArrayTypeId() const
{
	return objType->GetTypeId();
}

int ScriptArray::GetElementTypeId() const
{
	return subTypeId;
}

void ScriptArray::AddAt(asUINT index, void *value)
{
	if( index > buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Index out of bounds");
		return;
	}

	// Make room for the new element
	Resize(1, index);

	// Set the value of the new element
	SetValue(index, value);
}

void ScriptArray::AddAt(asUINT index, const ScriptArray &arr)
{
	if (index > buffer->numElements)
	{
		asIScriptContext *ctx = asGetActiveContext();
		if (ctx)
			ctx->SetException("Index out of bounds");
		return;
	}

	if (objType != arr.objType)
	{
		// This shouldn't really be possible to happen when
		// called from a script, but let's check for it anyway
		asIScriptContext *ctx = asGetActiveContext();
		if (ctx)
			ctx->SetException("Mismatching array types");
		return;
	}

	asUINT elements = arr.GetCount();
	Resize(elements, index);
	if (&arr != this)
	{
		for (asUINT n = 0; n < arr.GetCount(); n++)
		{
			// This const cast is allowed, since we know the
			// value will only be used to make a copy of it
			void *value = const_cast<void*>(arr.At(n));
			SetValue(index + n, value);
		}
	}
	else
	{
		// The array that is being inserted is the same as this one.
		// So we should iterate over the elements before the index,
		// and then the elements after
		for (asUINT n = 0; n < index; n++)
		{
			// This const cast is allowed, since we know the
			// value will only be used to make a copy of it
			void *value = const_cast<void*>(arr.At(n));
			SetValue(index + n, value);
		}

		for (asUINT n = index + elements, m = 0; n < arr.GetCount(); n++, m++)
		{
			// This const cast is allowed, since we know the
			// value will only be used to make a copy of it
			void *value = const_cast<void*>(arr.At(n));
			SetValue(index + index + m, value);
		}
	}
}

void ScriptArray::Add(void *value)
{
	AddAt(buffer->numElements, value);
}

void ScriptArray::RemoveAt(asUINT index)
{
	if( index >= buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Index out of bounds");
		return;
	}

	// Remove the element
	Resize(-1, index);
}

void ScriptArray::RemoveLast()
{
	RemoveAt(buffer->numElements-1);
}

// Return a pointer to the array element. Returns 0 if the index is out of bounds
const void *ScriptArray::At(asUINT index) const
{
	if( buffer == 0 || index >= buffer->numElements )
	{
		// If this is called from a script we raise a script exception
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Index out of bounds");
		return 0;
	}

	if( (subTypeId & asTYPEID_MASK_OBJECT) && !(subTypeId & asTYPEID_OBJHANDLE) )
		return *(void**)(buffer->data + elementSize*index);
	else
		return buffer->data + elementSize*index;
}
void *ScriptArray::At(asUINT index)
{
	return const_cast<void*>(const_cast<const ScriptArray *>(this)->At(index));
}

void *ScriptArray::GetBuffer()
{
	return buffer->data;
}


// internal
void ScriptArray::CreateBuffer(ArrayBuffer **buf, asUINT numElements)
{
	*buf = reinterpret_cast<ArrayBuffer*>(userAlloc(sizeof(ArrayBuffer)-1+elementSize*numElements));

	if( *buf )
	{
		(*buf)->numElements = numElements;
		(*buf)->maxElements = numElements;
		Construct(*buf, 0, numElements);
	}
	else
	{
		// Oops, out of memory
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Out of memory");
	}
}

// internal
void ScriptArray::DeleteBuffer(ArrayBuffer *buf)
{
	Destruct(buf, 0, buf->numElements);

	// Free the buffer
	userFree(buf);
}

// internal
void ScriptArray::Construct(ArrayBuffer *buf, asUINT start, asUINT end)
{
	if( (subTypeId & asTYPEID_MASK_OBJECT) && !(subTypeId & asTYPEID_OBJHANDLE) )
	{
		// Create an object using the default constructor/factory for each element
		void **max = (void**)(buf->data + end * sizeof(void*));
		void **d = (void**)(buf->data + start * sizeof(void*));

		asIScriptEngine *engine = objType->GetEngine();
		asITypeInfo *subType = objType->GetSubType();

		for( ; d < max; d++ )
		{
			*d = (void*)engine->CreateScriptObject(subType);
			if( *d == 0 )
			{
				// Set the remaining entries to null so the destructor
				// won't attempt to destroy invalid objects later
				memset(d, 0, sizeof(void*)*(max-d));

				// There is no need to set an exception on the context,
				// as CreateScriptObject has already done that
				return;
			}
		}
	}
	else
	{
		// Set all elements to zero whether they are handles or primitives
		void *d = (void*)(buf->data + start * elementSize);
		memset(d, 0, (end-start)*elementSize);
	}
}

// internal
void ScriptArray::Destruct(ArrayBuffer *buf, asUINT start, asUINT end)
{
	if( subTypeId & asTYPEID_MASK_OBJECT )
	{
		asIScriptEngine *engine = objType->GetEngine();

		void **max = (void**)(buf->data + end * sizeof(void*));
		void **d   = (void**)(buf->data + start * sizeof(void*));

		for( ; d < max; d++ )
		{
			if( *d )
				engine->ReleaseScriptObject(*d, objType->GetSubType());
		}
	}
}


// internal
bool ScriptArray::Less(const void *a, const void *b, bool asc)
{
	if( !asc )
	{
		// Swap items
		const void *TEMP = a;
		a = b;
		b = TEMP;
	}

	if( !(subTypeId & ~asTYPEID_MASK_SEQNBR) )
	{
		// Simple compare of values
		switch( subTypeId )
		{
			#define COMPARE(T) *((T*)a) < *((T*)b)
			case asTYPEID_BOOL:   return COMPARE(bool);
			case asTYPEID_INT8:   return COMPARE(asINT8);
			case asTYPEID_INT16:  return COMPARE(asINT16);
			case asTYPEID_INT32:  return COMPARE(asINT32);
			case asTYPEID_INT64:  return COMPARE(asINT64);
			case asTYPEID_UINT8:  return COMPARE(asBYTE);
			case asTYPEID_UINT16: return COMPARE(asWORD);
			case asTYPEID_UINT32: return COMPARE(asDWORD);
			case asTYPEID_UINT64: return COMPARE(asQWORD);
			case asTYPEID_FLOAT:  return COMPARE(float);
			case asTYPEID_DOUBLE: return COMPARE(double);
			default: return COMPARE(signed int); // All enums fall in this case. TODO: update this when enums can have different sizes and types
			#undef COMPARE
		}
	}

	return false;
}

void ScriptArray::Reverse()
{
	asUINT size = GetCount();

	if( size >= 2 )
	{
		asBYTE TEMP[16];

		for( asUINT i = 0; i < size / 2; i++ )
		{
			Copy(TEMP, GetArrayItemPointer(i));
			Copy(GetArrayItemPointer(i), GetArrayItemPointer(size - i - 1));
			Copy(GetArrayItemPointer(size - i - 1), TEMP);
		}
	}
}

bool ScriptArray::operator==(const ScriptArray &other) const
{
	if( objType != other.objType )
		return false;

	if( GetCount() != other.GetCount() )
		return false;

	asIScriptContext *cmpContext = 0;
	bool isNested = false;

	if( subTypeId & ~asTYPEID_MASK_SEQNBR )
	{
		// Try to reuse the active context
		cmpContext = asGetActiveContext();
		if( cmpContext )
		{
			if( cmpContext->GetEngine() == objType->GetEngine() && cmpContext->PushState() >= 0 )
				isNested = true;
			else
				cmpContext = 0;
		}
		if( cmpContext == 0 )
		{
			// TODO: Ideally this context would be retrieved from a pool, so we don't have to
			//       create a new one everytime. We could keep a context with the array object
			//       but that would consume a lot of resources as each context is quite heavy.
			cmpContext = objType->GetEngine()->CreateContext();
		}
	}

	// Check if all elements are equal
	bool isEqual = true;
	ArrayCache *cache = reinterpret_cast<ArrayCache*>(objType->GetUserData(ARRAY_CACHE));
	for( asUINT n = 0; n < GetCount(); n++ )
		if( !Equals(At(n), other.At(n), cmpContext, cache) )
		{
			isEqual = false;
			break;
		}

	if( cmpContext )
	{
		if( isNested )
		{
			asEContextState state = cmpContext->GetState();
			cmpContext->PopState();
			if( state == asEXECUTION_ABORTED )
				cmpContext->Abort();
		}
		else
			cmpContext->Release();
	}

	return isEqual;
}

// internal
bool ScriptArray::Equals(const void *a, const void *b, asIScriptContext *ctx, ArrayCache *cache) const
{
	if( !(subTypeId & ~asTYPEID_MASK_SEQNBR) )
	{
		// Simple compare of values
		switch( subTypeId )
		{
			#define COMPARE(T) *((T*)a) == *((T*)b)
			case asTYPEID_BOOL:   return COMPARE(bool);
			case asTYPEID_INT8:   return COMPARE(asINT8);
			case asTYPEID_INT16:  return COMPARE(asINT16);
			case asTYPEID_INT32:  return COMPARE(asINT32);
			case asTYPEID_INT64:  return COMPARE(asINT64);
			case asTYPEID_UINT8:  return COMPARE(asBYTE);
			case asTYPEID_UINT16: return COMPARE(asWORD);
			case asTYPEID_UINT32: return COMPARE(asDWORD);
			case asTYPEID_UINT64: return COMPARE(asQWORD);
			case asTYPEID_FLOAT:  return COMPARE(float);
			case asTYPEID_DOUBLE: return COMPARE(double);
			default: return COMPARE(signed int); // All enums fall here. TODO: update this when enums can have different sizes and types
			#undef COMPARE
		}
	}
	else
	{
		int r = 0;

		if( subTypeId & asTYPEID_OBJHANDLE )
		{
			// Allow the find to work even if the array contains null handles
			if( *(void**)a == *(void**)b ) return true;
		}

		// Execute object opEquals if available
		if( cache && cache->eqFunc )
		{
			// TODO: Add proper error handling
			r = ctx->Prepare(cache->eqFunc); H_ASSERT(r >= 0);

			if( subTypeId & asTYPEID_OBJHANDLE )
			{
				r = ctx->SetObject(*((void**)a)); H_ASSERT(r >= 0);
				r = ctx->SetArgObject(0, *((void**)b)); H_ASSERT(r >= 0);
			}
			else
			{
				r = ctx->SetObject((void*)a); H_ASSERT(r >= 0);
				r = ctx->SetArgObject(0, (void*)b); H_ASSERT(r >= 0);
			}

			r = ctx->Execute();

			if( r == asEXECUTION_FINISHED )
				return ctx->GetReturnByte() != 0;

			return false;
		}

		// Execute object opCmp if available
		if( cache && cache->cmpFunc )
		{
			// TODO: Add proper error handling
			r = ctx->Prepare(cache->cmpFunc); H_ASSERT(r >= 0);

			if( subTypeId & asTYPEID_OBJHANDLE )
			{
				r = ctx->SetObject(*((void**)a)); H_ASSERT(r >= 0);
				r = ctx->SetArgObject(0, *((void**)b)); H_ASSERT(r >= 0);
			}
			else
			{
				r = ctx->SetObject((void*)a); H_ASSERT(r >= 0);
				r = ctx->SetArgObject(0, (void*)b); H_ASSERT(r >= 0);
			}

			r = ctx->Execute();

			if( r == asEXECUTION_FINISHED )
				return (int)ctx->GetReturnDWord() == 0;

			return false;
		}
	}

	return false;
}

int ScriptArray::FindByRef(void *ref) const
{
	return FindByRef(0, ref);
}

int ScriptArray::FindByRef(asUINT startAt, void *ref) const
{
	// Find the matching element by its reference
	asUINT size = GetCount();
	if( subTypeId & asTYPEID_OBJHANDLE )
	{
		// Dereference the pointer
		ref = *(void**)ref;
		for( asUINT i = startAt; i < size; i++ )
		{
			if( *(void**)At(i) == ref )
				return i;
		}
	}
	else
	{
		// Compare the reference directly
		for( asUINT i = startAt; i < size; i++ )
		{
			if( At(i) == ref )
				return i;
		}
	}

	return -1;
}

int ScriptArray::Find(void *value) const
{
	return Find(0, value);
}

int ScriptArray::Find(asUINT startAt, void *value) const
{
	// Check if the subtype really supports find()
	// TODO: Can't this be done at compile time too by the template callback
	ArrayCache *cache = 0;
	if( subTypeId & ~asTYPEID_MASK_SEQNBR )
	{
		cache = reinterpret_cast<ArrayCache*>(objType->GetUserData(ARRAY_CACHE));
		if( !cache || (cache->cmpFunc == 0 && cache->eqFunc == 0) )
		{
			asIScriptContext *ctx = asGetActiveContext();
			asITypeInfo* subType = objType->GetEngine()->GetTypeInfoById(subTypeId);

			// Throw an exception
			if( ctx )
			{
				char tmp[512];

				if( cache && cache->eqFuncReturnCode == asMULTIPLE_FUNCTIONS )
#if defined(_MSC_VER) && _MSC_VER >= 1500 && !defined(__S3E__)
					sprintf_s(tmp, 512, "Type '%s' has multiple matching opEquals or opCmp methods", subType->GetName());
#else
					snprintf(tmp, sizeof(tmp), "Type '%s' has multiple matching opEquals or opCmp methods", subType->GetName());
#endif
				else
#if defined(_MSC_VER) && _MSC_VER >= 1500 && !defined(__S3E__)
					sprintf_s(tmp, 512, "Type '%s' does not have a matching opEquals or opCmp method", subType->GetName());
#else
					snprintf(tmp, sizeof(tmp), "Type '%s' does not have a matching opEquals or opCmp method", subType->GetName());
#endif
				ctx->SetException(tmp);
			}

			return -1;
		}
	}

	asIScriptContext *cmpContext = 0;
	bool isNested = false;

	if( subTypeId & ~asTYPEID_MASK_SEQNBR )
	{
		// Try to reuse the active context
		cmpContext = asGetActiveContext();
		if( cmpContext )
		{
			if( cmpContext->GetEngine() == objType->GetEngine() && cmpContext->PushState() >= 0 )
				isNested = true;
			else
				cmpContext = 0;
		}
		if( cmpContext == 0 )
		{
			// TODO: Ideally this context would be retrieved from a pool, so we don't have to
			//       create a new one everytime. We could keep a context with the array object
			//       but that would consume a lot of resources as each context is quite heavy.
			cmpContext = objType->GetEngine()->CreateContext();
		}
	}

	// Find the matching element
	int ret = -1;
	asUINT size = GetCount();

	for( asUINT i = startAt; i < size; i++ )
	{
		// value passed by reference
		if( Equals(At(i), value, cmpContext, cache) )
		{
			ret = (int)i;
			break;
		}
	}

	if( cmpContext )
	{
		if( isNested )
		{
			asEContextState state = cmpContext->GetState();
			cmpContext->PopState();
			if( state == asEXECUTION_ABORTED )
				cmpContext->Abort();
		}
		else
			cmpContext->Release();
	}

	return ret;
}



// internal
// Copy object handle or primitive value
// Even in arrays of objects the objects are allocated on
// the heap and the array stores the pointers to the objects
void ScriptArray::Copy(void *dst, void *src)
{
	memcpy(dst, src, elementSize);
}


// internal
// Swap two elements
// Even in arrays of objects the objects are allocated on 
// the heap and the array stores the pointers to the objects.
void ScriptArray::Swap(void* a, void* b)
{
	asBYTE tmp[16];
	Copy(tmp, a);
	Copy(a, b);
	Copy(b, tmp);
}


// internal
// Return pointer to array item (object handle or primitive value)
void *ScriptArray::GetArrayItemPointer(int index)
{
	return buffer->data + index * elementSize;
}

// internal
// Return pointer to data in buffer (object or primitive)
void *ScriptArray::GetDataPointer(void *buf)
{
	if ((subTypeId & asTYPEID_MASK_OBJECT) && !(subTypeId & asTYPEID_OBJHANDLE) )
	{
		// Real address of object
		return reinterpret_cast<void*>(*(size_t*)buf);
	}
	else
	{
		// Primitive is just a raw data
		return buf;
	}
}


// Sort ascending
void ScriptArray::SortAsc()
{
	Sort(0, GetCount(), true);
}

// Sort ascending
void ScriptArray::SortAsc(asUINT startAt, asUINT count)
{
	Sort(startAt, count, true);
}

// Sort descending
void ScriptArray::SortDesc()
{
	Sort(0, GetCount(), false);
}

// Sort descending
void ScriptArray::SortDesc(asUINT startAt, asUINT count)
{
	Sort(startAt, count, false);
}


// internal
void ScriptArray::Sort(asUINT startAt, asUINT count, bool asc)
{
	// Subtype isn't primitive and doesn't have opCmp
	ArrayCache *cache = reinterpret_cast<ArrayCache*>(objType->GetUserData(ARRAY_CACHE));
	if( subTypeId & ~asTYPEID_MASK_SEQNBR )
	{
		if( !cache || cache->cmpFunc == 0 )
		{
			asIScriptContext *ctx = asGetActiveContext();
			asITypeInfo* subType = objType->GetEngine()->GetTypeInfoById(subTypeId);

			// Throw an exception
			if( ctx )
			{
				char tmp[512];

				if( cache && cache->cmpFuncReturnCode == asMULTIPLE_FUNCTIONS )
#if defined(_MSC_VER) && _MSC_VER >= 1500 && !defined(__S3E__)
					sprintf_s(tmp, 512, "Type '%s' has multiple matching opCmp methods", subType->GetName());
#else
					snprintf(tmp, sizeof(tmp), "Type '%s' has multiple matching opCmp methods", subType->GetName());
#endif
				else
#if defined(_MSC_VER) && _MSC_VER >= 1500 && !defined(__S3E__)
					sprintf_s(tmp, 512, "Type '%s' does not have a matching opCmp method", subType->GetName());
#else
					snprintf(tmp, sizeof(tmp), "Type '%s' does not have a matching opCmp method", subType->GetName());
#endif

				ctx->SetException(tmp);
			}

			return;
		}
	}

	// No need to sort
	if( count < 2 )
	{
		return;
	}

	int start = startAt;
	int end = startAt + count;

	// Check if we could access invalid item while sorting
	if( start >= (int)buffer->numElements || end > (int)buffer->numElements )
	{
		asIScriptContext *ctx = asGetActiveContext();

		// Throw an exception
		if( ctx )
		{
			ctx->SetException("Index out of bounds");
		}

		return;
	}

	if( subTypeId & ~asTYPEID_MASK_SEQNBR )
	{
		asIScriptContext *cmpContext = 0;
		bool isNested = false;

		// Try to reuse the active context
		cmpContext = asGetActiveContext();
		if( cmpContext )
		{
			if( cmpContext->GetEngine() == objType->GetEngine() && cmpContext->PushState() >= 0 )
				isNested = true;
			else
				cmpContext = 0;
		}
		if( cmpContext == 0 )
			cmpContext = objType->GetEngine()->RequestContext();

		// Do the sorting
		struct {
			bool               asc;
			asIScriptContext  *cmpContext;
			asIScriptFunction *cmpFunc;
			bool operator()(void *a, void *b) const
			{
				if( !asc )
				{
					// Swap items
					void *TEMP = a;
					a = b;
					b = TEMP;
				}

				int r = 0;

				// Allow sort to work even if the array contains null handles
				if( a == 0 ) return true;
				if( b == 0 ) return false;

				// Execute object opCmp
				if( cmpFunc )
				{
					// TODO: Add proper error handling
					r = cmpContext->Prepare(cmpFunc); H_ASSERT(r >= 0);
					r = cmpContext->SetObject(a); H_ASSERT(r >= 0);
					r = cmpContext->SetArgObject(0, b); H_ASSERT(r >= 0);
					r = cmpContext->Execute();

					if( r == asEXECUTION_FINISHED )
					{
						return (int)cmpContext->GetReturnDWord() < 0;
					}
				}

				return false;
			}
		} customLess = {asc, cmpContext, cache ? cache->cmpFunc : 0};
		std::sort((void**)GetArrayItemPointer(start), (void**)GetArrayItemPointer(end), customLess);

		// Clean up
		if( cmpContext )
		{
			if( isNested )
			{
				asEContextState state = cmpContext->GetState();
				cmpContext->PopState();
				if( state == asEXECUTION_ABORTED )
					cmpContext->Abort();
			}
			else
				objType->GetEngine()->ReturnContext(cmpContext);
		}
	}
	else
	{
		// TODO: Use std::sort for primitive types too

		// Insertion sort
		asBYTE tmp[16];
		for( int i = start + 1; i < end; i++ )
		{
			Copy(tmp, GetArrayItemPointer(i));

			int j = i - 1;

			while( j >= start && Less(GetDataPointer(tmp), At(j), asc) )
			{
				Copy(GetArrayItemPointer(j + 1), GetArrayItemPointer(j));
				j--;
			}

			Copy(GetArrayItemPointer(j + 1), tmp);
		}
	}
}

// Sort with script callback for comparing elements
void ScriptArray::Sort(asIScriptFunction *func, asUINT startAt, asUINT count)
{
	// No need to sort
	if (count < 2)
		return;

	// Check if we could access invalid item while sorting
	asUINT start = startAt;
	asUINT end = asQWORD(startAt) + asQWORD(count) >= (asQWORD(1)<<32) ? 0xFFFFFFFF : startAt + count;
	if (end > buffer->numElements)
		end = buffer->numElements;

	if (start >= buffer->numElements)
	{
		asIScriptContext *ctx = asGetActiveContext();

		// Throw an exception
		if (ctx)
			ctx->SetException("Index out of bounds");

		return;
	}

	asIScriptContext *cmpContext = 0;
	bool isNested = false;

	// Try to reuse the active context
	cmpContext = asGetActiveContext();
	if (cmpContext)
	{
		if (cmpContext->GetEngine() == objType->GetEngine() && cmpContext->PushState() >= 0)
			isNested = true;
		else
			cmpContext = 0;
	}
	if (cmpContext == 0)
		cmpContext = objType->GetEngine()->RequestContext();

	// TODO: Security issue: If the array is accessed from the callback while the sort is going on the result may be unpredictable
	//       For example, the callback resizes the array in the middle of the sort
	//       Possible solution: set a lock flag on the array, and prohibit modifications while the lock flag is set

	// Bubble sort
	// TODO: optimize: Use an efficient sort algorithm
	for (asUINT i = start; i+1 < end; i++)
	{
		asUINT best = i;
		for (asUINT j = i + 1; j < end; j++)
		{
			cmpContext->Prepare(func);
			cmpContext->SetArgAddress(0, At(j));
			cmpContext->SetArgAddress(1, At(best));
			int r = cmpContext->Execute();
			if (r != asEXECUTION_FINISHED)
				break;
			if (*(bool*)(cmpContext->GetAddressOfReturnValue()))
				best = j;
		}

		// With Swap we guarantee that the array always sees all references
		// if the GC calls the EnumReferences in the middle of the sorting
		if( best != i )
			Swap(GetArrayItemPointer(i), GetArrayItemPointer(best));
	}

	if (cmpContext)
	{
		if (isNested)
		{
			asEContextState state = cmpContext->GetState();
			cmpContext->PopState();
			if (state == asEXECUTION_ABORTED)
				cmpContext->Abort();
		}
		else
			objType->GetEngine()->ReturnContext(cmpContext);
	}
}

// internal
void ScriptArray::CopyBuffer(ArrayBuffer *dst, ArrayBuffer *src)
{
	asIScriptEngine *engine = objType->GetEngine();
	if( subTypeId & asTYPEID_OBJHANDLE )
	{
		// Copy the references and increase the reference counters
		if( dst->numElements > 0 && src->numElements > 0 )
		{
			int count = dst->numElements > src->numElements ? src->numElements : dst->numElements;

			void **max = (void**)(dst->data + count * sizeof(void*));
			void **d   = (void**)dst->data;
			void **s   = (void**)src->data;

			for( ; d < max; d++, s++ )
			{
				void *tmp = *d;
				*d = *s;
				if( *d )
					engine->AddRefScriptObject(*d, objType->GetSubType());
				// Release the old ref after incrementing the new to avoid problem incase it is the same ref
				if( tmp )
					engine->ReleaseScriptObject(tmp, objType->GetSubType());
			}
		}
	}
	else
	{
		if( dst->numElements > 0 && src->numElements > 0 )
		{
			int count = dst->numElements > src->numElements ? src->numElements : dst->numElements;
			if( subTypeId & asTYPEID_MASK_OBJECT )
			{
				// Call the assignment operator on all of the objects
				void **max = (void**)(dst->data + count * sizeof(void*));
				void **d   = (void**)dst->data;
				void **s   = (void**)src->data;

				asITypeInfo *subType = objType->GetSubType();
				if (subType->GetFlags() & asOBJ_ASHANDLE)
				{
					// For objects that should work as handles we must use the opHndlAssign method
					// TODO: Must support alternative syntaxes as well
					// TODO: Move the lookup of the opHndlAssign method to Precache() so it is only done once
					StringL decl = StringL::Format("%s%s%s%s",subType->GetName(),"& opHndlAssign(const ",subType->GetName(), "&in)");
					asIScriptFunction *func = subType->GetMethodByDecl(decl.Data());
					if (func)
					{
						// TODO: Reuse active context if existing
						asIScriptContext* ctx = engine->RequestContext();
						for (; d < max; d++, s++)
						{
							ctx->Prepare(func);
							ctx->SetObject(*d);
							ctx->SetArgAddress(0, *s);
							// TODO: Handle errors
							ctx->Execute();
						}
						engine->ReturnContext(ctx);
					}
					else
					{
						// opHndlAssign doesn't exist, so try ordinary value assign instead
						for (; d < max; d++, s++)
							engine->AssignScriptObject(*d, *s, subType);
					}
				}
				else
					for( ; d < max; d++, s++ )
						engine->AssignScriptObject(*d, *s, subType);
			}
			else
			{
				// Primitives are copied byte for byte
				memcpy(dst->data, src->data, count*elementSize);
			}
		}
	}
}

// internal
// Precache some info
void ScriptArray::Precache()
{
	subTypeId = objType->GetSubTypeId();

	// Check if it is an array of objects. Only for these do we need to cache anything
	// Type ids for primitives and enums only has the sequence number part
	if( !(subTypeId & ~asTYPEID_MASK_SEQNBR) )
		return;

	// The opCmp and opEquals methods are cached because the searching for the
	// methods is quite time consuming if a lot of array objects are created.

	// First check if a cache already exists for this array type
	ArrayCache *cache = reinterpret_cast<ArrayCache*>(objType->GetUserData(ARRAY_CACHE));
	if( cache ) return;

	// We need to make sure the cache is created only once, even
	// if multiple threads reach the same point at the same time
	asAcquireExclusiveLock();

	// Now that we got the lock, we need to check again to make sure the
	// cache wasn't created while we were waiting for the lock
	cache = reinterpret_cast<ArrayCache*>(objType->GetUserData(ARRAY_CACHE));
	if( cache )
	{
		asReleaseExclusiveLock();
		return;
	}

	// Create the cache
	cache = reinterpret_cast<ArrayCache*>(userAlloc(sizeof(ArrayCache)));
	if( !cache )
	{
		asIScriptContext *ctx = asGetActiveContext();
		if( ctx )
			ctx->SetException("Out of memory");
		asReleaseExclusiveLock();
		return;
	}
	memset(cache, 0, sizeof(ArrayCache));

	// If the sub type is a handle to const, then the methods must be const too
	bool mustBeConst = (subTypeId & asTYPEID_HANDLETOCONST) ? true : false;

	asITypeInfo *subType = objType->GetEngine()->GetTypeInfoById(subTypeId);
	if( subType )
	{
		for( asUINT i = 0; i < subType->GetMethodCount(); i++ )
		{
			asIScriptFunction *func = subType->GetMethodByIndex(i);

			if( func->GetParamCount() == 1 && (!mustBeConst || func->IsReadOnly()) )
			{
				asDWORD flags = 0;
				int returnTypeId = func->GetReturnTypeId(&flags);

				// The method must not return a reference
				if( flags != asTM_NONE )
					continue;

				// opCmp returns an int and opEquals returns a bool
				bool isCmp = false, isEq = false;
				if( returnTypeId == asTYPEID_INT32 && strcmp(func->GetName(), "opCmp") == 0 )
					isCmp = true;
				if( returnTypeId == asTYPEID_BOOL && strcmp(func->GetName(), "opEquals") == 0 )
					isEq = true;

				if( !isCmp && !isEq )
					continue;

				// The parameter must either be a reference to the subtype or a handle to the subtype
				int paramTypeId;
				func->GetParam(0, &paramTypeId, &flags);

				if( (paramTypeId & ~(asTYPEID_OBJHANDLE|asTYPEID_HANDLETOCONST)) != (subTypeId &  ~(asTYPEID_OBJHANDLE|asTYPEID_HANDLETOCONST)) )
					continue;

				if( (flags & asTM_INREF) )
				{
					if( (paramTypeId & asTYPEID_OBJHANDLE) || (mustBeConst && !(flags & asTM_CONST)) )
						continue;
				}
				else if( paramTypeId & asTYPEID_OBJHANDLE )
				{
					if( mustBeConst && !(paramTypeId & asTYPEID_HANDLETOCONST) )
						continue;
				}
				else
					continue;

				if( isCmp )
				{
					if( cache->cmpFunc || cache->cmpFuncReturnCode )
					{
						cache->cmpFunc = 0;
						cache->cmpFuncReturnCode = asMULTIPLE_FUNCTIONS;
					}
					else
						cache->cmpFunc = func;
				}
				else if( isEq )
				{
					if( cache->eqFunc || cache->eqFuncReturnCode )
					{
						cache->eqFunc = 0;
						cache->eqFuncReturnCode = asMULTIPLE_FUNCTIONS;
					}
					else
						cache->eqFunc = func;
				}
			}
		}
	}

	if( cache->eqFunc == 0 && cache->eqFuncReturnCode == 0 )
		cache->eqFuncReturnCode = asNO_FUNCTION;
	if( cache->cmpFunc == 0 && cache->cmpFuncReturnCode == 0 )
		cache->cmpFuncReturnCode = asNO_FUNCTION;

	// Set the user data only at the end so others that retrieve it will know it is complete
	objType->SetUserData(cache, ARRAY_CACHE);

	asReleaseExclusiveLock();
}

// GC behaviour
void ScriptArray::EnumReferences(asIScriptEngine *engine)
{
	// TODO: If garbage collection can be done from a separate thread, then this method must be
	//       protected so that it doesn't get lost during the iteration if the array is modified

	// If the array is holding handles, then we need to notify the GC of them
	if( subTypeId & asTYPEID_MASK_OBJECT )
	{
		void **d = (void**)buffer->data;

		asITypeInfo *subType = engine->GetTypeInfoById(subTypeId);
		if ((subType->GetFlags() & asOBJ_REF))
		{
			// For reference types we need to notify the GC of each instance
			for (asUINT n = 0; n < buffer->numElements; n++)
			{
				if (d[n])
					engine->GCEnumCallback(d[n]);
			}
		}
		else if ((subType->GetFlags() & asOBJ_VALUE) && (subType->GetFlags() & asOBJ_GC))
		{
			// For value types we need to forward the enum callback
			// to the object so it can decide what to do
			for (asUINT n = 0; n < buffer->numElements; n++)
			{
				if (d[n])
					engine->ForwardGCEnumReferences(d[n], subType);
			}
		}
	}
}

// GC behaviour
void ScriptArray::ReleaseAllHandles(asIScriptEngine *)
{
	// Resizing to zero will release everything
	Resize(0);
}

void ScriptArray::AddRef() const
{
	// Clear the GC flag then increase the counter
	gcFlag = false;
	asAtomicInc(refCount);
}

void ScriptArray::Release() const
{
	// Clearing the GC flag then descrease the counter
	gcFlag = false;
	if( asAtomicDec(refCount) == 0 )
	{
		// When reaching 0 no more references to this instance
		// exists and the object should be destroyed
		this->~ScriptArray();
		userFree(const_cast<ScriptArray*>(this));
	}
}

// GC behaviour
int ScriptArray::GetRefCount()
{
	return refCount;
}

// GC behaviour
void ScriptArray::SetFlag()
{
	gcFlag = true;
}

// GC behaviour
bool ScriptArray::GetFlag()
{
	return gcFlag;
}

//--------------------------------------------
// Generic calling conventions

static void ScriptArrayFactory_Generic(asIScriptGeneric *gen)
{
	asITypeInfo *ti = *(asITypeInfo**)gen->GetAddressOfArg(0);

	*reinterpret_cast<ScriptArray**>(gen->GetAddressOfReturnLocation()) = ScriptArray::Create(ti);
}

static void ScriptArrayFactory2_Generic(asIScriptGeneric *gen)
{
	asITypeInfo *ti = *(asITypeInfo**)gen->GetAddressOfArg(0);
	asUINT length = gen->GetArgDWord(1);

	*reinterpret_cast<ScriptArray**>(gen->GetAddressOfReturnLocation()) = ScriptArray::Create(ti, length);
}

static void ScriptArrayListFactory_Generic(asIScriptGeneric *gen)
{
	asITypeInfo *ti = *(asITypeInfo**)gen->GetAddressOfArg(0);
	void *buf = gen->GetArgAddress(1);

	*reinterpret_cast<ScriptArray**>(gen->GetAddressOfReturnLocation()) = ScriptArray::Create(ti, buf);
}

static void ScriptArrayFactoryDefVal_Generic(asIScriptGeneric *gen)
{
	asITypeInfo *ti = *(asITypeInfo**)gen->GetAddressOfArg(0);
	asUINT length = gen->GetArgDWord(1);
	void *defVal = gen->GetArgAddress(2);

	*reinterpret_cast<ScriptArray**>(gen->GetAddressOfReturnLocation()) = ScriptArray::Create(ti, length, defVal);
}

static void ScriptArrayTemplateCallback_Generic(asIScriptGeneric *gen)
{
	asITypeInfo *ti = *(asITypeInfo**)gen->GetAddressOfArg(0);
	bool *dontGarbageCollect = *(bool**)gen->GetAddressOfArg(1);
	*reinterpret_cast<bool*>(gen->GetAddressOfReturnLocation()) = ScriptArrayTemplateCallback(ti, *dontGarbageCollect);
}

static void ScriptArrayAssignment_Generic(asIScriptGeneric *gen)
{
	ScriptArray *other = (ScriptArray*)gen->GetArgObject(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	*self = *other;
	gen->SetReturnObject(self);
}

static void ScriptArrayEquals_Generic(asIScriptGeneric *gen)
{
	ScriptArray *other = (ScriptArray*)gen->GetArgObject(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	gen->SetReturnByte(self->operator==(*other));
}

static void ScriptArrayFind_Generic(asIScriptGeneric *gen)
{
	void *value = gen->GetArgAddress(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	gen->SetReturnDWord(self->Find(value));
}

static void ScriptArrayFind2_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	void *value = gen->GetArgAddress(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	gen->SetReturnDWord(self->Find(index, value));
}

static void ScriptArrayFindByRef_Generic(asIScriptGeneric *gen)
{
	void *value = gen->GetArgAddress(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	gen->SetReturnDWord(self->FindByRef(value));
}

static void ScriptArrayFindByRef2_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	void *value = gen->GetArgAddress(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	gen->SetReturnDWord(self->FindByRef(index, value));
}

static void ScriptArrayAt_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();

	gen->SetReturnAddress(self->At(index));
}

static void ScriptArrayAddAt_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	void *value = gen->GetArgAddress(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->AddAt(index, value);
}

static void ScriptArrayAddAtArray_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	ScriptArray *array = (ScriptArray*)gen->GetArgAddress(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->AddAt(index, *array);
}

static void ScriptArrayRemoveAt_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->RemoveAt(index);
}

static void ScriptArrayRemoveRange_Generic(asIScriptGeneric *gen)
{
	asUINT start = gen->GetArgDWord(0);
	asUINT count = gen->GetArgDWord(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->RemoveRange(start, count);
}

static void ScriptArrayAdd_Generic(asIScriptGeneric *gen)
{
	void *value = gen->GetArgAddress(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->Add(value);
}

static void ScriptArrayRemoveLast_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->RemoveLast();
}

static void ScriptArrayCount_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();

	gen->SetReturnDWord(self->GetCount());
}

static void ScriptArrayResize_Generic(asIScriptGeneric *gen)
{
	asUINT size = gen->GetArgDWord(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();

	self->Resize(size);
}

static void ScriptArrayReserve_Generic(asIScriptGeneric *gen)
{
	asUINT size = gen->GetArgDWord(0);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->Reserve(size);
}

static void ScriptArraySortAsc_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->SortAsc();
}

static void ScriptArrayReverse_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->Reverse();
}

static void ScriptArrayIsEmpty_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	*reinterpret_cast<bool*>(gen->GetAddressOfReturnLocation()) = self->IsEmpty();
}

static void ScriptArraySortAsc2_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	asUINT count = gen->GetArgDWord(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->SortAsc(index, count);
}

static void ScriptArraySortDesc_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->SortDesc();
}

static void ScriptArraySortDesc2_Generic(asIScriptGeneric *gen)
{
	asUINT index = gen->GetArgDWord(0);
	asUINT count = gen->GetArgDWord(1);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->SortDesc(index, count);
}

static void ScriptArraySortCallback_Generic(asIScriptGeneric *gen)
{
	asIScriptFunction *callback = (asIScriptFunction*)gen->GetArgAddress(0);
	asUINT startAt = gen->GetArgDWord(1);
	asUINT count = gen->GetArgDWord(2);
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->Sort(callback, startAt, count);
}

static void ScriptArrayAddRef_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->AddRef();
}

static void ScriptArrayRelease_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->Release();
}

static void ScriptArrayGetRefCount_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	*reinterpret_cast<int*>(gen->GetAddressOfReturnLocation()) = self->GetRefCount();
}

static void ScriptArraySetFlag_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	self->SetFlag();
}

static void ScriptArrayGetFlag_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	*reinterpret_cast<bool*>(gen->GetAddressOfReturnLocation()) = self->GetFlag();
}

static void ScriptArrayEnumReferences_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	asIScriptEngine *engine = *(asIScriptEngine**)gen->GetAddressOfArg(0);
	self->EnumReferences(engine);
}

static void ScriptArrayReleaseAllHandles_Generic(asIScriptGeneric *gen)
{
	ScriptArray *self = (ScriptArray*)gen->GetObject();
	asIScriptEngine *engine = *(asIScriptEngine**)gen->GetAddressOfArg(0);
	self->ReleaseAllHandles(engine);
}

static void RegisterScriptArray_Generic(TypeRegistry* pTypeRegistry)
{
	int r = 0;
	UNUSED_VAR(r);

	pTypeRegistry->GetEngine()->SetTypeInfoUserDataCleanupCallback(CleanupTypeInfoArrayCache, ARRAY_CACHE);
	// Register the array type as a template
	r = pTypeRegistry->RegisterType("Array<class T>", 0, asOBJ_REF | asOBJ_GC | asOBJ_TEMPLATE, H_FILE_LINE); H_ASSERT(r);
	r = pTypeRegistry->RegisterVariableFunction("Array<class T>", &GetScriptArrayVariableData); H_ASSERT(r);

	asIScriptEngine* engine = pTypeRegistry->GetEngine();
	bool bResult;
	// Register a callback for validating the subtype before it is used
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "bool f(%s)", { { "in", "int", false, true }, { "out", "bool", false, true } }, asBEHAVE_TEMPLATE_CALLBACK, asCALL_GENERIC, asFUNCTION(ScriptArrayTemplateCallback_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Templates receive the object type as the first parameter. To the script writer this is hidden
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s)", { { "in", "int", false, true } }, asBEHAVE_FACTORY, asCALL_GENERIC, asFUNCTION(ScriptArrayFactory_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s) explicit", { { "in", "int", false, true }, { "length", "uint" } }, asBEHAVE_FACTORY, asCALL_GENERIC, asFUNCTION(ScriptArrayFactory2_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s)", { { "in", "int", false, true }, { "length", "uint" }, { "in", "T", EVariableTypeDataState::ConstRef, "value" } }, asBEHAVE_FACTORY, asCALL_GENERIC, asFUNCTION(ScriptArrayFactoryDefVal_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Register the factory that will be used for initialization lists
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "Array<T>@ f(%s) {repeat T}", { { "in", "int", EVariableTypeDataState::Ref, "type"}, {"in", "int", EVariableTypeDataState::Ref, "list"} }, asBEHAVE_LIST_FACTORY, asCALL_GENERIC, asFUNCTION(ScriptArrayListFactory_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// The memory management methods
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f()", { }, asBEHAVE_ADDREF, asCALL_GENERIC, asFUNCTION(ScriptArrayAddRef_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f()", { }, asBEHAVE_RELEASE, asCALL_GENERIC, asFUNCTION(ScriptArrayRelease_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// The index operator returns the template subtype
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "opIndex", "T", EVariableTypeDataState::Ref }, { {"index", "uint"} }, asFUNCTION(ScriptArrayAt_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "opIndex", "T", EVariableTypeDataState::ConstRef }, { {"index", "uint"} }, asFUNCTION(ScriptArrayAt_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// The assignment operator
	bResult = pTypeRegistry->RegisterClassOperatorOverloadGenericFuncPtr("Array<T>", { "opAssign", "Array<T>", EVariableTypeDataState::Ref }, asFUNCTION(ScriptArrayAssignment_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Other methods
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "AddAt", "void" }, { {"index", "uint"}, {"in", "T", EVariableTypeDataState::ConstRef, "value"} }, asFUNCTION(ScriptArrayAddAt_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "AddAt", "void" }, { {"index", "uint"}, {"arr", "Array<T>", EVariableTypeDataState::ConstRef} }, asFUNCTION(ScriptArrayAddAtArray_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Add", "void" }, { {"in", "T", EVariableTypeDataState::ConstRef, "value"} }, asFUNCTION(ScriptArrayAdd_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "RemoveAt", "void" }, { {"index", "uint"} }, asFUNCTION(ScriptArrayRemoveAt_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "RemoveLast", "void" }, { }, asFUNCTION(ScriptArrayRemoveLast_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "RemoveRange", "void" }, { {"start", "uint"}, {"count", "uint"} }, asFUNCTION(ScriptArrayRemoveRange_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
#if AS_USE_ACCESSORS != 1
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Size", "uint", EVariableTypeDataState::Const }, { }, asFUNCTION(ScriptArrayCount_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
#endif
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Reserve", "void" }, { {"length", "uint"} }, asFUNCTION(ScriptArrayReserve_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Resize", "void" }, { {"length", "uint"} }, asFUNCTION(ScriptArrayResize_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "SortAsc", "void" }, { }, asFUNCTION(ScriptArraySortAsc_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "SortAsc", "void" }, { {"startAt", "uint"}, {"count", "uint"} }, asFUNCTION(ScriptArraySortAsc2_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "SortDesc", "void" }, { }, asFUNCTION(ScriptArraySortDesc_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "SortDesc", "void" }, { {"startAt", "uint"}, {"count", "uint"} }, asFUNCTION(ScriptArraySortDesc2_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Reverse", "void" }, { }, asFUNCTION(ScriptArrayReverse_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// The token 'if_handle_then_const' tells the engine that if the type T is a handle, then it should refer to a read-only object
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Find", "int", EVariableTypeDataState::Const }, { {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asFUNCTION(ScriptArrayFind_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// TODO: It should be "int find(const T&in value, uint startAt = 0) const"
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "Find", "int", EVariableTypeDataState::Const }, { {"startAt", "uint"}, {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asFUNCTION(ScriptArrayFind2_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "FindByRef", "int", EVariableTypeDataState::Const }, { {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asFUNCTION(ScriptArrayFindByRef_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// TODO: It should be "int findByRef(const T&in value, uint startAt = 0) const"
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "FindByRef", "int", EVariableTypeDataState::Const }, { {"startAt", "uint"}, {"in", "T", EVariableTypeDataState::ConstRef, "if_handle_then_const value"} }, asFUNCTION(ScriptArrayFindByRef2_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "opEquals", "bool", EVariableTypeDataState::Const }, { {"in", "Array<T>", EVariableTypeDataState::ConstRef} }, asFUNCTION(ScriptArrayEquals_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "IsEmpty", "bool", EVariableTypeDataState::Const }, { }, asFUNCTION(ScriptArrayIsEmpty_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	// Sort with callback for comparison
	r = engine->RegisterFuncdef("bool Array<T>::less(const T&in if_handle_then_const a, const T&in if_handle_then_const b)");
	bResult = pTypeRegistry->RegisterClassMethodGenericFuncPtr("Array<T>", { "sort", "void" }, { {"in", "less", EVariableTypeDataState::ConstRef }, {"startAt", "uint", EVariableTypeDataState::None, " = 0"}, {"count", "uint", EVariableTypeDataState::None, " =  uint(-1)"} }, asFUNCTION(ScriptArraySortCallback_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

	// Register GC behaviours in case the array needs to be garbage collected
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "int f()", { }, asBEHAVE_GETREFCOUNT, asCALL_GENERIC, asFUNCTION(ScriptArrayGetRefCount_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f()", { }, asBEHAVE_SETGCFLAG, asCALL_GENERIC, asFUNCTION(ScriptArraySetFlag_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "bool f()", { }, asBEHAVE_GETGCFLAG, asCALL_GENERIC, asFUNCTION(ScriptArrayGetFlag_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f(%s)", { { "in", "int", EVariableTypeDataState::Ref } }, asBEHAVE_ENUMREFS, asCALL_GENERIC, asFUNCTION(ScriptArrayEnumReferences_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");
	bResult = pTypeRegistry->RegisterManagedClassConstructor("Array<T>", "void f(%s)", { { "in", "int", EVariableTypeDataState::Ref } }, asBEHAVE_RELEASEREFS, asCALL_GENERIC, asFUNCTION(ScriptArrayReleaseAllHandles_Generic), H_FILE_LINE); H_ASSERT(bResult, "Failed to register Array func");

}

END_AS_NAMESPACE
