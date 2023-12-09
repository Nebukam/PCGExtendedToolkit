// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"

PCGExData::EInit UPCGExWriteIndexSettings::GetPointOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGElementPtr UPCGExWriteIndexSettings::CreateElement() const { return MakeShared<FPCGExWriteIndexElement>(); }

FPCGExWriteIndexContext::~FPCGExWriteIndexContext()
{
	IndicesBuffer.Empty();
	NormalizedIndicesBuffer.Empty();
	delete NormalizedIndexAccessor;
	delete IndexAccessor;
}

FPCGContext* FPCGExWriteIndexElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExWriteIndexContext* Context = new FPCGExWriteIndexContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExWriteIndexElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExWriteIndexContext* Context = static_cast<FPCGExWriteIndexContext*>(InContext);

	const UPCGExWriteIndexSettings* Settings = Context->GetInputSettings<UPCGExWriteIndexSettings>();
	check(Settings);

	const FName OutName = Settings->OutputAttributeName;
	if (!FPCGMetadataAttributeBase::IsValidName(OutName))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidName", "Output name is invalid."));
		return false;
	}

	Context->bOutputNormalizedIndex = Settings->bOutputNormalizedIndex;
	Context->OutName = Settings->OutputAttributeName;
	return true;
}


bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	FPCGExWriteIndexContext* Context = static_cast<FPCGExWriteIndexContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done();
		}
		else
		{
			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->bOutputNormalizedIndex)
	{
	}
	else
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			PointIO.GetInKeys();
			Context->IndicesBuffer.Reset(PointIO.GetNum());
			Context->IndexAccessor = PCGEx::FAttributeAccessor<int32>::FindOrCreate(PointIO.GetOut(), Context->OutName, -1, false);
			Context->IndexAccessor->GetRange(Context->IndicesBuffer, 0);
		};

		if (Context->ProcessCurrentPoints(
			Initialize, [&](const int32 Index, const PCGExData::FPointIO& PointIO)
			{
				Context->IndicesBuffer[Index] = Index;
			}))
		{
			Context->IndexAccessor->SetRange(Context->IndicesBuffer);
			delete Context->IndexAccessor;
			Context->IndexAccessor = nullptr;
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}


	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
