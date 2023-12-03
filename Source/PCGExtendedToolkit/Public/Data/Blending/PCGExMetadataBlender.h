// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"
#include "PCGExDataBlendingOperation.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExDataBlendingCopy.h"
#include "PCGExDataBlendingMin.h"
#include "PCGExDataBlendingMax.h"
#include "PCGExDataBlendingAverage.h"
#include "PCGExDataBlendingWeight.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExMetadataBlender.generated.h"

namespace PCGExDataBlending
{
	static UPCGExDataBlendingOperation* CreateOperation(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = NewObject<UPCGExDataBlending##_ID##_NAME>(); break;

		UPCGExDataBlendingOperation* NewOperation = nullptr;
		switch (Type)
		{
		default:
		case EPCGExDataBlendingType::None:
#define PCGEX_SAO_NONE(_TYPE, _NAME) case EPCGMetadataTypes::_NAME : NewOperation = NewObject<UPCGExBlend##_NAME##Base>(); break;
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NONE) }
#undef PCGEX_SAO_NONE
			break;
		case EPCGExDataBlendingType::Copy:
#define PCGEX_SAO_COPY(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Copy)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_COPY) }
#undef PCGEX_SAO_COPY
			break;
		case EPCGExDataBlendingType::Average:
#define PCGEX_SAO_AVG(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Average)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_AVG) }
#undef PCGEX_SAO_AVG
			break;
		case EPCGExDataBlendingType::Weight:
#define PCGEX_SAO_WHT(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Weight)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_WHT) }
#undef PCGEX_SAO_WHT
			break;
		case EPCGExDataBlendingType::Min:
#define PCGEX_SAO_MIN(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Min)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_MIN) }
#undef PCGEX_SAO_MIN
			break;
		case EPCGExDataBlendingType::Max:
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
	EPCGExDataBlendingType DefaultOperation = EPCGExDataBlendingType::Copy;

	void PrepareForData(
		const UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData = nullptr);

	/**
	 * 
	 * @param InPrimaryData 
	 * @param InSecondaryData Can be nullptr. 
	 * @param OperationTypeOverrides 
	 */
	void PrepareForData(
		const UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);

	UPCGExMetadataBlender* Copy(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData) const;
	
	void PrepareForOperations(PCGMetadataEntryKey InPrimaryOutputKey) const;
	void DoOperations(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha = 0) const;
	void FinalizeOperations(PCGMetadataEntryKey InPrimaryOutputKey, double Alpha) const;
	void ResetToDefaults(PCGMetadataEntryKey InPrimaryOutputKey) const;
	
protected:
	TMap<FName, EPCGExDataBlendingType> BlendingOverrides;
	TArray<UPCGExDataBlendingOperation*> Attributes;
	TArray<UPCGExDataBlendingOperation*> AttributesToBePrepared;
	TArray<UPCGExDataBlendingOperation*> AttributesToBeFinalized;
	void InternalPrepareForData(
		const UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);
};
