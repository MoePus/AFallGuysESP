#pragma once
#include "iostream"
#include <tchar.h>
#include <memory>
#include "windows.h"

HANDLE getVictimHandle(bool force = false);
BOOL RPM(
	LPCVOID lpBaseAddress,
	LPVOID lpBuffer,
	size_t nSize,
	size_t* lpNumberOfBytesRead
);

struct _RemoteType_Deleter
{
	template <class T>
	void operator() (T* p)
	{
		free(p);
	}
};

template<typename T>
class RemoteType {
	size_t addr;
public:
	T* address()
	{
		return (T*)addr;
	}

	explicit RemoteType(size_t _addr) :addr(_addr) {
	};

	explicit RemoteType(void* _addr) :addr((size_t)_addr) {
	};

	explicit RemoteType(T* _addr) :addr((size_t)_addr) {
	};

	explicit RemoteType() :addr(0) {
	};

	template<typename V>
	RemoteType(V) = delete;

	template<typename U = std::remove_pointer<T>::type>
	typename std::enable_if<std::is_pointer<T>::value, RemoteType<U>>::type operator[] (const int index)
	{
		T result;
		memset(&result, 0, sizeof(result));
		size_t rb;
		RPM((void*)(addr + sizeof(size_t)*index), &result, sizeof(T), &rb);
		RemoteType<U> a((size_t)result);
		return a;
	};

	template<typename U = T>
	typename std::enable_if<
		!std::is_pointer<U>::value, U>::type
		operator[](const int index)
	{
		U result;
		memset(&result, 0, sizeof(result));
		size_t rb;
		RPM((void*)(addr + sizeof(U)*index), &result, sizeof(U), &rb);
		return result;
	};

	template<typename U = T>
	typename std::enable_if<!std::is_pointer<U>::value,
		std::unique_ptr<U[], _RemoteType_Deleter> >::type
		shareArray(int size) const
	{
		U* result = (U*)malloc(sizeof(T)*size);
		size_t rb;
		RPM((void*)addr,
			result, sizeof(U)*size, &rb);
		return std::unique_ptr<U[], _RemoteType_Deleter>(result);
	}

	template<typename U = std::remove_pointer<T>::type>
	typename std::enable_if<std::is_pointer<T>::value,
		std::unique_ptr<RemoteType<U>[], _RemoteType_Deleter> >::type
		shareArray(int size) const
	{
		RemoteType<U>* result = (RemoteType<U>*)malloc(sizeof(T)*size);
		size_t rb;
		RPM((void*)addr,
			result, sizeof(T)*size, &rb);
		return std::unique_ptr<RemoteType<U>[], _RemoteType_Deleter>(result);
	}

	T get() const
	{
		T result;
		memset(&result, 0, sizeof(result));
		size_t rb;
		RPM((void*)addr, &result, sizeof(T), &rb);
		return result;
	}

	std::unique_ptr<T,_RemoteType_Deleter> share() const
	{
		T* result = (T*)malloc(sizeof(T));
		size_t rb;
		RPM((void*)addr,
			result, sizeof(T), &rb);
		return std::unique_ptr<T, _RemoteType_Deleter>(result);
	}

	operator const T() const
	{
		return this->get();
	};

	template<typename U = std::remove_pointer<T>::type>
	typename std::enable_if<std::is_pointer<T>::value, RemoteType<U>>::type
		operator*() const
	{
		T result;
		memset(&result, 0, sizeof(result));
		size_t rb;
		RPM((void*)(addr), &result, sizeof(T), &rb);
		RemoteType<U> a((size_t)result);
		return a;
	}

	template<typename U = T>
	typename std::enable_if<!std::is_pointer<T>::value, U>::type
		operator*() const
	{
		return this->get();
	}

	template<typename U = T>
	typename std::enable_if<std::is_class<U>::value, std::unique_ptr<U, _RemoteType_Deleter>>::type
	operator->() const noexcept
	{	
		U* result = (U*)malloc(sizeof(U));
		size_t rb;
		RPM((void*)(addr), result, sizeof(U), &rb);
		return std::unique_ptr<U, _RemoteType_Deleter>(result);
	}

	template<typename U = std::remove_pointer<T>::type>
	typename std::enable_if<std::is_pointer<T>::value, RemoteType<U>>::type
		operator->() const noexcept
	{
		return this->operator*();
	}

	template<typename W>
	RemoteType<W> cast()
	{
		return RemoteType<W>(addr);
	}
};

template<typename T>
class ReflectPointer {
public:
	size_t addr;
	ReflectPointer(T _addr) :addr((size_t)_addr) {
		static_assert(std::is_pointer<T>::value, "ReflectPointer must be a pointer.");
	};
	ReflectPointer() :addr(0) {
		static_assert(std::is_pointer<T>::value, "ReflectPointer must be a pointer.");
	};

	T address()
	{
		return (T)addr;
	}

	template<typename U = std::remove_pointer<T>::type>
	RemoteType<U> toRemoteType() const
	{
		return RemoteType<U>(addr);
	}

	template<typename W>
	ReflectPointer<W> cast()
	{
		return ReflectPointer<W>((W)addr);
	}

	template<typename U = std::remove_pointer<T>::type>
	RemoteType<U> operator->() const
	{
		return toRemoteType();
	}

	template<typename U = std::remove_pointer<T>::type>
	U get() const
	{
		return toRemoteType().get();
	}

	template<typename U = std::remove_pointer<T>::type>
	std::unique_ptr<U, _RemoteType_Deleter> share() const
	{
		return toRemoteType().share();
	}

	template<typename U = std::remove_pointer<T>::type>
	typename std::enable_if<std::is_pointer<U>::value, ReflectPointer<U>>::type
		operator*() const
	{
		return ReflectPointer<U>(this->get());
	}
};