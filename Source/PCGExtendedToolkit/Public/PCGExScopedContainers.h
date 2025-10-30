﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExMT.h"

namespace PCGExMT
{
	template <int32 NumShards = 32>
	struct TH64SetShards
	{
	private:
		const int32 Log2NumShards = FMath::FloorLog2(NumShards);
		TStaticArray<TSet<uint64>, NumShards> Shards;
		TStaticArray<FRWLock, NumShards> Locks;
		const uint32 ShardMask = NumShards - 1;

	public:
		TH64SetShards()
		{
			static_assert(FMath::IsPowerOfTwo(NumShards));
		}

		void Reserve(const int32 ShardReserve)
		{
			int32 NumReserve = ShardReserve / NumShards;
			for (int32 i = 0; i < NumShards; i++)
			{
				FReadScopeLock ScopeLock(Locks[i]);
				Shards[i].Reserve(NumReserve);
			}
		}

		void Add(uint64 Value)
		{
			const uint32 Index = FastHashToShard(Value);
			FWriteScopeLock ScopeLock(Locks[Index]);
			Shards[Index].Add(Value);
		}

		void Add(uint64 Value, bool& bIsAlreadySet)
		{
			const uint32 Index = FastHashToShard(Value);
			FWriteScopeLock ScopeLock(Locks[Index]);
			Shards[Index].Add(Value, &bIsAlreadySet);
		}

		int32 Remove(uint64 Value)
		{
			const uint32 Index = FastHashToShard(Value);
			FWriteScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].Remove(Value);
		}

		bool Contains(uint64 Value)
		{
			const uint32 Index = FastHashToShard(Value);
			FReadScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].Contains(Value);
		}

		void Collapse(TSet<uint64>& OutMerged)
		{
			int32 Num = 0;
			for (int32 i = 0; i < NumShards; i++) { Num += Shards[i].Num(); }
			for (int32 i = 0; i < NumShards; i++)
			{
				OutMerged.Append(Shards[i]);
				Shards[i].Empty();
			}
		}

		void Empty()
		{
			for (int32 i = 0; i < NumShards; i++) { Shards[i].Empty(); }
		}

	private:
		FORCEINLINE uint32 FastHashToShard(uint64 Value) const
		{
			return static_cast<uint32>((Value * 2654435761ULL) >> (64 - Log2NumShards));
		}
	};

	template <typename T, int32 NumShards = 32>
	struct TH64MapShards
	{
	private:
		const int32 Log2NumShards = FMath::FloorLog2(NumShards);
		TStaticArray<TMap<uint64, T>, NumShards> Shards;
		TStaticArray<FRWLock, NumShards> Locks;
		const uint32 ShardMask = NumShards - 1;

	public:
		TH64MapShards()
		{
			static_assert(FMath::IsPowerOfTwo(NumShards));
		}

		void Reserve(const int32 ShardReserve)
		{
			int32 NumReserve = ShardReserve / NumShards;
			for (int32 i = 0; i < NumShards; i++)
			{
				FReadScopeLock ScopeLock(Locks[i]);
				Shards[i].Reserve(NumReserve);
			}
		}

		T& Add(uint64 Key, T Value)
		{
			const uint32 Index = FastHashToShard(Key);
			FWriteScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].Add(Key, Value);
		}

		T* Find(uint64 Key)
		{
			const uint32 Index = FastHashToShard(Key);
			FWriteScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].Find(Key);
		}

		T& FindOrAdd(uint64 Key, T& Value)
		{
			const uint32 Index = FastHashToShard(Key);
			FWriteScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].FindOrAdd(Key, Value);
		}

		int32 Remove(uint64 Key)
		{
			const uint32 Index = FastHashToShard(Key);
			FWriteScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].Remove(Key);
		}

		bool Contains(uint64 Key)
		{
			const uint32 Index = FastHashToShard(Key);
			FReadScopeLock ScopeLock(Locks[Index]);
			return Shards[Index].Contains(Key);
		}

		void Collapse(TMap<uint64, T>& OutMerged)
		{
			int32 Num = 0;
			for (int32 i = 0; i < NumShards; i++) { Num += Shards[i].Num(); }
			for (int32 i = 0; i < NumShards; i++)
			{
				OutMerged.Append(Shards[i]);
				Shards[i].Empty();
			}
		}

		void Empty()
		{
			for (int32 i = 0; i < NumShards; i++) { Shards[i].Empty(); }
		}

	private:
		FORCEINLINE uint32 FastHashToShard(uint64 Value) const
		{
			return static_cast<uint32>((Value * 2654435761ULL) >> (64 - Log2NumShards));
		}
	};

	template <typename T>
	class TScopedArray final : public TSharedFromThis<TScopedArray<T>>
	{
	public:
		TArray<TSharedPtr<TArray<T>>> Arrays;

		explicit TScopedArray(const TArray<FScope>& InScopes, const T InDefaultValue)
		{
			Arrays.Reserve(InScopes.Num());
			for (const FScope& Scope : InScopes) { Arrays[Arrays.Add(MakeShared<TArray<T>>())]->Init(InDefaultValue, Scope.Count); }
		};

		explicit TScopedArray(const TArray<FScope>& InScopes)
		{
			Arrays.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Arrays.Add(MakeShared<TArray<T>>()); }
		};

		~TScopedArray() = default;

		void Reserve(const int32 NumReserve)
		{
			for (int i = 0; i < Arrays.Num(); i++) { Arrays[i]->Reserve(NumReserve); }
		}

		FORCEINLINE TSharedPtr<TArray<T>> Get(const FScope& InScope) { return Arrays[InScope.LoopIndex]; }
		FORCEINLINE TArray<T>& Get_Ref(const FScope& InScope) { return *Arrays[InScope.LoopIndex].Get(); }

		int32 GetTotalNum()
		{
			int32 TotalNum = 0;
			for (int i = 0; i < Arrays.Num(); i++) { TotalNum += Arrays[i]->Num(); }
			return TotalNum;
		}

		using FForEachFunc = std::function<void (TArray<T>&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Arrays.Num(); i++) { Func(*Arrays[i].Get()); } }

		void Collapse(TArray<T>& InTarget)
		{
			int32 Reserve = 0;
			for (int i = 0; i < Arrays.Num(); i++) { Reserve += Arrays[i]->Num(); }
			for (int i = 0; i < Arrays.Num(); i++)
			{
				InTarget.Append(*Arrays[i].Get());
				Arrays[i] = nullptr;
			}

			Arrays.Empty();
		}
	};

	template <typename T>
	class TScopedSet final : public TSharedFromThis<TScopedSet<T>>
	{
	public:
		TArray<TSharedPtr<TSet<T>>> Sets;

		explicit TScopedSet(const TArray<FScope>& InScopes, const int32 InReserve)
		{
			Sets.Reserve(InScopes.Num());
			if (InReserve > 0)
			{
				for (int i = 0; i < InScopes.Num(); i++) { Sets.Add_GetRef(MakeShared<TSet<T>>())->Reserve(InReserve); }
			}
			else if (InReserve == 0)
			{
				for (int i = 0; i < InScopes.Num(); i++) { Sets.Add(MakeShared<TSet<T>>()); }
			}
			else
			{
				const int32 ReserveFactor = FMath::Abs(InReserve);
				for (int i = 0; i < InScopes.Num(); i++) { Sets.Add_GetRef(MakeShared<TSet<T>>())->Reserve(InScopes[i].Count * ReserveFactor); }
			}
		};

		~TScopedSet() = default;

		FORCEINLINE TSharedPtr<TSet<T>> Get(const FScope& InScope) { return Sets[InScope.LoopIndex]; }
		FORCEINLINE TSet<T>& Get_Ref(const FScope& InScope) { return *Sets[InScope.LoopIndex].Get(); }

		using FForEachFunc = std::function<void (TSet<T>&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Sets.Num(); i++) { Func(*Sets[i].Get()); } }

		void Collapse(TSet<T>& InTarget)
		{
			for (int i = 0; i < Sets.Num(); i++)
			{
				InTarget.Reserve(InTarget.Num() + Sets[i].Get()->Num());
				InTarget.Append(*Sets[i].Get());
				Sets[i] = nullptr;
			}

			Sets.Empty();
		}
	};

	template <typename T>
	class TScopedValue : public TSharedFromThis<TScopedValue<T>>
	{
	public:
		TArray<T> Values;

		using FFlattenFunc = std::function<T(const T&, const T&)>;

		TScopedValue(const TArray<FScope>& InScopes, const T InDefaultValue)
		{
			Values.Init(InDefaultValue, InScopes.Num());
		};

		~TScopedValue() = default;

		FORCEINLINE T Get(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE T& GetMutable(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE T Set(const FScope& InScope, const T& InValue) { return Values[InScope.LoopIndex] = InValue; }

		FORCEINLINE T Flatten(FFlattenFunc&& Func)
		{
			T Result = Values[0];
			if (Values.Num() > 1) { for (int i = 1; i < Values.Num(); i++) { Result = Func(Values[i], Result); } }
			return Result;
		}
	};

	template <typename T>
	class TScopedPtr : public TSharedFromThis<TScopedPtr<T>>
	{
	public:
		TArray<TSharedPtr<T>> Data;

		using FFlattenFunc = std::function<T(const T&, const T&)>;

		TScopedPtr(const TArray<FScope>& InScopes)
		{
			Data.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Data.Add(MakeShared<T>()); }
		};

		template <typename... Args>
		TScopedPtr(const TArray<FScope>& InScopes, Args&&... InArgs)
		{
			Data.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Data.Add(MakeShared<T>(std::forward<Args>(InArgs)...)); }
		};

		~TScopedPtr() = default;

		FORCEINLINE TSharedPtr<T> Get(const FScope& InScope) { return Data[InScope.LoopIndex]; }
		FORCEINLINE T& Get_Ref(const FScope& InScope) { return *Data[InScope.LoopIndex].Get(); }

		using FForEachFunc = std::function<void (T&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Data.Num(); i++) { Func(*Data[i].Get()); } }
	};

	template <typename T>
	class TScopedNumericValue final : public TScopedValue<T>
	{
		using TScopedValue<T>::Values;

	public:
		TScopedNumericValue(const TArray<FScope>& InScopes, const T InDefaultValue)
			: TScopedValue<T>(InScopes, InDefaultValue)
		{
		};

		FORCEINLINE T Min()
		{
			T Result = Values[0];
			if (Values.Num() > 1) { for (int i = 1; i < Values.Num(); i++) { Result = FMath::Min(Values[i], Result); } }
			return Result;
		}

		FORCEINLINE T Max()
		{
			T Result = Values[0];
			if (Values.Num() > 1) { for (int i = 1; i < Values.Num(); i++) { Result = FMath::Max(Values[i], Result); } }
			return Result;
		}

		FORCEINLINE T Sum()
		{
			T Result = Values[0];
			if (Values.Num() > 1) { for (int i = 1; i < Values.Num(); i++) { Result += Values[i]; } }
			return Result;
		}
	};
}
