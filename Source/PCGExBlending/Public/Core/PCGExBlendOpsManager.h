// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExProxyDataBlending.h"
#include "PCGExBlendOpFactoryProvider.h"

class UPCGExBlendOpFactory;
class FPCGExBlendOperation;

namespace PCGExMT
{
	template <typename T>
	class TScopedArray;
}

namespace PCGExBlending
{
	PCGEXBLENDING_API void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	PCGEXBLENDING_API void RegisterBuffersDependencies_SourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	PCGEXBLENDING_API void RegisterBuffersDependencies_SourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	PCGEXBLENDING_API void RegisterBuffersDependencies_Sources(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories);

	class PCGEXBLENDING_API FBlendOpsManager : public IBlender
	{
	protected:
		TSharedPtr<PCGExData::FFacade> WeightFacade;

		TSharedPtr<PCGExData::FFacade> SourceAFacade;
		PCGExData::EIOSide SideA = PCGExData::EIOSide::In;

		TSharedPtr<PCGExData::FFacade> SourceBFacade;
		PCGExData::EIOSide SideB = PCGExData::EIOSide::In;

		TSharedPtr<PCGExData::FFacade> TargetFacade;
		TSharedPtr<TArray<TSharedPtr<FPCGExBlendOperation>>> Operations;
		TArray<FPCGExBlendOperation*> CachedOperations;

		bool bUsedForMultiBlendOnly = false;

	public:
		explicit FBlendOpsManager(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool MultiBlendOnly = false);
		explicit FBlendOpsManager(const bool MultiBlendOnly = false);
		virtual ~FBlendOpsManager() override = default;

		void SetWeightFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		void SetSourceA(const TSharedPtr<PCGExData::FFacade>& InDataFacade, PCGExData::EIOSide Side = PCGExData::EIOSide::In);
		void SetSourceB(const TSharedPtr<PCGExData::FFacade>& InDataFacade, PCGExData::EIOSide Side = PCGExData::EIOSide::In);
		void SetSources(const TSharedPtr<PCGExData::FFacade>& InDataFacade, PCGExData::EIOSide Side = PCGExData::EIOSide::In);
		void SetTargetFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& InFactories);

		void BlendAutoWeight(const int32 SourceIndex, const int32 TargetIndex) const;
		virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight) const override;
		virtual void Blend(const int32 SourceAIndex, const int32 SourceBIndex, const int32 TargetIndex, const double InWeight) const override;

		void InitScopedTrackers(const TArray<PCGExMT::FScope>& Loops);
		TArray<PCGEx::FOpStats>& GetScopedTrackers(const PCGExMT::FScope& Scope) const;

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const override;

		void virtual BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const override;
		void virtual MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight, TArray<PCGEx::FOpStats>& Trackers) const override;
		void virtual EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const override;

		void Cleanup(FPCGExContext* InContext);

	protected:
		TSharedPtr<PCGExMT::TScopedArray<PCGEx::FOpStats>> ScopedTrackers;
	};
}
