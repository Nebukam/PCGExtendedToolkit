// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExCompare.h"

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

UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGlobalBitmaskManager : public UObject
{
	GENERATED_BODY()

public:
	static UPCGExGlobalBitmaskManager* Get();
	static UPCGParamData* GetOrCreate(const int64 Bitmask);

	UPROPERTY()
	TMap<int64, UPCGParamData*> SharedInstances;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBitmaskSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExBitmaskElement;

public:
	bool bCacheResult = false;
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Bitmask, "Bitmask", "A Simple bitmask attribute.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** Operations executed on the flag if all filters pass */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBitmask Bitmask;

	/** If marked global, this bitmask will always be loaded in memory. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bGlobalInstance = false;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBitmaskElement final : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	//virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
