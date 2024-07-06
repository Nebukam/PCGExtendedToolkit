// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Data/Blending/PCGExCompoundBlender.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "Data/Blending//PCGExDataBlendingOperations.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	FCompoundBlender::FCompoundBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails):
		CarryOverDetails(InCarryOverDetails), BlendingDetails(InBlendingDetails)
	{
		Sources.Empty();
		IOIndices.Empty();
		AttributeSourceMaps.Empty();
	}

	FCompoundBlender::~FCompoundBlender()
	{
		Sources.Empty();
		PCGEX_DELETE(PropertiesBlender)
		PCGEX_DELETE_TARRAY(AttributeSourceMaps)
	}

	void FCompoundBlender::AddSource(PCGExData::FFacade* InFacade)
	{
		const int32 SourceIdx = Sources.Add(InFacade);
		const int32 NumSources = Sources.Num();
		IOIndices.Add(InFacade->Source->IOIndex, SourceIdx);

		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps) { SrcMap->SetNum(NumSources); }

		TArray<PCGEx::FAttributeIdentity> SourceAttributes;
		PCGEx::FAttributeIdentity::Get(InFacade->GetIn()->Metadata, SourceAttributes);
		CarryOverDetails->Filter(SourceAttributes);
		BlendingDetails->Filter(SourceAttributes);

		UPCGMetadata* SourceMetadata = InFacade->GetIn()->Metadata;

		for (const PCGEx::FAttributeIdentity& Identity : SourceAttributes)
		{
			FPCGMetadataAttributeBase* SourceAttribute = SourceMetadata->GetMutableAttribute(Identity.Name);
			if (!SourceAttribute) { continue; }

			FAttributeSourceMap* Map = nullptr;
			const EPCGExDataBlendingType* BlendTypePtr = BlendingDetails->AttributesOverrides.Find(Identity.Name);

			// Search for an existing attribute map

			for (FAttributeSourceMap* SrcMap : AttributeSourceMaps)
			{
				if (SrcMap->Identity.Name == Identity.Name)
				{
					Map = SrcMap;
					break;
				}
			}

			if (Map)
			{
				if (Identity.UnderlyingType != Map->Identity.UnderlyingType)
				{
					// Type mismatch, ignore for this source
					//TODO : Support broadcasting
					continue;
				}
			}
			else
			{
				Map = new FAttributeSourceMap(Identity);
				Map->SetNum(NumSources);

				if (PCGEx::IsPCGExAttribute(Identity.Name)) { Map->TargetBlendOp = CreateOperation(EPCGExDataBlendingType::Copy, Identity); }
				else { Map->TargetBlendOp = CreateOperation(BlendTypePtr ? *BlendTypePtr : BlendingDetails->DefaultBlending, Identity); }

				AttributeSourceMaps.Add(Map);
			}

			Map->Attributes[SourceIdx] = SourceAttribute;
			Map->BlendOps[SourceIdx] = CreateOperation(BlendTypePtr ? *BlendTypePtr : BlendingDetails->DefaultBlending, Identity);

			if (!SourceAttribute->AllowsInterpolation()) { Map->AllowsInterpolation = false; }
		}

		InFacade->Source->CreateInKeys();
	}

	void FCompoundBlender::AddSources(const TArray<PCGExData::FFacade*>& InFacades)
	{
		for (PCGExData::FFacade* Facade : InFacades) { AddSource(Facade); }
	}

	void FCompoundBlender::PrepareMerge(
		PCGExData::FFacade* TargetData,
		PCGExData::FIdxCompoundList* CompoundList)
	{
		CurrentCompoundList = CompoundList;
		CurrentTargetData = TargetData;

		PCGEX_DELETE(PropertiesBlender)
		PropertiesBlender = new FPropertiesBlender(BlendingDetails->GetPropertiesBlendingDetails());

		CurrentTargetData->Source->CreateOutKeys();

		// Create blending operations
		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			PCGEX_DELETE(SrcMap->Writer)

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					PCGEx::TFAttributeWriter<T>* Writer;
					if (const FPCGMetadataAttribute<T>* ExistingAttribute = CurrentTargetData->FindConstAttribute<T>(SrcMap->Identity.Name))
					{
						Writer = CurrentTargetData->GetOrCreateWriter<T>(ExistingAttribute, false);
					}
					else
					{
						Writer = CurrentTargetData->GetOrCreateWriter<T>(SrcMap->Identity.Name, T{}, SrcMap->AllowsInterpolation, false);
					}

					SrcMap->Writer = Writer;

					for (int i = 0; i < Sources.Num(); i++)
					{
						if (FDataBlendingOperationBase* SrcOp = SrcMap->BlendOps[i]) { SrcOp->PrepareForData(Writer, Sources[i]); }
					}

					SrcMap->TargetBlendOp->PrepareForData(Writer, TargetData, PCGExData::ESource::Out);
				});
		}
	}

	void FCompoundBlender::MergeSingle(const int32 CompoundIndex, const FPCGExDistanceDetails& InDistanceDetails)
	{
		PCGExData::FIdxCompound* Compound = (*CurrentCompoundList)[CompoundIndex];

		TArray<uint64> CompoundHashes;
		TArray<double> Weights;

		Compound->ComputeWeights(
			Sources, IOIndices,
			CurrentTargetData->Source->GetOutPoint(CompoundIndex), InDistanceDetails,
			CompoundHashes, Weights);

		const int32 NumCompounded = CompoundHashes.Num();

		int32 ValidCompounds = 0;
		double TotalWeight = 0;

		// Blend Properties

		FPCGPoint& Target = CurrentTargetData->Source->GetMutablePoint(CompoundIndex);
		PropertiesBlender->PrepareBlending(Target, Target);

		for (int k = 0; k < NumCompounded; k++)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64(CompoundHashes[k], IOIndex, PtIndex);

			const int32* IOIdx = IOIndices.Find(IOIndex);
			if (!IOIdx) { continue; }

			const double Weight = Weights[k];

			PropertiesBlender->Blend(Target, Sources[*IOIdx]->Source->GetInPoint(PtIndex), Target, Weight);

			ValidCompounds++;
			TotalWeight += Weight;
		}

		PropertiesBlender->CompleteBlending(Target, ValidCompounds, TotalWeight);

		// Blend Attributes

		for (const FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			SrcMap->TargetBlendOp->PrepareOperation(CompoundIndex);

			ValidCompounds = 0;
			TotalWeight = 0;

			for (int k = 0; k < NumCompounded; k++)
			{
				uint32 IOIndex;
				uint32 PtIndex;
				PCGEx::H64(CompoundHashes[k], IOIndex, PtIndex);

				const int32* IOIdx = IOIndices.Find(IOIndex);
				if (!IOIdx) { continue; }

				const FDataBlendingOperationBase* Operation = SrcMap->BlendOps[*IOIdx];
				if (!Operation) { continue; }

				const double Weight = Weights[k];

				Operation->DoOperation(
					CompoundIndex, Sources[*IOIdx]->Source->GetInPoint(PtIndex),
					CompoundIndex, Weight, k == 0);

				ValidCompounds++;
				TotalWeight += Weight;
			}

			if (ValidCompounds == 0) { continue; } // No valid attribute to merge on any compounded source

			SrcMap->TargetBlendOp->FinalizeOperation(CompoundIndex, ValidCompounds, TotalWeight);
		}
	}
}
