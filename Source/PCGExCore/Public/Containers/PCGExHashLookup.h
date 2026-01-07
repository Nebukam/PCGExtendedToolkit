// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGEx
{
	class FHashLookup : public TSharedFromThis<FHashLookup>
	{
	protected:
		uint64 InternalInitValue;

	public:
		virtual ~FHashLookup() = default;

		explicit FHashLookup(const uint64 InitValue, const int32 Size)
			: InternalInitValue(InitValue)
		{
		}

		FORCEINLINE virtual void Set(const int32 At, const uint64 Value) = 0;
		FORCEINLINE virtual uint64 Get(const int32 At) = 0;
		FORCEINLINE virtual bool IsInitValue(const uint64 InValue) { return InValue == InternalInitValue; }
		virtual void Reset() = 0;
	};

	class FHashLookupArray : public FHashLookup
	{
	protected:
		TArray<uint64> Data;

	public:
		explicit FHashLookupArray(const uint64 InitValue, const int32 Size)
			: FHashLookup(InitValue, Size)
		{
			Data.Init(InitValue, Size);
		}

		FORCEINLINE virtual void Set(const int32 At, const uint64 Value) override { Data[At] = Value; }
		FORCEINLINE virtual uint64 Get(const int32 At) override { return Data[At]; }
		virtual void Reset() override { for (uint64& V : Data) { V = InternalInitValue; } }

		operator TArrayView<const uint64>() const { return Data; }
		operator TArrayView<uint64>() { return Data; }
	};

	class FHashLookupMap : public FHashLookup
	{
	protected:
		TMap<int32, uint64> Data;

	public:
		explicit FHashLookupMap(const uint64 InitValue, const int32 Size)
			: FHashLookup(InitValue, Size)
		{
			if (Size > 0) { Data.Reserve(Size); }
		}

		FORCEINLINE virtual void Set(const int32 At, const uint64 Value) override { Data.Add(At, Value); }
		FORCEINLINE virtual uint64 Get(const int32 At) override
		{
			if (const uint64* Value = Data.Find(At)) { return *Value; }
			return InternalInitValue;
		}

		virtual void Reset() override { Data.Reset(); }

		FORCEINLINE bool Contains(const int32 Index) const { return Data.Contains(Index); }
	};

	template <typename T>
	static TSharedPtr<FHashLookup> NewHashLookup(const uint64 InitValue, const int32 Size)
	{
		TSharedPtr<T> TypedPtr = MakeShared<T>(InitValue, Size);
		return StaticCastSharedPtr<FHashLookup>(TypedPtr);
	}
}
