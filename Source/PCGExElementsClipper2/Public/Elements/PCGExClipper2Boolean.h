// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClipper2Processor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Paths/PCGExPath.h"

#include "PCGExClipper2Boolean.generated.h"

namespace PCGExClipper2
{
	class FPolyline;
}

UENUM(BlueprintType)
enum class EPCGExClipper2BooleanOp : uint8
{
	Intersection = 0 UMETA(DisplayName = "Intersection", ToolTip="TBD"),
	Union        = 1 UMETA(DisplayName = "Union", ToolTip="TBD"),
	Difference   = 2 UMETA(DisplayName = "Difference", ToolTip="TBD"),
	Xor          = 3 UMETA(DisplayName = "XOR", ToolTip="TBD"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/clipper2/clipper2-boolean"))
class UPCGExClipper2BooleanSettings : public UPCGExClipper2ProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Clipper2Boolean, "Clipper2 : Boolean", "Does a Clipper2 Boolean operation.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2BooleanOp Operation = EPCGExClipper2BooleanOp::Union;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Operation != EPCGExClipper2BooleanOp::Difference"))
	bool bUseOperandsPin = false;

	/** Filter in/out which attributes get carried over from inputs to outputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCarryOverDetails CarryOver;

	virtual bool NeedsOperands() const override;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
	virtual PCGExClipper2::EMainGroupingPolicy GetGroupingPolicy() const override;
};

struct FPCGExClipper2BooleanContext final : FPCGExClipper2ProcessorContext
{
	friend class FPCGExClipper2BooleanElement;

	virtual void Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group) override;
};

class FPCGExClipper2BooleanElement final : public FPCGExClipper2ProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Boolean)
};