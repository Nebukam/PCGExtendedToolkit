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
	struct FPropertiesBlender;
}

namespace PCGExDataBlending
{
	struct PCGEXTENDEDTOOLKIT_API FAttributeSourceMap
	{
		TArray<FPCGMetadataAttributeBase*> Attributes;
		TArray<FDataBlendingOperationBase*> BlendOps;
		PCGEx::FAttributeIdentity Identity;
		bool AllowsInterpolation = true;

		FDataBlendingOperationBase* TargetBlendOp = nullptr;
		PCGEx::FAAttributeIO* Writer = nullptr;

		explicit FAttributeSourceMap(const PCGEx::FAttributeIdentity InIdentity)
			: Identity(InIdentity)
		{
			Attributes.Empty();
			BlendOps.Empty();
		}

		~FAttributeSourceMap()
		{
			PCGEX_DELETE(TargetBlendOp)

			for (int i = 0; i < BlendOps.Num(); i++)
			{
				const FDataBlendingOperationBase* Op = BlendOps[i];
				if (!Op) { continue; }
				PCGEX_DELETE(Op)
			}

			Attributes.Empty();
			BlendOps.Empty();
		}

		template <typename T>
		FPCGMetadataAttribute<T>* Get(const int32 SourceIndex)
		{
			FPCGMetadataAttributeBase* Att = Attributes[SourceIndex];
			if (!Att) { return nullptr; }
			return static_cast<FPCGMetadataAttribute<T>*>(Att);
		}

		void SetNum(const int32 InNum)
		{
			const int32 Diff = InNum - Attributes.Num();
			Attributes.SetNum(InNum);
			BlendOps.SetNum(InNum);

			for (int i = 0; i < Diff; i++)
			{
				Attributes[Attributes.Num() - 1 - i] = nullptr;
				BlendOps[BlendOps.Num() - 1 - i] = nullptr;
			}
		}
	};

	class PCGEXTENDEDTOOLKIT_API FCompoundBlender
	{
		friend class FPCGExCompoundBlendTask;
		friend class FPCGExCompoundedPointBlendTask;

	public:
		explicit FCompoundBlender(FPCGExBlendingSettings* InBlendingSettings);
		virtual ~FCompoundBlender();

		void AddSource(PCGExData::FPointIO& InData);
		void AddSources(const PCGExData::FPointIOCollection& InDataGroup);

		void PrepareMerge(PCGExData::FPointIO* TargetData, PCGExData::FIdxCompoundList* CompoundList);
		void Merge(FPCGExAsyncManager* AsyncManager, PCGExData::FPointIO* TargetData, PCGExData::FIdxCompoundList* CompoundList, const FPCGExDistanceSettings& DistSettings);
		void MergeSingle(const int32 CompoundIndex, const FPCGExDistanceSettings& DistSettings);

		void Write();

	protected:
		FPCGExBlendingSettings* BlendingSettings = nullptr;

		TArray<FAttributeSourceMap*> AttributeSourceMaps;
		TMap<int32, int32> IOIndices;
		TArray<PCGExData::FPointIO*> Sources;

		PCGExData::FIdxCompoundList* CurrentCompoundList = nullptr;
		PCGExData::FPointIO* CurrentTargetData = nullptr;
		FPropertiesBlender* PropertiesBlender = nullptr;
	};

	class PCGEXTENDEDTOOLKIT_API FPCGExCompoundBlendTask : public FPCGExNonAbandonableTask
	{
	public:
		FPCGExCompoundBlendTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                        FCompoundBlender* InMerger,
		                        const FPCGExDistanceSettings& InDistSettings)
			: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			  Merger(InMerger),
			  DistSettings(InDistSettings)

		{
		}

		FCompoundBlender* Merger = nullptr;
		FPCGExDistanceSettings DistSettings;


		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FPCGExCompoundedPointBlendTask : public FPCGExNonAbandonableTask
	{
	public:
		FPCGExCompoundedPointBlendTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                               FCompoundBlender* InMerger,
		                               const FPCGExDistanceSettings& InDistSettings)
			: FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			  Merger(InMerger),
			  DistSettings(InDistSettings)
		{
		}

		FCompoundBlender* Merger = nullptr;
		FPCGExDistanceSettings DistSettings;

		virtual bool ExecuteTask() override;
	};
}
