// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

namespace PCGEx
{
	// Unsigned uint64 hash
	FORCEINLINE static uint64 H64U(const uint32 A, const uint32 B)
	{
		return A > B ?
			       static_cast<uint64>(A) << 32 | B :
			       static_cast<uint64>(B) << 32 | A;
	}

	// Signed uint64 hash
	FORCEINLINE static uint64 H64(const uint32 A, const uint32 B) { return static_cast<uint64>(A) << 32 | B; }
	FORCEINLINE static uint64 NH64(const int32 A, const int32 B) { return H64(A + 1, B + 1); }
	FORCEINLINE static uint64 NH64U(const int32 A, const int32 B) { return H64U(A + 1, B + 1); }

	// Expand uint64 hash
	FORCEINLINE static uint32 H64A(const uint64 Hash) { return static_cast<uint32>(Hash >> 32); }
	FORCEINLINE static uint32 H64B(const uint64 Hash) { return static_cast<uint32>(Hash); }

	FORCEINLINE static int32 NH64A(const uint64 Hash) { return static_cast<int32>(H64A(Hash)) - 1; }
	FORCEINLINE static int32 NH64B(const uint64 Hash) { return static_cast<int32>(H64B(Hash)) - 1; }

	FORCEINLINE static void H64(const uint64 Hash, uint32& A, uint32& B)
	{
		A = H64A(Hash);
		B = H64B(Hash);
	}

	FORCEINLINE static void NH64(const uint64 Hash, int32& A, int32& B)
	{
		A = NH64A(Hash);
		B = NH64B(Hash);
	}

	FORCEINLINE static uint64 H64NOT(const uint64 Hash, const uint32 Not)
	{
		const uint32 A = H64A(Hash);
		return A == Not ? H64B(Hash) : A;
	}

	FORCEINLINE static int32 NH64NOT(const uint64 Hash, const int32 Not)
	{
		const int32 A = NH64A(Hash);
		return A == Not ? NH64B(Hash) : A;
	}

	FORCEINLINE static uint64 H6416(const uint16 A, const uint16 B, const uint16 C, const uint16 D)
	{
		return (static_cast<uint64>(A) << 48) |
			(static_cast<uint64>(B) << 32) |
			(static_cast<uint64>(C) << 16) |
			static_cast<uint64>(D);
	}

	FORCEINLINE static void H6416(const uint64_t H, uint16& A, uint16& B, uint16& C, uint16& D)
	{
		A = static_cast<uint16>(H >> 48);
		B = static_cast<uint16>((H >> 32) & 0xFFFF);
		C = static_cast<uint16>((H >> 16) & 0xFFFF);
		D = static_cast<uint16>(H & 0xFFFF);
	}

	FORCEINLINE static uint64 H64S(const uint32 A, const uint32 B, const uint32 C)
	{
		return HashCombineFast(A, HashCombineFast(B, C));
	}

	FORCEINLINE static uint64 H64S(int32 ABC[3]) { return H64S(ABC[0], ABC[1], ABC[2]); }

	FORCEINLINE static uint32 GH(const FInt64Vector3& Seed) { return GetTypeHash(Seed); }

	FORCEINLINE static FInt32Vector3 I323(const FVector& Seed, const FVector& Tolerance)
	{
		return FInt32Vector3(
			FMath::RoundToDouble(Seed.X * Tolerance.X),
			FMath::RoundToDouble(Seed.Y * Tolerance.Y),
			FMath::RoundToDouble(Seed.Z * Tolerance.Z));
	}

	FORCEINLINE static FInt32Vector3 I323(const FVector& Seed, const FInt32Vector3& Tolerance)
	{
		return FInt32Vector3(
			FMath::RoundToDouble(Seed.X * Tolerance.X),
			FMath::RoundToDouble(Seed.Y * Tolerance.Y),
			FMath::RoundToDouble(Seed.Z * Tolerance.Z));
	}

	FORCEINLINE static FInt64Vector3 I643(const FVector& Seed, const FVector& Tolerance)
	{
		return FInt64Vector3(
			FMath::RoundToDouble(Seed.X * Tolerance.X),
			FMath::RoundToDouble(Seed.Y * Tolerance.Y),
			FMath::RoundToDouble(Seed.Z * Tolerance.Z));
	}

	FORCEINLINE static FInt64Vector3 I643(const FVector& Seed, const FInt64Vector3& Tolerance)
	{
		return FInt64Vector3(
			FMath::RoundToDouble(Seed.X * Tolerance.X),
			FMath::RoundToDouble(Seed.Y * Tolerance.Y),
			FMath::RoundToDouble(Seed.Z * Tolerance.Z));
	}

	FORCEINLINE static uint32 GH(const FVector& Seed, const FInt64Vector3& Tolerance) { return GetTypeHash(I643(Seed, Tolerance)); }

	FORCEINLINE static uint32 GH(const FVector& Seed, const FVector& Tolerance) { return GetTypeHash(I643(Seed, Tolerance)); }
}
