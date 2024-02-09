// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExFusePointsContext::~FPCGExFusePointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExFusePointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	PCGEX_FWD(FuseSettings)
	Context->FuseSettings.FuseSettings.Init();

	PCGEX_SOFT_VALIDATE_NAME(Context->FuseSettings.bWriteCompounded, Context->FuseSettings.CompoundedAttributeName, Context)
	PCGEX_SOFT_VALIDATE_NAME(Context->FuseSettings.bWriteCompoundSize, Context->FuseSettings.CompoundSizeAttributeName, Context)

	PCGEX_FWD(bPreserveOrder)

	return true;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 IOIndex = 0;
		while (Context->AdvancePointsIO()) { Context->GetAsyncManager()->Start<FPCGExFuseTask>(IOIndex++, Context->CurrentIO); }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExFuseTask::ExecuteTask()
{
	const FPCGExFusePointsContext* Context = Manager->GetContext<FPCGExFusePointsContext>();
	PCGEX_SETTINGS(FusePoints)

	PointIO->CreateInKeys();

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	TArray<PCGExFuse::FFusedPoint> FusedPoints;
	FusedPoints.Reserve(InPoints.Num());

	TArray<int32> InSorted;
	InSorted.SetNum(InPoints.Num());
	int32 Index = 0;
	for (int32& i : InSorted) { i = Index++; }

	InSorted.Sort(
		[&](const int32 A, const int32 B)
		{
			const FVector V = InPoints[A].Transform.GetLocation() - InPoints[B].Transform.GetLocation();
			return FMath::IsNearlyZero(V.X) ? FMath::IsNearlyZero(V.Y) ? V.Z > 0 : V.Y > 0 : V.X > 0;
		});

	const FPCGExFuseSettings& FSettings = Context->FuseSettings.FuseSettings;
	for (const int32 PointIndex : InSorted)
	{
		const FVector PtPosition = InPoints[PointIndex].Transform.GetLocation();
		double DistSquared = 0;
		PCGExFuse::FFusedPoint* FuseTarget = nullptr;

		if (FSettings.bComponentWiseTolerance)
		{
			for (PCGExFuse::FFusedPoint& FusedPoint : FusedPoints)
			{
				if (FVector SourceCenter = FSettings.GetSourceCenter(InPoints[PointIndex], PtPosition, FusedPoint.Position);
					FSettings.IsWithinToleranceComponentWise(SourceCenter, FusedPoint.Position))
				{
					DistSquared = FVector::DistSquared(FusedPoint.Position, SourceCenter);
					FuseTarget = &FusedPoint;
					break;
				}
			}
		}
		else
		{
			for (PCGExFuse::FFusedPoint& FusedPoint : FusedPoints)
			{
				DistSquared = FSettings.GetSourceDistSquared(InPoints[PointIndex], PtPosition, FusedPoint.Position);
				if (FSettings.IsWithinTolerance(DistSquared))
				{
					FuseTarget = &FusedPoint;
					break;
				}
			}
		}

		if (!FuseTarget) { FusedPoints.Emplace_GetRef(PointIndex, PtPosition).Add(PointIndex, 0); }
		else { FuseTarget->Add(PointIndex, DistSquared); }
	}

	if (Context->bPreserveOrder)
	{
		FusedPoints.Sort([&](const PCGExFuse::FFusedPoint& A, const PCGExFuse::FFusedPoint& B) { return A.Index > B.Index; });
	}

	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
	MutablePoints.Reserve(FusedPoints.Num());

	for (const PCGExFuse::FFusedPoint& FPoint : FusedPoints) { MutablePoints.Add(InPoints[FPoint.Index]); }

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));
	MetadataBlender->PrepareForData(*PointIO);

	for (int PointIndex = 0; PointIndex < MutablePoints.Num(); PointIndex++)
	{
		PCGExFuse::FFusedPoint& FusedPoint = FusedPoints[PointIndex];

		if (FusedPoint.Fused.IsEmpty()) { continue; }

		const int32 NumFused = FusedPoint.Fused.Num();
		const double AverageDivider = NumFused;

		const PCGEx::FPointRef Target = PointIO->GetOutPointRef(PointIndex);
		MetadataBlender->PrepareForBlending(Target);

		for (int i = 0; i < NumFused; i++)
		{
			const double Dist = FusedPoint.Distances[i];
			const double Weight = Dist == 0 ? 1 : FusedPoint.MaxDistance == 0 ? 0 : 1 - (Dist / FusedPoint.MaxDistance);
			MetadataBlender->Blend(Target, PointIO->GetInPointRef(FusedPoint.Fused[i]), Target, Weight);
		}

		MetadataBlender->CompleteBlending(Target, AverageDivider);
	}

	MetadataBlender->Write();
	PCGEX_DELETE(MetadataBlender);

	// Write fuse meta after, so we don't blend it
	if (Context->FuseSettings.bWriteCompounded ||
		Context->FuseSettings.bWriteCompoundSize)
	{
		
		PCGEx::TFAttributeWriter<bool>* CompoundedWriter = Context->FuseSettings.bWriteCompounded ? new PCGEx::TFAttributeWriter<bool>(Context->FuseSettings.CompoundedAttributeName, false, false) : nullptr;
		PCGEx::TFAttributeWriter<int32>* CompoundSizeWriter = Context->FuseSettings.bWriteCompoundSize ? new PCGEx::TFAttributeWriter<int32>(Context->FuseSettings.CompoundSizeAttributeName, 0, false) : nullptr;

		if (CompoundedWriter) { CompoundedWriter->BindAndGet(*PointIO); }
		if (CompoundSizeWriter) { CompoundSizeWriter->BindAndGet(*PointIO); }

		for (int PointIndex = 0; PointIndex < MutablePoints.Num(); PointIndex++)
		{
			PCGExFuse::FFusedPoint& FusedPoint = FusedPoints[PointIndex];
			const int32 NumFused = FusedPoint.Fused.Num();

			if (CompoundedWriter) { CompoundedWriter->Values[PointIndex] = NumFused > 1; }
			if (CompoundSizeWriter) { CompoundSizeWriter->Values[PointIndex] = NumFused; }
		}

		if (CompoundedWriter) { CompoundedWriter->Write(); }
		if (CompoundSizeWriter) { CompoundSizeWriter->Write(); }

		PCGEX_DELETE(CompoundedWriter);
		PCGEX_DELETE(CompoundSizeWriter);
	}

	FusedPoints.Empty();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
