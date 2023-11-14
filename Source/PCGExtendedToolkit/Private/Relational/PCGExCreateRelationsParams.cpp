// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExCreateRelationsParams.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Data/PCGExRelationsParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalParamsBuilderElementBase"

namespace PCGExDebugColors
{
	constexpr uint8 Plus = 255;
	constexpr uint8 Minus = 200;
	constexpr FColor XPlus = FColor(Plus, 0, 0);
	constexpr FColor XMinus = FColor(Minus, 0, 0);
	constexpr FColor YPlus = FColor(0, Plus, 0);
	constexpr FColor YMinus = FColor(0, Minus, 0);
	constexpr FColor ZPlus = FColor(0, 0, Plus);
	constexpr FColor ZMinus = FColor(0, 0, Minus);
}

UPCGExCreateRelationsParamsSettings::UPCGExCreateRelationsParamsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (CustomSockets.IsEmpty()) { UPCGExCreateRelationsParamsSettings::InitDefaultSockets(); }
	InitSocketContent(SocketsPreset);
}

void UPCGExCreateRelationsParamsSettings::InitDefaultSockets()
{
	CustomSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", PCGExDebugColors::XPlus));
	CustomSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", PCGExDebugColors::XMinus));
	CustomSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", PCGExDebugColors::YPlus));
	CustomSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", PCGExDebugColors::YMinus));
	CustomSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", PCGExDebugColors::ZPlus));
	CustomSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", PCGExDebugColors::ZMinus));
}

void UPCGExCreateRelationsParamsSettings::InitSocketContent(TArray<FPCGExSocketDescriptor>& OutSockets) const
{
	OutSockets.Empty();

	const FRotator ToTheLeft = FRotator(0,45,0);
	const FRotator ToTheRight = FRotator(0,-45,0);
	
	switch (RelationsModel) {
	case EPCGExRelationsModel::Custom:
		OutSockets.Append(CustomSockets);
		break;
	case EPCGExRelationsModel::Grid3D:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", PCGExDebugColors::YMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", PCGExDebugColors::ZPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", PCGExDebugColors::ZMinus));
		break;
	case EPCGExRelationsModel::GridXY:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", PCGExDebugColors::YMinus));
		break;
	case EPCGExRelationsModel::GridXZ:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", PCGExDebugColors::ZPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", PCGExDebugColors::ZMinus));
		break;
	case EPCGExRelationsModel::GridYZ:
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", PCGExDebugColors::YMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", PCGExDebugColors::ZPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", PCGExDebugColors::ZMinus));
		break;
	case EPCGExRelationsModel::FFork:
		OutSockets.Add(FPCGExSocketDescriptor("Lefty", ToTheLeft.RotateVector(FVector::ForwardVector), PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Righty", ToTheRight.RotateVector(FVector::ForwardVector), PCGExDebugColors::XMinus));
		break;
	default: ;
	}
}

const TArray<FPCGExSocketDescriptor>& UPCGExCreateRelationsParamsSettings::GetSockets() const
{
	return RelationsModel == EPCGExRelationsModel::Custom ? CustomSockets : SocketsPreset;
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

void UPCGExCreateRelationsParamsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		const FName& PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExCreateRelationsParamsSettings, RelationsModel))
		{
			InitSocketContent(SocketsPreset);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
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
