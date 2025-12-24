// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include "CoreMinimal.h"

namespace PCGExMT
{
	using FExecuteCallback = std::function<void()>;
	using FCompletionCallback = std::function<void()>;
	using FEndCallback = std::function<void(const bool)>;
	using FSimpleCallback = std::function<void()>;


	struct PCGEXCORE_API FScope
	{
		int32 Start = -1;
		int32 Count = -1;
		int32 End = -1;
		int32 LoopIndex = -1;

		FScope() = default;

		FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex = -1);

		~FScope() = default;
		bool IsValid() const { return Start != -1 && Count > 0; }
		int32 GetNextScopeIndex() const { return LoopIndex + 1; }
		void GetIndices(TArray<int32>& OutIndices) const;

		static int32 GetMaxRange(const TArray<FScope>& InScopes)
		{
			int32 MaxRange = 0;
			for (const FScope& S : InScopes) { MaxRange = FMath::Max(MaxRange, S.Count); }
			return MaxRange;
		}

		template <typename T>
		FORCEINLINE TArrayView<T> GetView(TArray<T>& InArray) const { return TArrayView<T>(InArray.GetData() + Start, Count); }

		template <typename T>
		FORCEINLINE TArrayView<const T> GetView(const TArray<T>& InArray) const { return TArrayView<T>(InArray.GetData() + Start, Count); }
	};
}

#pragma region MT MACROS

#ifndef PCGEX_MT_MACROS
#define PCGEX_MT_MACROS

#define PCGEX_ASYNC_TASK_NAME(_NAME) virtual FString DEBUG_HandleId() const override { return TEXT(""#_NAME); }

#define PCGEX_ASYNC_GROUP_CHKD_VOID(_MANAGER, _NAME) TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; if(!_NAME){ return; }
#define PCGEX_ASYNC_SUBGROUP_CHKD_RET(_MANAGER, _PARENT, _NAME, _RET) TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME), _PARENT) : nullptr; if(!_NAME){ return _RET; }
#define PCGEX_ASYNC_SUBGROUP_REQ_CHKD_VOID(_MANAGER, _PARENT, _NAME) TSharedPtr<PCGExMT::FTaskGroup> _NAME = (_MANAGER && _PARENT) ? _MANAGER->TryCreateTaskGroup(FName(#_NAME), _PARENT) : nullptr; if(!_NAME){ return; }
#define PCGEX_ASYNC_GROUP_CHKD_RET(_MANAGER, _NAME, _RET) TSharedPtr<PCGExMT::FTaskGroup> _NAME= _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; if(!_NAME){ return _RET; }
#define PCGEX_ASYNC_GROUP_CHKD(_MANAGER, _NAME) PCGEX_ASYNC_GROUP_CHKD_RET(_MANAGER, _NAME, false);

#define PCGEX_ASYNC_HANDLE_CHKD_VOID(_MANAGER, _HANDLE) if (!_MANAGER->TryRegisterHandle(_HANDLE)){return;}
#define PCGEX_ASYNC_HANDLE_CHKD_CUSTOM(_MANAGER, _HANDLE, _RET) if (!_MANAGER->TryRegisterHandle(_HANDLE)){return _RET;}
#define PCGEX_ASYNC_HANDLE_CHKD(_MANAGER, _HANDLE) PCGEX_ASYNC_HANDLE_CHKD_CUSTOM(_MANAGER, _HANDLE, false);

#define PCGEX_ASYNC_RELEASE_TOKEN(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); _TOKEN.Reset(); }
#define PCGEX_ASYNC_RELEASE_CAPTURED_TOKEN(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); }
#define PCGEX_ASYNC_RELEASE_TOKEN_ELSE(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); _TOKEN.Reset(); }else

#define PCGEX_SHARED_THIS_DECL TSharedPtr<std::remove_reference_t<decltype(*this)>> ThisPtr = SharedThis(this);
#define PCGEX_ASYNC_THIS_DECL TWeakPtr<std::remove_reference_t<decltype(*this)>> AsyncThis = SharedThis(this);
#define PCGEX_ASYNC_THIS_CAPTURE AsyncThis = TWeakPtr<std::remove_reference_t<decltype(*this)>>(SharedThis(this))
#define PCGEX_ASYNC_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> This = AsyncThis.Pin(); if (!This) { return; }
#define PCGEX_ASYNC_THIS_RET(_RET) const TSharedPtr<std::remove_reference_t<decltype(*this)>> This = AsyncThis.Pin(); if (!This) { return _RET; }
#define PCGEX_ASYNC_NESTED_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> NestedThis = AsyncThis.Pin(); if (!NestedThis) { return; }

#define PCGEX_ASYNC_CHKD_VOID(_MANAGER) if (!_MANAGER->IsAvailable()) { return; }
#define PCGEX_ASYNC_CHKD(_MANAGER) if (!_MANAGER->IsAvailable()) { return false; }

#define PCGEX_LAUNCH(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); TaskManager->Launch(Task);
#define PCGEX_LAUNCH_INTERNAL(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); Launch(Task);

#define PCGEX_SCOPE_LOOP(_VAR) for(int _VAR = Scope.Start; _VAR < Scope.End; _VAR++)
#define PCGEX_SUBSCOPE_LOOP(_VAR) for(int _VAR = SubScope.Start; _VAR < SubScope.End; _VAR++)

#define PCGEX_PARALLEL_FOR(_NUM, ...) { const int32 ITERATIONS = _NUM; if(ITERATIONS < 512){ for(int i = 0; i < ITERATIONS; i++){ __VA_ARGS__ }}else{ ParallelFor(ITERATIONS, [&](const int32 i){ __VA_ARGS__ }); }}
#define PCGEX_PARALLEL_FOR_RET(_NUM, _RET, ...) { const int32 ITERATIONS = _NUM; if(ITERATIONS < 512){ for(int i = 0; i < ITERATIONS; i++){ __VA_ARGS__ }}else{ ParallelFor(ITERATIONS, [&](const int32 i){ __VA_ARGS__ return _RET;}); }}

#endif
#pragma endregion
