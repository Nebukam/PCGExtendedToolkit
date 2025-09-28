// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExTuple.h"

#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Tuple

FPCGMetadataAttributeBase* FPCGExTupleValues::Write(FName Name, UPCGParamData* ParamData) const
{
	// Create attribute
	return nullptr;
}

void FPCGExTupleValues::WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key, int32 Index) const
{
	if (Values.IsValidIndex(Index))
	{
	}
	else
	{
		Attribute->SetValue()
	}
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

	TArray<FPCGMetadataAttributeBase*> Attributes;
	TArray<FName> Ids;
	Attributes.Reserve(Settings->Composition.Num());
	Ids.Reserve(Settings->Composition.Num());
	int32 MaxIterations = 0;

	for (const TPair<FName, FPCGExTupleValues>& Pair : Settings->Composition)
	{
		MaxIterations = FMath::Max(MaxIterations, Pair.Value.Values.Num());
		Ids.Add(Pair.Key);
		Attributes.Add(Pair.Value.Write(Pair.Key, TupleData));
	}

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		int64 Key = TupleData->Metadata->AddEntry();
		for (int i = 0; i < Ids.Num(); ++i) { Settings->Composition[Ids[i]].WriteValue(Attributes[i], Key, Iteration); }
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
