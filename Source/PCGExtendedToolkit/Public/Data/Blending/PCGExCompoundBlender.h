// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExMT.h"
#include "PCGExSettings.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/PCGExAttributeHelpers.h"

namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API FCompoundBlender
	{
		friend class FPCGExCompoundBlendTask;
		friend class FPCGExCompoundedPointBlendTask;

	public:
		explicit FCompoundBlender(FPCGExBlendingSettings* InBlendingSettings);
		virtual ~FCompoundBlender();

		void Cleanup();

		void AddSource(PCGExData::FPointIO& InData);
		void AddSources(const PCGExData::FPointIOCollection& InDataGroup);
		void Merge(FPCGExAsyncManager* AsyncManager, PCGExData::FPointIO* TargetData, PCGExData::FIdxCompoundList* CompoundList, const FPCGExDistanceSettings& DistSettings);
		void Write();

	protected:
		FPCGExBlendingSettings* BlendingSettings = nullptr;

		TMap<FName, PCGEx::FAttributeIdentity> UniqueIdentitiesMap;
		TMap<FName, bool> AllowsInterpolation;
		TArray<PCGEx::FAttributeIdentity> UniqueIdentitiesList;

		TArray<PCGExData::FPointIO*> Sources;
		TArray<TMap<FName, FPCGMetadataAttributeBase*>*> PerSourceAttMap;
		TArray<TMap<FName, FDataBlendingOperationBase*>*> PerSourceOpsMap;
		TArray<TArray<FDataBlendingOperationBase*>> CachedOperations;

		TArray<PCGEx::FAAttributeIO*> Writers;
	};

	class PCGEXTENDEDTOOLKIT_API FPCGExCompoundBlendTask : public FPCGExNonAbandonableTask
	{
	public:
		FPCGExCompoundBlendTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                        FCompoundBlender* InMerger,
		                        PCGExData::FIdxCompoundList* InCompoundList,
		                        const FPCGExDistanceSettings& InDistSettings)
			: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			  Merger(InMerger),
			  CompoundList(InCompoundList),
			  DistSettings(InDistSettings)

		{
		}

		FCompoundBlender* Merger = nullptr;
		PCGExData::FIdxCompoundList* CompoundList = nullptr;
		FPCGExDistanceSettings DistSettings;


		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FPCGExCompoundedPointBlendTask : public FPCGExNonAbandonableTask
	{
	public:
		FPCGExCompoundedPointBlendTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                               FCompoundBlender* InMerger,
		                               PCGExData::FIdxCompoundList* InCompoundList,
		                               const FPCGExDistanceSettings& InDistSettings)
			: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			  Merger(InMerger),
			  CompoundList(InCompoundList),
			  DistSettings(InDistSettings)
		{
		}

		FCompoundBlender* Merger = nullptr;
		PCGExData::FIdxCompoundList* CompoundList = nullptr;
		FPCGExDistanceSettings DistSettings;

		virtual bool ExecuteTask() override;
	};
}
