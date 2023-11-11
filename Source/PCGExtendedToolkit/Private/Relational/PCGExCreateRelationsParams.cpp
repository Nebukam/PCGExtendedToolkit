// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExCreateRelationsParams.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Data/PCGExRelationsParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalParamsBuilderElementBase"

#if WITH_EDITOR
UPCGExCreateRelationsParamsSettings::UPCGExCreateRelationsParamsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (Sockets.IsEmpty()) { UPCGExCreateRelationsParamsSettings::InitDefaultSockets(); }
}

void UPCGExCreateRelationsParamsSettings::InitDefaultSockets()
{
	FPCGExSocketDescriptor& Socket = Sockets.Emplace_GetRef("Forward", FPCGExSocketDirection{FVector::ForwardVector});
	Socket.DebugColor = FColor(255, 0, 0);

	Socket = Sockets.Emplace_GetRef("Backward", FPCGExSocketDirection{FVector::BackwardVector});
	Socket.DebugColor = FColor(200, 0, 0);

	Socket = Sockets.Emplace_GetRef("Right", FPCGExSocketDirection{FVector::RightVector});
	Socket.DebugColor = FColor(0, 255, 0);

	Socket = Sockets.Emplace_GetRef("Left", FPCGExSocketDirection{FVector::LeftVector});
	Socket.DebugColor = FColor(0, 200, 0);

	Socket = Sockets.Emplace_GetRef("Up", FPCGExSocketDirection{FVector::UpVector});
	Socket.DebugColor = FColor(0, 0, 255);

	Socket = Sockets.Emplace_GetRef("Down", FPCGExSocketDirection{FVector::DownVector});
	Socket.DebugColor = FColor(0, 0, 200);
}

FText UPCGExCreateRelationsParamsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("DataFromActorTooltip", "Builds a collection of PCG-compatible data from the selected actors.");
}

#endif

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
	OutParams->InitializeSockets(
		const_cast<TArray<FPCGExSocketDescriptor>&>(Settings->Sockets),
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
