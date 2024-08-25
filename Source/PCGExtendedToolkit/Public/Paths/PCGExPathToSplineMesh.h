// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPointsProcessor.h"

#include "Tangents/PCGExTangentsOperation.h"
#include "Components/SplineMeshComponent.h"

#include "PCGExPathToSplineMesh.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Transform Component Selector"))
enum class EPCGExMeshSelectionMode : uint8
{
	NameAttribute UMETA(DisplayName = "Name Attribute", ToolTip="Uses a name attribute to fetch a datatable entry."),
	IndexAttribute UMETA(DisplayName = "Index Attribute", ToolTip="Uses an index attribute to fetch a datatable entry."),
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathToSplineMeshSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathToSplineMesh, "Path : To Spline Mesh", "Create spline mesh components from paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/** Whether to read tangents from attributes or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tangents", meta = (PCG_Overridable))
	bool bTangentsFromAttributes = false;

	/** Arrive tangent attribute (will be broadcast to FVector under the hood) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tangents", meta=(PCG_Overridable, EditCondition="bTangentsFromAttributes", EditConditionHides))
	FPCGAttributePropertyInputSelector Arrive;

	/** Leave tangent attribute (will be broadcast to FVector under the hood) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tangents", meta=(PCG_Overridable, EditCondition="bTangentsFromAttributes", EditConditionHides))
	FPCGAttributePropertyInputSelector Leave;

	/** In-place tangent solver */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings|Tangents", Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault, EditCondition="!bTangentsFromAttributes", EditConditionHides))
	TObjectPtr<UPCGExTangentsOperation> Tangents;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

	/** Force meshes/materials to load synchronously. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
	bool bSynchronousLoad = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathToSplineMeshContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPathToSplineMeshElement;

	virtual ~FPCGExPathToSplineMeshContext() override;

	UPCGExTangentsOperation* Tangents = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathToSplineMeshElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathToSplineMesh
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		const UPCGExPathToSplineMeshSettings* LocalSettings = nullptr;
		bool bClosedPath = false;
		int32 LastIndex = 0;

		PCGExData::FCache<FVector>* ArriveReader = nullptr;
		PCGExData::FCache<FVector>* LeaveReader = nullptr;

		UPCGExTangentsOperation* Tangents = nullptr;

		TArray<FSplineMeshParams> SplineMeshParams;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;

		virtual void Output() override;
	};
}
