// Fill out your copyright notice in the Description page of Project Settings.


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Data/Blending/PCGExCompoundBlender.h"

#include "Data/PCGExData.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	FCompoundBlender::FCompoundBlender(FPCGExBlendingSettings* InBlendingSettings)
	{
		BlendingSettings = InBlendingSettings;
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

	void FCompoundBlender::AddSource(PCGExData::FPointIO& InData)
	{
		const int32 SourceIdx = Sources.Add(&InData);
		const int32 NumSources = Sources.Num();
		IOIndices.Add(InData.IOIndex, SourceIdx);

		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps) { SrcMap->SetNum(NumSources); }

		TArray<PCGEx::FAttributeIdentity> SourceAttributes;
		PCGEx::FAttributeIdentity::Get(InData.GetIn()->Metadata, SourceAttributes);
		BlendingSettings->Filter(SourceAttributes);

		UPCGMetadata* SourceMetadata = InData.GetIn()->Metadata;

		for (const PCGEx::FAttributeIdentity& Identity : SourceAttributes)
		{
			FPCGMetadataAttributeBase* SourceAttribute = SourceMetadata->GetMutableAttribute(Identity.Name);
			if (!SourceAttribute) { continue; }

			FAttributeSourceMap* Map = nullptr;
			const EPCGExDataBlendingType* BlendTypePtr = BlendingSettings->AttributesOverrides.Find(Identity.Name);

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
				Map->TargetBlendOp = CreateOperation(BlendTypePtr ? *BlendTypePtr : BlendingSettings->DefaultBlending, Identity);
				AttributeSourceMaps.Add(Map);
			}

			Map->Attributes[SourceIdx] = SourceAttribute;
			Map->BlendOps[SourceIdx] = CreateOperation(BlendTypePtr ? *BlendTypePtr : BlendingSettings->DefaultBlending, Identity);

			if (!SourceAttribute->AllowsInterpolation()) { Map->AllowsInterpolation = false; }
		}

		InData.CreateInKeys();
	}

	void FCompoundBlender::AddSources(const PCGExData::FPointIOCollection& InDataGroup)
	{
		for (PCGExData::FPointIO* IOPair : InDataGroup.Pairs) { AddSource(*IOPair); }
	}

	void FCompoundBlender::PrepareMerge(
		PCGExData::FPointIO* TargetData,
		PCGExData::FIdxCompoundList* CompoundList)
	{
		CurrentCompoundList = CompoundList;
		CurrentTargetData = TargetData;

		PCGEX_DELETE(PropertiesBlender)
		PropertiesBlender = new FPropertiesBlender(BlendingSettings->GetPropertiesBlendingSettings());

		CurrentTargetData->CreateOutKeys();

		// Create blending operations
		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			PCGEX_DELETE(SrcMap->Writer)

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					bool bAttributeWasPresent = TargetData->GetOut()->Metadata->HasAttribute(SrcMap->Identity.Name);

					PCGEx::TFAttributeWriter<T>* Writer = new PCGEx::TFAttributeWriter<T>(SrcMap->Identity.Name, T{}, SrcMap->AllowsInterpolation);
					Writer->BindAndGet(*CurrentTargetData);

					SrcMap->Writer = Writer;

					for (int i = 0; i < Sources.Num(); i++)
					{
						if (FDataBlendingOperationBase* SrcOp = SrcMap->BlendOps[i]) { SrcOp->PrepareForData(Writer, *Sources[i]); }
					}

					SrcMap->TargetBlendOp->PrepareForData(Writer, *TargetData, PCGExData::ESource::Out);
				});
		}
	}

	void FCompoundBlender::Merge(
		FPCGExAsyncManager* AsyncManager,
		PCGExData::FPointIO* TargetData,
		PCGExData::FIdxCompoundList* CompoundList,
		const FPCGExDistanceSettings& DistSettings)
	{
		PrepareMerge(TargetData, CompoundList);
		FCompoundBlender* Merger = this;
		AsyncManager->Start<FPCGExCompoundBlendTask>(-1, TargetData, Merger, DistSettings);
	}

	void FCompoundBlender::MergeSingle(const int32 CompoundIndex, const FPCGExDistanceSettings& DistSettings)
	{
		PCGExData::FIdxCompound* Compound = (*CurrentCompoundList)[CompoundIndex];
		Compound->ComputeWeights(Sources, CurrentTargetData->GetOutPoint(CompoundIndex), DistSettings);

		const int32 NumCompounded = Compound->Num();

		int32 ValidCompounds = 0;
		double TotalWeight = 0;

		// Blend Properties

		FPCGPoint& Target = CurrentTargetData->GetMutablePoint(CompoundIndex);
		PropertiesBlender->PrepareBlending(Target, Target);

		for (int k = 0; k < NumCompounded; k++)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64((*Compound)[k], IOIndex, PtIndex);

			const int32* IOIdx = IOIndices.Find(IOIndex);
			if (!IOIdx) { continue; }

			const double Weight = Compound->Weights[k];

			PropertiesBlender->Blend(Target, Sources[*IOIdx]->GetInPoint(PtIndex), Target, Weight);

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
				PCGEx::H64((*Compound)[k], IOIndex, PtIndex);

				const int32* IOIdx = IOIndices.Find(IOIndex);
				if (!IOIdx) { continue; }

				const FDataBlendingOperationBase* Operation = SrcMap->BlendOps[*IOIdx];
				if (!Operation) { continue; }

				const double Weight = Compound->Weights[k];

				Operation->DoOperation(
					CompoundIndex, Sources[*IOIdx]->GetInPoint(PtIndex),
					CompoundIndex, Weight, k == 0);

				ValidCompounds++;
				TotalWeight += Weight;
			}

			if (ValidCompounds == 0) { continue; } // No valid attribute to merge on any compounded source

			SrcMap->TargetBlendOp->FinalizeOperation(CompoundIndex, ValidCompounds, TotalWeight);
		}
	}

	void FCompoundBlender::Write()
	{
		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					static_cast<PCGEx::TFAttributeWriter<T>*>(SrcMap->Writer)->Write();
					PCGEX_DELETE(SrcMap->Writer)
				});
		}
	}

	bool FPCGExCompoundBlendTask::ExecuteTask()
	{
		for (int i = 0; i < Merger->CurrentCompoundList->Compounds.Num(); i++)
		{
			Manager->Start<FPCGExCompoundedPointBlendTask>(i, PointIO, Merger, DistSettings);
		}
		return true;
	}

	bool FPCGExCompoundedPointBlendTask::ExecuteTask()
	{
		Merger->MergeSingle(TaskIndex, DistSettings);
		return true;
	}
}
