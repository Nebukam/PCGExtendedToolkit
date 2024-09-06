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

				Map->DefaultValuesSource = SourceAttribute; // TODO : Find a better way to choose this?

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
		const FPCGExPropertiesBlendingDetails PropertiesBlendingDetails = BlendingDetails->GetPropertiesBlendingDetails();
		PropertiesBlender = PropertiesBlendingDetails.HasNoBlending() ? nullptr : new FPropertiesBlender(PropertiesBlendingDetails);

		CurrentTargetData->Source->CreateOutKeys();

		// Create blending operations
		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			SrcMap->Writer = nullptr;

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					PCGEx::TAttributeWriter<T>* Writer;
					if (const FPCGMetadataAttribute<T>* ExistingAttribute = CurrentTargetData->FindConstAttribute<T>(SrcMap->Identity.Name))
					{
						Writer = CurrentTargetData->GetWriter<T>(ExistingAttribute, false);
					}
					else
					{
						Writer = CurrentTargetData->GetWriter<T>(static_cast<FPCGMetadataAttribute<T>*>(SrcMap->DefaultValuesSource), false);
					}

					SrcMap->Writer = Writer;

					for (int i = 0; i < Sources.Num(); i++)
					{
						if (FDataBlendingOperationBase* SrcOp = SrcMap->BlendOps[i]) { SrcOp->PrepareForData(Writer, Sources[i]); }
					}

					SrcMap->TargetBlendOp->PrepareForData(Writer, CurrentTargetData, PCGExData::ESource::Out);
				});
		}
	}

	void FCompoundBlender::MergeSingle(const int32 CompoundIndex, const FPCGExDistanceDetails& InDistanceDetails)
	{
		PCGExData::FIdxCompound* Compound = (*CurrentCompoundList)[CompoundIndex];

		TArray<int32> IdxIO;
		TArray<int32> IdxPt;
		TArray<double> Weights;

		Compound->ComputeWeights(
			Sources, IOIndices,
			CurrentTargetData->Source->GetOutPoint(CompoundIndex), InDistanceDetails,
			IdxIO, IdxPt, Weights);

		const int32 NumCompounded = IdxPt.Num();

		if (NumCompounded == 0) { return; }

		// Blend Properties

		FPCGPoint& Target = CurrentTargetData->Source->GetMutablePoint(CompoundIndex);

		// Blend Properties
		BlendProperties(Target, IdxIO, IdxPt, Weights);

		// Blend Attributes

		for (const FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			SrcMap->TargetBlendOp->PrepareOperation(CompoundIndex);

			int32 ValidCompounds = 0;
			double TotalWeight = 0;

			for (int k = 0; k < NumCompounded; k++)
			{
				const FDataBlendingOperationBase* Operation = SrcMap->BlendOps[IdxIO[k]];
				if (!Operation) { continue; }

				const double Weight = Weights[k];

				Operation->DoOperation(
					CompoundIndex, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]),
					CompoundIndex, Weight, k == 0);

				ValidCompounds++;
				TotalWeight += Weight;
			}

			if (ValidCompounds == 0) { continue; } // No valid attribute to merge on any compounded source

			SrcMap->TargetBlendOp->FinalizeOperation(CompoundIndex, ValidCompounds, TotalWeight);
		}
	}

	// Soft blending

	void FCompoundBlender::PrepareSoftMerge(
		PCGExData::FFacade* TargetData,
		PCGExData::FIdxCompoundList* CompoundList)
	{
		CurrentCompoundList = CompoundList;
		CurrentTargetData = TargetData;

		PCGEX_DELETE(PropertiesBlender)
		const FPCGExPropertiesBlendingDetails PropertiesBlendingDetails = BlendingDetails->GetPropertiesBlendingDetails();
		PropertiesBlender = PropertiesBlendingDetails.HasNoBlending() ? nullptr : new FPropertiesBlender(PropertiesBlendingDetails);

		CurrentTargetData->Source->CreateOutKeys();

		// Create blending operations
		for (FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			SrcMap->Writer = nullptr;

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					// TODO : Only prepare for data if the matching IO exists

					for (int i = 0; i < Sources.Num(); i++)
					{
						if (FDataBlendingOperationBase* SrcOp = SrcMap->BlendOps[i]) { SrcOp->SoftPrepareForData(CurrentTargetData, Sources[i]); }
					}

					SrcMap->TargetBlendOp->SoftPrepareForData(CurrentTargetData, CurrentTargetData, PCGExData::ESource::Out);
				});
		}
	}

	void FCompoundBlender::SoftMergeSingle(const int32 CompoundIndex, const FPCGExDistanceDetails& InDistanceDetails)
	{
		PCGExData::FIdxCompound* Compound = (*CurrentCompoundList)[CompoundIndex];

		TArray<int32> IdxIO;
		TArray<int32> IdxPt;
		TArray<double> Weights;

		Compound->ComputeWeights(
			Sources, IOIndices,
			CurrentTargetData->Source->GetOutPoint(CompoundIndex), InDistanceDetails,
			IdxIO, IdxPt, Weights);

		const int32 NumCompounded = IdxPt.Num();

		if (NumCompounded == 0) { return; }

		FPCGPoint& Target = CurrentTargetData->Source->GetMutablePoint(CompoundIndex);

		// Blend Properties
		BlendProperties(Target, IdxIO, IdxPt, Weights);

		// Blend Attributes

		for (const FAttributeSourceMap* SrcMap : AttributeSourceMaps)
		{
			SrcMap->TargetBlendOp->PrepareOperation(Target.MetadataEntry);

			int32 ValidCompounds = 0;
			double TotalWeight = 0;

			for (int k = 0; k < NumCompounded; k++)
			{
				const FDataBlendingOperationBase* Operation = SrcMap->BlendOps[IdxIO[k]];
				if (!Operation) { continue; }

				const double Weight = Weights[k];

				Operation->DoOperation(
					Target.MetadataEntry, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]).MetadataEntry,
					Target.MetadataEntry, Weight, k == 0);

				ValidCompounds++;
				TotalWeight += Weight;
			}

			if (ValidCompounds == 0) { continue; } // No valid attribute to merge on any compounded source

			SrcMap->TargetBlendOp->FinalizeOperation(Target.MetadataEntry, ValidCompounds, TotalWeight);
		}
	}

	void FCompoundBlender::BlendProperties(FPCGPoint& TargetPoint, TArray<int32>& IdxIO, TArray<int32>& IdxPt, TArray<double>& Weights)
	{
		if (!PropertiesBlender) { return; }

		PropertiesBlender->PrepareBlending(TargetPoint, TargetPoint);

		const int32 NumCompounded = IdxIO.Num();
		double TotalWeight = 0;
		for (int k = 0; k < NumCompounded; k++)
		{
			const double Weight = Weights[k];
			PropertiesBlender->Blend(TargetPoint, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]), TargetPoint, Weight);
			TotalWeight += Weight;
		}

		PropertiesBlender->CompleteBlending(TargetPoint, NumCompounded, TotalWeight);
	}
}
