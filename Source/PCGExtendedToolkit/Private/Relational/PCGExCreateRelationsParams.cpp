// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExCreateRelationsParams.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Data/PCGExRelationsParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalParamsBuilderElementBase"

UPCGExCreateRelationsParamsSettings::UPCGExCreateRelationsParamsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (Sockets.IsEmpty()) { UPCGExCreateRelationsParamsSettings::InitDefaultSockets(); }
}

void UPCGExCreateRelationsParamsSettings::InitDefaultSockets()
{
	Sockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", FColor(255, 0, 0)));
	Sockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", FColor(200, 0, 0)));
	Sockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", FColor(0, 255, 0)));
	Sockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", FColor(0, 200, 0)));
	Sockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", FColor(0, 0, 255)));
	Sockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", FColor(0, 0, 200)));
}

const TArray<FPCGExSocketDescriptor>& UPCGExCreateRelationsParamsSettings::GetSockets() const
{
	return Sockets;
}

FPCGElementPtr UPCGExCreateRelationsParamsSettings::CreateElement() const
{
	return MakeShared<FPCGExCreateRelationsParamsElement>();
}

TArray<FPCGPinProperties> UPCGExCreateRelationsParamsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> NoInput;
	return NoInput;
}

TArray<FPCGPinProperties> UPCGExCreateRelationsParamsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs Directional Sampling parameters to be used with other nodes.");
#endif // WITH_EDITOR

	return PinProperties;
}

template <typename T>
T* FPCGExCreateRelationsParamsElement::BuildParams(
	FPCGContext* Context) const
{
	const UPCGExCreateRelationsParamsSettings* Settings = Context->GetInputSettings<UPCGExCreateRelationsParamsSettings>();
	check(Settings);

	if (Settings->RelationIdentifier.IsNone() || !PCGEx::Common::IsValidName(Settings->RelationIdentifier.ToString()))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnamedOutput", "Output name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return nullptr;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	T* OutParams = NewObject<T>();

	OutParams->RelationIdentifier = Settings->RelationIdentifier;
	OutParams->Initialize(
		const_cast<TArray<FPCGExSocketDescriptor>&>(Settings->GetSockets()),
		Settings->bApplyGlobalOverrides,
		const_cast<FPCGExSocketGlobalOverrides&>(Settings->GlobalOverrides));

	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutParams;
	Output.bPinlessData = true;

	return OutParams;
}

bool FPCGExCreateRelationsParamsElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateRelationsParamsElement::Execute);
	BuildParams<UPCGExRelationsParamsData>(Context);
	return true;
}

#undef LOCTEXT_NAMESPACE
