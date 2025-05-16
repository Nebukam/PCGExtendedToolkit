// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FMetadataBlender final : public TSharedFromThis<FMetadataBlender>
	{
	public:
		bool bBlendProperties = true;

		TMap<FName, FDataBlendingProcessorBase*> OperationIdMap;

		~FMetadataBlender() = default;

		explicit FMetadataBlender(const FPCGExBlendingDetails* InBlendingDetails);
		explicit FMetadataBlender(const FMetadataBlender* ReferenceBlender);

		void PrepareForData(
			const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
			const bool bInitFirstOperation = true,
			const TSet<FName>* IgnoreAttributeSet = nullptr,
			const bool bSoftMode = false);

		void PrepareForData(
			const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
			const TSharedRef<PCGExData::FFacade>& InSecondaryFacade,
			const PCGExData::ESource SecondarySource = PCGExData::ESource::In,
			const bool bInitFirstOperation = true,
			const TSet<FName>* IgnoreAttributeSet = nullptr,
			const bool bSoftMode = false);


		void PrepareForBlending(int32 TargetIndex, const PCGExData::FConstPoint& Defaults = nullptr) const;
		void PrepareForBlending(const int32 PrimaryIndex, const PCGExData::FConstPoint& Defaults = nullptr) const;

		void Blend(const PCGExData::FConstPoint& A, const PCGExData::FConstPoint& B, const PCGExData::FMutablePoint& Target, const double Weight);
		void Blend(const int32 PrimaryIndex, const int32 SecondaryIndex, const int32 TargetIndex, const double Weight);

		void Copy(const int32 TargetIndex, const int32 SecondaryIndex);

		void CompleteBlending(const PCGExData::FMutablePoint& Target, const int32 Count, const double TotalWeight) const;
		void CompleteBlending(const int32 PrimaryIndex, const int32 Count, const double TotalWeight) const;

		void PrepareRangeForBlending(const int32 StartIndex, const int32 Range) const;
		void BlendRange(const PCGExData::FConstPoint& A, const PCGExData::FConstPoint& B, const int32 StartIndex, const int32 Range, const TArrayView<double>& Weights);
		void CompleteRangeBlending(const int32 StartIndex, const int32 Range, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const;

		void BlendRangeFromTo(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const int32 StartIndex, const TArrayView<double>& Weights);

		// Soft ops

		void PrepareForBlending(PCGExData::FMutablePoint& Target, const PCGExData::FConstPoint& Defaults = nullptr) const;

		void Copy(const PCGExData::FMutablePoint& Target, const PCGExData::FConstPoint& Source);
		void Blend(const PCGExData::FConstPoint& A, const PCGExData::FConstPoint& B, const PCGExData::FMutablePoint& Target, const double Weight, const bool bIsFirstOperation = false);
		void CompleteBlending(const PCGExData::FMutablePoint& Target, const int32 Count, const double TotalWeight) const;

		void Cleanup();

	protected:
		const FPCGExBlendingDetails* BlendingDetails = nullptr;
		TUniquePtr<FPropertiesBlender> PropertiesBlender;
		bool bSkipProperties = false;
		TArray<TSharedPtr<FDataBlendingProcessorBase>> Operations;

		TArray<FPCGPoint>* PrimaryPoints = nullptr;
		const TArray<FPCGPoint>* SecondaryPoints = nullptr;

		TArray<int8> FirstPointOperation;

		void InternalPrepareForData(
			const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade,
			const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade,
			const PCGExData::ESource SecondarySource,
			const bool bInitFirstOperation,
			const TSet<FName>* IgnoreAttributeSet,
			const bool bSoftMode = false);
	};
}
