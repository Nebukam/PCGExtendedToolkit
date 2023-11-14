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

PCGEx::EIOInit UPCGExDrawRelationsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::Forward; }

FPCGElementPtr UPCGExDrawRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExDrawRelationsElement>();
}

bool FPCGExDrawRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawRelationsElement::Execute);

#if  WITH_EDITOR

	FPCGExRelationsProcessorContext* Context = static_cast<FPCGExRelationsProcessorContext*>(InContext);

	const UPCGExDrawRelationsSettings* Settings = Context->GetInputSettings<UPCGExDrawRelationsSettings>();
	check(Settings);

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	if (Context->IsCurrentOperation(PCGEx::EOperation::Setup))
	{
		if (Context->Params.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
			Context->Points->OutputTo(Context);
			return true;
		}

		FlushPersistentDebugLines(EditorWorld);

		if (!Settings->bDebug)
		{
			Context->Points->OutputTo(Context);
			return true;
		}
		
		Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context, &EditorWorld](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		const FVector Start = Point.Transform.GetLocation();
		for (const PCGExRelational::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const PCGExRelational::FSocketMetadata SocketMetadata = SocketInfos.Socket->GetData(Point.MetadataEntry);

			if (SocketMetadata.Index == -1) { continue; }

			FPCGPoint PtB = IO->Out->GetPoint(SocketMetadata.Index);
			FVector End = PtB.Transform.GetLocation();
			float Thickness = 1.0f;
			float Alpha = 1.0f;
			float Lerp = 1.0f;

			switch (SocketMetadata.RelationType)
			{
			case EPCGExRelationType::Unknown:
			case EPCGExRelationType::Unique:
				Lerp = 0.4f;
				break;
			case EPCGExRelationType::Shared:
				Lerp = 0.8f;
				Thickness = 2.0f;
				break;
			case EPCGExRelationType::Match:
				Thickness = 4.0f;
				break;
			case EPCGExRelationType::Complete:
				Thickness = 5.0f;
				Alpha = 0.5f;
				break;
			case EPCGExRelationType::Mirror:
				break;
			default:
				//Lerp = 0.0f;
				//Alpha = 0.0f;
				;
			}

			FColor col = SocketInfos.Socket->Descriptor.DebugColor;
			col.A = Alpha * 255;
			DrawDebugLine(EditorWorld, Start, FMath::Lerp(Start, End, Lerp), col, true, -1, 0, Thickness);
		}
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
		}
		else
		{
			Context->SetOperation(PCGEx::EOperation::ProcessingParams);
		}
	}

	auto Initialize = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingParams))
	{
		// Debug draw is not thread-safe T_T
		//if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize)) { Context->SetOperation(PCGEx::EOperation::ReadyForNextParams); }
		Initialize(Context->CurrentIO);
		for (int i = 0; i < Context->CurrentIO->NumPoints; i++) { ProcessPoint(Context->CurrentIO->Out->GetPoint(i), i, Context->CurrentIO); }
		Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Points->OutputTo(Context);
		return true;
	}

#elif
	return  true;
#endif

	return false;
}

#undef LOCTEXT_NAMESPACE
