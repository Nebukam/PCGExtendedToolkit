// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExRelationsParamsBuilder.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Data/PCGExRelationsParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalParamsBuilderElementBase"

#if WITH_EDITOR
UPCGExRelationsParamsBuilderSettings::UPCGExRelationsParamsBuilderSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (Sockets.IsEmpty()) { UPCGExRelationsParamsBuilderSettings::InitDefaultSockets(); }
}

void UPCGExRelationsParamsBuilderSettings::InitDefaultSockets()
{
	Sockets.Add(FPCGExSocketDescriptor{"Forward", FPCGExSocketDirection{FVector::ForwardVector}, true, false, FPCGExSocketModifierDescriptor{}, true, FColor(255, 0, 0)});
	Sockets.Add(FPCGExSocketDescriptor{"Backward", FPCGExSocketDirection{FVector::BackwardVector}, true, false, FPCGExSocketModifierDescriptor{}, true, FColor(200, 0, 0)});
	Sockets.Add(FPCGExSocketDescriptor{"Right", FPCGExSocketDirection{FVector::RightVector}, true, false, FPCGExSocketModifierDescriptor{}, true, FColor(0, 255, 0)});
	Sockets.Add(FPCGExSocketDescriptor{"Left", FPCGExSocketDirection{FVector::LeftVector}, true, false, FPCGExSocketModifierDescriptor{}, true, FColor(0, 200, 0)});
	Sockets.Add(FPCGExSocketDescriptor{"Up", FPCGExSocketDirection{FVector::UpVector}, true, false, FPCGExSocketModifierDescriptor{}, true, FColor(0, 0, 255)});
	Sockets.Add(FPCGExSocketDescriptor{"Down", FPCGExSocketDirection{FVector::DownVector}, true, false, FPCGExSocketModifierDescriptor{}, true, FColor(0, 0, 200)});
}

FText UPCGExRelationsParamsBuilderSettings::GetNodeTooltipText() const
{
	return LOCTEXT("DataFromActorTooltip", "Builds a collection of PCG-compatible data from the selected actors.");
}

#endif

FPCGElementPtr UPCGExRelationsParamsBuilderSettings::CreateElement() const
{
	return MakeShared<FPCGExRelationsParamsBuilderElement>();
}

TArray<FPCGPinProperties> UPCGExRelationsParamsBuilderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> NoInput;
	return NoInput;
}

TArray<FPCGPinProperties> UPCGExRelationsParamsBuilderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs Directional Sampling parameters to be used with other nodes.");
#endif // WITH_EDITOR

	return PinProperties;
}

template <typename T>
T* FPCGExRelationsParamsBuilderElement::BuildParams(FPCGContext* Context) const
{
	const UPCGExRelationsParamsBuilderSettings* Settings = Context->GetInputSettings<UPCGExRelationsParamsBuilderSettings>();
	check(Settings);

	if (Settings->RelationIdentifier.IsNone())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnamedOutput", "Output name is invalid."));
		return nullptr;
	}
	
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	T* OutParams = NewObject<T>();

	OutParams->RelationIdentifier = Settings->RelationIdentifier;
	OutParams->bMarkMutualRelations = Settings->bMarkMutualRelations;
	OutParams->InitializeSockets(const_cast<TArray<FPCGExSocketDescriptor>&>(Settings->Sockets));

	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutParams;
	Output.bPinlessData = true;

	return OutParams;
}

bool FPCGExRelationsParamsBuilderElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelationsParamsBuilderElement::Execute);
	BuildParams<UPCGExRelationsParamsData>(Context);
	return true;
}

#undef LOCTEXT_NAMESPACE
