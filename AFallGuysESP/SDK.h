#pragma once
#define _XM_NO_INTRINSICS_
#include "iostream"
#include <set>
#include <vector>
#include <memory>
#include <functional>
#include "RemoteType.h"
#include "FMath.h"
#include <optional>
#include <array>
#include <directxmath.h>

namespace UnityOffset {
	const size_t OffsetGameObjectManager = 0x17B70E8;
	const size_t OffsetUnityConstTable = 0x17B4E68;
}

namespace FG {
	using namespace UnityOffset;
	size_t GetRemoteDllBase(const wchar_t* name);

	template<typename T>
	class dynamic_array_data { // 0x20
	public:
		RemoteType<std::pair<uint64_t, T*>> buffer;
		uint32_t MemberLabel; // 0x8
		size_t count; // 0x10
		size_t size; // 0x18

		RemoteType<T> GetItem(int i)
		{
			if (i >= count)
			{
				return RemoteType<T>{};
			}
			return RemoteType<T>(buffer[i].second);
		}
	};

	class Il2CppObject;
	class Object {
	public:
		uint64_t vPtr;
		int unk_0x0008; // 0x8
		int tabIndex; //0xc
		uint64_t unk_0x0010[3];
		RemoteType<Il2CppObject> il2CppObj;
	};

	class Transform;
	class Component;
	class GameObject: public Object {
	public:
		dynamic_array_data<Component> Components; // 0x30
		char pad_0x0050[0x6]; //0x50
		char active;
		char pad_0x0057[0x8]; //0x57
		char* name; // 0x60

		std::string GetName()
		{
			auto _name = RemoteType<std::array<char, 96>>((size_t)name).share();
			return (char*)_name.get();
		}

		Transform GetTransform();

		RemoteType<Component> GetBasicComponent(std::string name);
		RemoteType<Component> GetComponent(std::string name);
	};

	class Component: public Object{
	public:
		ReflectPointer<GameObject*> parent; // 0x30

		std::string GetParentName()
		{
			if (!parent.address())
			{
				return "";
			}

			return parent->GetName();
		}

		std::string GetBasicName();
		std::string GetName();
	};

	struct TransformData
	{
		char pad_0x0000[0x18]; //0x0
		void*		pTransformArray;
		void*		pTransformIndices; // 0x20
		void*		unk_0x0028; // 0x28
		RemoteType<RemoteType<Transform>> ppRoot;
	};

	class Transform;
	struct TransformAccessReadOnly
	{
		RemoteType<TransformData>		pTransformData;
		int			index; // 0x8
	};

	class Transform : public Component {
	public:
		TransformAccessReadOnly transformAccess; // 0x38
		char padding_0x0048[0x48]; // 0x48
		RemoteType<Transform> parentTransform; // 0x90

		FVector GetPosition() {
			// CalculateGlobalPosition
			// https://www.unknowncheats.me/forum/unity/280145-unity-external-bone-position-transform.html
			__m128 result;
			const __m128 mulVec0 = { -2.000, 2.000, -2.000, 0.000 };
			const __m128 mulVec1 = { 2.000, -2.000, -2.000, 0.000 };
			const __m128 mulVec2 = { -2.000, -2.000, 2.000, 0.000 };
			struct Matrix34
			{
				__m128 vec0;
				__m128 vec1;
				__m128 vec2;
			};

			int safeCount = 1024;
			transformAccess.index = std::min<int>(1024, transformAccess.index);

			TransformData trans = transformAccess.pTransformData;
			if (!trans.pTransformArray || !trans.pTransformIndices)
			{
				return FVector{};
			}
			size_t sizeMatriciesBuf = sizeof(Matrix34) * transformAccess.index + sizeof(Matrix34);
			size_t sizeIndicesBuf = sizeof(int) * transformAccess.index + sizeof(int);

			auto matriciesBuf = std::make_unique<char[]>(sizeMatriciesBuf);
			auto indicesBuf = std::make_unique<char[]>(sizeIndicesBuf);

			RPM(trans.pTransformArray, matriciesBuf.get(), sizeMatriciesBuf, 0);
			RPM(trans.pTransformIndices, indicesBuf.get(), sizeIndicesBuf, 0);

			result = *(__m128*)((size_t)matriciesBuf.get() + 0x30 * transformAccess.index);
			int transformIndex = *(int*)((size_t)indicesBuf.get() + 0x4 * transformAccess.index);

			while (transformIndex >= 0 && safeCount > 0)
			{
				if (transformIndex > transformAccess.index)
				{
					return FVector{};
				}
				safeCount--;
				Matrix34 matrix34 = *(Matrix34*)((size_t)matriciesBuf.get() + 0x30 * transformIndex);

				__m128 xxxx = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x00));	// xxxx
				__m128 yyyy = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x55));	// yyyy
				__m128 zwxy = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x8E));	// zwxy
				__m128 wzyw = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0xDB));	// wzyw
				__m128 zzzz = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0xAA));	// zzzz
				__m128 yxwy = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x71));	// yxwy
				__m128 tmp7 = _mm_mul_ps(*(__m128*)(&matrix34.vec2), result);

				result = _mm_add_ps(
					_mm_add_ps(
						_mm_add_ps(
							_mm_mul_ps(
								_mm_sub_ps(
									_mm_mul_ps(_mm_mul_ps(xxxx, mulVec1), zwxy),
									_mm_mul_ps(_mm_mul_ps(yyyy, mulVec2), wzyw)),
								_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0xAA))),
							_mm_mul_ps(
								_mm_sub_ps(
									_mm_mul_ps(_mm_mul_ps(zzzz, mulVec2), wzyw),
									_mm_mul_ps(_mm_mul_ps(xxxx, mulVec0), yxwy)),
								_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x55)))),
						_mm_add_ps(
							_mm_mul_ps(
								_mm_sub_ps(
									_mm_mul_ps(_mm_mul_ps(yyyy, mulVec0), yxwy),
									_mm_mul_ps(_mm_mul_ps(zzzz, mulVec1), zwxy)),
								_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x00))),
							tmp7)), *(__m128*)(&matrix34.vec0));

				transformIndex = *(int*)((size_t)indicesBuf.get() + 0x4 * transformIndex);
			}

			return FVector(result);
		}

		RemoteType<GameObject> GetParentGameObject() {
			return parentTransform->parent.toRemoteType();
		}

		Transform GetParentTransform() {
			return parentTransform.get();
		}

		Transform GetRoot() {
			auto ppRoot = transformAccess.pTransformData->ppRoot;
			auto pRoot = ppRoot.get();
			return pRoot.get();
		}
	};

	class RectTransform: public Transform {
	public:
	};

	class Camera : public Component
	{
	public:
		char pad_0x0038[0xa4]; // 0x38
		float matrix[16];

		DirectX::XMMATRIX GetXMatrix()
		{
			//return *(DirectX::XMMATRIX*)matrix;
			return DirectX::XMMatrixTranspose(*(DirectX::XMMATRIX*)matrix);
		}

		FVector ToScreen(__m128 worldLocation)
		{
			return WorldToScreen(worldLocation, GetXMatrix());
		}
	};

	class MonoBehaviour : public Component {

	};

	class Il2CppImage;
	class Il2CppAssembly {
	public:
		RemoteType<Il2CppImage> image;
		unsigned int token;
	};

	class Il2CppImage {
	public:
		char* name;
		char* nameNoExt;
		RemoteType<Il2CppAssembly> assembly;
	};

	const struct Il2CppTypeDefinition
	{
		int nameIndex;
		int namespaceIndex;
		int byvalTypeIndex;
		int byrefTypeIndex;
		int declaringTypeIndex;
		int parentIndex;
		int elementTypeIndex;
		int rgctxStartIndex;
		int rgctxCount;
		int genericContainerIndex;
		unsigned int flags;
		int fieldStart;
		int methodStart;
		int eventStart;
		int propertyStart;
		int nestedTypesStart;
		int interfacesStart;
		int vtableStart;
		int interfaceOffsetsStart;
		unsigned __int16 method_count;
		unsigned __int16 property_count;
		unsigned __int16 field_count;
		unsigned __int16 event_count;
		unsigned __int16 nested_type_count;
		unsigned __int16 vtable_count;
		unsigned __int16 interfaces_count;
		unsigned __int16 interface_offsets_count;
		unsigned int bitfield;
		unsigned int token;
	};

	struct __declspec(align(8)) Il2CppType
	{
		void*		 data;
		unsigned __int32 attrs : 16;
		__int32 type : 8;
		unsigned __int32 num_mods : 6;
		unsigned __int32 byref : 1;
		unsigned __int32 pinned : 1;
	};

	class Il2CppClass;
	struct FieldInfo
	{
		const char* name;
		RemoteType<Il2CppType> type;
		RemoteType<Il2CppClass> parent;
		int offset;
		unsigned int token;

		std::string GetName()
		{
			auto _name = RemoteType<std::array<char, 64>>((size_t)name).share();
			return (char*)_name.get();
		}
	};

	struct ParameterInfo
	{
		const char* name;
		int position;
		unsigned int token;
		RemoteType<Il2CppType> parameter_type;
		char DONT_CARE[0x1a]; // 0x0030;
		uint8_t	parameters_count; // 0x4a
	};

	struct MethodInfo
	{
		void(__fastcall* methodPointer)();
		void* (__fastcall* invoker_method)(void(__fastcall*)(), MethodInfo*, void*, void**);
		const char* name;
		RemoteType<Il2CppClass> klass;
		RemoteType<Il2CppType> return_type;
		RemoteType<ParameterInfo> parameters;
	};

	class Il2CppClass {
	public:
		RemoteType<Il2CppImage> image;
		void* gc_desc;
		char* name;
		char* namespaze;
		Il2CppType byval_arg;
		Il2CppType this_arg;
		RemoteType<Il2CppClass> element_class;
		RemoteType<Il2CppClass> castClass;
		RemoteType<Il2CppClass> declaringType;
		RemoteType<Il2CppClass> parent;
		void* generic_class; // Il2CppGenericClass *
		RemoteType<Il2CppTypeDefinition> typeDefinition;
		void* interopData; //Il2CppInteropData*
		RemoteType<Il2CppClass> klass;
		RemoteType<FieldInfo> fields;
		void* events; // EventInfo*
		void* properties; // PropertyInfo*
		MethodInfo** methods;
		Il2CppClass** nestedTypes;
		Il2CppClass** implementedInterfaces;
		void* interfaceOffsets;
		void* static_fields;
		void* rgctx_data;
		Il2CppClass** typeHierarchy;
		unsigned int initializationExceptionGCHandle;
		unsigned int cctor_started;
		unsigned int cctor_finished;
		unsigned __int64 cctor_thread;
		int genericContainerIndex;
		unsigned int instance_size;
		unsigned int actualSize;
		unsigned int element_size;
		int native_size;
		unsigned int static_fields_size;
		unsigned int thread_static_fields_size;
		int thread_static_fields_offset;
		unsigned int flags;
		unsigned int token;
		unsigned __int16 method_count;
		unsigned __int16 property_count;
		unsigned __int16 field_count;
		unsigned __int16 event_count;
		unsigned __int16 nested_type_count;
		unsigned __int16 vtable_count;
		unsigned __int16 interfaces_count;
		unsigned __int16 interface_offsets_count;

		std::string GetName() {
			auto _name = RemoteType<std::array<char, 64>>((size_t)name).share();
			return (char*)_name.get();
		}

		std::string GetNameSapce() {
			auto _namespaze = RemoteType<std::array<char, 96>>((size_t)namespaze).share();
			return (char*)_namespaze.get();
		}

		std::string GetFullName() {
			return GetNameSapce() + "::" + GetName();
		}

		int GetMemberOffset(std::string name) {
			if (!fields.address())
				return -1;

			for (int i = 0; i < field_count; i++)
			{
				FieldInfo fi = fields[i];
				auto fieldName = fi.GetName();
				if (fieldName == name)
				{
					return fi.offset;
				}
			}
			return -1;
		}
	};

	class Il2CppObject {
	public:
		RemoteType<Il2CppClass> klass;
		void* monitor;
	};

	template<typename T>
	class Il2CppWrap : public Il2CppObject {
	public:
		RemoteType<T> obj;
	};
	using Il2CppGameObject = Il2CppWrap<GameObject>;

	struct GameObjectPtrLink
	{
		ReflectPointer<GameObjectPtrLink*> previousObjectLink; // 0x0000
		ReflectPointer<GameObjectPtrLink*> nextObjectLink; // 0x0008
		RemoteType<GameObject> object; // 0x0010
	};

	class GameObjectManager
	{
	public:
		ReflectPointer<GameObjectPtrLink*> lastTaggedObject; // 0x0000
		ReflectPointer<GameObjectPtrLink*> taggedObjects; // 0x0008
		ReflectPointer<GameObjectPtrLink*> lastActiveObject; // 0x0010
		ReflectPointer<GameObjectPtrLink*> activeObjects; // 0x0018

		void EnumObjects(bool bTagged, std::function<bool(RemoteType<GameObject>)> callback)
		{
			auto ptr = bTagged ? taggedObjects : activeObjects;
			if (!ptr.addr)
				return;

			auto last = ptr.addr;
			ptr = ptr->nextObjectLink;
			while (ptr.addr && ptr.addr != last)
			{
				auto obj = ptr->object;
				if (obj.address() == 0)
					break;

				if (!callback(obj))
				{
					break;
				}
				ptr = ptr->nextObjectLink;
			}
		}

		RemoteType<GameObject> GetObjectByName(const char* name, bool tagged = false)
		{
			auto result = RemoteType<GameObject>{};
			EnumObjects(tagged, [&](RemoteType<GameObject> obj) -> bool {
				auto objName = obj->GetName();
				if (objName == name)
				{
					result = RemoteType<GameObject>{ obj.address() };
					return false;
				}
				return true;
			});
			
			return result;
		}
	};
	std::unique_ptr<GameObjectManager, _RemoteType_Deleter> GetGameObjectManager();
}
