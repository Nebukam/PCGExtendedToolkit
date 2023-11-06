// Fill out your copyright notice in the Description page of Project Settings.

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "Misc/PCGExWriteIndex.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "PCGPin.h"
#include "PCGContext.h"
//#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
//#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "PCGExCommon.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"

namespace PCGExWriteIndex
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGExWriteIndexSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExWriteIndexTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExWriteIndex::SourceLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip", "For each of the source points, their index position in the data will be written to an attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGExOutputPinTooltip", "The source points will be output with the newly added attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGElementPtr UPCGExWriteIndexSettings::CreateElement() const
{
	return MakeShared<FPCGExWriteIndexElement>();
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);


	const UPCGExWriteIndexSettings* Settings = Context->GetInputSettings<UPCGExWriteIndexSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExWriteIndex::SourceLabel);

	const FPCGAttributePropertyOutputNoSourceSelector OutSelector = Settings->OutSelector;

	if (!OutSelector.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidOutput", "Output is invalid."));
		return true;
	}

	const FName AttributeName = OutSelector.GetName();
	FPCGMetadataAttribute<int64>* IndexAttribute = nullptr;

	auto OnDataCopyBegin = [&IndexAttribute, AttributeName](const int32 Index, const int32 PointCount, FPCGExPointDataPair& Pair)
	{
		IndexAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute<int64>(Pair.OutputPointData->Metadata, AttributeName, -1);
		return true;
	};
		
	auto OnPointCopied = [IndexAttribute](const int32 Index, const FPCGPoint& InPoint, FPCGPoint& OutPoint, FPCGExPointDataPair& Pair)
	{
		Pair.OutputPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
		IndexAttribute->SetValue(OutPoint.MetadataEntry, Index);
	};

	auto OnDataCopyEnd = [](const int32 Index, FPCGExPointDataPair& Pair){};
		
	TArray<FPCGExPointDataPair> Pairs;
	FPCGExCommon::ForwardSourcePoints(Context, Sources, Pairs, OnDataCopyBegin, OnPointCopied, OnDataCopyEnd);

	if (Pairs.Num() != Sources.Num())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidOutput", "Some inputs were not PointData and have been omitted."));
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
