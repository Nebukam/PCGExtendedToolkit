// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"

PCGExPointIO::EInit UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NewOutput; }

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
	Context->FMetadataBlender = NewObject<FMetadataBlender>();
	Context->FMetadataBlender->DefaultOperation = Settings->BlendingSettings.DefaultBlending;

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
		Context->PropertyBlender.Init(Settings->BlendingSettings);
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
		auto Initialize = [&](UPCGExPointIO* PointIO)
		{
			Context->FusedPoints.Reset(PointIO->NumInPoints);
			Context->FMetadataBlender->PrepareForData(PointIO->Out, nullptr, Context->AttributesBlendingOverrides);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& Point = PointIO->GetInPoint(PointIndex);
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
		auto FusePoint = [&](int32 ReadIndex)
		{
			PCGExFuse::FFusedPoint& FusedPointData = Context->FusedPoints[ReadIndex];
			int32 NumFused = static_cast<double>(FusedPointData.Fused.Num());
			double AverageDivider = NumFused;

			const FPCGPoint& RootPoint = Context->CurrentIO->GetInPoint(FusedPointData.MainIndex);
			FPCGPoint NewPoint = Context->CurrentIO->CopyPoint(RootPoint);
			PCGMetadataEntryKey NewPointKey = NewPoint.MetadataEntry;
			Context->FMetadataBlender->PrepareForBlending(NewPointKey);

			PCGExDataBlending::FPropertiesBlender PropertiesBlender = PCGExDataBlending::FPropertiesBlender(Context->PropertyBlender);

			if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.PrepareBlending(NewPoint, RootPoint); }

			for (int i = 0; i < NumFused; i++)
			{
				const double Weight = 1 - (FusedPointData.Distances[i] / FusedPointData.MaxDistance);
				FPCGPoint Point = Context->CurrentIO->GetInPoint(FusedPointData.Fused[i]);
				PropertiesBlender.Blend(NewPoint, Point, NewPoint, Weight);
				Context->FMetadataBlender->Blend(NewPointKey, Point.MetadataEntry, NewPointKey, Weight);
			}

			if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.CompleteBlending(NewPoint); }
			Context->FMetadataBlender->CompleteBlending(NewPointKey, AverageDivider);
		};

		if (PCGExMT::ParallelForLoop(
			Context, Context->FusedPoints.Num(), []()
			{
			}, FusePoint, Context->ChunkSize))
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
