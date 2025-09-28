// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExTuple.h"

#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Tuple

void FPCGExTupleValues::Write(FName Name, UPCGParamData* ParamData) const
{
	
}

#if WITH_EDITOR
void UPCGExTupleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	for (TPair<FName, FPCGExTupleValues>& Pair : Composition)
	{
		for (FPCGExTupleValue& Value : Pair.Value.Values) { Value.Type = Pair.Value.Type; }
	}

	(void)MarkPackageDirty();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExTupleSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExTupleSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Tuple"), TEXT("Tuple."), Required)
	return PinProperties;
}

FPCGElementPtr UPCGExTupleSettings::CreateElement() const { return MakeShared<FPCGExTupleElement>(); }

#if WITH_EDITOR
void UPCGExTupleSettings::EDITOR_Equalize()
{
	Modify(true);

	int32 MaxNum = 0;
	for (TPair<FName, FPCGExTupleValues>& Pair : Composition) { MaxNum = FMath::Max(MaxNum, Pair.Value.Values.Num()); }
	for (TPair<FName, FPCGExTupleValues>& Pair : Composition) { PCGEx::InitArray(Pair.Value.Values, MaxNum); }
	MarkPackageDirty();
}

void UPCGExTupleSettings::EDITOR_AddOneToAll()
{
	Modify(true);
	for (TPair<FName, FPCGExTupleValues>& Pair : Composition) { PCGEx::InitArray(Pair.Value.Values, Pair.Value.Values.Num() + 1); }
	MarkPackageDirty();
}
#endif

bool FPCGExTupleElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Tuple)

	UPCGParamData* TupleData = Context->ManagedObjects->New<UPCGParamData>();

	for (const TPair<FName, FPCGExTupleValues>& Pair : Settings->Composition)
	{
		Pair.Value.Write(Pair.Key, TupleData);
	}
	
	//TupleData->Metadata->CreateAttribute<int64>(FName("Tuple"), Tuple, false, true);
	//TupleData->Metadata->AddEntry();

	FPCGTaggedData& StagedData = Context->StageOutput(TupleData, true);
	StagedData.Pin = FName("Tuple");

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
