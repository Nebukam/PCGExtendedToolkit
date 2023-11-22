// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"

#define PCGEX_FUSE_FOREACH_POINTPROPERTY(MACRO, MACRO_TRANSFORM, MACRO_ROTATOR)\
MACRO(double, Density, Density, 0) \
MACRO(FVector, Extents, GetExtents(), FVector::Zero()) \
MACRO(FVector4, Color, Color, FVector4::Zero()) \
MACRO_TRANSFORM(FTransform, Transform, Transform, FTransform::Identity)\
MACRO(FVector, Position, Transform.GetLocation(), FVector::Zero()) \
MACRO_ROTATOR(FRotator, Rotation, Transform.Rotator(), FRotator::ZeroRotator)\
MACRO(FVector, Scale, Transform.GetScale3D(), FVector::Zero()) \
MACRO(double, Steepness, Steepness, 0) \
MACRO(int32, Seed, Seed, 0)

#define PCGEX_FUSE_FOREACH_POINTPROPERTY_SETTER(MACRO)\
MACRO(Density, Density) \
MACRO(Extents, SetExtents()) \
MACRO(Color, Color) \
MACRO(Transform, Transform) \
MACRO(Steepness, Steepness) \
MACRO(Seed, Seed)

#define PCGEX_FUSE_IGNORE(_TYPE, _NAME, _ACCESSOR, ...) // Ignore 
#define PCGEX_FUSE_DECLARE(_TYPE, _NAME, _ACCESSOR, _DEFAULT_VALUE, ...) _TYPE Out##_NAME = _DEFAULT_VALUE;
#define PCGEX_FUSE_ADD(_TYPE, _NAME, _ACCESSOR, ...) Out##_NAME += Point._ACCESSOR;
#define PCGEX_FUSE_DIVIDE(_TYPE, _NAME, _ACCESSOR, ...) Out##_NAME /= NumFused;
#define PCGEX_FUSE_RDIVIDE(_TYPE, _NAME, _ACCESSOR, ...) Out##_NAME = FRotator(Out##_NAME.Yaw / NumFused, Out##_NAME.Pitch / NumFused, Out##_NAME.Roll / NumFused);

PCGEx::EIOInit UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NewOutput; }

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RefreshFuseMethodHiddenNames();
}

void UPCGExFusePointsSettings::RefreshFuseMethodHiddenNames()
{
	if (!FuseMethodOverrides.IsEmpty())
	{
		for (FPCGExInputDescriptorWithFuseMethod& Descriptor : FuseMethodOverrides)
		{
			Descriptor.HiddenDisplayName = Descriptor.Selector.GetName().ToString();
		}
	}
}

void UPCGExFusePointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshFuseMethodHiddenNames();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

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

	Context->FuseMethod = Settings->FuseMethod;
	Context->Radius = FMath::Pow(Settings->Radius, 2);
	Context->bComponentWiseRadius = Settings->bComponentWiseRadius;
	Context->Radiuses = Settings->Radiuses;

	return Context;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	FPCGExFusePointsContext* Context = static_cast<FPCGExFusePointsContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->FusedPoints.Reset(IO->NumPoints);
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 Index, const UPCGExPointIO* IO)
	{
		const FVector PtPosition = Point.Transform.GetLocation();
		double Distance = -1;
		PCGExFuse::FFusedPoint* FuseTarget = nullptr;

		{
			FReadScopeLock ScopeLock(Context->PointsLock);
			if (Context->bComponentWiseRadius)
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					if (abs(PtPosition.X - FusedPoint.Position.X) <= Context->Radiuses.X &&
						abs(PtPosition.Y - FusedPoint.Position.Y) <= Context->Radiuses.Y &&
						abs(PtPosition.Z - FusedPoint.Position.Z) <= Context->Radiuses.Z)
					{
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
		}

		FWriteScopeLock ScopeLock(Context->PointsLock);
		if (!FuseTarget)
		{
			FuseTarget = &Context->FusedPoints.Emplace_GetRef();
			FuseTarget->MainIndex = Index;
			FuseTarget->Position = PtPosition;
		}

		FuseTarget->Fused.Add(Index);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph2ndPass);
		}
	}


	auto InitializeReconcile = [&Context]()
	{
		Context->OutPoints = &Context->CurrentIO->Out->GetMutablePoints();
	};

	auto FusePoints = [&Context](int32 ReadIndex)
	{
		FPCGPoint NewPoint;
		Context->CurrentIO->Out->Metadata->InitializeOnSet(NewPoint.MetadataEntry);
		{
			FReadScopeLock ScopeLock(Context->PointsLock);
			PCGExFuse::FFusedPoint& FusedPointData = Context->FusedPoints[ReadIndex];
			
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_DECLARE, PCGEX_FUSE_DECLARE, PCGEX_FUSE_DECLARE)
			
			for (const int32 Index : FusedPointData.Fused)
			{
				FPCGPoint Point = Context->CurrentIO->In->GetPoint(Index);
				PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_ADD, PCGEX_FUSE_IGNORE, PCGEX_FUSE_ADD)
			}

			double NumFused = static_cast<double>(FusedPointData.Fused.Num());
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_DIVIDE, PCGEX_FUSE_IGNORE, PCGEX_FUSE_RDIVIDE)

			OutTransform.SetLocation(OutPosition);
			OutTransform.SetRotation(OutRotation.Quaternion());
			OutTransform.SetScale3D(OutScale);

			NewPoint.Density = OutDensity;
			NewPoint.SetExtents(OutExtents);
			NewPoint.Color = OutColor;
			NewPoint.Transform = OutTransform;
			NewPoint.Steepness = OutSteepness;
			NewPoint.Seed = OutSeed;
			
		}
		{
			FWriteScopeLock ScopeLock(Context->PointsLock);
			Context->OutPoints->Add(NewPoint);
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph2ndPass))
	{
		if (PCGEx::Common::ParallelForLoop(Context, Context->FusedPoints.Num(), InitializeReconcile, FusePoints, Context->ChunkSize))
		{
			Context->OutPoints = nullptr;
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
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

#undef PCGEX_FUSE_FOREACH_POINTPROPERTY
#undef PCGEX_FUSE_FOREACH_POINTPROPERTY_SETTER
#undef PCGEX_FUSE_IGNORE 
#undef PCGEX_FUSE_DECLARE
#undef PCGEX_FUSE_ADD
#undef PCGEX_FUSE_DIVIDE
#undef PCGEX_FUSE_RDIVIDE

#undef LOCTEXT_NAMESPACE
