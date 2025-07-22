// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataForward.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointData.h"

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

bool FPCGExAttributeToTagDetails::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSourceFacade)
{
	PCGExHelpers::AppendUniqueSelectorsFromCommaSeparatedList(CommaSeparatedAttributeSelectors, Attributes);

	for (FPCGAttributePropertyInputSelector& Selector : Attributes)
	{
		if (const TSharedPtr<PCGEx::TAttributeBroadcaster<FString>>& Getter = Getters.Add_GetRef(MakeShared<PCGEx::TAttributeBroadcaster<FString>>());
			!Getter->Prepare(Selector, InSourceFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing specified Tag attribute."));
			Getters.Empty();
			return false;
		}
	}

	SourceDataFacade = InSourceFacade;
	return true;
}

void FPCGExAttributeToTagDetails::Tag(const PCGExData::FConstPoint& TagSource, TSet<FString>& InTags) const
{
	if (bAddIndexTag) { InTags.Add(IndexTagPrefix + ":" + FString::Printf(TEXT("%d"), TagSource.Index)); }

	if (!Getters.IsEmpty())
	{
		for (const TSharedPtr<PCGEx::TAttributeBroadcaster<FString>>& Getter : Getters)
		{
			FString Tag = Getter->FetchSingle(TagSource, TEXT(""));
			if (Tag.IsEmpty()) { continue; }
			if (bPrefixWithAttributeName) { Tag = Getter->GetName() + ":" + Tag; }
			InTags.Add(Tag);
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
		if (PCGEx::IsWritableAttributeName(FName(IndexTagPrefix)))
		{
			InMetadata->FindOrCreateAttribute<FString>(FName(IndexTagPrefix), IndexTagPrefix + ":" + FString::Printf(TEXT("%d"), TagSource.Index));
		}
	}

	if (!Getters.IsEmpty())
	{
		for (const TSharedPtr<PCGEx::TAttributeBroadcaster<FString>>& Getter : Getters)
		{
			FString Tag = Getter->FetchSingle(TagSource, TEXT(""));
			if (Tag.IsEmpty()) { continue; }
			if (bPrefixWithAttributeName) { Tag = Getter->GetName() + ":" + Tag; }
			InMetadata->FindOrCreateAttribute<FString>(FName(Getter->GetName()), Tag);
		}
	}
}

namespace PCGExData
{
	FDataForwardHandler::FDataForwardHandler(const FPCGExForwardDetails& InDetails, const TSharedPtr<FFacade>& InSourceDataFacade, const bool ElementDomainToDataDomain):
		Details(InDetails), SourceDataFacade(InSourceDataFacade), TargetDataFacade(nullptr), bElementDomainToDataDomain(ElementDomainToDataDomain)
	{
		if (!Details.bEnabled) { return; }

		Details.Init();
		PCGEx::FAttributeIdentity::Get(InSourceDataFacade->GetIn()->Metadata, Identities);
		Details.Filter(Identities);
	}

	FDataForwardHandler::FDataForwardHandler(const FPCGExForwardDetails& InDetails, const TSharedPtr<FFacade>& InSourceDataFacade, const TSharedPtr<FFacade>& InTargetDataFacade, const bool ElementDomainToDataDomain):
		Details(InDetails), SourceDataFacade(InSourceDataFacade), TargetDataFacade(InTargetDataFacade), bElementDomainToDataDomain(ElementDomainToDataDomain)
	{
		Details.Init();
		PCGEx::FAttributeIdentity::Get(InSourceDataFacade->GetIn()->Metadata, Identities);
		Details.Filter(Identities);

		const int32 NumAttributes = Identities.Num();
		Readers.Reserve(NumAttributes);
		Writers.Reserve(NumAttributes);

		// Init forwarded attributes on target		
		for (int i = 0; i < NumAttributes; i++)
		{
			const PCGEx::FAttributeIdentity& Identity = Identities[i];

			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					TSharedPtr<TBuffer<T>> Reader = SourceDataFacade->GetReadable<T>(Identity.Identifier);
					TSharedPtr<TBuffer<T>> Writer = TargetDataFacade->GetWritable<T>(Reader->GetTypedInAttribute(), EBufferInit::Inherit);

					if (!Reader || !Writer) { return; }

					Readers.Add(Reader);
					Writers.Add(Writer);
				});
		}
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, const int32 TargetIndex)
	{
		const int32 NumAttributes = Identities.Num();

		for (int i = 0; i < NumAttributes; i++)
		{
			const PCGEx::FAttributeIdentity& Identity = Identities[i];

			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					TSharedPtr<TBuffer<T>> Reader = StaticCastSharedPtr<TBuffer<T>>(Readers[i]);
					TSharedPtr<TBuffer<T>> Writer = StaticCastSharedPtr<TBuffer<T>>(Writers[i]);
					Writer->SetValue(TargetIndex, Reader->Read(SourceIndex));
				});
		}
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, const TSharedPtr<FFacade>& InTargetDataFacade)
	{
		if (Identities.IsEmpty()) { return; }

		const UPCGBasePointData* InSourceData = SourceDataFacade->GetIn();

		if (Details.bPreserveAttributesDefaultValue)
		{
			for (const PCGEx::FAttributeIdentity& Identity : Identities)
			{
				PCGEx::ExecuteWithRightType(
					Identity.UnderlyingType, [&](auto DummyValue)
					{
						using T = decltype(DummyValue);

						const FPCGMetadataAttribute<T>* SourceAtt = PCGEx::TryGetConstAttribute<T>(InSourceData, Identity.Identifier);
						if (!SourceAtt) { return; }

						const T ForwardValue = Identity.InDataDomain() ?
							                       PCGExDataHelpers::ReadDataValue(SourceAtt) :
							                       SourceAtt->GetValueFromItemKey(InSourceData->GetMetadataEntry(SourceIndex));

						TSharedPtr<TBuffer<T>> Writer = nullptr;

						if (bElementDomainToDataDomain)
						{
							const FPCGAttributeIdentifier ToDataIdentifier(Identity.Identifier.Name, PCGMetadataDomainID::Data);
							Writer = InTargetDataFacade->GetWritable<T>(ToDataIdentifier, EBufferInit::New);
						}
						else
						{
							Writer = InTargetDataFacade->GetWritable<T>(SourceAtt, EBufferInit::New);
						}

						if (!Writer) { return; }

						if (Writer->GetUnderlyingDomain() == EDomainType::Elements)
						{
							TSharedPtr<TArrayBuffer<T>> ElementsWriter = StaticCastSharedPtr<TArrayBuffer<T>>(Writer);
							TArray<T>& Values = *ElementsWriter->GetOutValues();
							for (T& Value : Values) { Value = ForwardValue; }
						}
						else
						{
							Writer->SetValue(0, ForwardValue);
						}
					});
			}

			return;
		}

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					const FPCGMetadataAttribute<T>* SourceAtt = PCGEx::TryGetConstAttribute<T>(InSourceData, Identity.Identifier);
					if (!SourceAtt) { return; }

					const T ForwardValue = Identity.InDataDomain() ?
						                       PCGExDataHelpers::ReadDataValue(SourceAtt) :
						                       SourceAtt->GetValueFromItemKey(InSourceData->GetMetadataEntry(SourceIndex));

					const FPCGAttributeIdentifier Identifier = bElementDomainToDataDomain ? FPCGAttributeIdentifier(Identity.Identifier.Name, PCGMetadataDomainID::Data) : Identity.Identifier;

					InTargetDataFacade->Source->DeleteAttribute(Identifier);
					FPCGMetadataAttribute<T>* TargetAtt = InTargetDataFacade->Source->FindOrCreateAttribute<T>(Identifier, ForwardValue, SourceAtt->AllowsInterpolation());
					if (bElementDomainToDataDomain) { PCGExDataHelpers::SetDataValue(TargetAtt, ForwardValue); }
				});
		}
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, const TSharedPtr<FFacade>& InTargetDataFacade, const TArray<int32>& Indices)
	{
		if (Identities.IsEmpty()) { return; }

		const UPCGBasePointData* InSourceData = SourceDataFacade->GetIn();

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					const FPCGMetadataAttribute<T>* SourceAtt = PCGEx::TryGetConstAttribute<T>(InSourceData, Identity.Identifier);
					if (!SourceAtt) { return; }

					const T ForwardValue = Identity.InDataDomain() ?
						                       PCGExDataHelpers::ReadDataValue(SourceAtt) :
						                       SourceAtt->GetValueFromItemKey(InSourceData->GetMetadataEntry(SourceIndex));

					TSharedPtr<TBuffer<T>> Writer = InTargetDataFacade->GetWritable<T>(SourceAtt, EBufferInit::Inherit);
					if (Writer->GetUnderlyingDomain() == EDomainType::Elements)
					{
						TSharedPtr<TArrayBuffer<T>> ElementsWriter = StaticCastSharedPtr<TArrayBuffer<T>>(Writer);
						TArray<T>& Values = *ElementsWriter->GetOutValues();
						for (int32 Index : Indices) { Values[Index] = ForwardValue; }
					}
					else
					{
						Writer->SetValue(0, ForwardValue);
					}
				});
		}
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, UPCGMetadata* InTargetMetadata)
	{
		if (Identities.IsEmpty()) { return; }

		const UPCGBasePointData* InSourceData = SourceDataFacade->GetIn();

		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			PCGEx::ExecuteWithRightType(
				Identity.UnderlyingType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					const FPCGMetadataAttribute<T>* SourceAtt = PCGEx::TryGetConstAttribute<T>(InSourceData, Identity.Identifier);
					if (!SourceAtt) { return; }

					const T ForwardValue = Identity.InDataDomain() ?
						                       PCGExDataHelpers::ReadDataValue(SourceAtt) :
						                       SourceAtt->GetValueFromItemKey(InSourceData->GetMetadataEntry(SourceIndex));

					const FPCGAttributeIdentifier Identifier = bElementDomainToDataDomain ? FPCGAttributeIdentifier(Identity.Identifier.Name, PCGMetadataDomainID::Data) : Identity.Identifier;

					InTargetMetadata->DeleteAttribute(Identifier);
					FPCGMetadataAttribute<T>* TargetAtt = InTargetMetadata->FindOrCreateAttribute<T>(Identifier, ForwardValue, SourceAtt->AllowsInterpolation(), true, true);
					if (bElementDomainToDataDomain) { PCGExDataHelpers::SetDataValue(TargetAtt, ForwardValue); }
				});
		}
	}
}
