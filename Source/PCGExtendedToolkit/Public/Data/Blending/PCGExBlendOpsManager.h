// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExPointsProcessor.h"
#include "PCGExProxyDataBlending.h"
#include "PCGExBlendOpFactoryProvider.h"
#include "PCGExScopedContainers.h"

namespace PCGExDataBlending
{
	PCGEXTENDEDTOOLKIT_API
	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	PCGEXTENDEDTOOLKIT_API
	void RegisterBuffersDependencies_SourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	PCGEXTENDEDTOOLKIT_API
	void RegisterBuffersDependencies_SourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	PCGEXTENDEDTOOLKIT_API
	void RegisterBuffersDependencies_Sources(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	class FBlendOpsManager : public TSharedFromThis<FBlendOpsManager>
	{
	protected:
		TSharedPtr<PCGExData::FFacade> WeightFacade;
		TSharedPtr<PCGExData::FFacade> SourceAFacade;
		TSharedPtr<PCGExData::FFacade> SourceBFacade;
		TSharedPtr<PCGExData::FFacade> TargetFacade;
		TSharedPtr<TArray<TSharedPtr<FPCGExBlendOperation>>> Operations;

	public:
		explicit FBlendOpsManager(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		explicit FBlendOpsManager();

		void SetWeightFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		void SetSourceA(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		void SetSourceB(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		void SetSources(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		void SetTargetFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& InFactories) const;

		FORCEINLINE void Blend(const int32 Index) const
		{
			for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->Blend(Index); }
		}

		FORCEINLINE void Blend(const int32 SourceIndex, const int32 TargetIndex) const
		{
			for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->Blend(SourceIndex, TargetIndex); }
		}

		FORCEINLINE void Blend(const int32 SourceAIndex, const int32 SourceBIndex, const int32 TargetIndex, const double InWeight) const
		{
			for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->Blend(SourceAIndex, SourceBIndex, TargetIndex, InWeight); }
		}

		FORCEINLINE void Blend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight) const
		{
			for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->Blend(SourceIndex, TargetIndex, InWeight); }
		}

		void InitScopedTrackers(const TArray<PCGExMT::FScope>& Loops);
		TArray<PCGEx::FOpStats>& GetTracking(const PCGExMT::FScope& Scope);
		
		void BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& OutTrackers) const;
		void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight, TArray<PCGEx::FOpStats>& Trackers) const;
		void EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const;

		void Cleanup(FPCGExContext* InContext);

	protected:
		TSharedPtr<PCGExMT::TScopedArray<PCGEx::FOpStats>> ScopedTrackers;
	};
}
