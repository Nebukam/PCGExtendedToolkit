// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Pickers/PCGExPickerAttributeSetRanges.h"

#define LOCTEXT_NAMESPACE "PCGExCreatePickerConstantSet"
#define PCGEX_NAMESPACE CreatePickerConstantSet

PCGEX_PICKER_BOILERPLATE(AttributeSetRanges, {}, {})

#if WITH_EDITOR
FString UPCGExPickerAttributeSetRangesSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Pick Set(s)");

	if (Config.bTreatAsNormalized)
	{
		//DisplayName += FString::Printf(TEXT("%.2f"), Config.RelativeIndex);
	}
	else
	{
		//DisplayName += FString::Printf(TEXT("%d"), Config.DiscreteIndex);
	}

	return DisplayName;
}
#endif

void UPCGExPickerAttributeSetRangesFactory::AddPicks(const int32 InNum, TSet<int32>& OutPicks) const
{
	for (const FPCGExPickerConstantRangeConfig& RangeConfig : Ranges) { UPCGExPickerConstantRangeFactory::AddPicksFromConfig(RangeConfig, InNum, OutPicks); }
}

bool UPCGExPickerAttributeSetRangesFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	TArray<TSharedPtr<PCGExData::FFacade>> Facades;
	if (!TryGetFacades(InContext, FName("Indices"), Facades, false, true))
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No valid data was found for indices."));
		return false;
	}

	TSet<FVector2D> UniqueRanges;

	for (const TSharedPtr<PCGExData::FFacade>& Facade : Facades)
	{
		if (Config.Attributes.IsEmpty())
		{
			const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(Facade->Source->GetIn()->Metadata);
			if (Infos->Attributes.IsEmpty())
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Some input have no attributes."));
				continue;
			}

			const TSharedPtr<PCGEx::TAttributeBroadcaster<FVector2D>> Values = PCGEx::TAttributeBroadcaster<FVector2D>::Make(Infos->Attributes[0]->Name, Facade->Source);
			if (!Values) { continue; }
			Values->GrabUniqueValues(UniqueRanges);
		}
		else
		{
			for (const FPCGAttributePropertyInputSelector& Selector : Config.Attributes)
			{
				const TSharedPtr<PCGEx::TAttributeBroadcaster<FVector2D>> Values = PCGEx::TAttributeBroadcaster<FVector2D>::Make(Selector, Facade->Source);
				if (!Values) { continue; }
				Values->GrabUniqueValues(UniqueRanges);
			}
		}
	}

	// Create a range config per unique range found
	for (const FVector2D& Range : UniqueRanges)
	{
		FPCGExPickerConstantRangeConfig& RangeConfig = Ranges.Emplace_GetRef();

		RangeConfig.bTreatAsNormalized = Config.bTreatAsNormalized;
		RangeConfig.TruncateMode = Config.TruncateMode;
		RangeConfig.Safety = Config.Safety;

		RangeConfig.DiscreteStartIndex = Range.X;
		RangeConfig.RelativeStartIndex = Range.X;

		RangeConfig.DiscreteEndIndex = Range.Y;
		RangeConfig.RelativeEndIndex = Range.Y;
	}

	return true;
}

TArray<FPCGPinProperties> UPCGExPickerAttributeSetRangesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(FName("Indices"), "Data to read attribute from", Required, {})
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
