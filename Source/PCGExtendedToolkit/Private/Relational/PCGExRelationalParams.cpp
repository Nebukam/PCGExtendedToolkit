// Fill out your copyright notice in the Description page of Project Settings.

#include "..\..\Public\Relational\PCGExRelationalParams.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalParamsElement"

#if WITH_EDITOR
FText UPCGExRelationalParamsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("DataFromActorTooltip", "Builds a collection of PCG-compatible data from the selected actors.");
}

#endif

FPCGElementPtr UPCGExRelationalParamsSettings::CreateElement() const
{
	return MakeShared<FPCGExRelationalParamsElement>();
}

TArray<FPCGPinProperties> UPCGExRelationalParamsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> NoInput;
	return NoInput;
}

TArray<FPCGPinProperties> UPCGExRelationalParamsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs Directional Sampling parameters to be used with other nodes.");
#endif // WITH_EDITOR

	return PinProperties;
}

bool FPCGExRelationalParamsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelationalParamsElement::Execute);

	const UPCGExRelationalParamsSettings* Settings = Context->GetInputSettings<UPCGExRelationalParamsSettings>();
	check(Settings);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGExRelationalParamsData* OutRelationalParamsData = NewObject<UPCGExRelationalParamsData>();

	OutRelationalParamsData->RelationalIdentifier = Settings->RelationalIdentifier;
	OutRelationalParamsData->bMarkMutualRelations = Settings->bMarkMutualRelations;
	OutRelationalParamsData->Initialize(Settings->RelationalIdentifier, Settings->Slots);
		
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutRelationalParamsData;
	Output.bPinlessData = true;
	
	return true;
}

#undef LOCTEXT_NAMESPACE
