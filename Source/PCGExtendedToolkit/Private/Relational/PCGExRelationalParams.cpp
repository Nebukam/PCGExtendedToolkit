// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExRelationalParams.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "Elements/PCGMergeElement.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExRelationalParams)

#define LOCTEXT_NAMESPACE "PCGExRelationalParamsElement"

#if WITH_EDITOR
FText UPCGExRelationalParams::GetNodeTooltipText() const
{
	return LOCTEXT("DataFromActorTooltip", "Builds a collection of PCG-compatible data from the selected actors.");
}

#endif

FPCGElementPtr UPCGExRelationalParams::CreateElement() const
{
	return MakeShared<FPCGExRelationalParamsElement>();
}

TArray<FPCGPinProperties> UPCGExRelationalParams::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs Directional Sampling parameters to be used with other nodes.");
#endif // WITH_EDITOR

	return PinProperties;
}

bool FPCGExRelationalParamsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelationalParamsElement::Execute);

	const UPCGExRelationalParams* Settings = Context->GetInputSettings<UPCGExRelationalParams>();
	check(Settings);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGExRelationalData* OutRelationalData = NewObject<UPCGExRelationalData>();
	OutRelationalData->InitializeLocalDefinition(Settings->Slots);
	
	FPCGDataCollection Collection;
	FPCGTaggedData& TaggedData = Collection.TaggedData.Emplace_GetRef();
	TaggedData.Data = OutRelationalData;
	
	Outputs.Add_GetRef(TaggedData);
	
	return true;
}

#undef LOCTEXT_NAMESPACE
