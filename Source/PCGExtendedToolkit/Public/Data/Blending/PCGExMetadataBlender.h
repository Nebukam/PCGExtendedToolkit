// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FMetadataBlender final
	{
	public:
		bool bBlendProperties = true;

		TMap<FName, FDataBlendingOperationBase*> OperationIdMap;

		~FMetadataBlender();

		explicit FMetadataBlender(const FPCGExBlendingSettings* InBlendingSettings);
		explicit FMetadataBlender(const FMetadataBlender* ReferenceBlender);

		void PrepareForData(
			PCGExData::FFacade* InPrimaryData,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
			const bool bInitFirstOperation = false,
			const TSet<FName>* IgnoreAttributeSet = nullptr);

		void PrepareForData(
			PCGExData::FFacade* InPrimaryData,
			PCGExData::FFacade* InSecondaryData,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
			const bool bInitFirstOperation = false,
			const TSet<FName>* IgnoreAttributeSet = nullptr);

		FORCEINLINE void PrepareForBlending(const PCGEx::FPointRef& Target, const FPCGPoint* Defaults = nullptr) const;
		FORCEINLINE void PrepareForBlending(const int32 PrimaryIndex, const FPCGPoint* Defaults = nullptr) const;

		FORCEINLINE void Blend(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const PCGEx::FPointRef& Target, const double Weight);
		FORCEINLINE void Blend(const int32 PrimaryIndex, const int32 SecondaryIndex, const int32 TargetIndex, const double Weight);

		FORCEINLINE void CompleteBlending(const PCGEx::FPointRef& Target, const int32 Count, double TotalWeight) const;
		FORCEINLINE void CompleteBlending(const int32 PrimaryIndex, const int32 Count, double TotalWeight) const;

		void PrepareRangeForBlending(const int32 StartIndex, const int32 Range) const;
		void BlendRange(const PCGEx::FPointRef& A, const PCGEx::FPointRef& B, const int32 StartIndex, const int32 Range, const TArrayView<double>& Weights);
		void CompleteRangeBlending(const int32 StartIndex, const int32 Range, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const;

		void BlendRangeFromTo(const PCGEx::FPointRef& From, const PCGEx::FPointRef& To, const int32 StartIndex, const TArrayView<double>& Weights);

		void Cleanup();

	protected:
		const FPCGExBlendingSettings* BlendingSettings = nullptr;
		const FPropertiesBlender* PropertiesBlender = nullptr;
		bool bSkipProperties = false;
		TArray<FDataBlendingOperationBase*> Operations;
		TArray<FDataBlendingOperationBase*> OperationsToBePrepared;
		TArray<FDataBlendingOperationBase*> OperationsToBeCompleted;

		TArray<FPCGPoint>* PrimaryPoints = nullptr;
		TArray<FPCGPoint>* SecondaryPoints = nullptr;

		TArray<bool> FirstPointOperation;

		void InternalPrepareForData(
			PCGExData::FFacade* InPrimaryData,
			PCGExData::FFacade* InSecondaryData,
			const PCGExData::ESource SecondarySource,
			const bool bInitFirstOperation,
			const TSet<FName>* IgnoreAttributeSet);
	};
}
