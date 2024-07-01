// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExFlatProject.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExFlatProjectSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FlatProject, "Flat Project", "Temporarily project points from their position in space to the XY plane.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName AttributePrefix = "FlatProject";

	/** Whether this is a new projection or an old one*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInverseExistingProjection = false;

	/** Whether this is a new projection or an old one*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bInverseExistingProjection"))
	bool bAlignLocalTransform = false;
	
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bInverseExistingProjection", DisplayName="Projection"))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

};

struct PCGEXTENDEDTOOLKIT_API FPCGExFlatProjectContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExFlatProjectElement;

	virtual ~FPCGExFlatProjectContext() override;

	FName CachedTransformAttributeName = NAME_None;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFlatProjectElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFlatProject
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		bool bInverseExistingProjection = false;
		bool bProjectLocalTransform = false;
		FPCGExGeo2DProjectionSettings ProjectionSettings;

		PCGEx::TFAttributeWriter<FTransform>* TransformWriter = nullptr;
		PCGEx::FAttributeIOBase<FTransform>* TransformReader = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
