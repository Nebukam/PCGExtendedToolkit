// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExPointsProcessor.h"
#include "PCGExSpatialTriage.generated.h"

namespace PCGExSpatialTriage
{
	static const FName SourceLabelBounds = FName("Bounds");

	static const FName OutputLabelInside = FName("Inside");
	static const FName OutputLabelTouching = FName("Touching");
	static const FName OutputLabelOutside = FName("Outside");
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="quality-of-life/spatial-triage"))
class UPCGExSpatialTriageSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SpatialTriage, "Spatial Triage", "Test relevance of spatial data against singular bounds. Primarily expected to be used with partition bounds to find data that can be uniquely processed by this partition. This is fast box-box check");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(FilterHub); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

	virtual bool OutputPinsCanBeDeactivated() const override { return true; }
	virtual bool HasDynamicPins() const override { return true; }

protected:
	virtual bool IsInputless() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings
};

struct FPCGExSpatialTriageContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSpatialTriageElement;
};

class FPCGExSpatialTriageElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SpatialTriage)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
