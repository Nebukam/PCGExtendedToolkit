// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExProxyDataBlending.h"





namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FMetadataBlender final : public TSharedFromThis<FMetadataBlender>
	{
	public:
		bool bBlendProperties = true;

		FMetadataBlender() = default;
		~FMetadataBlender() = default;

		void SetSourceData(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide InSourceSide = PCGExData::EIOSide::In);
		void SetTargetData(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		bool Init(FPCGExContext* InContext, const FPCGExBlendingDetails& InBlendingDetails, const TSet<FName>* IgnoreAttributeSet = nullptr);

		void Blend(const int32 SourceIndex, const int32 TargetIndex) const;
		void Blend(const int32 SourceAIndex, const int32 SourceBIndex, const int32 TargetIndex, const double Weight = 1) const;
		void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight = 1) const;

		void BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& OutTrackers) const;
		void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, TArray<PCGEx::FOpStats>& Trackers) const;
		void EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const;
		
	protected:
		TWeakPtr<PCGExData::FFacade> SourceFacadeHandle;
		PCGExData::EIOSide SourceSide = PCGExData::EIOSide::In;
		
		TWeakPtr<PCGExData::FFacade> TargetFacadeHandle;
		
		TArray<TSharedPtr<FProxyDataBlender>> Blenders;
		
	};
}
