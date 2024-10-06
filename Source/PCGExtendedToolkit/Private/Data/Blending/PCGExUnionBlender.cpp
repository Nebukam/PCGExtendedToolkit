// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Data/Blending/PCGExUnionBlender.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "Data/Blending//PCGExDataBlendingOperations.h"
#include "Data/Blending/PCGExPropertiesBlender.h"


namespace PCGExDataBlending
{
	FUnionBlender::FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails):
		CarryOverDetails(InCarryOverDetails), BlendingDetails(InBlendingDetails)
	{
	}

	FUnionBlender::~FUnionBlender()
	{
	}

	void FUnionBlender::AddSource(const TSharedPtr<PCGExData::FFacade>& InFacade)
	{
		const int32 SourceIdx = Sources.Add(InFacade);
		const int32 NumSources = Sources.Num();
		IOIndices.Add(InFacade->Source->IOIndex, SourceIdx);

		UniqueTags.Append(InFacade->Source->Tags->RawTags);

		for (const TSharedPtr<FAttributeSourceMap>& SrcMap : AttributeSourceMaps) { SrcMap->SetNum(NumSources); }

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

			for (const TSharedPtr<FAttributeSourceMap>& SrcMap : AttributeSourceMaps)
			{
				if (SrcMap->Identity.Name == Identity.Name)
				{
					Map = SrcMap.Get();
					break;
				}
			}

			if (Map)
			{
				if (Identity.UnderlyingType != Map->Identity.UnderlyingType)
				{
					// Type mismatch, ignore for this source
					continue;
				}
			}
			else
			{
				Map = AttributeSourceMaps.Add_GetRef(MakeShared<FAttributeSourceMap>(Identity)).Get();
				Map->SetNum(NumSources);

				Map->DefaultValuesSource = SourceAttribute;

				if (PCGEx::IsPCGExAttribute(Identity.Name)) { Map->TargetBlendOp = CreateOperation(EPCGExDataBlendingType::Copy, Identity); }
				else { Map->TargetBlendOp = CreateOperation(BlendTypePtr, BlendingDetails->DefaultBlending, Identity); }
			}

			check(Map)

			Map->Attributes[SourceIdx] = SourceAttribute;
			Map->BlendOps[SourceIdx] = CreateOperation(BlendTypePtr, BlendingDetails->DefaultBlending, Identity);

			if (!SourceAttribute->AllowsInterpolation()) { Map->AllowsInterpolation = false; }
		}
	}

	void FUnionBlender::AddSources(const TArray<TSharedPtr<PCGExData::FFacade>>& InFacades)
	{
		for (TSharedPtr<PCGExData::FFacade> Facade : InFacades) { AddSource(Facade); }
	}

	void FUnionBlender::PrepareMerge(
		const TSharedPtr<PCGExData::FFacade>& TargetData,
		const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSet<FName>* IgnoreAttributeSet)
	{
		CurrentUnionMetadata = InUnionMetadata;
		CurrentTargetData = TargetData;

		const FPCGExPropertiesBlendingDetails PropertiesBlendingDetails = BlendingDetails->GetPropertiesBlendingDetails();
		PropertiesBlender = PropertiesBlendingDetails.HasNoBlending() ? nullptr : MakeUnique<FPropertiesBlender>(PropertiesBlendingDetails);

		// Create blending operations
		for (const TSharedPtr<FAttributeSourceMap>& SrcMap : AttributeSourceMaps)
		{
			SrcMap->Writer = nullptr;

			if (IgnoreAttributeSet && IgnoreAttributeSet->Contains(SrcMap->Identity.Name)) { continue; }

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					TSharedPtr<PCGExData::TBuffer<T>> Writer;
					if (const FPCGMetadataAttribute<T>* ExistingAttribute = CurrentTargetData->FindConstAttribute<T>(SrcMap->Identity.Name))
					{
						Writer = CurrentTargetData->GetWritable<T>(ExistingAttribute, false);
					}
					else
					{
						Writer = CurrentTargetData->GetWritable<T>(static_cast<FPCGMetadataAttribute<T>*>(SrcMap->DefaultValuesSource), false);
					}

					SrcMap->Writer = Writer;

					for (int i = 0; i < Sources.Num(); i++)
					{
						if (const TSharedPtr<FDataBlendingOperationBase>& SrcOp = SrcMap->BlendOps[i]) { SrcOp->PrepareForData(Writer, Sources[i]); }
					}

					SrcMap->TargetBlendOp->PrepareForData(Writer, CurrentTargetData, PCGExData::ESource::Out);
				});
		}
	}

	void FUnionBlender::MergeSingle(const int32 UnionIndex, const FPCGExDistanceDetails& InDistanceDetails)
	{
		MergeSingle(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), InDistanceDetails);
	}

	void FUnionBlender::MergeSingle(const int32 WriteIndex, const PCGExData::FUnionData* InUnionData, const FPCGExDistanceDetails& InDistanceDetails)
	{
		TArray<int32> IdxIO;
		TArray<int32> IdxPt;
		TArray<double> Weights;

		FPCGPoint& Target = CurrentTargetData->Source->GetMutablePoint(WriteIndex);

		InUnionData->ComputeWeights(
			Sources, IOIndices,
			Target, InDistanceDetails,
			IdxIO, IdxPt, Weights);

		const int32 UnionCount = IdxPt.Num();

		if (UnionCount == 0) { return; }

		// Blend Properties


		// Blend Properties
		BlendProperties(Target, IdxIO, IdxPt, Weights);

		// Blend Attributes

		for (const TSharedPtr<FAttributeSourceMap>& SrcMap : AttributeSourceMaps)
		{
			SrcMap->TargetBlendOp->PrepareOperation(WriteIndex);

			int32 ValidUnions = 0;
			double TotalWeight = 0;

			for (int k = 0; k < UnionCount; k++)
			{
				const TSharedPtr<FDataBlendingOperationBase>& Operation = SrcMap->BlendOps[IdxIO[k]];
				if (!Operation) { continue; }

				const double Weight = Weights[k];

				Operation->DoOperation(
					WriteIndex, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]),
					WriteIndex, Weight, k == 0);

				ValidUnions++;
				TotalWeight += Weight;
			}

			if (ValidUnions == 0) { continue; } // No valid attribute to merge on any union source

			SrcMap->TargetBlendOp->FinalizeOperation(WriteIndex, ValidUnions, TotalWeight);
		}
	}

	// Soft blending

	void FUnionBlender::PrepareSoftMerge(
		const TSharedPtr<PCGExData::FFacade>& TargetData,
		const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata,
		const TSet<FName>* IgnoreAttributeSet)
	{
		CurrentUnionMetadata = InUnionMetadata;
		CurrentTargetData = TargetData;

		const FPCGExPropertiesBlendingDetails PropertiesBlendingDetails = BlendingDetails->GetPropertiesBlendingDetails();
		PropertiesBlender = PropertiesBlendingDetails.HasNoBlending() ? nullptr : MakeUnique<FPropertiesBlender>(PropertiesBlendingDetails);

		CarryOverDetails->Reduce(UniqueTags);

		// Create blending operations
		for (const TSharedPtr<FAttributeSourceMap>& SrcMap : AttributeSourceMaps)
		{
			SrcMap->Writer = nullptr;

			if (IgnoreAttributeSet && IgnoreAttributeSet->Contains(SrcMap->Identity.Name)) { continue; }

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(SrcMap->Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					for (int i = 0; i < Sources.Num(); i++)
					{
						if (const TSharedPtr<FDataBlendingOperationBase>& SrcOp = SrcMap->BlendOps[i]) { SrcOp->SoftPrepareForData(CurrentTargetData, Sources[i]); }
					}

					SrcMap->TargetBlendOp->SoftPrepareForData(CurrentTargetData, CurrentTargetData, PCGExData::ESource::Out);
				});
		}

		// Strip existing attribute names that may conflict with tags

		TArray<FName> ReservedNames;
		TArray<EPCGMetadataTypes> ReservedTypes;
		TargetData->Source->GetOut()->Metadata->GetAttributes(ReservedNames, ReservedTypes);
		for (FName ReservedName : ReservedNames) { UniqueTags.Remove(ReservedName.ToString()); }
		UniqueTagsList = UniqueTags.Array();
		TagAttributes.Reserve(UniqueTagsList.Num());

		for (FString TagName : UniqueTagsList)
		{
			TagAttributes.Add(TargetData->Source->FindOrCreateAttribute<bool>(FName(TagName), false));
		}
	}

	void FUnionBlender::SoftMergeSingle(const int32 UnionIndex, const FPCGExDistanceDetails& InDistanceDetails)
	{
		SoftMergeSingle(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), InDistanceDetails);
	}

	void FUnionBlender::SoftMergeSingle(const int32 UnionIndex, const PCGExData::FUnionData* InUnionData, const FPCGExDistanceDetails& InDistanceDetails)
	{
		TArray<int32> IdxIO;
		TArray<int32> IdxPt;
		TArray<double> Weights;
		TArray<bool> InheritedTags;

		FPCGPoint& Target = CurrentTargetData->Source->GetMutablePoint(UnionIndex);

		InUnionData->ComputeWeights(
			Sources, IOIndices,
			Target, InDistanceDetails,
			IdxIO, IdxPt, Weights);

		const int32 NumUnions = IdxPt.Num();
		if (NumUnions == 0) { return; }

		InheritedTags.Init(false, TagAttributes.Num());

		// Blend Properties
		BlendProperties(Target, IdxIO, IdxPt, Weights);

		// Blend Attributes
		for (const TSharedPtr<FAttributeSourceMap>& SrcMap : AttributeSourceMaps)
		{
			SrcMap->TargetBlendOp->PrepareOperation(Target.MetadataEntry);

			int32 ValidUnions = 0;
			double TotalWeight = 0;

			for (int k = 0; k < NumUnions; k++)
			{
				const TSharedPtr<FDataBlendingOperationBase>& Operation = SrcMap->BlendOps[IdxIO[k]];
				if (!Operation) { continue; }

				const double Weight = Weights[k];

				Operation->DoOperation(
					Target.MetadataEntry, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]).MetadataEntry,
					Target.MetadataEntry, Weight, k == 0);

				ValidUnions++;
				TotalWeight += Weight;
			}

			if (ValidUnions == 0) { continue; } // No valid attribute to merge on any union source

			SrcMap->TargetBlendOp->FinalizeOperation(Target.MetadataEntry, ValidUnions, TotalWeight);
		}

		// Tag flags

		for (const int32 IOI : IdxIO)
		{
			for (int i = 0; i < TagAttributes.Num(); i++)
			{
				if (Sources[IOI]->Source->Tags->IsTagged(UniqueTagsList[i])) { InheritedTags[i] = true; }
			}
		}

		for (int i = 0; i < TagAttributes.Num(); i++) { TagAttributes[i]->SetValue(Target.MetadataEntry, InheritedTags[i]); }
	}

	void FUnionBlender::BlendProperties(FPCGPoint& TargetPoint, TArray<int32>& IdxIO, TArray<int32>& IdxPt, TArray<double>& Weights)
	{
		if (!PropertiesBlender) { return; }

		PropertiesBlender->PrepareBlending(TargetPoint, TargetPoint);

		const int32 NumUnions = IdxIO.Num();
		double TotalWeight = 0;
		for (int k = 0; k < NumUnions; k++)
		{
			const double Weight = Weights[k];
			PropertiesBlender->Blend(TargetPoint, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]), TargetPoint, Weight);
			TotalWeight += Weight;
		}

		PropertiesBlender->CompleteBlending(TargetPoint, NumUnions, TotalWeight);
	}
}
