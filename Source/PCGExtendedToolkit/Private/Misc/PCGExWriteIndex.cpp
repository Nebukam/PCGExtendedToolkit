// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"

#if WITH_EDITOR
FText UPCGExWriteIndexSettings::GetNodeTooltipText() const { return LOCTEXT("PCGExWriteIndexTooltip", "Write the current point index to an attribute."); }
#endif // WITH_EDITOR

FPCGElementPtr UPCGExWriteIndexSettings::CreateElement() const { return MakeShared<FPCGExWriteIndexElement>(); }

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(InContext);

	if (!Context->IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidContext", "Inputs are missing or invalid."));
		return true;
	}

	const UPCGExWriteIndexSettings* Settings = Context->GetInputSettings<UPCGExWriteIndexSettings>();
	check(Settings);

	const FPCGAttributePropertyOutputNoSourceSelector OutSelector = Settings->OutSelector;

	if (!OutSelector.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidOutput", "Output is invalid."));
		return true;
	}

	const FName AttributeName = OutSelector.GetName();

	auto ProcessPair = [&Context, &AttributeName](PCGEx::FPointIO* IO, const int32)
	{
		FPCGMetadataAttribute<int64>* IndexAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute<int64>(IO->Out->Metadata, AttributeName, -1);
		auto ProcessPoint = [&IO, &IndexAttribute](FPCGPoint& Point, const int32 Index, const FPCGPoint& OtherPoint)
		{
			IndexAttribute->SetValue(Index, Index);
		};

		IO->ForwardPoints(Context, ProcessPoint);
	};

	Context->Points.ForEach(Context, ProcessPair);
	Context->Points.OutputTo(Context);

	return true;
}

#undef LOCTEXT_NAMESPACE
