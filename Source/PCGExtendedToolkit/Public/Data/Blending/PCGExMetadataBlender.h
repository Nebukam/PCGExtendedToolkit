// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExSAO.h"
#include "PCGExMetadataOperation.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExSAOCopy.h"
#include "PCGExSAOMin.h"
#include "PCGExSAOMax.h"
#include "PCGExSAOAverage.h"
#include "PCGExSAOWeight.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExMetadataBlender.generated.h"

UENUM(BlueprintType)
enum class EPCGExMetadataBlendingOperationType : uint8
{
	None UMETA(DisplayName = "None", ToolTip="TBD"),
	Average UMETA(DisplayName = "Average", ToolTip="Average values"),
	Weight UMETA(DisplayName = "Weight", ToolTip="TBD"),
	Min UMETA(DisplayName = "Min", ToolTip="TBD"),
	Max UMETA(DisplayName = "Max", ToolTip="TBD"),
	Copy UMETA(DisplayName = "Copy", ToolTip="TBD"),
};

namespace PCGExSAO
{
	static UPCGExMetadataOperation* CreateOperation(EPCGExMetadataBlendingOperationType Type, PCGEx::FAttributeIdentity Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = NewObject<UPCGExSAO##_ID##_NAME>(); break;

		UPCGExMetadataOperation* NewOperation = nullptr;
		switch (Type)
		{
		default:
		case EPCGExMetadataBlendingOperationType::None:
#define PCGEX_SAO_NONE(_TYPE, _NAME) case EPCGMetadataTypes::_NAME : NewOperation = NewObject<UPCGExBlend##_NAME##Base>(); break;
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NONE) }
#undef PCGEX_SAO_NONE
			break;
		case EPCGExMetadataBlendingOperationType::Copy:
#define PCGEX_SAO_COPY(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Copy)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_COPY) }
#undef PCGEX_SAO_COPY
			break;
		case EPCGExMetadataBlendingOperationType::Average:
#define PCGEX_SAO_AVG(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Average)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_AVG) }
#undef PCGEX_SAO_AVG
			break;
		case EPCGExMetadataBlendingOperationType::Weight:
#define PCGEX_SAO_WHT(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Weight)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_WHT) }
#undef PCGEX_SAO_WHT
			break;
		case EPCGExMetadataBlendingOperationType::Min:
#define PCGEX_SAO_MIN(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Min)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_MIN) }
#undef PCGEX_SAO_MIN
			break;
		case EPCGExMetadataBlendingOperationType::Max:
#define PCGEX_SAO_MAX(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Max)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_MAX) }
#undef PCGEX_SAO_MAX
			break;
		}
#undef PCGEX_SAO_NEW
		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;
	}
}

#undef PCGEX_SAO_CLASS

UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExMetadataBlender : public UObject
{
	GENERATED_BODY()

public:
	EPCGExMetadataBlendingOperationType DefaultOperation = EPCGExMetadataBlendingOperationType::Copy;
	void PrepareForData(const UPCGPointData* InData, const TMap<FName, EPCGExMetadataBlendingOperationType>& OperationTypeOverrides);
	void PrepareForOperations(PCGMetadataEntryKey OutputKey) const;
	void DoOperations(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha = 0) const;
	void FinalizeOperations(PCGMetadataEntryKey OutputKey, double Alpha) const;
	void ResetToDefaults(PCGMetadataEntryKey OutputKey) const;

protected:
	TArray<UPCGExMetadataOperation*> Attributes;
	TArray<UPCGExMetadataOperation*> AttributesPrep;
	TArray<UPCGExMetadataOperation*> AttributesFinalizers;
};
