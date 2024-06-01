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

		TMap<FName, FDataBlendingOperationBase*> OperationIdMap;
		
		virtual ~FMetadataBlender();

		explicit FMetadataBlender(FPCGExBlendingSettings* InBlendingSettings);
		explicit FMetadataBlender(const FMetadataBlender* ReferenceBlender);

		void PrepareForData(
			PCGExData::FPointIO& InData,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

		void PrepareForData(
			PCGExData::FPointIO& InPrimaryData,
			const PCGExData::FPointIO& InSecondaryData,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In);

		FMetadataBlender* Copy(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData) const;

		void PrepareForBlending(const PCGEx::FPointRef& Target, const FPCGPoint* Defaults = nullptr) const;
		void Blend(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const PCGEx::FPointRef& Target, const double Weight = 0) const;
		void CompleteBlending(const PCGEx::FPointRef& Target, const int32 Count, double TotalWeight) const;

		void PrepareRangeForBlending(const int32 StartIndex, const int32 Range) const;
		void BlendRange(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const int32 StartIndex, const int32 Range, const TArrayView<double>& Weights) const;
		void CompleteRangeBlending(const int32 StartIndex, const int32 Range, const TArrayView<int32>& Counts, const TArrayView<double>& TotalWeights) const;

		void BlendRangeFromTo(const PCGEx::FPointRef& From, const PCGEx::FPointRef& To, const int32 StartIndex, const TArrayView<double>& Weights) const;

		void BlendEachPrimaryToSecondary(const TArrayView<double>& Weights) const;

		void Write(bool bFlush = true);
		void Flush();

		void InitializeFromScratch();

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
			const PCGExData::ESource SecondarySource);
	};
}
