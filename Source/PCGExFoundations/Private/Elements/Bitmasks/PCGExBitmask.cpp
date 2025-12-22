// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Bitmasks/PCGExBitmask.h"

#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGExVersion.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "Containers/PCGExManagedObjects.h"
#include "PCGExCoreMacros.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Bitmask

#if WITH_EDITOR
void UPCGExBitmaskSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_UPDATE_TO_DATA_VERSION(1, 71, 2)
	{
		Bitmask.ApplyDeprecation();
	}

	Super::ApplyDeprecation(InOutNode);
}

FName UPCGExBitmaskSettings::GetDisplayName() const
{
	FString S = TEXT("");
	if (Bitmask.Compositions.IsEmpty())
	{
		S = FString::Printf(TEXT("%lld"), Bitmask.Get());
	}
	else
	{
		TArray<FString> Ss;
		Ss.Reserve(Bitmask.Compositions.Num());
		for (const FPCGExBitmaskRef& Ref : Bitmask.Compositions) { Ss.Add(Ref.Identifier.ToString()); }
		S = FString::Join(Ss, TEXT(", "));
	}

	if (S.Len() > TitleCharLimit)
	{
		const int32 TruncLen = TitleCharLimit - 3;
		S = S.Left(TruncLen) + TEXT("...");
	}

	return FName(S);
}
#endif

TArray<FPCGPinProperties> UPCGExBitmaskSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBitmaskSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Bitmask"), TEXT("Bitmask."), Required)
	return PinProperties;
}

FPCGElementPtr UPCGExBitmaskSettings::CreateElement() const { return MakeShared<FPCGExBitmaskElement>(); }


bool FPCGExBitmaskElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Bitmask)

	const int64 Bitmask = Settings->Bitmask.Get();

	UPCGParamData* BitmaskData = Context->ManagedObjects->New<UPCGParamData>();
	BitmaskData->Metadata->CreateAttribute<int64>(FName("Bitmask"), Bitmask, false, true);
	BitmaskData->Metadata->AddEntry();

	Context->StageOutput(BitmaskData, FName("Bitmask"), PCGExData::EStaging::Managed);

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
