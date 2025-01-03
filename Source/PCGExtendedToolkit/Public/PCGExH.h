// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <vector>


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
		return A > B ?
			       static_cast<uint64>(A) << 32 | B :
			       static_cast<uint64>(B) << 32 | A;
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

	constexpr FORCEINLINE static uint64 H6416(const uint16 A, const uint16 B, const uint16 C, const uint16 D)
	{
		return (static_cast<uint64>(A) << 48) |
			(static_cast<uint64>(B) << 32) |
			(static_cast<uint64>(C) << 16) |
			static_cast<uint64>(D);
	}

	constexpr FORCEINLINE static void H6416(const uint64_t H, uint16& A, uint16& B, uint16& C, uint16& D)
	{
		A = static_cast<uint16>(H >> 48);
		B = static_cast<uint16>((H >> 32) & 0xFFFF);
		C = static_cast<uint16>((H >> 16) & 0xFFFF);
		D = static_cast<uint16>(H & 0xFFFF);
	}

	FORCEINLINE static uint32 H64S(const uint32 A, const uint32 B, const uint32 C)
	{
		return HashCombineFast(A, HashCombineFast(B, C));
	}

	FORCEINLINE static uint32 H64S(int32 ABC[3]) { return H64S(ABC[0], ABC[1], ABC[2]); }

	FORCEINLINE static uint32 GH3(const FInt64Vector3& Seed) { return GetTypeHash(Seed); }

	template <typename T>
	FORCEINLINE static FInt32Vector3 I323(const T& Seed, const T& Tolerance)
	{
		return FInt32Vector3(
			FMath::RoundToDouble(Seed[0] * Tolerance[0]),
			FMath::RoundToDouble(Seed[1] * Tolerance[1]),
			FMath::RoundToDouble(Seed[2] * Tolerance[2]));
	}

	template <typename S, typename T>
	FORCEINLINE static FInt32Vector3 I323(const S& Seed, const T& Tolerance)
	{
		return FInt32Vector3(
			FMath::RoundToDouble(Seed[0] * Tolerance[0]),
			FMath::RoundToDouble(Seed[1] * Tolerance[1]),
			FMath::RoundToDouble(Seed[2] * Tolerance[2]));
	}

	template <typename T>
	FORCEINLINE static FInt64Vector3 I643(const T& Seed, const T& Tolerance)
	{
		return FInt64Vector3(
			FMath::RoundToDouble(Seed[0] * Tolerance[0]),
			FMath::RoundToDouble(Seed[1] * Tolerance[1]),
			FMath::RoundToDouble(Seed[2] * Tolerance[2]));
	}

	template <typename S, typename T>
	FORCEINLINE static FInt64Vector3 I643(const S& Seed, const T& Tolerance)
	{
		return FInt64Vector3(
			FMath::RoundToDouble(Seed[0] * Tolerance[0]),
			FMath::RoundToDouble(Seed[1] * Tolerance[1]),
			FMath::RoundToDouble(Seed[2] * Tolerance[2]));
	}

	template <typename T>
	FORCEINLINE static uint32 GH3(const T& Seed, const T& Tolerance) { return GetTypeHash(I643(Seed, Tolerance)); }

	template <typename S, typename T>
	FORCEINLINE static uint32 GH3(const S& Seed, const T& Tolerance) { return GetTypeHash(I643(Seed, Tolerance)); }

	FORCEINLINE static uint32 UH3(const int32 A, const int32 B, const int32 C)
	{
		int32 X = A, Y = B, Z = C;
		if (X > Y) { Swap(X, Y); }
		if (X > Z) { Swap(X, Z); }
		if (Y > Z) { Swap(Y, Z); }
		return GetTypeHash(FInt32Vector3(X, Y, Z));
	}


	template <typename T>
	FORCEINLINE static FInt64Vector2 I642(const T& Seed, const T& Tolerance)
	{
		return FInt64Vector2(
			FMath::RoundToDouble(Seed[0] * Tolerance[0]),
			FMath::RoundToDouble(Seed[1] * Tolerance[1]));
	}

	template <typename S, typename T>
	FORCEINLINE static FInt64Vector2 I642(const S& Seed, const T& Tolerance)
	{
		return FInt64Vector2(
			FMath::RoundToDouble(Seed[0] * Tolerance[0]),
			FMath::RoundToDouble(Seed[1] * Tolerance[1]));
	}

	template <typename T>
	FORCEINLINE static uint32 GH2(const T& Seed, const T& Tolerance) { return GetTypeHash(I642(Seed, Tolerance)); }

	template <typename S, typename T>
	FORCEINLINE static uint32 GH2(const S& Seed, const T& Tolerance) { return GetTypeHash(I642(Seed, Tolerance)); }

	FORCEINLINE static uint32 UH2(const int32 A, const int32 B)
	{
		return A > B ? GetTypeHash(FInt32Vector2(B, A)) : GetTypeHash(FInt32Vector2(A, B));
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

		FORCEINLINE int32& operator[](const int32 At) { return Data[At]; }
		FORCEINLINE int32 operator[](const int32 At) const { return Data[At]; }
		FORCEINLINE void Set(const int32 At, const int32 Value) { Data[At] = Value; }
		FORCEINLINE int32 Get(const int32 At) { return Data[At]; }
		FORCEINLINE int32& GetMutable(const int32 At) { return Data[At]; }
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
	};

	class FArrayHashLookup : public FHashLookup
	{
	protected:
		TArray<uint64> Data;

	public:
		explicit FArrayHashLookup(const uint64 InitValue, const int32 Size)
			: FHashLookup(InitValue, Size)
		{
			Data.Init(InitValue, Size);
		}

		FORCEINLINE virtual void Set(const int32 At, const uint64 Value) override { Data[At] = Value; }
		FORCEINLINE virtual uint64 Get(const int32 At) override { return Data[At]; }
	};

	class FMapHashLookup : public FHashLookup
	{
	protected:
		TMap<int32, uint64> Data;

	public:
		explicit FMapHashLookup(const uint64 InitValue, const int32 Size)
			: FHashLookup(InitValue, Size)
		{
		}

		FORCEINLINE virtual void Set(const int32 At, const uint64 Value) override { Data.Add(At, Value); }
		FORCEINLINE virtual uint64 Get(const int32 At) override
		{
			if (const uint64* Value = Data.Find(At)) { return *Value; }
			return InternalInitValue;
		}
	};

	template <typename T>
	static TSharedPtr<FHashLookup> NewHashLookup(const uint64 InitValue, const int32 Size)
	{
		TSharedPtr<T> TypedPtr = MakeShared<T>(InitValue, Size);
		return StaticCastSharedPtr<FHashLookup>(TypedPtr);
	}
}
