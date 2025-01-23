// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "Data/PCGExData.h"

#include "PCGExDetailsData.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExInfluenceDetails
{
	GENERATED_BODY()

	FPCGExInfluenceDetails()
	{
	}

	/** Type of Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType InfluenceInput = EPCGExInputValueType::Constant;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Influence (Attr)", EditCondition="InfluenceInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalInfluence;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Influence", EditCondition="InfluenceInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;

	TSharedPtr<PCGExData::TBuffer<double>> InfluenceBuffer;

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
	{
		if (InfluenceInput == EPCGExInputValueType::Attribute)
		{
			InfluenceBuffer = InPointDataFacade->GetBroadcaster<double>(LocalInfluence);
			if (!InfluenceBuffer)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Influence attribute: \"{0}\"."), FText::FromName(LocalInfluence.GetName())));
				return false;
			}
		}
		return true;
	}

	FORCEINLINE double GetInfluence(const int32 PointIndex) const
	{
		return InfluenceBuffer ? InfluenceBuffer->Read(PointIndex) : Influence;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExComponentTaggingDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bForwardInputDataTags = true;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bOutputTagsToAttributes = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bAddTagsToData = false;
};

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS
