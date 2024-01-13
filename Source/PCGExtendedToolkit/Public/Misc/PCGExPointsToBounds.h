// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExPointsToBounds.generated.h"

#define PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(MACRO)\
MACRO(Density) \
MACRO(Extents) \
MACRO(Color) \
MACRO(Position) \
MACRO(Rotation)\
MACRO(Scale) \
MACRO(Steepness) \
MACRO(Seed)

#define PCGEX_FUSE_UPROPERTY(_NAME)\
UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))\
bool bOverride##_NAME = false;\
UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (EditCondition="bOverride" #_NAME))\
EPCGExDataBlendingType _NAME##Blending = EPCGExDataBlendingType::Skip;

#define PCGEX_FUSE_CONTEXT(_NAME)\
EPCGExDataBlendingType _NAME##Blending;


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPointsToBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPointsToBoundsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PointsToBounds, "Points to Bounds", "Merge points group to a single point representing their bounds.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Defines how fused point properties and attributes are merged into the final point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExBlendingSettings BlendingSettings;

	/** Write point counts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bWritePointsCount = false;

	/** Attribute to write points count to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWritePointsCount"))
	FName PointsCountAttributeName = NAME_None;

private:
	friend class FPCGExPointsToBoundsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsToBoundsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPointsToBoundsElement;

	virtual ~FPCGExPointsToBoundsContext() override;

	bool bPreserveOrder;

	PCGExDataBlending::FMetadataBlender* MetadataBlender;

	TArray<FPCGPoint>* OutPoints;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsToBoundsElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#undef PCGEX_FUSE_FOREACH_POINTPROPERTYNAME
#undef PCGEX_FUSE_UPROPERTY
#undef PCGEX_FUSE_CONTEXT
