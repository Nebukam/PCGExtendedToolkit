// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"
#include "Misc/PCGExPointsToBounds.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

PCGExData::EInit UPCGExPointsToBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExPointsToBoundsContext::~FPCGExPointsToBoundsContext()
{
	PCGEX_TERMINATE_ASYNC

	OutPoints = nullptr;
	PCGEX_DELETE(MetadataBlender)
	PCGEX_DELETE_TARRAY(IOBounds)
}

UPCGExPointsToBoundsSettings::UPCGExPointsToBoundsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExPointsToBoundsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(PointsToBounds)

bool FPCGExPointsToBoundsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)
	PCGEX_FWD(bWritePointsCount)

	PCGEX_SOFT_VALIDATE_NAME(Context->bWritePointsCount, Settings->PointsCountAttributeName, Context)

	Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));

	return true;
}

bool FPCGExPointsToBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsToBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExPointsToBounds::State_ComputeBounds);
	}

	if (Context->IsState(PCGExPointsToBounds::State_ComputeBounds))
	{
		PCGExPointsToBounds::ComputeBounds(
			Context->GetAsyncManager(), Context->MainPoints,
			Context->IOBounds, Settings->BoundsSource, &Settings->LocalBoundsSource);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();
		UPCGPointData* OutData = Context->GetCurrentOut();
		TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
		MutablePoints.Add(InPoints[0]);

		Context->MetadataBlender->PrepareForData(*Context->CurrentIO);
		const double AverageDivider = InPoints.Num();

		const PCGExPointsToBounds::FBounds* Bounds = Context->IOBounds[Context->CurrentPointIOIndex];

		const FBox& Box = Bounds->Bounds;
		const FVector Center = Box.GetCenter();
		const double SqrDist = Box.GetExtent().SquaredLength();

		MutablePoints[0].Transform.SetLocation(Center);
		MutablePoints[0].BoundsMin = Box.Min - Center;
		MutablePoints[0].BoundsMax = Box.Max - Center;

		const PCGEx::FPointRef Target = Context->CurrentIO->GetOutPointRef(0);
		Context->MetadataBlender->PrepareForBlending(Target);

		for (int i = 0; i < AverageDivider; i++)
		{
			FVector Location = InPoints[i].Transform.GetLocation();
			const double Weight = FVector::DistSquared(Center, Location) / SqrDist;
			Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(i), Target, Weight);
		}

		Context->MetadataBlender->CompleteBlending(Target, AverageDivider);

		MutablePoints[0].Transform.SetLocation(Center);
		MutablePoints[0].BoundsMin = Box.Min - Center;
		MutablePoints[0].BoundsMax = Box.Max - Center;

		if (Settings->bWritePointsCount) { PCGExData::WriteMark(OutData->Metadata, Settings->PointsCountAttributeName, AverageDivider); }

		Context->MetadataBlender->Write();

		Context->CurrentIO->Flatten();
		Context->CurrentIO->OutputTo(Context);

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

bool FPCGExComputeIOBounds::ExecuteTask()
{
	const TArray<FPCGPoint>& InPoints = Bounds->PointIO->GetIn()->GetPoints();

	if (BoundsSource == EPCGExPointBoundsSource::LocalExtents && LocalInputDescriptor)
	{
		PCGEx::FLocalVectorGetter* LocalExtents = new PCGEx::FLocalVectorGetter();
		LocalExtents->Capture(*LocalInputDescriptor);
		LocalExtents->Grab(*PointIO);
		for (int i = 0; i < InPoints.Num(); i++)
		{
			const FPCGPoint& Pt = InPoints[i];
			Bounds->Bounds += FBoxCenterAndExtent(Pt.Transform.GetLocation(), LocalExtents->SafeGet(i, FVector::OneVector)).GetBox();
		}

		PCGEX_DELETE(LocalExtents)
	}
	else if (BoundsSource == EPCGExPointBoundsSource::LocalRadius && LocalInputDescriptor)
	{
		PCGEx::FLocalSingleFieldGetter* LocalRadius = new PCGEx::FLocalSingleFieldGetter();
		LocalRadius->Capture(*LocalInputDescriptor);
		LocalRadius->Grab(*PointIO);
		for (int i = 0; i < InPoints.Num(); i++)
		{
			const FPCGPoint& Pt = InPoints[i];
			Bounds->Bounds += FBoxCenterAndExtent(Pt.Transform.GetLocation(), FVector(LocalRadius->SafeGet(i, 1))).GetBox();
		}

		PCGEX_DELETE(LocalRadius)
	}
	else
	{
		switch (BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBoundsSphere:
			for (const FPCGPoint& Pt : InPoints) { Bounds->Bounds += Pt.GetDensityBounds().GetBox(); }
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			for (const FPCGPoint& Pt : InPoints) { Bounds->Bounds += Pt.GetDensityBounds().GetBox(); }
			break;
		case EPCGExPointBoundsSource::ScaledExtents:
			for (const FPCGPoint& Pt : InPoints) { Bounds->Bounds += Pt.GetDensityBounds().GetBox(); }
			break;
		case EPCGExPointBoundsSource::Extents:
			for (const FPCGPoint& Pt : InPoints) { Bounds->Bounds += Pt.GetDensityBounds().GetBox(); }
			break;
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
