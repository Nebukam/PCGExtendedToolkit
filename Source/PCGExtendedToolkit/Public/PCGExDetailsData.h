// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }

#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"

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

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade);
	double GetInfluence(const int32 PointIndex) const;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeToTagMatchDetails
{
	GENERATED_BODY()

	FPCGExAttributeToTagMatchDetails()
	{
	}

	/** Type of Tag Name value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType TagNameInput = EPCGExInputValueType::Constant;

	/** Attribute to read tag name value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name (Attr)", EditCondition="TagNameInput!=EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FName TagNameAttribute = FName("Tag");

	/** Constant tag name value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name", EditCondition="TagNameInput==EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FString TagName = TEXT("Tag");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Match"))
	EPCGExStringMatchMode NameMatch = EPCGExStringMatchMode::Equals;

	/** Whether to do a tag value match or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bDoValueMatch = false;

	/** Expected value type, this is a strict check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoValueMatch && Mode==EPCGExAttributeToTagMatchValueMode::TagValue"))
	EPCGExSupportedTagValue ExpectedType = EPCGExSupportedTagValue::Integer;

	/** Attribute to read tag name value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoValueMatch"))
	FPCGAttributePropertyInputSelector ValueAttribute;

	TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> TagNameGetter;

#define PCGEX_INIT_TAGMATCH(_NAME, _TYPE) TSharedPtr<PCGEx::TAttributeBroadcaster<_TYPE>> Tag##_NAME##Getter;
	PCGEX_FOREACH_SUPPORTEDTAGTYPE(PCGEX_INIT_TAGMATCH)
#undef PCGEX_INIT_TAGMATCH

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
	{
		if (TagNameInput == EPCGExInputValueType::Attribute)
		{
			TagNameGetter = MakeShared<PCGEx::TAttributeBroadcaster<FString>>();
			if (!TagNameGetter->Prepare(ValueAttribute, InPointDataFacade->Source))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag name attribute."));
				return false;
			}
		}

		if (!bDoValueMatch) { return true; }

#define PCGEX_INIT_TAGMATCH(_NAME, _TYPE) \
		Tag##_NAME##Getter = MakeShared<PCGEx::TAttributeBroadcaster<_TYPE>>(); \
		if (!Tag##_NAME##Getter->Prepare(ValueAttribute, InPointDataFacade->Source)) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag value attribute.")); return false; }

		switch (ExpectedType)
		{
#define PCGEX_INIT_TAGMATCH_CASE(_NAME, _TYPE) case EPCGExSupportedTagValue::_NAME : PCGEX_INIT_TAGMATCH(_NAME, _TYPE) break;
		PCGEX_FOREACH_SUPPORTEDTAGTYPE(PCGEX_INIT_TAGMATCH_CASE)
#undef PCGEX_INIT_TAGMATCH_CASE
		}

#undef PCGEX_INIT_TAGMATCH

		return true;
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
