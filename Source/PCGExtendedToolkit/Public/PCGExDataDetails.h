// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "PCGEx.h"
#include "Data/PCGExData.h"

#include "PCGExDataDetails.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExInfluenceDetails
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

	TSharedPtr<PCGExData::TBuffer<double>> InfluenceCache;

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
	{
		if (bUseLocalInfluence)
		{
			InfluenceCache = InPointDataFacade->GetBroadcaster<double>(LocalInfluence);
			if (!InfluenceCache)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Influence attribute: \"{0}\"."), FText::FromName(LocalInfluence.GetName())));
				return false;
			}
		}
		return true;
	}

	FORCEINLINE double GetInfluence(const int32 PointIndex) const
	{
		return InfluenceCache ? InfluenceCache->Read(PointIndex) : Influence;
	}
};

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
