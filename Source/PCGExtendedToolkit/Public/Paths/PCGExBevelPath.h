// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExBevelPath.generated.h"

namespace PCGExBevelPath
{
	const FName SourceBevelFilters = TEXT("BevelConditions");
	const FName SourceCustomProfile = TEXT("Profile");
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bevel Profile Type"))
enum class EPCGExBevelProfileType : uint8
{
	Straight UMETA(DisplayName = "Straight", ToolTip="Straight profile"),
	Round UMETA(DisplayName = "Round", ToolTip="Round profile"),
	Custom UMETA(DisplayName = "Custom", ToolTip="Custom profile"),
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExBevelPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BevelPath, "Path : Bevel", "Bevel paths points.");
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

	/** Type of Bevel operation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExBevelProfileType Type = EPCGExBevelProfileType::Straight;

	/** Whether to keep the corner point or not. If enabled, subdivision is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	bool bKeepCornerPoint = false;

	/** Insert additional points between start & end */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Type != EPCGExBevelProfileType::Custom && !bKeepCornerPoint", ClampMin=0, EditConditionHides))
	int32 NumSubdivision = 0;

	/** Bevel width value interpretation.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExMeanMeasure WidthMeasure = EPCGExMeanMeasure::Discrete;

	/** Bevel width source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFetchType WidthSource = EPCGExFetchType::Constant;

	/** Bevel width constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WidthSource == EPCGExFetchType::Constant", EditConditionHides))
	double WidthConstant = 0.1;

	/** Bevel width attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WidthSource == EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector WidthAttribute;

	/** If enabled, will use the smallest relative value for both sides of the bevel.\n This is often the preferred option. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="WidthMeasure == EPCGExMeanMeasure::Relative"))
	bool bUseSmallestRelative = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bLimit = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagEndpoints = false;

	/** Name of the boolean flag to write whether the point is a Bevel endpoint or not (Either start or end) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, EditCondition="bFlagEndpoints"))
	FName EndpointsFlagName = "IsBevelEndpoint";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagStartPoint = false;

	/** Name of the boolean flag to write whether the point is a Bevel start point or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, EditCondition="bFlagStartPoint"))
	FName StartPointFlagName = "IsBevelStart";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagEndPoint = false;

	/** Name of the boolean flag to write whether the point is a Bevel end point or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, EditCondition="bFlagEndPoint"))
	FName EndPointFlagName = "IsBevelEnd";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagSubdivision = false;

	/** Name of the boolean flag to write whether the point is a subdivision point or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, EditCondition="bFlagSubdivision"))
	FName SubdivisionFlagName = "IsSubdivision";


	void InitOutputFlags(const PCGExData::FPointIO* InPointIO) const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBevelPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExBevelPathElement;

	virtual ~FPCGExBevelPathContext() override;

	TArray<UPCGExFilterFactoryBase*> BevelFilterFactories;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBevelPathElement final : public FPCGExPathProcessorElement
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

namespace PCGExBevelPath
{
	class FProcessor;

	struct PCGEXTENDEDTOOLKIT_API FSegmentInfos
	{
		int32 StartIndex = -1;
		int32 EndIndex = -1;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;

		FSegmentInfos()
		{
		}

		FSegmentInfos(const FProcessor* InProcessor, const int32 InStartIndex, const int32 InEndIndex);

		FORCEINLINE double GetLength() const { return FVector::Dist(Start, End); }

		FORCEINLINE FVector GetStartEndDir() const { return (End - Start).GetSafeNormal(); }
		FORCEINLINE FVector GetEndStartDir() const { return (Start - End).GetSafeNormal(); }
	};

	struct PCGEXTENDEDTOOLKIT_API FBevel
	{
		int32 Index = -1;
		int32 StartOutputIndex = -1;
		int32 EndOutputIndex = -1;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		TArray<FVector> Subdivisions;

		FBevel(const int32 InIndex):
			Index(InIndex)
		{
		}
	};

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		friend struct FSegmentInfos;

		FPCGExBevelPathContext* LocalTypedContext = nullptr;
		const UPCGExBevelPathSettings* LocalSettings = nullptr;

		int32 Subdivisions = 0;

		TArray<FBevel*> Bevels;
		TArray<FSegmentInfos> Segments;
		TArray<int32> StartIndices;

		bool bClosedPath = false;
		PCGExData::FCache<double>* WidthGetter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
			DefaultPointFilterValue = true;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		void PrepareSinglePoint(const int32 Index, const FPCGPoint& Point);
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}
