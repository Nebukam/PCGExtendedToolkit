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

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bevel Mode"))
enum class EPCGExBevelMode : uint8
{
	Radius   = 0 UMETA(DisplayName = "Radius", ToolTip="Width is used as a radius value to compute distance along each point neighboring segments"),
	Distance = 1 UMETA(DisplayName = "Distance", ToolTip="Width is used as a distance along each point neighboring segments"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bevel Profile Type"))
enum class EPCGExBevelProfileType : uint8
{
	Line   = 0 UMETA(DisplayName = "Line", ToolTip="Line profile"),
	Arc    = 1 UMETA(DisplayName = "Arc", ToolTip="Arc profile"),
	Custom = 2 UMETA(DisplayName = "Custom", ToolTip="Custom profile"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bevel Limit"))
enum class EPCGExBevelLimit : uint8
{
	None            = 0 UMETA(DisplayName = "None", ToolTip="Bevel is not limited"),
	ClosestNeighbor = 1 UMETA(DisplayName = "Closest neighbor", ToolTip="Closest neighbor position is used as upper limit"),
	Balanced        = 2 UMETA(DisplayName = "Balanced", ToolTip="Weighted balance against opposite bevel position, falling back to closest neighbor"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBevelPathSettings : public UPCGExPathProcessorSettings
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
	PCGEX_NODE_POINT_FILTER(PCGExBevelPath::SourceBevelFilters, "Filters used to know if a point should be Beveled", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

public:
	/** Type of Bevel operation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExBevelMode Mode = EPCGExBevelMode::Radius;

	/** Type of Bevel profile */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExBevelProfileType Type = EPCGExBevelProfileType::Line;

	/** Whether to keep the corner point or not. If enabled, subdivision is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	bool bKeepCornerPoint = false;

	/** Bevel width value interpretation.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExMeanMeasure WidthMeasure = EPCGExMeanMeasure::Relative;

	/** Bevel width source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFetchType WidthSource = EPCGExFetchType::Constant;

	/** Bevel width constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WidthSource == EPCGExFetchType::Constant", EditConditionHides))
	double WidthConstant = 0.1;

	/** Bevel width attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WidthSource == EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector WidthAttribute;

	/** Bevel limit type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBevelLimit Limit = EPCGExBevelLimit::Balanced;


	/** Whether to subdivide the profile */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta = (PCG_Overridable, EditCondition="!bKeepCornerPoint && Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	bool bSubdivide = false;

	/** Subdivision method */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta = (PCG_Overridable, EditCondition="!bKeepCornerPoint && bSubdivide && Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	EPCGExSubdivideMode SubdivideMethod = EPCGExSubdivideMode::Count;

	/** Whether to subdivide the profile */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta = (PCG_Overridable, EditCondition="!bKeepCornerPoint && bSubdivide && Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	EPCGExFetchType SubdivisionValueSource = EPCGExFetchType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, EditCondition="!bKeepCornerPoint && bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod==EPCGExSubdivideMode::Distance && SubdivisionValueSource==EPCGExFetchType::Constant", EditConditionHides, ClampMin=0.1))
	double SubdivisionDistance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, EditCondition="!bKeepCornerPoint && bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod==EPCGExSubdivideMode::Count && SubdivisionValueSource==EPCGExFetchType::Constant", EditConditionHides, ClampMin=1))
	int32 SubdivisionCount = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, EditCondition="!bKeepCornerPoint && bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivisionValueSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SubdivisionAmount;

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


	void InitOutputFlags(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBevelPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExBevelPathElement;

	TSharedPtr<PCGExData::FFacade> CustomProfileFacade;

	TArray<FVector> CustomProfilePositions;
	double CustomLength;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBevelPathElement final : public FPCGExPathProcessorElement
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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FBevel
	{
		int32 Index = -1;
		int32 ArriveIdx = -1;
		int32 LeaveIdx = -1;

		int32 StartOutputIndex = -1;
		int32 EndOutputIndex = -1;

		FVector Corner = FVector::ZeroVector;

		FVector PrevLocation = FVector::ZeroVector;
		FVector Arrive = FVector::ZeroVector;
		FVector ArriveDir = FVector::ZeroVector;
		double ArriveAlpha = 0;

		FVector NextLocation = FVector::ZeroVector;
		FVector Leave = FVector::ZeroVector;
		FVector LeaveDir = FVector::ZeroVector;
		double LeaveAlpha = 0;

		double Width = 0;

		TArray<FVector> Subdivisions;

		FBevel(const int32 InIndex, const FProcessor* InProcessor);

		~FBevel()
		{
			Subdivisions.Empty();
		}

		void Balance(const FProcessor* InProcessor);
		void Compute(const FProcessor* InProcessor);

		void SubdivideLine(const double Factor, const double bIsCount);
		void SubdivideArc(const double Factor, const double bIsCount);
		void SubdivideCustom(const FProcessor* InProcessor);
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBevelPathContext, UPCGExBevelPathSettings>
	{
		friend struct FBevel;

		TArray<double> Lengths;

		TArray<TSharedPtr<FBevel>> Bevels;
		TArray<int32> StartIndices;

		bool bClosedLoop = false;
		bool bSubdivide = false;
		bool bSubdivideCount = false;
		bool bArc = false;
		TSharedPtr<PCGExData::TBuffer<double>> WidthGetter;
		TSharedPtr<PCGExData::TBuffer<double>> SubdivAmountGetter;
		double ConstantSubdivAmount = 0;

		TSharedPtr<PCGExData::TBuffer<bool>> EndpointsWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> StartPointWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> EndPointWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> SubdivisionWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
			DefaultPointFilterValue = true;
		}

		FORCEINLINE double Len(const int32 Index) const { return Lengths[Index]; }

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		void WriteFlags(const int32 Index);
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
