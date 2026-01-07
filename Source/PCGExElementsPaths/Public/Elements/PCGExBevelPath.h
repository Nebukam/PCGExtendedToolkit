// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Math/PCGExMathMean.h"
#include "Core/PCGExPathProcessor.h"
#include "Details/PCGExSubdivisionDetails.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExBevelPath.generated.h"

namespace PCGExPaths
{
	class FPathEdgeLength;
	class FPath;
	template <typename T>
	class TPathEdgeExtra;
}

namespace PCGExBevelPath
{
	const FName SourceBevelFilters = TEXT("Bevel Conditions");
	const FName SourceCustomProfile = TEXT("Profile");
}

UENUM()
enum class EPCGExBevelMode : uint8
{
	Radius   = 0 UMETA(DisplayName = "Radius", ToolTip="Width is used as a radius value to compute distance along each point neighboring segments"),
	Distance = 1 UMETA(DisplayName = "Distance", ToolTip="Width is used as a distance along each point neighboring segments"),
};

UENUM()
enum class EPCGExBevelProfileType : uint8
{
	Line   = 0 UMETA(DisplayName = "Line", ToolTip="Line profile"),
	Arc    = 1 UMETA(DisplayName = "Arc", ToolTip="Arc profile"),
	Custom = 2 UMETA(DisplayName = "Custom", ToolTip="Custom profile"),
};

UENUM()
enum class EPCGExBevelLimit : uint8
{
	None            = 0 UMETA(DisplayName = "None", ToolTip="Bevel is not limited"),
	ClosestNeighbor = 1 UMETA(DisplayName = "Closest neighbor", ToolTip="Closest neighbor position is used as upper limit"),
	Balanced        = 2 UMETA(DisplayName = "Balanced", ToolTip="Weighted balance against opposite bevel position, falling back to closest neighbor"),
};

UENUM()
enum class EPCGExBevelCustomProfileScaling : uint8
{
	Uniform  = 0 UMETA(DisplayName = "Uniform", ToolTip="Keep the profile ratio uniform"),
	Scale    = 1 UMETA(DisplayName = "Scale", ToolTip="Use a scale factor relative to the bevel distance"),
	Distance = 2 UMETA(DisplayName = "Distance", ToolTip="Use a fixed distance relative to the bevelled point"),
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/bevel"))
class UPCGExBevelPathSettings : public UPCGExPathProcessorSettings
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
	PCGEX_NODE_POINT_FILTER(PCGExBevelPath::SourceBevelFilters, "Filters used to know if a point should be Beveled", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Type of Bevel operation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExBevelMode Mode = EPCGExBevelMode::Radius;

	/** Type of Bevel profile */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExBevelProfileType Type = EPCGExBevelProfileType::Line;

	/** Whether to keep the corner point or not. If enabled, subdivision is ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Type == EPCGExBevelProfileType::Line", EditConditionHides))
	bool bKeepCornerPoint = false;

	/** Define how the custom profile will be scaled on the main axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Profile Scaling", meta = (PCG_Overridable, EditCondition="Type == EPCGExBevelProfileType::Custom", EditConditionHides))
	EPCGExBevelCustomProfileScaling MainAxisScaling = EPCGExBevelCustomProfileScaling::Uniform;

	/** Scale or Distance value for the main axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Profile Scaling", meta = (PCG_Overridable, EditCondition="Type == EPCGExBevelProfileType::Custom && (MainAxisScaling == EPCGExBevelCustomProfileScaling::Scale || MainAxisScaling == EPCGExBevelCustomProfileScaling::Distance)", EditConditionHides))
	double MainAxisScale = 1;

	/** Define how the custom profile will be scaled on the cross axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Profile Scaling", meta = (PCG_Overridable, EditCondition="Type == EPCGExBevelProfileType::Custom", EditConditionHides))
	EPCGExBevelCustomProfileScaling CrossAxisScaling = EPCGExBevelCustomProfileScaling::Uniform;

	/** Scale or Distance value for the cross axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Profile Scaling", meta = (PCG_Overridable, EditCondition="Type == EPCGExBevelProfileType::Custom && (CrossAxisScaling == EPCGExBevelCustomProfileScaling::Scale || CrossAxisScaling == EPCGExBevelCustomProfileScaling::Distance)", EditConditionHides))
	double CrossAxisScale = 1;

	/** Bevel width value interpretation.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExMeanMeasure WidthMeasure = EPCGExMeanMeasure::Relative;

	/** Bevel width source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType WidthInput = EPCGExInputValueType::Constant;

	/** Bevel width attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Width (Attr)", EditCondition="WidthInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WidthAttribute;

	/** Bevel width constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Width", EditCondition="WidthInput == EPCGExInputValueType::Constant", EditConditionHides))
	double WidthConstant = 0.1;

	/** Bevel limit type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExBevelLimit Limit = EPCGExBevelLimit::Balanced;


	/** Whether to subdivide the profile */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta = (PCG_Overridable, EditCondition="Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	bool bSubdivide = false;

	/** Subdivision method */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta = (PCG_Overridable, EditCondition="bSubdivide && Type != EPCGExBevelProfileType::Custom", EditConditionHides))
	EPCGExSubdivideMode SubdivideMethod = EPCGExSubdivideMode::Count;

	/** Whether to subdivide the profile */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta = (PCG_Overridable, EditCondition="bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod != EPCGExSubdivideMode::Manhattan", EditConditionHides))
	EPCGExInputValueType SubdivisionAmountInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, DisplayName="Subdivisions (Distance)", EditCondition="bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod == EPCGExSubdivideMode::Distance && SubdivisionAmountInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0.1))
	double SubdivisionDistance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, DisplayName="Subdivisions (Count)", EditCondition="bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod == EPCGExSubdivideMode::Count && SubdivisionAmountInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 SubdivisionCount = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, DisplayName="Subdividions (Attr)", EditCondition="bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod != EPCGExSubdivideMode::Manhattan && SubdivisionAmountInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SubdivisionAmount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Subdivision", meta=(PCG_Overridable, DisplayName="Manhattan", EditCondition="bSubdivide && Type != EPCGExBevelProfileType::Custom && SubdivideMethod == EPCGExSubdivideMode::Manhattan", EditConditionHides))
	FPCGExManhattanDetails ManhattanDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagPoles = false;

	/** Name of the boolean flag to write whether the point is a Bevel endpoint or not (Either start or end) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Flags", meta = (PCG_Overridable, EditCondition="bFlagPoles"))
	FName PoleFlagName = "IsBevelPole";

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

	PCGEX_SETTING_VALUE_DECL(Width, double)
	PCGEX_SETTING_VALUE_DECL(Subdivisions, double)

	void InitOutputFlags(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const;
};

struct FPCGExBevelPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExBevelPathElement;

	TSharedPtr<PCGExData::FFacade> CustomProfileFacade;

	TArray<FVector> CustomProfilePositions;
	double CustomLength;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBevelPathElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BevelPath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExBevelPath
{
	class FProcessor;

	struct PCGEXELEMENTSPATHS_API FBevel
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

		double Length = 0;
		double Width = 0;

		double CustomMainAxisScale = 1;
		double CustomCrossAxisScale = 1;

		TArray<FVector> Subdivisions;

		FPCGExManhattanDetails ManhattanDetails;

		FBevel(const int32 InIndex, const FProcessor* InProcessor);

		~FBevel()
		{
			Subdivisions.Empty();
		}

		void Balance(const FProcessor* InProcessor);
		void Compute(const FProcessor* InProcessor);

		void SubdivideLine(const double Factor, bool bIsCount, const bool bKeepCorner);
		void SubdivideArc(const double Factor, bool bIsCount);
		void SubdivideCustom(const FProcessor* InProcessor);
		void SubdivideManhattan(const FProcessor* InProcessor);
	};

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBevelPathContext, UPCGExBevelPathSettings>
	{
		friend struct FBevel;

		TArray<TSharedPtr<FBevel>> Bevels;
		TArray<int32> StartIndices;

		bool bKeepCorner = false;
		bool bSubdivide = false;
		bool bSubdivideCount = false;
		bool bArc = false;

		TSharedPtr<PCGExDetails::TSettingValue<double>> WidthGetter;
		TSharedPtr<PCGExDetails::TSettingValue<double>> SubdivAmountGetter;

		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;
		TSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>> PathDirection;

		TSharedPtr<PCGExData::TBuffer<bool>> EndpointsWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> StartPointWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> EndPointWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> SubdivisionWriter;

		FPCGExManhattanDetails ManhattanDetails;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
			DefaultPointFilterValue = true;
		}

		double Len(const int32 Index) const;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		void PrepareSinglePoint(const int32 Index);

		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		void WriteFlags(const int32 Index);
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
