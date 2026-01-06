// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClipper2Processor.h"
#include "Paths/PCGExPath.h"

#include "PCGExClipper2Boolean.generated.h"

namespace PCGExClipper2
{
	class FPolyline;
}

UENUM(BlueprintType)
enum class EPCGExClipper2BooleanOp : uint8
{
	Intersection = 0 UMETA(DisplayName = "Intersection", ToolTip="TBD", SearchHints = "Intersection"),
	Union        = 1 UMETA(DisplayName = "Union", ToolTip="TBD", SearchHints = "Union"),
	Difference   = 2 UMETA(DisplayName = "Difference", ToolTip="TBD", SearchHints = "Difference"),
	Xor          = 3 UMETA(DisplayName = "XOR", ToolTip="TBD", SearchHints = "XOR"),
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
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(Clipper2Boolean, "Clipper2 : Boolean", "Does a Clipper2 Boolean operation.", FName(GetDisplayName()));
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif
	
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2BooleanOp Operation = EPCGExClipper2BooleanOp::Union;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2FillRule FillRule = EPCGExClipper2FillRule::NonZero;
	
	/** Display operand pin as a separate pin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Processing", meta = (PCG_NotOverridable, EditCondition="Operation!=EPCGExClipper2BooleanOp::Union && Operation!=EPCGExClipper2BooleanOp::Difference"))
	bool bUseOperandPin = false;
	
	virtual bool WantsOperands() const override;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
	
	virtual bool SupportOpenOperandPaths() const override;
	
#if WITH_EDITOR
	FString GetDisplayName() const;
#endif
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