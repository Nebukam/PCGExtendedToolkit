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
#pragma region FMultiSourceBlender

	FUnionBlender::FMultiSourceBlender::FMultiSourceBlender(const PCGEx::FAttributeIdentity& InIdentity, const TArray<TSharedPtr<PCGExData::FFacade>>& InSources)
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

		if (Header.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (Identity.UnderlyingType == EPCGMetadataTypes::Unknown)
			{
				// Unknown attribute type
				return false;
			}

			TSharedPtr<PCGExData::IBuffer> InitializationBuffer = nullptr;

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
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("FMultiSourceBlender : Cannot create writable output for : \"{0}\""), FText::FromName(Identity.Name)));
				return false;
			}

			bool bError = false;
			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					MainBlender = PCGExDataBlending::CreateProxyBlender<T>(Header.Blending);

					for (int i = 0; i < Sources.Num(); i++)
					{
						TSharedPtr<PCGExData::FFacade> Source = Sources[i];
						if (!SupportedSources.Contains(i)) { continue; }

						TSharedPtr<FProxyDataBlender> SubBlender = PCGExDataBlending::CreateProxyBlender<T>(Header.Blending);
						SubBlenders[i] = SubBlender;

						if (!SubBlender->InitFromHeader(InContext, Header, InTargetData, Sources[i], PCGExData::EIOSide::In, bWantsDirectAccess))
						{
							bError = true;
							return;
						}
					}

					bError = !MainBlender->InitFromHeader(InContext, Header, InTargetData, InTargetData, PCGExData::EIOSide::Out, bWantsDirectAccess);
				});

			if (bError) { return false; }
		}
		else if (Header.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
		{
			bool bError = false;
			PCGEx::ExecuteWithRightType(
				PCGEx::GetPropertyType(Header.Selector.GetPointProperty()), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					MainBlender = PCGExDataBlending::CreateProxyBlender<T>(Header.Blending);

					for (int i = 0; i < Sources.Num(); i++)
					{
						TSharedPtr<PCGExData::FFacade> Source = Sources[i];
						TSharedPtr<FProxyDataBlender> SubBlender = PCGExDataBlending::CreateProxyBlender<T>(Header.Blending);
						SubBlenders[i] = SubBlender;

						if (!SubBlender->InitFromHeader(InContext, Header, InTargetData, Sources[i], PCGExData::EIOSide::In, bWantsDirectAccess))
						{
							bError = true;
							return;
						}
					}

					bError = !MainBlender->InitFromHeader(InContext, Header, InTargetData, InTargetData, PCGExData::EIOSide::Out, bWantsDirectAccess);
				});

			if (bError) { return false; }
		}
		else
		{
			return false;
		}


		return true;
	}

#pragma endregion

	FUnionBlender::FUnionBlender(const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails)
		: CarryOverDetails(InCarryOverDetails), BlendingDetails(InBlendingDetails), DistanceDetails(InDistanceDetails)
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

		SourcesData.Add(InFacade->GetIn());
		IOIndices.Add(InFacade->Source->IOIndex, SourceIndex);

		UniqueTags.Append(InFacade->Source->Tags->RawTags);

		// Update global source count on all multi attributes
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : Blenders) { MultiAttribute->SetNum(NumSources); }

		TArray<PCGEx::FAttributeIdentity> SourceAttributes;
		PCGExDataBlending::GetFilteredIdentities(
			InFacade->GetIn()->Metadata, SourceAttributes,
			BlendingDetails, CarryOverDetails, IgnoreAttributeSet);

		// Check of this new source' attributes
		// See if it adds any new, non-conflicting one
		for (const PCGEx::FAttributeIdentity& Identity : SourceAttributes)
		{
			// First, grab the header for this attribute
			// Getting a fail means it's filtered out.
			PCGExDataBlending::FBlendingHeader Header{};
			if (!BlendingDetails->GetBlendingHeader(Identity.Name, Header)) { continue; }

			const FPCGMetadataAttributeBase* SourceAttribute = InFacade->FindConstAttribute(Identity.Name);
			if (!SourceAttribute) { continue; }

			TSharedPtr<FMultiSourceBlender> MultiAttribute = nullptr;

			// Search for an existing multi attribute
			// This could be done more efficiently with a map, but we need the array later on
			for (const TSharedPtr<FMultiSourceBlender>& ExistingMultiSourceBlender : Blenders)
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
				MultiAttribute = Blenders.Add_GetRef(MakeShared<FMultiSourceBlender>(Identity, Sources));
				MultiAttribute->Header = Header;
				MultiAttribute->DefaultValue = SourceAttribute;
				MultiAttribute->SetNum(NumSources);
			}

			check(MultiAttribute)

			MultiAttribute->SupportedSources.Add(SourceIndex);
		}
	}

	void FUnionBlender::AddSources(const TArray<TSharedRef<PCGExData::FFacade>>& InFacades, const TSet<FName>* IgnoreAttributeSet)
	{
		for (TSharedRef<PCGExData::FFacade> Facade : InFacades) { AddSource(Facade, IgnoreAttributeSet); }
	}

	bool FUnionBlender::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& TargetData, const bool bWantsDirectAccess)
	{
		CurrentTargetData = TargetData;

		if (!Validate(InContext, false)) { return false; }

		// Create property blender at the last moment
		Blenders.Reserve(Blenders.Num() + PropertyHeaders.Num());
		for (const FBlendingHeader& Header : PropertyHeaders)
		{
			TSharedPtr<FMultiSourceBlender> MultiAttribute = Blenders.Add_GetRef(MakeShared<FMultiSourceBlender>(Sources));
			MultiAttribute->Header = Header;
			MultiAttribute->SetNum(Sources.Num());
		}

		// Initialize all blending operations
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : Blenders)
		{
			if (!MultiAttribute->Init(InContext, CurrentTargetData, bWantsDirectAccess)) { return false; }
		}

		return true;
	}

	bool FUnionBlender::Init(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& TargetData,
		const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata
		, const bool bWantsDirectAccess)
	{
		CurrentUnionMetadata = InUnionMetadata;
		return Init(InContext, TargetData, bWantsDirectAccess);
	}

	void FUnionBlender::MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::FUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints)
	{
		check(InUnionData)

		PCGExData::FConstPoint Target = CurrentTargetData->Source->GetOutPoint(WriteIndex);
		const int32 UnionCount = InUnionData->ComputeWeights(SourcesData, IOIndices, Target, DistanceDetails, OutWeightedPoints);

		if (UnionCount == 0) { return; }

		// For each attribute/property we want to blend
		for (const TSharedPtr<FMultiSourceBlender>& MultiAttribute : Blenders)
		{
			PCGEx::FOpStats Tracking = MultiAttribute->MainBlender->BeginMultiBlend(WriteIndex);

			// For each point in the union, check if there is an attribute blender for that source; and if so, add it to the blend
			for (const PCGExData::FWeightedPoint& P : OutWeightedPoints)
			{
				if (const TSharedPtr<FProxyDataBlender>& Blender = MultiAttribute->SubBlenders[P.IO])
				{
					Blender->MultiBlend(P.Index, WriteIndex, P.Weight, Tracking);
				}
			}

			MultiAttribute->MainBlender->EndMultiBlend(WriteIndex, Tracking);
		}
	}

	void FUnionBlender::MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints)
	{
		MergeSingle(UnionIndex, CurrentUnionMetadata->Get(UnionIndex), OutWeightedPoints);
	}

	bool FUnionBlender::Validate(FPCGExContext* InContext, const bool bQuiet) const
	{
		if (TypeMismatches.IsEmpty()) { return true; }

		if (bQuiet) { return false; }

		PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("The following attributes have the same name but different types, and will not blend as expected: {0}"), FText::FromString(FString::Join(TypeMismatches.Array(), TEXT(", ")))));

		return false;
	}
}
