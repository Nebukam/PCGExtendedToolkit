// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Utils/PCGExDataForwardDetails.h"

#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataForward.h"

void FPCGExForwardDetails::Filter(TArray<PCGExData::FAttributeIdentity>& Identities) const
{
	for (int i = 0; i < Identities.Num(); i++)
	{
		if (!Test(Identities[i].Identifier.Name.ToString()))
		{
			Identities.RemoveAt(i);
			i--;
		}
	}
}

TSharedPtr<PCGExData::FDataForwardHandler> FPCGExForwardDetails::GetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, const bool bForwardToDataDomain) const
{
	return MakeShared<PCGExData::FDataForwardHandler>(*this, InSourceDataFacade, bForwardToDataDomain);
}

TSharedPtr<PCGExData::FDataForwardHandler> FPCGExForwardDetails::GetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, const TSharedPtr<PCGExData::FFacade>& InTargetDataFacade, const bool bForwardToDataDomain) const
{
	return MakeShared<PCGExData::FDataForwardHandler>(*this, InSourceDataFacade, InTargetDataFacade, bForwardToDataDomain);
}

TSharedPtr<PCGExData::FDataForwardHandler> FPCGExForwardDetails::TryGetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, const bool bForwardToDataDomain) const
{
	return bEnabled ? GetHandler(InSourceDataFacade, bForwardToDataDomain) : nullptr;
}

TSharedPtr<PCGExData::FDataForwardHandler> FPCGExForwardDetails::TryGetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, const TSharedPtr<PCGExData::FFacade>& InTargetDataFacade, const bool bForwardToDataDomain) const
{
	return bEnabled ? GetHandler(InSourceDataFacade, InTargetDataFacade, bForwardToDataDomain) : nullptr;
}

bool FPCGExAttributeToTagDetails::Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSourceFacade, const TSet<FName>* IgnoreAttributes)
{
	PCGExMetaHelpers::AppendUniqueSelectorsFromCommaSeparatedList(CommaSeparatedAttributeSelectors, Attributes);

	Getters.Reserve(Attributes.Num());
	for (FPCGAttributePropertyInputSelector& Selector : Attributes)
	{
		if (IgnoreAttributes)
		{
			if (IgnoreAttributes->Contains(Selector.GetAttributeName())) { continue; }
		}
		
		const TSharedPtr<PCGExData::IAttributeBroadcaster>& Getter = PCGExData::MakeBroadcaster(Selector, InSourceFacade->Source, true);
		if (!Getter)
		{
			PCGEX_LOG_INVALID_SELECTOR_C(InContext, Tag, Selector)
			continue;
		}

		Getters.Add(Getter);
	}

	SourceDataFacade = InSourceFacade;
	return true;
}

void FPCGExAttributeToTagDetails::Tag(const PCGExData::FConstPoint& TagSource, TSet<FString>& InTags) const
{
	if (bAddIndexTag) { InTags.Add(IndexTagPrefix + ":" + FString::Printf(TEXT("%d"), TagSource.Index)); }

	if (!Getters.IsEmpty())
	{
		for (const TSharedPtr<PCGExData::IAttributeBroadcaster>& Getter : Getters)
		{
			FString Value = TEXT("");
			FString Prefix = TEXT("");

			PCGExMetaHelpers::ExecuteWithRightType(Getter->GetMetadataType(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<PCGExData::TAttributeBroadcaster<T>> TypedGetter = StaticCastSharedPtr<PCGExData::TAttributeBroadcaster<T>>(Getter);
				if (!TypedGetter) { return; }

				Prefix = TypedGetter->GetName().ToString();

				T TypedValue = T{};
				if (TypedGetter->TryFetchSingle(TagSource, TypedValue)) { Value = PCGExTypeOps::Convert<T, FString>(TypedValue); }
			});

			if (Value.IsEmpty()) { continue; }

			if (bPrefixWithAttributeName) { Value = Prefix + ":" + Value; }
			InTags.Add(Value);
		}
	}
}

void FPCGExAttributeToTagDetails::Tag(const PCGExData::FConstPoint& TagSource, const TSharedPtr<PCGExData::FPointIO>& PointIO) const
{
	TSet<FString> Tags;
	Tag(TagSource, Tags);
	PointIO->Tags->Append(Tags);
}

void FPCGExAttributeToTagDetails::Tag(const PCGExData::FConstPoint& TagSource, UPCGMetadata* InMetadata) const
{
	if (bAddIndexTag)
	{
		if (PCGExMetaHelpers::IsWritableAttributeName(FName(IndexTagPrefix)))
		{
			const FPCGAttributeIdentifier Identifier = FPCGAttributeIdentifier(FName(IndexTagPrefix), PCGMetadataDomainID::Data);
			InMetadata->DeleteAttribute(Identifier);
			InMetadata->FindOrCreateAttribute<int32>(Identifier, TagSource.Index);
		}
	}

	if (!Getters.IsEmpty())
	{
		for (const TSharedPtr<PCGExData::IAttributeBroadcaster>& Getter : Getters)
		{
			PCGExMetaHelpers::ExecuteWithRightType(Getter->GetMetadataType(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<PCGExData::TAttributeBroadcaster<T>> TypedGetter = StaticCastSharedPtr<PCGExData::TAttributeBroadcaster<T>>(Getter);
				if (!TypedGetter) { return; }

				const FPCGAttributeIdentifier Identifier = FPCGAttributeIdentifier(Getter->GetName(), PCGMetadataDomainID::Data);
				UE_LOG(LogTemp, Warning, TEXT("Name : %s"), *Getter->GetName().ToString());
				InMetadata->DeleteAttribute(Identifier);
				InMetadata->FindOrCreateAttribute<T>(Identifier, TypedGetter->FetchSingle(TagSource, T{}));
			});
		}
	}
}
