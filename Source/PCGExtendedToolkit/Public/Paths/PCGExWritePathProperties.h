// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"


#include "Sampling/PCGExSampling.h"
#include "PCGExWritePathProperties.generated.h"

#define PCGEX_FOREACH_FIELD_PATH(MACRO)\
MACRO(Dot, double, 0)\
MACRO(Angle, double, 0)\
MACRO(DistanceToNext, double, 0)\
MACRO(DistanceToPrev, double, 0)\
MACRO(DistanceToStart, double, 0)\
MACRO(DistanceToEnd, double, 0)\
MACRO(PointTime, double, 0)\
MACRO(PointNormal, FVector, FVector::OneVector)\
MACRO(PointAvgNormal, FVector, FVector::OneVector)\
MACRO(PointBinormal, FVector, FVector::OneVector)\
MACRO(DirectionToNext, FVector, FVector::OneVector)\
MACRO(DirectionToPrev, FVector, FVector::OneVector)

#define PCGEX_FOREACH_FIELD_PATH_MARKS(MACRO)\
MACRO(PathLength, double, 0)\
MACRO(PathDirection, FVector, FVector::OneVector)\
MACRO(PathCentroid, FVector, FVector::ZeroVector)

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExWritePathPropertiesSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WritePathProperties, "Path : Properties", "Extract & write extra path informations to the path.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Output Path Length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Path", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePathLength = false;

	/** Name of the 'double' attribute to write path length to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Path", meta=(DisplayName="PathLength", PCG_Overridable, EditCondition="bWritePathLength"))
	FName PathLengthAttributeName = FName("PathLength");


	/** Output averaged path direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Path", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePathDirection = false;

	/** Name of the 'FVector' attribute to write averaged direction to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Path", meta=(DisplayName="PathDirection", PCG_Overridable, EditCondition="bWritePathDirection"))
	FName PathDirectionAttributeName = FName("PathDirection");

	/** Output averaged path direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Path", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePathCentroid = false;

	/** Name of the 'FVector' attribute to write averaged direction to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Path", meta=(DisplayName="PathCentroid", PCG_Overridable, EditCondition="bWritePathCentroid"))
	FName PathCentroidAttributeName = FName("PathCentroid");

	/** Up Attribute constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta = (PCG_Overridable))
	FVector UpVector = FVector::UpVector;

	/** Output Dot product of Prev/Next directions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDot = false;

	/** Name of the 'double' attribute to write distance to next point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="Dot", PCG_Overridable, EditCondition="bWriteDot"))
	FName DotAttributeName = FName("Dot");

	/** Output Dot product of Prev/Next directions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write angle to next point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="Angle", PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("Angle");

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle", EditConditionHides, HideEditConditionToggle))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;

	/** Output distance to next. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistanceToNext = false;

	/** Name of the 'double' attribute to write distance to next point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="DistanceToNext", PCG_Overridable, EditCondition="bWriteDistanceToNext"))
	FName DistanceToNextAttributeName = FName("DistanceToNext");


	/** Output distance to prev. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistanceToPrev = false;

	/** Name of the 'double' attribute to write distance to prev point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="DistanceToPrev", PCG_Overridable, EditCondition="bWriteDistanceToPrev"))
	FName DistanceToPrevAttributeName = FName("DistanceToPrev");


	/** Output distance to start. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistanceToStart = false;

	/** Name of the 'double' attribute to write distance to start to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="DistanceToStart", PCG_Overridable, EditCondition="bWriteDistanceToStart"))
	FName DistanceToStartAttributeName = FName("DistanceToStart");


	/** Output distance to end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistanceToEnd = false;

	/** Name of the 'double' attribute to write distance to start to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="DistanceToEnd", PCG_Overridable, EditCondition="bWriteDistanceToEnd"))
	FName DistanceToEndAttributeName = FName("DistanceToEnd");


	/** Output distance to end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointTime = false;

	/** Name of the 'double' attribute to write distance to start to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="PointTime", PCG_Overridable, EditCondition="bWritePointTime"))
	FName PointTimeAttributeName = FName("PointTime");

	/** Output point normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointNormal = false;

	/** Name of the 'FVector' attribute to write point normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="PointNormal", PCG_Overridable, EditCondition="bWritePointNormal"))
	FName PointNormalAttributeName = FName("PointNormal");

	/** Output point normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointAvgNormal = false;

	/** Name of the 'FVector' attribute to write point averaged normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="PointAverageNormal", PCG_Overridable, EditCondition="bWritePointAvgNormal"))
	FName PointAvgNormalAttributeName = FName("PointAvgNormal");

	/** Output point normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointBinormal = false;

	/** Name of the 'FVector' attribute to write point binormal to. Note that it's stabilized.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="PointBinormal", PCG_Overridable, EditCondition="bWritePointBinormal"))
	FName PointBinormalAttributeName = FName("PointBinormal");

	/** Output direction to next normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDirectionToNext = false;

	/** Name of the 'FVector' attribute to write direction to next point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="DirectionToNext", PCG_Overridable, EditCondition="bWriteDirectionToNext"))
	FName DirectionToNextAttributeName = FName("DirectionToNext");


	/** Output direction to prev normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDirectionToPrev = false;

	/** Name of the 'FVector' attribute to write direction to prev point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output - Points", meta=(DisplayName="DirectionToPrev", PCG_Overridable, EditCondition="bWriteDirectionToPrev"))
	FName DirectionToPrevAttributeName = FName("DirectionToPrev");

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tags", meta=(InlineEditConditionToggle))
	bool bTagConcave = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tags", meta=(EditCondition="bTagConcave"))
	FString ConcaveTag = TEXT("Concave");

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tags", meta=(InlineEditConditionToggle))
	bool bTagConvex = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tags", meta=(EditCondition="bTagConvex"))
	FString ConvexTag = TEXT("Convex");
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWritePathPropertiesContext final : FPCGExPathProcessorContext
{
	friend class FPCGExWritePathPropertiesElement;

	PCGEX_FOREACH_FIELD_PATH(PCGEX_OUTPUT_DECL_TOGGLE)
	PCGEX_FOREACH_FIELD_PATH_MARKS(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWritePathPropertiesElement final : public FPCGExPathProcessorElement
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

namespace PCGExWritePathProperties
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointDetails
	{
		int32 Index;
		FVector Normal;
		FVector Binormal;
		FVector ToPrev;
		FVector ToNext;
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExWritePathPropertiesContext, UPCGExWritePathPropertiesSettings>
	{
		PCGEX_FOREACH_FIELD_PATH(PCGEX_OUTPUT_DECL)

		bool bClosedLoop = false;
		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;
		TSharedPtr<PCGExPaths::FPathEdgeBinormal> PathBinormal;
		TSharedPtr<PCGExPaths::FPathEdgeAvgNormal> PathAvgNormal;

		TArray<FPointDetails> Details;

		FVector UpConstant = FVector::ZeroVector;
		TSharedPtr<PCGExData::TBuffer<FVector>> UpGetter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
