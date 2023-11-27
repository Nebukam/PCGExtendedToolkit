// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateGraphParams.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExCreateGraphParams"

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

UPCGExCreateGraphParamsSettings::UPCGExCreateGraphParamsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (CustomSockets.IsEmpty()) { UPCGExCreateGraphParamsSettings::InitDefaultSockets(); }
	InitSocketContent(PresetSockets);
	RefreshSocketNames();
}

void UPCGExCreateGraphParamsSettings::InitDefaultSockets()
{
	const EPCGExSocketType Input = bTypedPreset ? EPCGExSocketType::Input : EPCGExSocketType::None;
	const EPCGExSocketType Output = bTypedPreset ? EPCGExSocketType::Output : EPCGExSocketType::None;

	CustomSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", Output, PCGExDebugColors::XPlus));
	CustomSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", Input, PCGExDebugColors::XMinus));
	CustomSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", Output, PCGExDebugColors::YPlus));
	CustomSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", Input, PCGExDebugColors::YMinus));
	CustomSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", Output, PCGExDebugColors::ZPlus));
	CustomSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", Input, PCGExDebugColors::ZMinus));
}

void UPCGExCreateGraphParamsSettings::RefreshSocketNames()
{
	GeneratedSocketNames.Empty();
	TArray<FPCGExSocketDescriptor>& RefSockets = GraphModel == EPCGExGraphModel::Custom ? CustomSockets : PresetSockets;
	for (FPCGExSocketDescriptor& Socket : RefSockets)
	{
		FPCGExSocketQualityOfLifeInfos& Infos = GeneratedSocketNames.Emplace_GetRef();
		Infos.Populate(GraphIdentifier, Socket);
	}
}

void UPCGExCreateGraphParamsSettings::InitSocketContent(TArray<FPCGExSocketDescriptor>& OutSockets) const
{
	OutSockets.Empty();

	const FRotator ToTheLeft = FRotator(0, 45, 0);
	const FRotator ToTheRight = FRotator(0, -45, 0);

	const EPCGExSocketType Input = bTypedPreset ? EPCGExSocketType::Input : EPCGExSocketType::None;
	const EPCGExSocketType Output = bTypedPreset ? EPCGExSocketType::Output : EPCGExSocketType::None;

	switch (GraphModel)
	{
	case EPCGExGraphModel::Custom:
		OutSockets.Append(CustomSockets);
		break;
	case EPCGExGraphModel::Grid:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", Output, PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", Input, PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", Output, PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", Input, PCGExDebugColors::YMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", Output, PCGExDebugColors::ZPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", Input, PCGExDebugColors::ZMinus));
		break;
	case EPCGExGraphModel::PlaneXY:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", Output, PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", Input, PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", Output, PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", Input, PCGExDebugColors::YMinus));
		break;
	case EPCGExGraphModel::PlaneXZ:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", Output, PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", Input, PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", Output, PCGExDebugColors::ZPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", Input, PCGExDebugColors::ZMinus));
		break;
	case EPCGExGraphModel::PlaneYZ:
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", Output, PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", Input, PCGExDebugColors::YMinus));
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", Output, PCGExDebugColors::ZPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", Input, PCGExDebugColors::ZMinus));
		break;
	case EPCGExGraphModel::TwoSidedX:
		OutSockets.Add(FPCGExSocketDescriptor("Forward", FVector::ForwardVector, "Backward", Output, PCGExDebugColors::XPlus, 90));
		OutSockets.Add(FPCGExSocketDescriptor("Backward", FVector::BackwardVector, "Forward", Input, PCGExDebugColors::XMinus, 90));
		break;
	case EPCGExGraphModel::TwoSidedY:
		OutSockets.Add(FPCGExSocketDescriptor("Right", FVector::RightVector, "Left", Output, PCGExDebugColors::YPlus, 90));
		OutSockets.Add(FPCGExSocketDescriptor("Left", FVector::LeftVector, "Right", Input, PCGExDebugColors::YMinus, 90));
		break;
	case EPCGExGraphModel::TwoSidedZ:
		OutSockets.Add(FPCGExSocketDescriptor("Up", FVector::UpVector, "Down", Output, PCGExDebugColors::ZPlus, 90));
		OutSockets.Add(FPCGExSocketDescriptor("Down", FVector::DownVector, "Up", Input, PCGExDebugColors::ZMinus, 90));
		break;
	case EPCGExGraphModel::VFork:
		OutSockets.Add(FPCGExSocketDescriptor("Lefty", ToTheLeft.RotateVector(FVector::ForwardVector), EPCGExSocketType::Any, PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("Righty", ToTheRight.RotateVector(FVector::ForwardVector), EPCGExSocketType::Any, PCGExDebugColors::XMinus));
		break;
	case EPCGExGraphModel::XFork:
		OutSockets.Add(FPCGExSocketDescriptor("InLefty", ToTheLeft.RotateVector(FVector::ForwardVector), Output, PCGExDebugColors::XPlus));
		OutSockets.Add(FPCGExSocketDescriptor("InRighty", ToTheRight.RotateVector(FVector::ForwardVector), Output, PCGExDebugColors::YPlus));
		OutSockets.Add(FPCGExSocketDescriptor("OutLefty", ToTheLeft.RotateVector(FVector::BackwardVector), Input, PCGExDebugColors::XMinus));
		OutSockets.Add(FPCGExSocketDescriptor("OutRighty", ToTheRight.RotateVector(FVector::BackwardVector), Input, PCGExDebugColors::YMinus));
		break;
	default: ;
	}
}

const TArray<FPCGExSocketDescriptor>& UPCGExCreateGraphParamsSettings::GetSockets() const
{
	return GraphModel == EPCGExGraphModel::Custom ? CustomSockets : PresetSockets;
}

FPCGElementPtr UPCGExCreateGraphParamsSettings::CreateElement() const { return MakeShared<FPCGExCreateGraphParamsElement>(); }

TArray<FPCGPinProperties> UPCGExCreateGraphParamsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> NoInput;
	return NoInput;
}

TArray<FPCGPinProperties> UPCGExCreateGraphParamsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExGraph::SourceParamsLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "Outputs Directional Sampling parameters to be used with other nodes.");
#endif // WITH_EDITOR

	return PinProperties;
}

#if WITH_EDITOR 
void UPCGExCreateGraphParamsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		const FName& PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExCreateGraphParamsSettings, GraphModel))
		{
			InitSocketContent(PresetSockets);
		}
	}

	RefreshSocketNames();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

template <typename T>
T* FPCGExCreateGraphParamsElement::BuildParams(
	FPCGContext* Context) const
{
	const UPCGExCreateGraphParamsSettings* Settings = Context->GetInputSettings<UPCGExCreateGraphParamsSettings>();
	check(Settings);

	if (Settings->GraphIdentifier.IsNone() || !PCGEx::IsValidName(Settings->GraphIdentifier.ToString()))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnamedOutput", "Output name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return nullptr;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	T* OutParams = NewObject<T>();

	OutParams->GraphIdentifier = Settings->GraphIdentifier;
	OutParams->Initialize(
		const_cast<TArray<FPCGExSocketDescriptor>&>(Settings->GetSockets()),
		Settings->bApplyGlobalOverrides,
		const_cast<FPCGExSocketGlobalOverrides&>(Settings->GlobalOverrides));

	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutParams;
	Output.bPinlessData = true;

	return OutParams;
}

bool FPCGExCreateGraphParamsElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateGraphParamsElement::Execute);
	BuildParams<UPCGExGraphParamsData>(Context);
	return true;
}

#undef LOCTEXT_NAMESPACE
