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
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;
#define PCGEX_FOREACH_BLEND(MACRO)\
PCGEX_BLEND_CASE(None)\
PCGEX_BLEND_CASE(Copy)\
PCGEX_BLEND_CASE(Average)\
PCGEX_BLEND_CASE(Weight)\
PCGEX_BLEND_CASE(Min)\
PCGEX_BLEND_CASE(Max)

		FDataBlendingOperationBase* NewOperation = nullptr;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLEND(PCGEX_BLEND_CASE)
		}

		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
#undef PCGEX_FOREACH_BLEND
	}

	class PCGEXTENDEDTOOLKIT_API FMetadataBlender
	{
	public:
		virtual ~FMetadataBlender();

		EPCGExDataBlendingType DefaultOperation = EPCGExDataBlendingType::Copy;

		FMetadataBlender();
		FMetadataBlender(EPCGExDataBlendingType InDefaultBlending);
		FMetadataBlender(const FMetadataBlender* ReferenceBlender);

		void PrepareForData(
			PCGExData::FPointIO* InPointIO,
			const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);

		void PrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData,
			const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);

		void PrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData,
			FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
			FPCGAttributeAccessorKeysPoints* InSecondaryKeys,
			const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);

		FMetadataBlender* Copy(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData) const;

		void PrepareForBlending(const int32 WriteKey) const;
		void Blend(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha = 0) const;
		void CompleteBlending(const int32 WriteIndex, double Alpha) const;

		void PrepareRangeForBlending(const int32 StartIndex, const int32 Count) const;
		void BlendRange(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const;
		void CompleteRangeBlending(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const;

		void BlendRangeOnce(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const;

		void ResetToDefaults(const int32 WriteIndex) const;
		void Flush();

	protected:
		TMap<FName, EPCGExDataBlendingType> BlendingOverrides;
		TArray<FDataBlendingOperationBase*> Attributes;
		TArray<FDataBlendingOperationBase*> AttributesToBePrepared;
		TArray<FDataBlendingOperationBase*> AttributesToBeCompleted;
		FPCGAttributeAccessorKeysPoints* PrimaryKeys = nullptr;
		FPCGAttributeAccessorKeysPoints* SecondaryKeys = nullptr;
		bool bOwnsPrimaryKeys = false;
		bool bOwnsSecondaryKeys = false;

		void InternalPrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData,
			FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
			FPCGAttributeAccessorKeysPoints* InSecondaryKeys,
			const TMap<FName, EPCGExDataBlendingType>& OperationTypeOverrides);
	};
}
