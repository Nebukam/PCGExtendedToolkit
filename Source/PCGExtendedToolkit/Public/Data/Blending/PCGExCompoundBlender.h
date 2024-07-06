// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExMT.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataFilter.h"

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
			PCGEX_SET_NUM(Attributes, InNum)
			PCGEX_SET_NUM(BlendOps, InNum)

			for (int i = 0; i < Diff; i++)
			{
				Attributes[Attributes.Num() - 1 - i] = nullptr;
				BlendOps[BlendOps.Num() - 1 - i] = nullptr;
			}
		}
	};

	class PCGEXTENDEDTOOLKIT_API FCompoundBlender final
	{
		friend class FPCGExCompoundBlendTask;
		friend class FPCGExCompoundedPointBlendTask;

	public:
		const FPCGExCarryOverDetails* CarryOverDetails;

		explicit FCompoundBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails);
		~FCompoundBlender();

		void AddSource(PCGExData::FFacade* InFacade);
		void AddSources(const TArray<PCGExData::FFacade*>& InFacades);

		void PrepareMerge(PCGExData::FFacade* TargetData, PCGExData::FIdxCompoundList* CompoundList);
		//void Merge(PCGExMT::FTaskManager* AsyncManager, PCGExData::FFacade* TargetData, PCGExData::FIdxCompoundList* CompoundList, const FPCGExDistanceDetails& InDistDetails);
		void MergeSingle(const int32 CompoundIndex, const FPCGExDistanceDetails& InDistanceDetails);

	protected:
		const FPCGExBlendingDetails* BlendingDetails = nullptr;

		TArray<FAttributeSourceMap*> AttributeSourceMaps;
		TMap<uint32, int32> IOIndices;
		TArray<PCGExData::FFacade*> Sources;

		PCGExData::FIdxCompoundList* CurrentCompoundList = nullptr;
		PCGExData::FFacade* CurrentTargetData = nullptr;
		FPropertiesBlender* PropertiesBlender = nullptr;
	};
}
