// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"
#define PCGEX_NAMESPACE WriteIndex

PCGExData::EInit UPCGExWriteIndexSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WriteIndex)

FPCGExWriteIndexContext::~FPCGExWriteIndexContext()
{
	IndicesBuffer.Empty();
	NormalizedIndicesBuffer.Empty();
	PCGEX_DELETE(NormalizedIndexAccessor)
	PCGEX_DELETE(IndexAccessor)
}

bool FPCGExWriteIndexElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)

	PCGEX_FWD(bOutputNormalizedIndex)
	PCGEX_FWD(OutputAttributeName)

	PCGEX_VALIDATE_NAME(Context->OutputAttributeName)

	return true;
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	PCGEX_CONTEXT(WriteIndex)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (Context->bOutputNormalizedIndex)
		{
			auto Initialize = [&](PCGExData::FPointIO& PointIO)
			{
				Context->NormalizedIndicesBuffer.Reset(PointIO.GetNum());
				Context->NormalizedIndexAccessor = PCGEx::FAttributeAccessor<double>::FindOrCreate(PointIO, Context->OutputAttributeName, -1, false);
				Context->NormalizedIndexAccessor->GetRange(Context->NormalizedIndicesBuffer, 0);
			};

			if (!Context->ProcessCurrentPoints(
				Initialize, [&](const int32 Index, const PCGExData::FPointIO& PointIO)
				{
					Context->NormalizedIndicesBuffer[Index] = static_cast<double>(Index) / static_cast<double>(Context->NormalizedIndicesBuffer.Num());
				}))
			{
				return false;
			}

			Context->NormalizedIndexAccessor->SetRange(Context->NormalizedIndicesBuffer);
			PCGEX_DELETE(Context->NormalizedIndexAccessor)
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
		else
		{
			auto Initialize = [&](PCGExData::FPointIO& PointIO)
			{
				Context->IndicesBuffer.Reset(PointIO.GetNum());
				Context->IndexAccessor = PCGEx::FAttributeAccessor<int32>::FindOrCreate(PointIO, Context->OutputAttributeName, -1, false);
				Context->IndexAccessor->GetRange(Context->IndicesBuffer, 0);
			};

			if (!Context->ProcessCurrentPoints(
				Initialize, [&](const int32 Index, const PCGExData::FPointIO& PointIO)
				{
					Context->IndicesBuffer[Index] = Index;
				}))
			{
				return false;
			}

			Context->IndexAccessor->SetRange(Context->IndicesBuffer);
			PCGEX_DELETE(Context->IndexAccessor)
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
