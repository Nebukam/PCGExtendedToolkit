// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDataFilter.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExDataForward.generated.h"

namespace PCGExData
{
	class FDataForwardHandler;
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExForwardDetails : public FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExForwardDetails()
	{
	}

	/** Is forwarding enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	bool bEnabled = false;

	/** If enabled, will preserve the initial attribute default value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0, EditCondition="bEnabled"))
	bool bPreserveAttributesDefaultValue = false;

	void Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const
	{
		if (FilterMode == EPCGExAttributeFilter::All) { return; }
		for (int i = 0; i < Identities.Num(); ++i)
		{
			if (!Test(Identities[i].Name.ToString()))
			{
				Identities.RemoveAt(i);
				i--;
			}
		}
	}

	PCGExData::FDataForwardHandler* GetHandler(PCGExData::FFacade* InSourceDataFacade) const;
	PCGExData::FDataForwardHandler* GetHandler(PCGExData::FFacade* InSourceDataFacade, PCGExData::FFacade* InTargetDataFacade) const;
	PCGExData::FDataForwardHandler* TryGetHandler(PCGExData::FFacade* InSourceDataFacade) const;
	PCGExData::FDataForwardHandler* TryGetHandler(PCGExData::FFacade* InSourceDataFacade, PCGExData::FFacade* InTargetDataFacade) const;
};

namespace PCGExData
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FDataForwardHandler
	{
		FPCGExForwardDetails Details;
		FFacade* SourceDataFacade = nullptr;
		FFacade* TargetDataFacade = nullptr;
		TArray<PCGEx::FAttributeIdentity> Identities;
		TArray<PCGEx::FAttributeIOBase*> Readers;
		TArray<PCGEx::FAttributeIOBase*> Writers;

	public:
		~FDataForwardHandler();
		FDataForwardHandler(const FPCGExForwardDetails& InDetails, FFacade* InSourceDataFacade);
		FDataForwardHandler(const FPCGExForwardDetails& InDetails, FFacade* InSourceDataFacade, FFacade* InTargetDataFacade);
		FORCEINLINE bool IsEmpty() const { return Identities.IsEmpty(); }
		void Forward(const int32 SourceIndex, const int32 TargetIndex);
		void Forward(int32 SourceIndex, FFacade* InTargetDataFacade);
	};
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeToTagDetails
{
	GENERATED_BODY()

	FPCGExAttributeToTagDetails()
	{
	}

	/** Use reference point index to tag output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bAddIndexTag = false;

	/** Prefix added to the reference point index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bAddIndexTag"))
	FString IndexTagPrefix = TEXT("IndexTag:");

	/** Attributes which value will be used as tags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGAttributePropertyInputSelector> Attributes;

	PCGExData::FFacade* TagSource = nullptr;
	TArray<PCGEx::FLocalToStringGetter*> Getters;

	bool Init(const FPCGContext* InContext, PCGExData::FFacade* TagSourceFacade)
	{
		for (FPCGAttributePropertyInputSelector& Selector : Attributes)
		{
			PCGEx::FLocalToStringGetter* Getter = new PCGEx::FLocalToStringGetter();
			Getter->Capture(Selector);
			if (!Getter->SoftGrab(TagSourceFacade->Source))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing specified Tag attribute."));
				Cleanup();
				return false;
			}
			Getters.Add(Getter);
		}

		TagSource = TagSourceFacade;
		return true;
	}

	void Tag(const int32 TagIndex, const PCGExData::FPointIO* PointIO) const
	{
		if (bAddIndexTag) { PointIO->Tags->Add(IndexTagPrefix + FString::Printf(TEXT("%d"), TagIndex)); }

		if (!Getters.IsEmpty())
		{
			const FPCGPoint& Point = TagSource->GetIn()->GetPoint(TagIndex);
			for (PCGEx::FLocalToStringGetter* Getter : Getters)
			{
				FString Tag = Getter->SoftGet(TagIndex, Point, TEXT(""));
				if (Tag.IsEmpty()) { continue; }
				PointIO->Tags->Add(Tag);
			}
		}
	}

	void Cleanup()
	{
		PCGEX_DELETE_TARRAY(Getters);
	}
};
