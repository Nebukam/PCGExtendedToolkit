// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExBlendOpsManager.h"

#include "Data/Blending/PCGExBlendOpFactoryProvider.h"

#include "PCGExDetailsData.h"
#include "Data/Blending/PCGExProxyDataBlending.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"


#define LOCTEXT_NAMESPACE "PCGExCreateAttributeBlend"
#define PCGEX_NAMESPACE CreateAttributeBlend

namespace PCGExDataBlending
{
	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExBlendOpFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
		}
	}

	void RegisterBuffersDependencies_SourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExBlendOpFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependenciesForSourceA(InContext, FacadePreloader);
		}
	}

	void RegisterBuffersDependencies_SourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExBlendOpFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependenciesForSourceB(InContext, FacadePreloader);
		}
	}

	void RegisterBuffersDependencies_Sources(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExBlendOpFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependenciesForSourceA(InContext, FacadePreloader);
			Factory->RegisterBuffersDependenciesForSourceB(InContext, FacadePreloader);
		}
	}

	FBlendOpsManager::FBlendOpsManager(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		SetWeightFacade(InDataFacade);
		SetSources(InDataFacade);
		SetTargetFacade(InDataFacade);
		Operations = MakeShared<TArray<TSharedPtr<FPCGExBlendOperation>>>();
	}

	FBlendOpsManager::FBlendOpsManager()
	{
		Operations = MakeShared<TArray<TSharedPtr<FPCGExBlendOperation>>>();
	}

	void FBlendOpsManager::SetWeightFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		WeightFacade = InDataFacade;
	}

	void FBlendOpsManager::SetSources(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide Side)
	{
		SetSourceA(InDataFacade, Side);
		SetSourceB(InDataFacade, Side);
	}

	void FBlendOpsManager::SetSourceA(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide Side)
	{
		SourceAFacade = InDataFacade;
		SideA = Side;
	}

	void FBlendOpsManager::SetSourceB(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide Side)
	{
		SourceBFacade = InDataFacade;
		SideB = Side;
	}

	void FBlendOpsManager::SetTargetFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		TargetFacade = InDataFacade;
	}

	bool FBlendOpsManager::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExBlendOpFactory>>& InFactories)
	{
		check(SourceAFacade)
		check(SourceBFacade)
		check(TargetFacade)

		if (!WeightFacade) { WeightFacade = SourceAFacade; }
		check(WeightFacade)

		Operations->Reserve(InFactories.Num());

		for (const TObjectPtr<const UPCGExBlendOpFactory>& Factory : InFactories)
		{
			TSharedPtr<FPCGExBlendOperation> Op = Factory->CreateOperation(InContext);
			if (!Op)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("An operation could not be created."));
				return false; // FAIL
			}

			// Assign blender facades
			Op->WeightFacade = WeightFacade;

			Op->Source_A_Facade = SourceAFacade;
			Op->SideA = SideA;

			Op->Source_B_Facade = SourceBFacade;
			Op->SideB = SideB;

			Op->TargetFacade = TargetFacade;

			Op->OpIdx = Operations->Add(Op);
			Op->SiblingOperations = Operations;

			if (!Op->PrepareForData(InContext))
			{
				return false; // FAIL
			}
		}

		return true;
	}

	void FBlendOpsManager::InitScopedTrackers(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedTrackers = MakeShared<PCGExMT::TScopedArray<PCGEx::FOpStats>>(Loops);
		ScopedTrackers->ForEach([&](TArray<PCGEx::FOpStats>& Array) { InitTrackers(Array); });
	}

	void FBlendOpsManager::InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const
	{
		Trackers.SetNumUninitialized(Operations->Num());
	}

	void FBlendOpsManager::BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const
	{
		for (int i = 0; i < Operations->Num(); i++) { Trackers[i] = (*(Operations->GetData() + i))->BeginMultiBlend(TargetIndex); }
	}

	void FBlendOpsManager::MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight, TArray<PCGEx::FOpStats>& Trackers) const
	{
		for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->MultiBlend(SourceIndex, TargetIndex, InWeight, Trackers[i]); }
	}

	void FBlendOpsManager::EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const
	{
		for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->EndMultiBlend(TargetIndex, Trackers[i]); }
	}

	void FBlendOpsManager::Cleanup(FPCGExContext* InContext)
	{
		TSet<TSharedPtr<PCGExData::FBufferBase>> DisabledBuffers;
		for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->CompleteWork(DisabledBuffers); }

		for (const TSharedPtr<PCGExData::FBufferBase>& Buffer : DisabledBuffers)
		{
			// If disabled buffer does not exist on input, delete it entierely
			if (!Buffer->OutAttribute) { continue; }
			if (!TargetFacade->GetIn()->Metadata->HasAttribute(Buffer->OutAttribute->Name))
			{
				TargetFacade->GetOut()->Metadata->DeleteAttribute(Buffer->OutAttribute->Name);
				// TODO : Check types and make sure we're not deleting something
			}

			if (Buffer->InAttribute)
			{
				// Log a warning that can be silenced that we may have removed a valid attribute
			}
		}

		Operations->Empty();
		Operations.Reset();
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
