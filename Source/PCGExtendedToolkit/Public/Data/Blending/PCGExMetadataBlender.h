// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExDataBlending.h"
#include "PCGExProxyDataBlending.h"
#include "PCGExScopedContainers.h"


namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FMetadataBlender final : public IBlender
	{
	public:
		bool bBlendProperties = true;

		FMetadataBlender() = default;
		virtual ~FMetadataBlender() override = default;

		void SetSourceData(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide InSourceSide = PCGExData::EIOSide::In);
		void SetTargetData(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		bool Init(FPCGExContext* InContext, const FPCGExBlendingDetails& InBlendingDetails, const TSet<FName>* IgnoreAttributeSet = nullptr, const bool bWantsDirectAccess = false, const PCGExData::EIOSide BSide = PCGExData::EIOSide::Out);

		virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight) const override;
		virtual void Blend(const int32 SourceAIndex, const int32 SourceBIndex, const int32 TargetIndex, const double Weight) const override;

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const override;

		virtual void BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const override;
		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, TArray<PCGEx::FOpStats>& Trackers) const override;
		virtual void EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const override;

		const TArray<FPCGAttributeIdentifier>& GetAttributeIdentifiers() const { return AttributeIdentifiers; }

	protected:
		TWeakPtr<PCGExData::FFacade> SourceFacadeHandle;
		PCGExData::EIOSide SourceSide = PCGExData::EIOSide::In;
		TArray<FPCGAttributeIdentifier> AttributeIdentifiers;

		TWeakPtr<PCGExData::FFacade> TargetFacadeHandle;

		TArray<TSharedPtr<FProxyDataBlender>> Blenders;
		TSharedPtr<PCGExMT::TScopedArray<PCGEx::FOpStats>> ScopedTrackers;
	};
}
