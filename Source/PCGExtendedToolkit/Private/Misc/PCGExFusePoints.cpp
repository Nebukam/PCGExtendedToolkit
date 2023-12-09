// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"

PCGExData::EInit UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGExData::EInit::NewOutput; }

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

FPCGElementPtr UPCGExFusePointsSettings::CreateElement() const { return MakeShared<FPCGExFusePointsElement>(); }

FPCGContext* FPCGExFusePointsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFusePointsContext* Context = new FPCGExFusePointsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExFusePointsSettings* Settings = Context->GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	Context->AttributesBlendingOverrides = Settings->BlendingSettings.AttributesOverrides;
	Context->MetadataBlender = MakeUnique<PCGExDataBlending::FMetadataBlender>(Settings->BlendingSettings.DefaultBlending);
	Context->PropertyBlender = MakeUnique<PCGExDataBlending::FPropertiesBlender>(Settings->BlendingSettings);

	return Context;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	FPCGExFusePointsContext* Context = static_cast<FPCGExFusePointsContext*>(InContext);
	const UPCGExFusePointsSettings* Settings = Context->GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->CurrentIndex = 0;
			Context->SetState(PCGExFuse::State_FindingRootPoints);
		}
	}

	if (Context->IsState(PCGExFuse::State_FindingRootPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->FusedPoints.Reset(PointIO.GetNum() / 2);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetInPoint(PointIndex);
			const FVector PtPosition = Point.Transform.GetLocation();
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
					if (Distance < (Settings->Radius * Settings->Radius))
					{
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
			Context->PointsLock.ReadUnlock();

			FWriteScopeLock WriteLock(Context->PointsLock);
			if (!FuseTarget)
			{
				FuseTarget = &Context->FusedPoints.Emplace_GetRef();
				FuseTarget->MainIndex = PointIndex;
				FuseTarget->Position = PtPosition;
			}

			FuseTarget->Add(PointIndex, Distance);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExFuse::State_MergingPoints);
		}
	}

	if (Context->IsState(PCGExFuse::State_MergingPoints))
	{
		auto Initialize = [&]()
		{
			Context->GetCurrentOut()->GetMutablePoints().SetNumUninitialized(Context->FusedPoints.Num());
			Context->MetadataBlender->PrepareForData(Context->GetCurrentOut(), Context->GetCurrentIn(), Context->AttributesBlendingOverrides);
		};

		auto FusePoint = [&](int32 ReadIndex)
		{
			PCGExFuse::FFusedPoint& FusedPointData = Context->FusedPoints[ReadIndex];
			const int32 NumFused = FusedPointData.Fused.Num();
			const double AverageDivider = NumFused;

			FPCGPoint& ConsolidatedPoint = Context->GetCurrentOut()->GetMutablePoints()[ReadIndex];

			Context->MetadataBlender->PrepareForBlending(ReadIndex);

			PCGExDataBlending::FPropertiesBlender PropertiesBlender = PCGExDataBlending::FPropertiesBlender(*Context->PropertyBlender);

			if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.PrepareBlending(ConsolidatedPoint, Context->CurrentIO->GetInPoint(FusedPointData.MainIndex)); }

			for (int i = 0; i < NumFused; i++)
			{
				const int32 FusedPointIndex = FusedPointData.Fused[i];
				const double Weight = 1 - (FusedPointData.Distances[i] / FusedPointData.MaxDistance);
				PropertiesBlender.Blend(ConsolidatedPoint, Context->CurrentIO->GetInPoint(FusedPointIndex), ConsolidatedPoint, Weight);
				Context->MetadataBlender->Blend(ReadIndex, FusedPointIndex, ReadIndex, Weight);
			}

			if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.CompleteBlending(ConsolidatedPoint); }
			Context->MetadataBlender->CompleteBlending(ReadIndex, AverageDivider);
		};

		if (PCGExMT::ParallelForLoop(Context, Context->FusedPoints.Num(), Initialize, FusePoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->FusedPoints.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
