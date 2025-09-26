﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCompare.h"
#include "PCGExPointsProcessor.h"
#include "PCGSettings.h"

#include "PCGExBitmask.generated.h"

namespace PCGExBitmask
{
	FORCEINLINE static void Do(const EPCGExBitOp Op, int64& Flags, const int64 Mask)
	{
		switch (Op)
		{
		default: ;
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
		}
	}

	FORCEINLINE static void Do(const EPCGExBitOp Op, int64& Flags, const TArray<FClampedBit>& Mask)
	{
		switch (Op)
		{
		default: ;
		case EPCGExBitOp::Set:
			for (const FClampedBit& Bit : Mask)
			{
				if (Bit.bValue) { Flags |= Bit.Get(); } // Set the bit
				else { Flags &= ~Bit.Get(); }           // Clear the bit
			}
			break;
		case EPCGExBitOp::AND:
			for (const FClampedBit& Bit : Mask) { Flags &= Bit.Get(); }
			break;
		case EPCGExBitOp::OR:
			for (const FClampedBit& Bit : Mask) { Flags |= Bit.Get(); }
			break;
		case EPCGExBitOp::NOT:
			for (const FClampedBit& Bit : Mask) { Flags &= ~Bit.Get(); }
			break;
		case EPCGExBitOp::XOR:
			for (const FClampedBit& Bit : Mask) { Flags ^= Bit.Get(); }
			break;
		}
	}
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="TBD"))
class UPCGExBitmaskSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExBitmaskElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(Bitmask, "Bitmask", "A Simple bitmask attribute.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorConstant; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** Operations executed on the flag if all filters pass */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBitmask Bitmask;
};

class FPCGExBitmaskElement final : public IPCGElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
