// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGEx.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExDataDetails.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInfluenceDetails
{
	GENERATED_BODY()

	FPCGExInfluenceDetails()
	{
	}

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalInfluence = false;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseLocalInfluence"))
	FPCGAttributePropertyInputSelector LocalInfluence;

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;

	PCGExData::FCache<double>* InfluenceCache = nullptr;

	bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
	{
		if (bUseLocalInfluence)
		{
			InfluenceCache = InPointDataFacade->GetOrCreateGetter<double>(LocalInfluence);
			if (!InfluenceCache)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Influence attribute: {0}."), FText::FromName(LocalInfluence.GetName())));
				return false;
			}
		}
		return true;
	}

	FORCEINLINE double GetInfluence(const int32 PointIndex) const
	{
		return InfluenceCache ? InfluenceCache->Values[PointIndex] : Influence;
	}
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBoxIntersectionDetails
{
	GENERATED_BODY()

	/** If enabled, mark non-intersecting points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bMarkPointsIntersections = true;

	/** Name of the attribute to write point intersection boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bMarkPointsIntersections" ))
	FName IsIntersectionAttributeName = FName("IsIntersection");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bMarkIntersectingBoundIndex = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bMarkIntersectingBoundIndex" ))
	FName IntersectionBoundIndexAttributeName = FName("BoundIndex");

	/** If enabled, mark points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bMarkPointsInside = false;

	/** Name of the attribute to write inside boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bMarkPointsInside" ))
	FName IsInsideAttributeName = FName("IsInside");

	bool Validate(const FPCGContext* InContext) const
	{
		if (bMarkPointsIntersections) { PCGEX_VALIDATE_NAME_C(InContext, IsIntersectionAttributeName) }
		if (bMarkIntersectingBoundIndex) { PCGEX_VALIDATE_NAME_C(InContext, IntersectionBoundIndexAttributeName) }
		if (bMarkPointsInside) { PCGEX_VALIDATE_NAME_C(InContext, IsInsideAttributeName) }
		return true;
	}

	bool WillWriteAny() const
	{
		return
			bMarkPointsIntersections ||
			bMarkPointsInside ||
			bMarkIntersectingBoundIndex;
	}

	void Mark(UPCGMetadata* Metadata) const
	{
		if (bMarkPointsIntersections) { PCGExData::WriteMark(Metadata, IsIntersectionAttributeName, false); }
		if (bMarkPointsInside) { PCGExData::WriteMark(Metadata, IsInsideAttributeName, false); }
		if (bMarkIntersectingBoundIndex) { PCGExData::WriteMark(Metadata, IntersectionBoundIndexAttributeName, -1); }
	}
};

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
