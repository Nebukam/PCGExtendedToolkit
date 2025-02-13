// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Pickers/PCGExPickerConstantSet.h"

#define LOCTEXT_NAMESPACE "PCGExCreatePickerConstantSet"
#define PCGEX_NAMESPACE CreatePickerConstantSet

PCGEX_PICKER_BOILERPLATE(ConstantSet, {}, {})

#if WITH_EDITOR
FString UPCGExPickerConstantSetSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Pick @");

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

void UPCGExPickerConstantSetFactory::AddPicks(const int32 InNum, TSet<int32>& OutPicks) const
{
	int32 TargetIndex = 0;
	const int32 MaxIndex = InNum - 1;

	if (Config.bTreatAsNormalized)
	{
		OutPicks.Reserve(OutPicks.Num() + RelativePicks.Num());
		for (const double Pick : RelativePicks)
		{
			TargetIndex = PCGEx::TruncateDbl(static_cast<double>(MaxIndex) * Pick, Config.TruncateMode);

			if (TargetIndex < 0) { TargetIndex = InNum + TargetIndex; }
			TargetIndex = PCGExMath::SanitizeIndex(TargetIndex, MaxIndex, Config.Safety);

			if (FMath::IsWithin(TargetIndex, 0, InNum)) { OutPicks.Add(TargetIndex); }
		}
	}
	else
	{
		OutPicks.Reserve(OutPicks.Num() + DiscretePicks.Num());
		for (const int32 Pick : DiscretePicks)
		{
			TargetIndex = Pick;

			if (TargetIndex < 0) { TargetIndex = InNum + TargetIndex; }
			TargetIndex = PCGExMath::SanitizeIndex(TargetIndex, MaxIndex, Config.Safety);

			if (!FMath::IsWithin(TargetIndex, 0, InNum)) { OutPicks.Add(TargetIndex); }
		}
	}
}

bool UPCGExPickerConstantSetFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	TArray<TSharedPtr<PCGExData::FFacade>> Facades;
	if (!TryGetFacades(InContext, FName("Indices"), Facades, false, true))
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No valid data was found for indices."));
		return false;
	}

	if (Config.bTreatAsNormalized)
	{
		TSet<double> UniqueIndices;
		for (const TSharedPtr<PCGExData::FFacade>& Facade : Facades)
		{
			for (const FPCGAttributePropertyInputSelector& Selector : Config.Attributes)
			{
				const TSharedPtr<PCGExData::TBuffer<double>> Buffer = Facade->GetBroadcaster<double>(Selector);
				if (!Buffer) { continue; }
				UniqueIndices.Append(*Buffer->GetOutValues().Get());
			}
		}

		RelativePicks = UniqueIndices.Array();
	}
	else
	{
		TSet<int32> UniqueIndices;
		for (const TSharedPtr<PCGExData::FFacade>& Facade : Facades)
		{
			for (const FPCGAttributePropertyInputSelector& Selector : Config.Attributes)
			{
				const TSharedPtr<PCGExData::TBuffer<int32>> Buffer = Facade->GetBroadcaster<int32>(Selector);
				if (!Buffer) { continue; }
				UniqueIndices.Append(*Buffer->GetOutValues().Get());
			}
		}

		DiscretePicks = UniqueIndices.Array();
	}

	return true;
}

TArray<FPCGPinProperties> UPCGExPickerConstantSetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(FName("Indices"), "Data to read attribute from", Required, {})
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
