// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


// Cherry picker merges metadata from varied sources into one.
// Initially to handle metadata merging for Fuse Clusters

#include "Blenders/PCGExUnionBlender.h"

#include "Containers/PCGExIndexLookup.h"
#include "Core/PCGExOpStats.h"
#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Core/PCGExUnionData.h"
#include "Details/PCGExBlendingDetails.h"

namespace PCGExBlending
{
#pragma region FMultiSourceBlender

	FUnionBlender::FMultiSourceBlender::FMultiSourceBlender(const PCGExData::FAttributeIdentity& InIdentity, const TArray<TSharedPtr<PCGExData::FFacade>>& InSources)
		: Identity(InIdentity), Sources(InSources)
	{
	}

	FUnionBlender::FMultiSourceBlender::FMultiSourceBlender(const TArray<TSharedPtr<PCGExData::FFacade>>& InSources)
		: Sources(InSources)
	{
	}

	bool FUnionBlender::FMultiSourceBlender::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetData, const bool bWantsDirectAccess)
	{
		check(InTargetData);

		if (Param.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			const EPCGMetadataTypes WorkingType = Identity.UnderlyingType;
			if (WorkingType == EPCGMetadataTypes::Unknown)
			{
				// Unknown attribute type
				return false;
			}

			TSharedPtr<PCGExData::IBuffer> InitializationBuffer = nullptr;

			if (const FPCGMetadataAttributeBase* ExistingAttribute = InTargetData->FindConstAttribute(Identity.Identifier); ExistingAttribute && ExistingAttribute->GetTypeId() == static_cast<int16>(Identity.UnderlyingType))
			{
				// This attribute exists on target already
				InitializationBuffer = InTargetData->GetWritable(WorkingType, ExistingAttribute, PCGExData::EBufferInit::Inherit);
			}
			else
			{
				// This attribute needs to be initialized
				InitializationBuffer = InTargetData->GetWritable(WorkingType, DefaultValue, PCGExData::EBufferInit::New);
			}

			if (!InitializationBuffer)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("FMultiSourceBlender : Cannot create writable output for : \"{0}\""), FText::FromName(Identity.Identifier.Name)));
				return false;
			}

			bool bError = false;

			MainBlender = CreateProxyBlender(WorkingType, Param.Blending);

			for (int i = 0; i < Sources.Num(); i++)
			{
				TSharedPtr<PCGExData::FFacade> Source = Sources[i];
				if (!SupportedSources.Contains(i)) { continue; }

				TSharedPtr<FProxyDataBlender> SubBlender = CreateProxyBlender(WorkingType, Param.Blending);
				SubBlenders[i] = SubBlender;

				if (!SubBlender->InitFromParam(InContext, Param, InTargetData, Sources[i], PCGExData::EIOSide::In, bWantsDirectAccess)) { return false; }
			}

			if (!MainBlender->InitFromParam(InContext, Param, InTargetData, InTargetData, PCGExData::EIOSide::Out, bWantsDirectAccess)) { return false; }
		}
		else if (Param.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
		{
			const EPCGMetadataTypes WorkingType = PCGExMetaHelpers::GetPropertyType(Param.Selector.GetPointProperty());

			MainBlender = CreateProxyBlender(WorkingType, Param.Blending);

			for (int i = 0; i < Sources.Num(); i++)
			{
				TSharedPtr<PCGExData::FFacade> Source = Sources[i];
				TSharedPtr<FProxyDataBlender> SubBlender = CreateProxyBlender(WorkingType, Param.Blending);
				SubBlenders[i] = SubBlender;

				if (!SubBlender->InitFromParam(InContext, Param, InTargetData, Sources[i], PCGExData::EIOSide::In, bWantsDirectAccess)) { return false; }
			}

			if (!MainBlender->InitFromParam(InContext, Param, InTargetData, InTargetData, PCGExData::EIOSide::Out, bWantsDirectAccess)) { return false; }
		}
		else
		{
			return false;
		}


		return true;
	}

#pragma endregion

	FUnionBlender::FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails, const PCGExMath::FDistances* InDistanceDetails)
		: CarryOverDetails(InCarryOverDetails), BlendingDetails(InBlendingDetails), DistanceDetails(InDistanceDetails)
	{
		BlendingDetails->GetPointPropertyBlendingParams(PropertyParams);
	}

	FUnionBlender::~FUnionBlender()
	{
	}

	void FUnionBlender::AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InSources, const TSet<FName>* IgnoreAttributeSet)
	{
		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { MaxIndex = FMath::Max(Src->Source->IOIndex, MaxIndex); }
		IOLookup = MakeShared<PCGEx::FIndexLookup>(MaxIndex + 1);

		const int32 NumSources = InSources.Num();
		Sources.Reserve(NumSources);
		SourcesData.SetNumUninitialized(NumSources);

		for (int i = 0; i < InSources.Num(); i++)
		{
			const TSharedRef<PCGExData::FFacade>& Facade = InSources[i];

			Sources.Add(Facade);
			SourcesData[i] = Facade->GetIn();
			IOLookup->Set(Facade->Source->IOIndex, i);

			EnumAddFlags(AllocatedProperties, Facade->GetAllocations());

			UniqueTags.Append(Facade->Source->Tags->RawTags);

			TArray<PCGExData::FAttributeIdentity> SourceAttributes;
			GetFilteredIdentities(Facade->GetIn()->Metadata, SourceAttributes, BlendingDetails, CarryOverDetails, IgnoreAttributeSet);

			// Check of this new source' attributes
			// See if it adds any new, non-conflicting one
			for (const PCGExData::FAttributeIdentity& Identity : SourceAttributes)
			{
				// First, grab the Param for this attribute
				// Getting a fail means it's filtered out.
				FBlendingParam Param{};
				if (!BlendingDetails->GetBlendingParam(Identity.Identifier, Param)) { continue; }

				const FPCGMetadataAttributeBase* SourceAttribute = Facade->FindConstAttribute(Identity.Identifier);
				if (!SourceAttribute) { continue; }

				TSharedPtr<FMultiSourceBlender> MultiAttribute = nullptr;

				// Search for an existing multi attribute
				// This could be done more efficiently with a map, but we need the array later on
				for (const TSharedPtr<FMultiSourceBlender>& ExistingMultiSourceBlender : Blenders)
				{
					if (ExistingMultiSourceBlender->Identity.Identifier == Identity.Identifier)
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
						TypeMismatches.Add(Identity.Identifier.Name.ToString());
						continue;
					}
				}
				else
				{
					// Initialize new multi attribute
					// We give it the first source attribute we found, this will be used
					// to set the underlying default value of the attribute (as a best guess kind of move) 
					MultiAttribute = Blenders.Add_GetRef(MakeShared<FMultiSourceBlender>(Identity, Sources));
					MultiAttribute->Param = Param;
					MultiAttribute->DefaultValue = SourceAttribute;
					MultiAttribute->SetNum(NumSources);
				}

				check(MultiAttribute)

				MultiAttribute->SupportedSources.Add(i);
			}
		}
	}

	bool FUnionBlender::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const bool bWantsDirectAccess)
	{
		CurrentTargetData = TargetData;

		if (!Validate(InContext, false)) { return false; }

		// Create property blender at the last moment
		Blenders.Reserve(Blenders.Num() + PropertyParams.Num());
		for (const FBlendingParam& Param : PropertyParams)
		{
			if (!EnumHasAnyFlags(AllocatedProperties, PCGExMetaHelpers::GetPropertyNativeTypes(Param.Selector.GetPointProperty())))
			{
				// Don't create a blender for properties that no source has allocated
				continue;
			}

			TSharedPtr<FMultiSourceBlender> MultiAttribute = Blenders.Add_GetRef(MakeShared<FMultiSourceBlender>(Sources));
			MultiAttribute->Param = Param;
			MultiAttribute->SetNum(Sources.Num());
		}

		// Initialize all blending operations
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : Blenders)
		{
			if (!MultiAttribute->Init(InContext, CurrentTargetData, bWantsDirectAccess)) { return false; }
		}

		return true;
	}

	bool FUnionBlender::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const bool bWantsDirectAccess)
	{
		CurrentUnionMetadata = InUnionMetadata;
		return Init(InContext, TargetData, bWantsDirectAccess);
	}

	int32 FUnionBlender::ComputeWeights(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const
	{
		const PCGExData::FConstPoint Target = CurrentTargetData->Source->GetOutPoint(WriteIndex);
		return InUnionData->ComputeWeights(SourcesData, IOLookup, Target, DistanceDetails, OutWeightedPoints);
	}

	void FUnionBlender::Blend(const int32 WriteIndex, const TArray<PCGExData::FWeightedPoint>& InWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const
	{
		if (InWeightedPoints.IsEmpty()) { return; }

		// For each attribute/property we want to blend
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : Blenders)
		{
			PCGEx::FOpStats Tracking = MultiAttribute->MainBlender->BeginMultiBlend(WriteIndex);

			// For each point in the union, check if there is an attribute blender for that source; and if so, add it to the blend
			for (const PCGExData::FWeightedPoint& P : InWeightedPoints)
			{
				if (const TSharedPtr<FProxyDataBlender>& Blender = MultiAttribute->SubBlenders[P.IO])
				{
					Blender->MultiBlend(P.Index, WriteIndex, P.Weight, Tracking);
				}
			}

			MultiAttribute->MainBlender->EndMultiBlend(WriteIndex, Tracking);
		}
	}

	void FUnionBlender::MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const
	{
		check(InUnionData)
		if (!ComputeWeights(WriteIndex, InUnionData, OutWeightedPoints)) { return; }
		Blend(WriteIndex, OutWeightedPoints, Trackers);
	}

	void FUnionBlender::MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const
	{
		if (!ComputeWeights(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), OutWeightedPoints)) { return; }
		Blend(UnionIndex, OutWeightedPoints, Trackers);
	}

	bool FUnionBlender::Validate(FPCGExContext* InContext, const bool bQuiet) const
	{
		if (TypeMismatches.IsEmpty()) { return true; }

		if (bQuiet) { return false; }

		PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("The following attributes have the same name but different types, and will not blend as expected: {0}"), FText::FromString(FString::Join(TypeMismatches.Array(), TEXT(", ")))));

		return false;
	}
}
