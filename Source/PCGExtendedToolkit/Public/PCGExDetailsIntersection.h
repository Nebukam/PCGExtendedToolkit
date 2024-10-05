// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGExDetails.h"

#include "PCGExDetailsIntersection.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExIntersectionMetaDetails
{
	GENERATED_BODY()

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsUnion = false;

	/** Name of the attribute to mark point as union or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Is Union", EditCondition="bWriteIsUnion"))
	FName IsUnionAttributeName = "bIsUnion";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteUnionSize = false;

	/** Name of the attribute to mark the number of fused point held */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Union Size", EditCondition="bWriteUnionSize"))
	FName UnionSizeAttributeName = "UnionSize";

	bool WriteAny() const { return bWriteIsUnion || bWriteUnionSize; }
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointIntersectionMetaDetails : public FPCGExIntersectionMetaDetails
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeIntersectionMetaDetails : public FPCGExIntersectionMetaDetails
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointPointIntersectionDetails
{
	GENERATED_BODY()

	FPCGExPointPointIntersectionDetails()
	{
	}

	explicit FPCGExPointPointIntersectionDetails(bool InSupportEdges):
		bSupportsEdges(InSupportEdges)
	{
	}

	UPROPERTY()
	bool bSupportsEdges = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFuseDetails FuseDetails;

	/**  Point Union Data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPointIntersectionMetaDetails PointUnionData;

	/**  Edge Union Data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bSupportsEdges", EditConditionHides, HideEditConditionToggle))
	FPCGExPointIntersectionMetaDetails EdgeUnionData;

	bool WriteAny() const { return bSupportsEdges ? (PointUnionData.WriteAny() || EdgeUnionData.WriteAny()) : PointUnionData.WriteAny(); }
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointEdgeIntersectionDetails
{
	GENERATED_BODY()

	/** If disabled, points will only check edges they aren't mapped to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSourceFuseDetails FuseDetails;

	/** When enabled, point will be moved exactly on the edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSnapOnEdge = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsIntersector = false;

	/** Name of the attribute to flag point as intersector (result of an Point/Edge intersection) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteIsIntersector"))
	FName IsIntersectorAttributeName = "bIsIntersector";

	void MakeSafeForTolerance(const double FuseTolerance)
	{
		FuseDetails.Tolerance = FMath::Clamp(FuseDetails.Tolerance, 0, FuseTolerance * 0.5);
		FuseDetails.Tolerances.X = FMath::Clamp(FuseDetails.Tolerances.X, 0, FuseTolerance * 0.5);
		FuseDetails.Tolerances.Y = FMath::Clamp(FuseDetails.Tolerances.Y, 0, FuseTolerance * 0.5);
		FuseDetails.Tolerances.Z = FMath::Clamp(FuseDetails.Tolerances.Z, 0, FuseTolerance * 0.5);
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExEdgeEdgeIntersectionDetails
{
	GENERATED_BODY()

	/** If disabled, edges will only be checked against other datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = 0.001;
	double ToleranceSquared = 0.001;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinAngle = true;

	/** Min angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MinAngle = 0;
	double MinDot = -1;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxAngle = true;

	/** Maximum angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MaxAngle = 90;
	double MaxDot = 1;

	//

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCrossing = false;

	/** Name of the attribute to flag point as crossing (result of an Edge/Edge intersection) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteCrossing"))
	FName CrossingAttributeName = "bCrossing";

	/** Will copy the flag values of attributes from the edges onto the point in order to filter them. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable))
	bool bFlagCrossing = false;

	/** Name of an int32 flag to fetch from the first edge */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bFlagCrossing"))
	FName FlagA;

	/** Name of an int32 flag to fetch from the second edge */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bFlagCrossing"))
	FName FlagB;

	void Init()
	{
		MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
		MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
		ToleranceSquared = Tolerance * Tolerance;
	}

	FORCEINLINE bool CheckDot(const double InDot) const { return InDot <= MaxDot && InDot >= MinDot; }
};

namespace PCGExDetails
{
#pragma region Distance Settings

	static FPCGExDistanceDetails GetDistanceDetails(const FPCGExEdgeEdgeIntersectionDetails& InSettings)
	{
		return FPCGExDistanceDetails(EPCGExDistance::Center, EPCGExDistance::Center);
	}

	static FPCGExDistanceDetails GetDistanceDetails(const FPCGExPointPointIntersectionDetails& InSettings)
	{
		return FPCGExDistanceDetails(InSettings.FuseDetails.SourceDistance, InSettings.FuseDetails.TargetDistance);
	}

	static FPCGExDistanceDetails GetDistanceDetails(const FPCGExPointEdgeIntersectionDetails& InSettings)
	{
		return FPCGExDistanceDetails(InSettings.FuseDetails.SourceDistance, EPCGExDistance::Center);
	}

#pragma endregion
}

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
