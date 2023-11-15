// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExDrawRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExDrawRelations"

PCGEx::EIOInit UPCGExDrawRelationsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NoOutput; }

FPCGElementPtr UPCGExDrawRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExDrawRelationsElement>();
}

UPCGExDrawRelationsSettings::UPCGExDrawRelationsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DebugSettings.PointScale = 0.0f;
}

#if WITH_EDITOR
void UPCGExDrawRelationsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World()) { FlushPersistentDebugLines(EditorWorld); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExDrawRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawRelationsElement::Execute);

#if  WITH_EDITOR

	FPCGExRelationsProcessorContext* Context = static_cast<FPCGExRelationsProcessorContext*>(InContext);

	const UPCGExDrawRelationsSettings* Settings = Context->GetInputSettings<UPCGExDrawRelationsSettings>();
	check(Settings);

	UWorld* World = PCGEx::Common::GetWorld(Context);

	if (Context->IsSetup())
	{
		FlushPersistentDebugLines(World);

		if (!Validate(Context)) { return true; }
		if (!Settings->bDebug) { return true; }

		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more points
		}
		else
		{
			Context->SetState(PCGExMT::EState::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context, &World, &Settings](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		//FWriteScopeLock ScopeLock(Context->ContextLock);

		const FVector Start = Point.Transform.GetLocation();

		TArray<PCGExRelational::FSocketSampler> Samplers;
		if (Settings->bDrawSocketCones) { Context->PrepareSamplersForPoint(Point, Samplers); }

		for (const PCGExRelational::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const PCGExRelational::FSocketMetadata SocketMetadata = SocketInfos.Socket->GetData(Point.MetadataEntry);

			if (Settings->bDrawSocketCones)
			{
				for (PCGExRelational::FSocketSampler Sampler : Samplers)
				{
					double AngleWidth = FMath::Acos(FMath::Max(-1.0, FMath::Min(1.0, Sampler.DotTolerance)));
					DrawDebugCone(
						World,
						Sampler.Origin,
						Sampler.Direction,
						Sampler.MaxDistance,
						AngleWidth, AngleWidth, 12,
						Sampler.SocketInfos->Socket->Descriptor.DebugColor,
						true, -1, 0, .5f);
				}
			}

			if (Settings->bDrawRelations)
			{
				if (SocketMetadata.Index == -1) { continue; }
				if (Settings->bFilterRelations && SocketMetadata.RelationType != Settings->RelationType) { continue; }

				FPCGPoint PtB = IO->In->GetPoint(SocketMetadata.Index);
				FVector End = PtB.Transform.GetLocation();
				float Thickness = 1.0f;
				float ArrowSize = 0.0f;
				float Lerp = 1.0f;

				switch (SocketMetadata.RelationType)
				{
				case EPCGExRelationType::Unknown:
					Lerp = 0.8f;
					Thickness = 0.5f;
					ArrowSize = 1.0f;
					break;
				case EPCGExRelationType::Unique:
					Lerp = 0.8f;
					ArrowSize = 1.0f;
					break;
				case EPCGExRelationType::Shared:
					Lerp = 0.4f;
					ArrowSize = 2.0f;
					break;
				case EPCGExRelationType::Match:
					Lerp = 0.5f;
					Thickness = 2.0f;
					break;
				case EPCGExRelationType::Complete:
					Lerp = 0.5f;
					Thickness = 2.0f;
					break;
				case EPCGExRelationType::Mirror:
					ArrowSize = 2.0f;
					Lerp = 0.5f;
					break;
				default:
					//Lerp = 0.0f;
					//Alpha = 0.0f;
					;
				}

				if (ArrowSize > 0.0f)
				{
					DrawDebugDirectionalArrow(World, Start, FMath::Lerp(Start, End, Lerp), 3.0f, SocketInfos.Socket->Descriptor.DebugColor, true, -1, 0, Thickness);
				}
				else
				{
					DrawDebugLine(World, Start, FMath::Lerp(Start, End, Lerp), SocketInfos.Socket->Descriptor.DebugColor, true, -1, 0, Thickness);
				}
			}
		}
	};

	if (Context->IsState(PCGExMT::EState::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingParams);
		}
	}

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->In);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingParams))
	{
		//if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize)) { Context->SetState(PCGExMT::EState::ProcessingParams); }
		Initialize(Context->CurrentIO);
		for (int i = 0; i < Context->CurrentIO->NumPoints; i++) { ProcessPoint(Context->CurrentIO->In->GetPoint(i), i, Context->CurrentIO); }
		Context->SetState(PCGExMT::EState::ReadyForNextParams);
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		//Context->OutputPointsAndParams();
		return true;
	}

#elif
	return  true;
#endif

	return false;
}

#undef LOCTEXT_NAMESPACE
