// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Data/Blending/PCGExUnionBlender.h"

#include "PCGExDetailsData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "Data/PCGExUnionData.h"

namespace PCGExDataBlending
{
	FMultiSourceBlender::FMultiSourceBlender(const PCGEx::FAttributeIdentity& InIdentity)
		: Identity(InIdentity)
	{
	}

	bool FMultiSourceBlender::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetData, TArray<TSharedPtr<PCGExData::FFacade>>& Sources)
	{
		check(InTargetData);

		if (Header.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (Identity.UnderlyingType == EPCGMetadataTypes::Unknown)
			{
				// Unknown attribute type
				return false;
			}

			TSharedPtr<PCGExData::FBufferBase> InitializationBuffer = nullptr;

			if (const FPCGMetadataAttributeBase* ExistingAttribute = InTargetData->FindConstAttribute(Identity.Name);
				ExistingAttribute && ExistingAttribute->GetTypeId() == static_cast<int16>(Identity.UnderlyingType))
			{
				// This attribute exists on target already
				InitializationBuffer = InTargetData->GetWritable(Identity.UnderlyingType, ExistingAttribute, PCGExData::EBufferInit::Inherit);
			}
			else
			{
				// This attribute needs to be initialized
				InitializationBuffer = InTargetData->GetWritable(Identity.UnderlyingType, DefaultValue, PCGExData::EBufferInit::New);
			}

			if (!InitializationBuffer)
			{
				return false;
			}

			bool bError = false;
			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					for (int i = 0; i < Sources.Num(); i++)
					{
						TSharedPtr<PCGExData::FFacade> Source = Sources[i];
						if (!Source) { continue; }

						TSharedPtr<FProxyDataBlender> SubBlender = PCGExDataBlending::CreateProxyBlender<T>(Header.Blending);
						SubBlenders[i] = SubBlender;

						if (!SubBlender->InitFromHeader(InContext, Header, InTargetData, Sources[i], PCGExData::EIOSide::In))
						{
							bError = true;
							return;
						}

						bError = MainBlender->InitFromHeader(InContext, Header, InTargetData, InTargetData, PCGExData::EIOSide::Out);
					}
				});

			return !bError;
		}
		else if (Header.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
		{
			// TODO : Init properties
		}
	}

	void FMultiSourceBlender::SoftInit(const TSharedPtr<PCGExData::FFacade>& InTargetData, TArray<TSharedPtr<PCGExData::FFacade>>& Sources)
	{
		// TODO
		/*
		check(InTargetData);

		Buffer = nullptr;

		for (int i = 0; i < Sources.Num(); i++)
		{
			if (const TSharedPtr<FDataBlendingProcessorBase>& SrcProc = SubBlenders[i])
			{
				SrcProc->SoftPrepareForData(InTargetData, Sources[i]);
			}
		}

		MainBlender->SoftPrepareForData(InTargetData, InTargetData, PCGExData::EIOSide::Out);
		*/
	}

	FUnionBlender::FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails)
		:
		CarryOverDetails(InCarryOverDetails), BlendingDetails(InBlendingDetails)
	{
		BlendingDetails->GetPointPropertyBlendingHeaders(PropertyHeaders);
	}

	FUnionBlender::~FUnionBlender()
	{
	}

	void FUnionBlender::AddSource(const TSharedPtr<PCGExData::FFacade>& InFacade, const TSet<FName>* IgnoreAttributeSet)
	{
		const int32 SourceIndex = Sources.Add(InFacade);
		const int32 NumSources = Sources.Num();
		IOIndices.Add(InFacade->Source->IOIndex, SourceIndex);

		UniqueTags.Append(InFacade->Source->Tags->RawTags);

		// Update global source count on all multi attributes
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : MultiSourceBlender) { MultiAttribute->SetNum(NumSources); }

		TArray<PCGEx::FAttributeIdentity> SourceAttributes;
		PCGEx::FAttributeIdentity::Get(InFacade->GetIn()->Metadata, SourceAttributes);
		CarryOverDetails->Prune(SourceAttributes);
		BlendingDetails->Filter(SourceAttributes);

		// Check of this new source' attributes
		// See if it adds any new, non-conflicting one
		for (const PCGEx::FAttributeIdentity& Identity : SourceAttributes)
		{
			if (IgnoreAttributeSet && IgnoreAttributeSet
				->
				Contains(Identity.Name)
			)
			{
				continue;
			}

			// First, grab the header for this attribute
			// Getting a fail means it's filtered out.
			PCGExDataBlending::FBlendingHeader Header{};
			if (!BlendingDetails->GetBlendingHeader(Identity.Name, Header)) { continue; }

			const FPCGMetadataAttributeBase* SourceAttribute = InFacade->FindConstAttribute(Identity.Name);
			if (!SourceAttribute) { continue; }

			TSharedPtr<FMultiSourceBlender> MultiAttribute = nullptr;

			// Search for an existing multi attribute
			// This could be done more efficiently with a map, but we need the array later on
			for (const TSharedPtr<FMultiSourceBlender>& ExistingMultiSourceBlender : MultiSourceBlender)
			{
				if (ExistingMultiSourceBlender->Identity.Name == Identity.Name)
				{
					// We found one with the same name
					MultiAttribute = ExistingMultiSourceBlender;
					break;
				}
			}

			if (MultiAttribute)
			{
				// A multi-source blender was found for this attribute!

				if (Identity.UnderlyingType != MultiAttribute->Identity.UnderlyingType)
				{
					// Type mismatch, ignore for this source
					TypeMismatches.Add(Identity.Name.ToString());
					continue;
				}
			}
			else
			{
				// Initialize new multi attribute
				// We give it the first source attribute we found, this will be used
				// to set the underlying default value of the attribute (as a best guess kind of move) 
				MultiAttribute = MultiSourceBlender.Add_GetRef(MakeShared<FMultiSourceBlender>(Identity));
				MultiAttribute->Header = Header;
				MultiAttribute->DefaultValue = SourceAttribute;
				MultiAttribute->SetNum(NumSources);
			}

			check(MultiAttribute)

			// Setup a single blender per A/B pair			
			MultiAttribute->Sources[SourceIndex] = InFacade;
			MultiAttribute->Siblings[SourceIndex] = SourceAttribute;
		}

		// TODO : Initialize property blenders here
		// Weed need Source x Properties blenders :/
		// Might want to revisit that later
	}

	void FUnionBlender::AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InFacades, const TSet<FName>* IgnoreAttributeSet)
	{
		for (TSharedRef<PCGExData::FFacade> Facade : InFacades) { AddSource(Facade, IgnoreAttributeSet); }
	}

	void FUnionBlender::Init(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& TargetData,
		const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata)
	{
		CurrentUnionMetadata = InUnionMetadata;
		CurrentTargetData = TargetData;

		// Create property blender at the last moment
		// TODO : Create a multiblender per property (x source)

		// Initialize blending operations
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : MultiSourceBlender)
		{
			MultiAttribute->Init(InContext, CurrentTargetData, Sources);
		}

		Validate(InContext, false);
	}

	void FUnionBlender::InitTrackers(TArray<PCGEx::FOpStats>& Trackers)
	{
		Trackers.SetNumUninitialized(MultiSourceBlender.Num());
	}

	void FUnionBlender::MergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<PCGEx::FOpStats>& Trackers)
	{
		MergeSingle(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), InDistanceDetails, Trackers);
	}

	void FUnionBlender::MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<PCGEx::FOpStats>& Trackers)
	{
		check(InUnionData)

		TArray<int32> IdxIO;
		TArray<int32> IdxPt;
		TArray<double> Weights;

		FPCGPoint& Target = CurrentTargetData->Source->GetOutPoint(WriteIndex);

		InUnionData->ComputeWeights(
			Sources, IOIndices,
			Target, InDistanceDetails,
			IdxIO, IdxPt, Weights);

		const int32 UnionCount = IdxPt.Num();

		if (UnionCount == 0) { return; }

		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : MultiSourceBlender)
		{
			PCGEx::FOpStats Tracking = MultiAttribute->MainBlender->BeginMultiBlend(WriteIndex);

			for (int k = 0; k < UnionCount; k++)
			{
				if (const TSharedPtr<FProxyDataBlender>& Blender = MultiAttribute->SubBlenders[IdxIO[k]])
				{
					Blender->MultiBlend(IdxPt[k], WriteIndex, Weights[k], Tracking);
				}
			}

			MultiAttribute->MainBlender->EndMultiBlend(WriteIndex, Tracking);
		}
	}

	// Soft blending

	void FUnionBlender::PrepareSoftMerge(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& TargetData,
		const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata)
	{
		CurrentUnionMetadata = InUnionMetadata;
		CurrentTargetData = TargetData;

		const FPCGExPropertiesBlendingDetails PropertiesBlendingDetails = BlendingDetails->GetPropertiesBlendingDetails();
		PropertiesBlender = PropertiesBlendingDetails.HasNoBlending() ? nullptr : MakeUnique<FPropertiesBlender>(PropertiesBlendingDetails);

		CarryOverDetails->Prune(UniqueTags);

		// Initialize blending operations
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : MultiSourceBlender)
		{
			MultiAttribute->SoftInit(CurrentTargetData, Sources);
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

		Validate(InContext, false);
	}

	void FUnionBlender::SoftMergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails)
	{
		SoftMergeSingle(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), InDistanceDetails);
	}

	void FUnionBlender::SoftMergeSingle(const int32 UnionIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails)
	{
		TArray<int32> IdxIO;
		TArray<int32> IdxPt;
		TArray<double> Weights;
		TArray<int8> InheritedTags;

		FPCGPoint& Target = CurrentTargetData->Source->GetOutPoint(UnionIndex);

		InUnionData->ComputeWeights(
			Sources, IOIndices,
			Target, InDistanceDetails,
			IdxIO, IdxPt, Weights);

		const int32 NumUnions = IdxPt.Num();
		if (NumUnions == 0) { return; }

		InheritedTags.Init(0, TagAttributes.Num());

		// Blend Properties
		BlendProperties(Target, IdxIO, IdxPt, Weights);

		// Blend Attributes
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : MultiSourceBlender)
		{
			MultiAttribute->MainBlender->PrepareOperation(Target.MetadataEntry);

			int32 ValidUnions = 0;
			double TotalWeight = 0;

			for (int k = 0; k < NumUnions; k++)
			{
				const TSharedPtr<FDataBlendingProcessorBase>& Operation = MultiAttribute->SubBlenders[IdxIO[k]];
				if (!Operation) { continue; }

				const double Weight = Weights[k];

				Operation->DoOperation(
					Target.MetadataEntry, Sources[IdxIO[k]]->Source->GetInPoint(IdxPt[k]).MetadataEntry,
					Target.MetadataEntry, Weight, k == 0);

				ValidUnions++;
				TotalWeight += Weight;
			}

			if (ValidUnions == 0) { continue; } // No valid attribute to merge on any union source

			MultiAttribute->MainBlender->CompleteOperation(Target.MetadataEntry, ValidUnions, TotalWeight);
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

	bool FUnionBlender::Validate(FPCGExContext* InContext, const bool bQuiet) const
	{
		if (TypeMismatches.IsEmpty()) { return true; }

		if (bQuiet) { return false; }

		PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("The following attributes have the same name but different types, and will not blend as expected: {0}"), FText::FromString(FString::Join(TypeMismatches.Array(), TEXT(", ")))));

		return false;
	}
}
