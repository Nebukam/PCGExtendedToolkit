// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExRelationsProcessor.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalSettings"

#pragma region UPCGSettings interface


TArray<FPCGPinProperties> UPCGExRelationsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExRelational::SourceParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPropertyParams.Tooltip = LOCTEXT("PCGExSourceParamsPinTooltip", "Relations Params. Data is de-duped internally.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRelationsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinParamsOutput = PinProperties.Emplace_GetRef(PCGExRelational::OutputParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinParamsOutput.Tooltip = LOCTEXT("PCGExOutputParamsTooltip", "Relations Params forwarding. Data is de-duped internally.");
#endif // WITH_EDITOR

	return PinProperties;
}

bool FPCGExRelationsProcessorContext::AdvanceParams(bool bResetPointsIndex)
{
	if (bResetPointsIndex) { CurrentPointsIndex = -1; }
	CurrentParamsIndex++;
	if (Params.Params.IsValidIndex(CurrentParamsIndex))
	{
		CurrentParams = Params.Params[CurrentParamsIndex];
		CurrentParams->GetSocketsInfos(SocketInfos);
		return true;
	}

	CurrentParams = nullptr;
	return false;
}

bool FPCGExRelationsProcessorContext::AdvancePointsIO(bool bResetParamsIndex)
{
	if (bResetParamsIndex) { CurrentParamsIndex = -1; }
	return FPCGExPointsProcessorContext::AdvancePointsIO();
}

void FPCGExRelationsProcessorContext::Reset()
{
	FPCGExPointsProcessorContext::Reset();
	CurrentParamsIndex = -1;
}

void FPCGExRelationsProcessorContext::ComputeRelationsType(const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
{
	for (PCGExRelational::FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		EPCGExRelationType Type = EPCGExRelationType::Unknown;
		const int64 RelationIndex = CurrentSocketInfos.Socket->GetRelationIndex(Point.MetadataEntry);

		if (RelationIndex != -1)
		{
			const int32 Key = IO->Out->GetPoint(RelationIndex).MetadataEntry;
			for (PCGExRelational::FSocketInfos& OtherSocketInfos : SocketInfos)
			{
				if (OtherSocketInfos.Socket->GetRelationIndex(Key) == ReadIndex)
				{
					//TODO: Handle cases where there can be multiple sockets with a valid connection
					Type = PCGExRelational::Helpers::GetRelationType(CurrentSocketInfos, OtherSocketInfos);
				}
			}

			if (Type == EPCGExRelationType::Unknown) { Type = EPCGExRelationType::Unique; }
		}


		CurrentSocketInfos.Socket->SetRelationType(Point.MetadataEntry, Type);
	}
}

double FPCGExRelationsProcessorContext::PrepareSamplersForPoint(const FPCGPoint& Point, TArray<PCGExRelational::FSocketSampler>& OutSamplers)
{
	OutSamplers.Reset(SocketInfos.Num());
	double MaxDistance = 0.0;
	for (PCGExRelational::FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		PCGExRelational::FSocketSampler& NewSampler = OutSamplers.Emplace_GetRef();
		NewSampler.SocketInfos = &CurrentSocketInfos;
		PrepareSamplerForPointSocketPair(Point, NewSampler, CurrentSocketInfos);
		MaxDistance = FMath::Max(MaxDistance, NewSampler.MaxDistance);
	}
	return MaxDistance;
}

void FPCGExRelationsProcessorContext::PrepareSamplerForPointSocketPair(
	const FPCGPoint& Point,
	PCGExRelational::FSocketSampler& Sampler,
	PCGExRelational::FSocketInfos InSocketInfos)
{
	const FPCGExSocketDirection BaseDirection = InSocketInfos.Socket->Descriptor.Direction;

	FVector Direction = BaseDirection.Direction;
	double DotTolerance = BaseDirection.DotTolerance;
	double MaxDistance = BaseDirection.MaxDistance;

	const FTransform PtTransform = Point.Transform;
	Sampler.Origin = PtTransform.GetLocation();

	if (InSocketInfos.Socket->Descriptor.bRelativeOrientation)
	{
		Direction = PtTransform.Rotator().RotateVector(Direction);
		Direction.Normalize();
	}

	if (InSocketInfos.Modifier &&
		InSocketInfos.Modifier->bEnabled &&
		InSocketInfos.Modifier->bValid)
	{
		MaxDistance *= InSocketInfos.Modifier->GetValue(Point);
	}

	if (InSocketInfos.LocalDirection &&
		InSocketInfos.LocalDirection->bEnabled &&
		InSocketInfos.LocalDirection->bValid)
	{
		// TODO: Apply LocalDirection
	}

	Sampler.Direction = Direction;
	Sampler.DotTolerance = DotTolerance;
	Sampler.MaxDistance = MaxDistance;
}

FPCGContext* FPCGExRelationsProcessorElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExRelationsProcessorContext* Context = new FPCGExRelationsProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExRelationsProcessorElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExRelationsProcessorContext* Context = static_cast<FPCGExRelationsProcessorContext*>(InContext);

	if (Context->Params.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
		return false;
	}

	return true;
}

void FPCGExRelationsProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	FPCGExRelationsProcessorContext* Context = static_cast<FPCGExRelationsProcessorContext*>(InContext);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceParamsLabel);
	Context->Params.Initialize(InContext, Sources);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
