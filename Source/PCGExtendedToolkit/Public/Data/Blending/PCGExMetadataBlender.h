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
