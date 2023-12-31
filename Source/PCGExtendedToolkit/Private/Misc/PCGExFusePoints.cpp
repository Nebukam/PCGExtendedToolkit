﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

PCGExData::EInit UPCGExFusePointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExFusePointsContext::~FPCGExFusePointsContext()
{
	PCGEX_TERMINATE_ASYNC

	FusedPoints.Empty();
	OutPoints = nullptr;
	PCGEX_DELETE(MetadataBlender)
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

	Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->BlendingSettings));

	Context->Radius = Settings->Radius * Settings->Radius;

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
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExFuse::State_FindingFusePoints); }
	}

	if (Context->IsState(PCGExFuse::State_FindingFusePoints))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			Context->FusedPoints.Reset(PointIO.GetNum() / 2);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FVector PtPosition = PointIO.GetInPoint(PointIndex).Transform.GetLocation();
			double Distance = 0;
			PCGExFuse::FFusedPoint* FuseTarget = nullptr;

			Context->PointsLock.ReadLock();

			if (Settings->bComponentWiseRadius)
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					if (abs(PtPosition.X - FusedPoint.Position.X) <= Settings->Radiuses.X &&
						abs(PtPosition.Y - FusedPoint.Position.Y) <= Settings->Radiuses.Y &&
						abs(PtPosition.Z - FusedPoint.Position.Z) <= Settings->Radiuses.Z)
					{
						Distance = FVector::DistSquared(FusedPoint.Position, PtPosition);
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
			else
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					Distance = FVector::DistSquared(FusedPoint.Position, PtPosition);
					if (Distance < Context->Radius)
					{
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}

			Context->PointsLock.ReadUnlock();

			if (!FuseTarget)
			{
				FWriteScopeLock WriteLock(Context->PointsLock);
				Context->FusedPoints.Emplace_GetRef(PointIndex, PtPosition).Add(PointIndex, 0);
			}
			else
			{
				FuseTarget->Add(PointIndex, Distance);
			}
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			if (Context->bPreserveOrder)
			{
				Context->FusedPoints.Sort(
					[&](const PCGExFuse::FFusedPoint& A, const PCGExFuse::FFusedPoint& B)
					{
						return A.Index > B.Index;
					});
			}
			Context->SetState(PCGExFuse::State_MergingPoints);
		}
	}

	if (Context->IsState(PCGExFuse::State_MergingPoints))
	{
		auto Initialize = [&]()
		{
			const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();
			TArray<FPCGPoint>& MutablePoints = Context->GetCurrentOut()->GetMutablePoints();
			MutablePoints.Reserve(Context->FusedPoints.Num());

			for (const PCGExFuse::FFusedPoint& FPoint : Context->FusedPoints) { MutablePoints.Add(InPoints[FPoint.Index]); }

			Context->MetadataBlender->PrepareForData(*Context->CurrentIO);
		};

		auto FusePoint = [&](const int32 ReadIndex)
		{
			PCGExFuse::FFusedPoint& FusedPoint = Context->FusedPoints[ReadIndex];

			if (FusedPoint.Fused.IsEmpty()) { return; }

			const int32 NumFused = FusedPoint.Fused.Num();
			const double AverageDivider = NumFused;

			const PCGEx::FPointRef Target = Context->CurrentIO->GetOutPointRef(ReadIndex);
			Context->MetadataBlender->PrepareForBlending(Target);

			for (int i = 0; i < NumFused; i++)
			{
				const double Dist = FusedPoint.Distances[i];
				const double Weight = Dist == 0 ? 1 : FusedPoint.MaxDistance == 0 ? 0 : 1 - (Dist / FusedPoint.MaxDistance);
				Context->MetadataBlender->Blend(Target, Context->CurrentIO->GetInPointRef(FusedPoint.Fused[i]), Target, Weight);
			}

			Context->MetadataBlender->CompleteBlending(Target, AverageDivider);
		};

		if (PCGExMT::ParallelForLoop(Context, Context->FusedPoints.Num(), Initialize, FusePoint, Context->ChunkSize))
		{
			Context->MetadataBlender->Write();
			Context->CurrentIO->OutputTo(Context);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
