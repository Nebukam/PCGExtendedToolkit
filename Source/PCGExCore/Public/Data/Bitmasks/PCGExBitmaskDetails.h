// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBitmaskCommon.h"
#include "PCGExBitmaskDetails.generated.h"

struct FPCGExContext;
class UPCGExBitmaskCollection;

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExClampedBit
{
	GENERATED_BODY()

	/** Bit position to modify (0-63). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0", ClampMax = "63"))
	uint8 BitIndex;

	/** Value to set at BitIndex (true = 1, false = 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bValue;

	FPCGExClampedBit()
		: BitIndex(0), bValue(false)
	{
	}

	bool operator==(const FPCGExClampedBit& Other) const { return BitIndex == Other.BitIndex; }
	friend uint32 GetTypeHash(const FPCGExClampedBit& Key) { return HashCombine(0, GetTypeHash(Key.BitIndex)); }
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExClampedBitOp : public FPCGExClampedBit
{
	GENERATED_BODY()

	/** Bitwise operation to apply (OR adds bit, NOT removes bit, XOR toggles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EPCGExBitOp Op = EPCGExBitOp::OR;

	FPCGExClampedBitOp() = default;

	void Mutate(int64& Flags) const;

	bool operator==(const FPCGExClampedBitOp& Other) const { return BitIndex == Other.BitIndex; }
	friend uint32 GetTypeHash(const FPCGExClampedBitOp& Key) { return HashCombine(0, GetTypeHash(Key.BitIndex)); }
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExSimpleBitmask
{
	GENERATED_BODY()

	FPCGExSimpleBitmask() = default;

	/** The 64-bit mask value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 Bitmask = 0;

	/** How this mask is combined with existing flags. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExBitOp Op = EPCGExBitOp::OR;

	FORCEINLINE void Mutate(int64& Flags) const { PCGExBitmask::Mutate(Op, Flags, Bitmask); }
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Bitmask Ref")
struct PCGEXCORE_API FPCGExBitmaskRef
{
	GENERATED_BODY()

	FPCGExBitmaskRef() = default;
	explicit FPCGExBitmaskRef(TObjectPtr<UPCGExBitmaskCollection> InSource, const FName InIdentifier);

	/** Bitmask collection asset to reference. */
	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UPCGExBitmaskCollection> Source;

	/** Named entry within the collection. */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(GetOptions="EDITOR_GetIdentifierOptions"))
	FName Identifier = NAME_None;

	/** How this referenced mask is combined with existing flags. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExBitOp Op = EPCGExBitOp::OR;

#if WITH_EDITOR
	TArray<FName> EDITOR_GetIdentifierOptions() const;
#endif

	void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

	void Mutate(int64& Flags) const;
	FPCGExSimpleBitmask GetSimpleBitmask() const;
	bool TryGetAdjacencyInfos(FVector& OutDirection, FPCGExSimpleBitmask& OutSimpleBitmask) const;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Bitmask")
struct PCGEXCORE_API FPCGExBitmask
{
	GENERATED_BODY()

	FPCGExBitmask() = default;

	/** How the bitmask value is constructed. Direct uses raw value, Mutations applies per-bit operations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Individual;

	/** Base 64-bit mask value. In Direct mode, used as-is. In Mutations mode, modified by operations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 Bitmask = 0;

	UPROPERTY()
	TArray<FPCGExClampedBit> Bits;

	/** Per-bit operations applied to the base bitmask. Only used in Mutations mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExBitmaskMode::Individual", EditConditionHides))
	TArray<FPCGExClampedBitOp> Mutations;

	/** Additional bitmask references combined with the result. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TArray<FPCGExBitmaskRef> Compositions;

#pragma region DEPRECATED

	UPROPERTY()
	uint8 Range_00_08_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_08_16_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_16_24_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_24_32_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_32_40_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_40_48_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_48_56_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_56_64_DEPRECATED = 0;

#pragma endregion

	int64 Get() const;
	FORCEINLINE void Mutate(const EPCGExBitOp Op, int64& Flags) const { PCGExBitmask::Mutate(Op, Flags, Get()); }

	void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

#if WITH_EDITOR
	void ApplyDeprecation();
#endif
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExBitmaskWithOperation
{
	GENERATED_BODY()

	FPCGExBitmaskWithOperation() = default;

	/** How the bitmask value is constructed. Direct uses raw value, Mutations applies per-bit operations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Direct;

	/** Base 64-bit mask value. In Direct mode, used as-is. In Mutations mode, modified by operations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 Bitmask = 0;

	/** How the final mask is combined with existing flags when applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(PCG_NotOverridable, EditConditionHides))
	EPCGExBitOp Op = EPCGExBitOp::OR;

	/** Per-bit operations applied to the base bitmask. Only used in Mutations mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExBitmaskMode::Individual", TitleProperty="Bit # {BitIndex} = {bValue}", EditConditionHides))
	TArray<FPCGExClampedBitOp> Mutations;

	/** Additional bitmask references combined with the result. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TArray<FPCGExBitmaskRef> Compositions;

#pragma region DEPRECATED

	UPROPERTY()
	uint8 Range_00_08_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_08_16_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_16_24_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_24_32_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_32_40_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_40_48_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_48_56_DEPRECATED = 0;

	UPROPERTY()
	uint8 Range_56_64_DEPRECATED = 0;

#pragma endregion

	int64 Get() const;
	void Mutate(int64& Flags) const;

	void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

#if WITH_EDITOR
	void ApplyDeprecation();
#endif
};
