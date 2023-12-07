// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExDataBlendingOperations.h"
#include "Data/PCGExAttributeHelpers.h"

namespace PCGExDataBlending
{
	static FDataBlendingOperationBase* CreateOperation(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = new FDataBlending##_ID<_TYPE>(); break;

		FDataBlendingOperationBase* NewOperation = nullptr;
		switch (Type)
		{
		default:
		case EPCGExDataBlendingType::None:
#define PCGEX_SAO_NONE(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, None)
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
#undef PCGEX_SAO_CLASS
		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;
	}


	class PCGEXTENDEDTOOLKIT_API FMetadataBlender
	{
	public:
		virtual ~FMetadataBlender();

		EPCGExDataBlendingType DefaultOperation = EPCGExDataBlendingType::Copy;

		void PrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData = nullptr);

		/**
		 * 
		 * @param InPrimaryData 
		 * @param InSecondaryData Can be nullptr. 
		 * @param OperationTypeOverrides 
		 */
		void PrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData,
			const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);

		FMetadataBlender* Copy(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData) const;

		void PrepareForBlending(const int32 WriteKey) const;
		void Blend(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha = 0) const;
		void CompleteBlending(const int32 WriteIndex, double Alpha) const;
		void ResetToDefaults(const int32 WriteIndex) const;
		void Flush();

	protected:
		TMap<FName, EPCGExDataBlendingType> BlendingOverrides;
		TArray<FDataBlendingOperationBase*> Attributes;
		TArray<FDataBlendingOperationBase*> AttributesToBePrepared;
		TArray<FDataBlendingOperationBase*> AttributesToBeCompleted;
		FPCGAttributeAccessorKeysPoints* PrimaryKeys = nullptr;
		FPCGAttributeAccessorKeysPoints* SecondaryKeys = nullptr;
		
		void InternalPrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData,
			const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);
	};
}
