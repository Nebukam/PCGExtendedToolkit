// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExChamferPath.generated.h"

namespace PCGExChamferPath
{
	const FName SourceChamferFilters = TEXT("ChamferConditions");
	const FName SourceRemoveFilters = TEXT("RemoveConditions");

	struct PCGEXTENDEDTOOLKIT_API FPath
	{
		int32 Start = -1;
		int32 End = -1;
		int32 Count = 0;

		FPath()
		{
		}
	};
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Chamfer Action"))
enum class EPCGExPathChamferAction : uint8
{
	Chamfer UMETA(DisplayName = "Chamfer", ToolTip="Duplicate the Chamfer point so the original becomes a new end, and the copy a new start."),
	Remove UMETA(DisplayName = "Remove", ToolTip="Remove the Chamfer point, shrinking both the previous and next paths."),
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExChamferPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ChamferPath, "Path : Chamfer", "Chamfer paths points.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;
	
};

struct PCGEXTENDEDTOOLKIT_API FPCGExChamferPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExChamferPathElement;

	virtual ~FPCGExChamferPathContext() override;

	TArray<UPCGExFilterFactoryBase*> ChamferFilterFactories;
};

class PCGEXTENDEDTOOLKIT_API FPCGExChamferPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExChamferPath
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExChamferPathContext* LocalTypedContext = nullptr;
		const UPCGExChamferPathSettings* LocalSettings = nullptr;

		bool bClosedPath = false;

		TArray<bool> DoChamfer;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
			DefaultPointFilterValue = true;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}
