// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsBitmask.generated.h"

struct FPCGExBitmaskCollectionEntry;
struct FPCGExSimpleBitmask;
struct FPCGExBitmaskRef;
struct FPCGExContext;
class UPCGExBitmaskCollection;

UENUM()
enum class EPCGExBitOp : uint8
{
	Set = 0 UMETA(DisplayName = "SET", ToolTip="SET (Flags = Mask) Set the bit with the specified value."),
	AND = 1 UMETA(DisplayName = "AND", ToolTip="AND (Flags &= Mask) Output true if boths bits == 1, otherwise false."),
	OR  = 2 UMETA(DisplayName = "OR", ToolTip="OR (Flags |= Mask) Output true if any of the bits == 1, otherwise false."),
	NOT = 3 UMETA(DisplayName = "NOT", ToolTip="NOT (Flags &= ~Mask) Like AND, but inverts the masks."),
	XOR = 4 UMETA(DisplayName = "XOR", ToolTip="XOR (Flags ^= Mask) Invert the flag bit where the mask == 1."),
};

UENUM()
enum class EPCGExBitOp_OR : uint8
{
	OR  = 0 UMETA(DisplayName = "OR", ToolTip="OR (Flags |= Mask) Output true if any of the bits == 1, otherwise false."),
	Set = 1 UMETA(DisplayName = "SET", ToolTip="SET (Flags = Mask) Set the bit with the specified value."),
	AND = 2 UMETA(DisplayName = "AND", ToolTip="AND (Flags &= Mask) Output true if boths bits == 1, otherwise false."),
	NOT = 3 UMETA(DisplayName = "NOT", ToolTip="NOT (Flags &= ~Mask) Like AND, but inverts the masks."),
	XOR = 4 UMETA(DisplayName = "XOR", ToolTip="XOR (Flags ^= Mask) Invert the flag bit where the mask == 1."),
};

UENUM()
enum class EPCGExBitmaskMode : uint8
{
	Direct     = 0 UMETA(DisplayName = "Direct", ToolTip="Used for easy override mostly. Will use the value of the bitmask as-is", ActionIcon="Bit_Direct"),
	Individual = 1 UMETA(DisplayName = "Mutations", ToolTip="Use an array to mutate the bits of the incoming bitmask (will modify the constant value on output)", ActionIcon="Bit_Mutations"),
	Composite  = 2 UMETA(Hidden),
};

UENUM()
enum class EPCGExBitflagComparison : uint8
{
	MatchPartial   = 0 UMETA(DisplayName = "Match (any)", Tooltip="Value & Mask != 0 (At least some flags in the mask are set)"),
	MatchFull      = 1 UMETA(DisplayName = "Match (all)", Tooltip="Value & Mask == Mask (All the flags in the mask are set)"),
	MatchStrict    = 2 UMETA(DisplayName = "Match (strict)", Tooltip="Value == Mask (Flags strictly equals mask)"),
	NoMatchPartial = 3 UMETA(DisplayName = "No match (any)", Tooltip="Value & Mask == 0 (Flags does not contains any from mask)"),
	NoMatchFull    = 4 UMETA(DisplayName = "No match (all)", Tooltip="Value & Mask != Mask (Flags does not contains the mask)"),
};

namespace PCGExBitmask
{
	PCGEXTENDEDTOOLKIT_API
	FString ToString(const EPCGExBitflagComparison Comparison);

	PCGEXTENDEDTOOLKIT_API
	bool Compare(const EPCGExBitflagComparison Method, const int64& Flags, const int64& Mask);

	constexpr EPCGExBitOp OR_Ops[5] = {
		EPCGExBitOp::OR,
		EPCGExBitOp::Set,
		EPCGExBitOp::AND,
		EPCGExBitOp::NOT,
		EPCGExBitOp::XOR,
	};

	FORCEINLINE constexpr EPCGExBitOp GetBitOp(EPCGExBitOp_OR BitOp) { return OR_Ops[static_cast<uint8>(BitOp)]; }

	FORCEINLINE static void Mutate(const EPCGExBitOp Operation, int64& Flags, const int64 Mask)
	{
		switch (Operation)
		{
		case EPCGExBitOp::Set:
			Flags = Mask;
			break;
		case EPCGExBitOp::AND:
			Flags &= Mask;
			break;
		case EPCGExBitOp::OR:
			Flags |= Mask;
			break;
		case EPCGExBitOp::NOT:
			Flags &= ~Mask;
			break;
		case EPCGExBitOp::XOR:
			Flags ^= Mask;
			break;
		default: ;
		}
	}

	PCGEXTENDEDTOOLKIT_API
	void Mutate(const TArray<FPCGExBitmaskRef>& Compositions, int64& Flags);

	PCGEXTENDEDTOOLKIT_API
	void Mutate(const TArray<FPCGExSimpleBitmask>& Compositions, int64& Flags);
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClampedBit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0", ClampMax = "63"))
	uint8 BitIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bValue;

	FPCGExClampedBit() : BitIndex(0), bValue(false)
	{
	}

	bool operator==(const FPCGExClampedBit& Other) const { return BitIndex == Other.BitIndex; }
	friend uint32 GetTypeHash(const FPCGExClampedBit& Key) { return HashCombine(0, GetTypeHash(Key.BitIndex)); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClampedBitOp : public FPCGExClampedBit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EPCGExBitOp Op = EPCGExBitOp::OR;

	FPCGExClampedBitOp() = default;

	void Mutate(int64& Flags) const;

	bool operator==(const FPCGExClampedBitOp& Other) const { return BitIndex == Other.BitIndex; }
	friend uint32 GetTypeHash(const FPCGExClampedBitOp& Key) { return HashCombine(0, GetTypeHash(Key.BitIndex)); }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSimpleBitmask
{
	GENERATED_BODY()

	FPCGExSimpleBitmask() = default;

	/** Base value, how it will be mutated, if at all, depends on chosen mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 Bitmask = 0;

	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExBitOp Op = EPCGExBitOp::OR;

	FORCEINLINE void Mutate(int64& Flags) const { PCGExBitmask::Mutate(Op, Flags, Bitmask); }
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Bitmask Ref")
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UPCGExBitmaskCollection> Source;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(GetOptions="EDITOR_GetIdentifierOptions"))
	FName Identifier = NAME_None;

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
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmask
{
	GENERATED_BODY()

	FPCGExBitmask() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Individual;

	/** Base value, how it will be mutated, if at all, depends on chosen mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 Bitmask = 0;

	UPROPERTY()
	TArray<FPCGExClampedBit> Bits;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExBitmaskMode::Individual", EditConditionHides))
	TArray<FPCGExClampedBitOp> Mutations;

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
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskWithOperation
{
	GENERATED_BODY()

	FPCGExBitmaskWithOperation() = default;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitmaskMode Mode = EPCGExBitmaskMode::Direct;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 Bitmask = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExBitmaskMode::Individual", TitleProperty="Bit # {BitIndex} = {bValue}", EditConditionHides))
	TArray<FPCGExClampedBitOp> Mutations;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TArray<FPCGExBitmaskRef> Compositions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode != EPCGExBitmaskMode::Individual", EditConditionHides))
	EPCGExBitOp Op = EPCGExBitOp::OR;

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
