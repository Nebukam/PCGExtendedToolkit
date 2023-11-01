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
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
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
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip",
	                                    "For each of the source points, their index position in the data will be written to an attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGExOutputPinTooltip",
	                                    "The source points will be output with the newly added attribute.");
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
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	FPCGAttributePropertyOutputNoSourceSelector OutSelector = Settings->OutSelector;
	const EPCGAttributePropertySelection Sel = OutSelector.GetSelection();

	if (!OutSelector.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidOutput", "Output is invalid."));
		return true;
	}

	const FName AttributeName = OutSelector.GetName();

	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* InPointData = SourceData->ToPointData(Context);
		if (!InPointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		// Initialize output dataset
		UPCGPointData* OutPointData = NewObject<UPCGPointData>();
		OutPointData->InitializeFromData(InPointData);
		Outputs.Add_GetRef(Source).Data = OutPointData;

		FPCGMetadataAttribute<int64>* IndexAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute<int64>(OutPointData->Metadata, AttributeName, -1);

		//TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OutPointData, OutSelector);
		//TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(OutPointData, OutSelector);

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		int64 Index = 0;
		
		auto CopyAndAssignIndex = [OutPointData, IndexAttribute, &Index](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			IndexAttribute->SetValue(OutPoint.MetadataEntry, Index++);
			return true;
		};
		
		FPCGAsync::AsyncPointProcessing(Context, InPointData->GetPoints(), OutPoints, CopyAndAssignIndex);

		/*
		auto DoOperation = [&Accessor, &Keys, &IndexAttribute](auto DummyValue) -> bool
		{
			using AttributeType = decltype(DummyValue);
			AttributeType OutputValue{};
			
			bool bSuccess = PCGMetadataElementCommon::ApplyOnAccessor<AttributeType>(*Keys, *Accessor, [&IndexAttribute](const AttributeType& InValue, int32 Index)
			{
				IndexAttribute->SetValue(Index, static_cast<int64>(Index));
			});

			return bSuccess;
		};

		if (!PCGMetadataAttribute::CallbackWithRightType(PCG::Private::MetadataTypes<int64>::Id, DoOperation))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("OperationFailed", "Could not create attribute '{0}'"), FText::FromName(AttributeName)));
			return true;
		}
		*/

	}

	return true;
}

#undef LOCTEXT_NAMESPACE
