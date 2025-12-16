// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "HAL/Platform.h"
#include "Math/IntVector.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "type_traits"

namespace PCGEx
{
	template <typename, typename = void>
	struct HasGetTypeHash : std::false_type
	{
	};

	template <typename T>
	struct HasGetTypeHash<T, std::void_t<decltype(GetTypeHash(std::declval<T>()))>> : std::true_type
	{
	};

	// Check if `operator==` is available for type T
	template <typename, typename = void>
	struct HasEqualityOperator : std::false_type
	{
	};

	template <typename T>
	struct HasEqualityOperator<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>> : std::true_type
	{
	};

	// Final compile-time check to ensure both conditions are met
	template <typename T>
	struct IsValidForTMap : std::integral_constant<bool, HasGetTypeHash<T>::value && HasEqualityOperator<T>::value>
	{
	};

	// Unsigned uint64 hash
	constexpr FORCEINLINE static uint64 H64U(const uint32 A, const uint32 B)
	{
		return A > B ? static_cast<uint64>(A) << 32 | B : static_cast<uint64>(B) << 32 | A;
	}

	// Signed uint32 hash
	constexpr FORCEINLINE static uint32 H32(const uint16 A, const uint16 B) { return static_cast<uint32>(A) << 16 | B; }

	// Expand uint32 hash
	constexpr FORCEINLINE static uint32 H32A(const uint32 Hash) { return static_cast<uint16>(Hash >> 16); }
	constexpr FORCEINLINE static uint32 H32B(const uint32 Hash) { return static_cast<uint16>(Hash); }

	constexpr FORCEINLINE static void H32(const uint32 Hash, uint16& A, uint16& B)
	{
		A = H32A(Hash);
		B = H32B(Hash);
	}

	// Signed uint64 hash
	constexpr FORCEINLINE static uint64 H64(const uint32 A, const uint32 B) { return static_cast<uint64>(A) << 32 | B; }
	constexpr FORCEINLINE static uint64 NH64(const int32 A, const int32 B) { return H64(A + 1, B + 1); }
	constexpr FORCEINLINE static uint64 NH64U(const int32 A, const int32 B) { return H64U(A + 1, B + 1); }

	// Expand uint64 hash
	constexpr FORCEINLINE static uint32 H64A(const uint64 Hash) { return static_cast<uint32>(Hash >> 32); }
	constexpr FORCEINLINE static uint32 H64B(const uint64 Hash) { return static_cast<uint32>(Hash); }

	constexpr FORCEINLINE static int32 NH64A(const uint64 Hash) { return static_cast<int32>(H64A(Hash)) - 1; }
	constexpr FORCEINLINE static int32 NH64B(const uint64 Hash) { return static_cast<int32>(H64B(Hash)) - 1; }

	constexpr FORCEINLINE static void H64(const uint64 Hash, uint32& A, uint32& B)
	{
		A = H64A(Hash);
		B = H64B(Hash);
	}

	constexpr FORCEINLINE static void NH64(const uint64 Hash, int32& A, int32& B)
	{
		A = NH64A(Hash);
		B = NH64B(Hash);
	}

	constexpr FORCEINLINE static uint64 H64NOT(const uint64 Hash, const uint32 Not)
	{
		const uint32 A = H64A(Hash);
		return A == Not ? H64B(Hash) : A;
	}

	constexpr FORCEINLINE static int32 NH64NOT(const uint64 Hash, const int32 Not)
	{
		const int32 A = NH64A(Hash);
		return A == Not ? NH64B(Hash) : A;
	}

	template <typename T>
	FORCEINLINE static T SafeScalarTolerance(const T& InValue)
	{
		return FMath::Max(InValue, SMALL_NUMBER);
	}

	FORCEINLINE static FVector SafeTolerance(const FVector& InVector)
	{
		return FVector(
			FMath::Max(InVector.X, SMALL_NUMBER),
			FMath::Max(InVector.Y, SMALL_NUMBER),
			FMath::Max(InVector.Z, SMALL_NUMBER)
		);
	}

	FORCEINLINE static uint64 SH3(const FVector& Seed, const FVector& Tolerance)
	{
		// Hash needed when spatial consistency is necessary
		// FNV1A doesn't preserve it
		return GetTypeHash(FInt64Vector3(
			FMath::RoundToInt64(Seed.X / Tolerance.X),
			FMath::RoundToInt64(Seed.Y / Tolerance.Y),
			FMath::RoundToInt64(Seed.Z / Tolerance.Z)));
	}
	
#define PCGEX_FNV1A\
	uint64 Hash = 14695981039346656037ULL;\
	Hash = (Hash ^ X) * 1099511628211ULL;\
	Hash = (Hash ^ Y) * 1099511628211ULL;\
	Hash = (Hash ^ Z) * 1099511628211ULL;\
	return Hash;

	template <typename T>
	FORCEINLINE static uint64 GH3(const T& Seed, const T& Tolerance)
	{
		const int64 X = FMath::FloorToInt64(Seed[0] / Tolerance[0]);
		const int64 Y = FMath::FloorToInt64(Seed[1] / Tolerance[1]);
		const int64 Z = FMath::FloorToInt64(Seed[2] / Tolerance[2]);

		PCGEX_FNV1A
	}

	template <typename S, typename T>
	FORCEINLINE static uint64 GH3(const S& Seed, const T& Tolerance)
	{
		const int64 X = FMath::FloorToInt64(Seed[0] / Tolerance[0]);
		const int64 Y = FMath::FloorToInt64(Seed[1] / Tolerance[1]);
		const int64 Z = FMath::FloorToInt64(Seed[2] / Tolerance[2]);

		PCGEX_FNV1A
	}

	FORCEINLINE static uint64 UH3(const int32 A, const int32 B, const int32 C)
	{
		int64 X = A, Y = B, Z = C;
		if (X > Y) { Swap(X, Y); }
		if (X > Z) { Swap(X, Z); }
		if (Y > Z) { Swap(Y, Z); }

		PCGEX_FNV1A
	}

#undef PCGEX_FNV1A

	template <typename S, typename T>
	FORCEINLINE static uint64 GH2(const S& Seed, const T& Tolerance)
	{
		const int64 X = FMath::FloorToInt64(Seed[0] / Tolerance[0]);
		const int64 Y = FMath::FloorToInt64(Seed[1] / Tolerance[1]);

		uint64 Hash = 14695981039346656037ULL;
		Hash = (Hash ^ X) * 1099511628211ULL;
		Hash = (Hash ^ Y) * 1099511628211ULL;
		return Hash;
	}

	class FIndexLookup : public TSharedFromThis<FIndexLookup>
	{
	protected:
		TArray<int32> Data;

	public:
		explicit FIndexLookup(const int32 Size)
		{
			Data.Init(-1, Size);
		}

		FIndexLookup(const int32 Size, bool bFill)
		{
			Data.Init(-1, Size);
		}

		FORCEINLINE int32& operator[](const int32 At) { return Data[At]; }
		FORCEINLINE int32 operator[](const int32 At) const { return Data[At]; }
		FORCEINLINE void Set(const int32 At, const int32 Value) { Data[At] = Value; }
		FORCEINLINE int32 Get(const int32 At) { return Data[At]; }
		FORCEINLINE int32& GetMutable(const int32 At) { return Data[At]; }

		operator TArrayView<const int32>() const { return Data; }
		operator TArrayView<int32>() { return Data; }
	};

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
