// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExDataBlendingOperations.h"
#include "PCGExPropertiesBlender.h"
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
		bool bBlendProperties = true;

		virtual ~FMetadataBlender();

		explicit FMetadataBlender(FPCGExBlendingSettings* InBlendingSettings);
		explicit FMetadataBlender(const FMetadataBlender* ReferenceBlender);

		void PrepareForData(
			PCGExData::FPointIO& InData);

		void PrepareForData(
			PCGExData::FPointIO& InPrimaryData,
			const PCGExData::FPointIO& InSecondaryData,
			bool bSecondaryIn = true);

		FMetadataBlender* Copy(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData) const;

		void PrepareForBlending(const PCGEx::FPointRef& Target, const FPCGPoint* Defaults = nullptr) const;
		void Blend(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const PCGEx::FPointRef& Target, const double Alpha = 0) const;
		void CompleteBlending(const PCGEx::FPointRef& Target, double Alpha) const;

		void PrepareRangeForBlending(const int32 StartIndex, const int32 Count) const;
		void BlendRange(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const;
		void CompleteRangeBlending(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const;

		void BlendRangeOnce(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const;

		void FullBlendToOne(const TArrayView<double>& Alphas) const;

		void Write(bool bFlush = true);
		void Flush();

	protected:
		FPCGExBlendingSettings* BlendingSettings = nullptr;
		FPropertiesBlender* PropertiesBlender = nullptr;
		TArray<FDataBlendingOperationBase*> Attributes;
		TArray<FDataBlendingOperationBase*> AttributesToBePrepared;
		TArray<FDataBlendingOperationBase*> AttributesToBeCompleted;

		TArray<FPCGPoint>* PrimaryPoints = nullptr;
		TArray<FPCGPoint>* SecondaryPoints = nullptr;

		void InternalPrepareForData(
			PCGExData::FPointIO& InPrimaryData,
			const PCGExData::FPointIO& InSecondaryData,
			bool bSecondaryIn);
	};
}
