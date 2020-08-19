#include "SDK.h"
#include <mutex>
#include <memory>
#include <unordered_map>
#include "ExternalUtil.h"

using namespace FG;
using namespace std;

std::unordered_map<std::wstring, size_t> dllBaseCache;
size_t FG::GetRemoteDllBase(const wchar_t* name)
{
	{
		auto it = dllBaseCache.find(name);
		if (it != dllBaseCache.end())
		{
			return it->second;
		}
	}

	PROCESS_BASIC_INFORMATION BasicInfo;
	DWORD dwReturnLength = 0;
	auto result = NtQueryInformationProcess(getVictimHandle(), 0,
		&BasicInfo, sizeof(PROCESS_BASIC_INFORMATION), &dwReturnLength);

	auto peb = RemoteType<PEB>(BasicInfo.PebBaseAddress);
	auto ldr = ReflectPointer<decltype(peb->Ldr)>(peb->Ldr);

	auto entry_tail = ReflectPointer<PLIST_ENTRY>((PLIST_ENTRY)(ldr.addr + offsetof(PEB_LDR_DATA, InMemoryOrderModuleList)));
	auto entry = ReflectPointer<PLIST_ENTRY>(entry_tail->Flink);
	auto endaddr = entry.addr;

	do {
		auto mod = entry.cast<LDR_DATA_TABLE_ENTRY*>();
		entry = ReflectPointer<PLIST_ENTRY>(entry->Flink);

		_UNICODE_STRING dllName = mod->BaseDllName;
		{
			auto buffer = std::make_unique<wchar_t[]>(dllName.Length/2 + 1);
			memset(buffer.get(), 0, dllName.Length / 2 + 1);
			RPM(dllName.Buffer, buffer.get(), dllName.Length, 0);
			if (_wcsnicmp(name, buffer.get(), dllName.Length/2) == 0)
			{
				dllBaseCache[name] = (size_t)mod->DllBase;
				return (size_t)mod->DllBase;
			}
		}
	} while (entry.addr != endaddr);

	return 0;
}

std::unique_ptr<GameObjectManager, _RemoteType_Deleter> FG::GetGameObjectManager()
{
	auto base = GetRemoteDllBase(L"UnityPlayer.dll");
	return (*RemoteType<GameObjectManager*>(base + OffsetGameObjectManager)).share();
}

Transform FG::GameObject::GetTransform() {
	return Components.GetItem(0).cast<Transform>().get();
}

RemoteType<Component> FG::GameObject::GetBasicComponent(std::string name)
{
	for (int i = 0; i < std::min<int>(32, static_cast<int>(Components.size)); i++)
	{
		auto comp = Components.GetItem(i);
		auto compName = comp->GetBasicName();
		if (compName == name)
		{
			return comp;
		}
	}
	return RemoteType<Component>{};
}

RemoteType<Component> FG::GameObject::GetComponent(std::string name)
{
	for (int i = 0; i < std::min<int>(32, static_cast<int>(Components.size)); i++)
	{
		auto comp = Components.GetItem(i);
		auto compName = comp->GetName();
		if (compName == name)
		{
			return comp;
		}
	}
	return RemoteType<Component>{};
}

std::string FG::Component::GetBasicName()
{
	auto conTable = GetRemoteDllBase(L"UnityPlayer.dll") + OffsetUnityConstTable;
	size_t pString = RemoteType<size_t>((tabIndex >> 21) * size_t(8) + conTable);
	auto _name = RemoteType<std::array<char, 64>>(*RemoteType<size_t>(pString + 16)).share();
	return (char*)_name.get();
}

std::string FG::Component::GetName()
{
	if (!il2CppObj.address())
	{
		return GetBasicName();
	}
	return il2CppObj->klass->GetFullName();
}