﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeo.h"
#include "PCGExFlatProjection.generated.h"

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="transform/flat-projection"))
class UPCGExFlatProjectionSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FlatProjection, "Flat Projection", "Project points from their position in space to the XY plane.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Whether this is a new projection or an old one*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bRestorePreviousProjection = false;

	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName AttributePrefix = "FlatProjection";

	/** Whether this is a new projection or an old one*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSaveAttributeForRestore = true;

	/** Whether this is a new projection or an old one*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bRestorePreviousProjection"))
	bool bAlignLocalTransform = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bRestorePreviousProjection", DisplayName="Projection"))
	FPCGExGeo2DProjectionDetails ProjectionDetails;
};

struct FPCGExFlatProjectionContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFlatProjectionElement;

	FName CachedTransformAttributeName = NAME_None;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExFlatProjectionElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FlatProjection)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFlatProjection
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExFlatProjectionContext, UPCGExFlatProjectionSettings>
	{
		bool bWriteAttribute = false;
		bool bInverseExistingProjection = false;
		bool bProjectLocalTransform = false;

		FPCGExGeo2DProjectionDetails ProjectionDetails;

		TSharedPtr<PCGExData::TBuffer<FTransform>> TransformWriter;
		TSharedPtr<PCGExData::TBuffer<FTransform>> TransformReader;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
