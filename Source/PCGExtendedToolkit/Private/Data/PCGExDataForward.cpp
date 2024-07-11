// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataForward.h"
#include "Data/PCGExData.h"


PCGExData::FDataForwardHandler* FPCGExForwardDetails::GetHandler(PCGExData::FFacade* InSourceDataFacade) const
{
	return new PCGExData::FDataForwardHandler(*this, InSourceDataFacade);
}

PCGExData::FDataForwardHandler* FPCGExForwardDetails::GetHandler(PCGExData::FFacade* InSourceDataFacade, PCGExData::FFacade* InTargetDataFacade) const
{
	InTargetDataFacade->Source->CreateOutKeys();
	return new PCGExData::FDataForwardHandler(*this, InSourceDataFacade, InTargetDataFacade);
}

PCGExData::FDataForwardHandler* FPCGExForwardDetails::TryGetHandler(PCGExData::FFacade* InSourceDataFacade) const
{
	return bEnabled ? GetHandler(InSourceDataFacade) : nullptr;
}

PCGExData::FDataForwardHandler* FPCGExForwardDetails::TryGetHandler(PCGExData::FFacade* InSourceDataFacade, PCGExData::FFacade* InTargetDataFacade) const
{
	return bEnabled ? GetHandler(InSourceDataFacade, InTargetDataFacade) : nullptr;
}

namespace PCGExData
{
	FDataForwardHandler::~FDataForwardHandler()
	{
		Identities.Empty();
		Readers.Empty();
		Writers.Empty();
	}

	FDataForwardHandler::FDataForwardHandler(const FPCGExForwardDetails& InDetails, FFacade* InSourceDataFacade):
		Details(InDetails), SourceDataFacade(InSourceDataFacade), TargetDataFacade(nullptr)
	{
		if (!Details.bEnabled) { return; }

		Details.Init();
		PCGEx::FAttributeIdentity::Get(InSourceDataFacade->GetIn()->Metadata, Identities);
		Details.Filter(Identities);
	}

	FDataForwardHandler::FDataForwardHandler(const FPCGExForwardDetails& InDetails, FFacade* InSourceDataFacade, FFacade* InTargetDataFacade):
		Details(InDetails), SourceDataFacade(InSourceDataFacade), TargetDataFacade(InTargetDataFacade)
	{
		Details.Init();
		PCGEx::FAttributeIdentity::Get(InSourceDataFacade->GetIn()->Metadata, Identities);
		Details.Filter(Identities);

		const int32 NumAttributes = Identities.Num();
		PCGEX_SET_NUM_UNINITIALIZED(Readers, NumAttributes)
		PCGEX_SET_NUM_UNINITIALIZED(Writers, NumAttributes)

		// Init forwarded attributes on target		
		for (int i = 0; i < NumAttributes; i++)
		{
			const PCGEx::FAttributeIdentity& Identity = Identities[i];

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					PCGEx::FAttributeIOBase<T>* Reader = SourceDataFacade->GetOrCreateReader<T>(Identity.Name);
					PCGEx::FAttributeIOBase<T>* Writer = TargetDataFacade->GetOrCreateWriter<T>(Reader->Accessor->GetTypedAttribute(), false);
					Readers[i] = Reader;
					Writers[i] = Writer;
				});
		}
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, const int32 TargetIndex)
	{
		const int32 NumAttributes = Identities.Num();

		for (int i = 0; i < NumAttributes; i++)
		{
			const PCGEx::FAttributeIdentity& Identity = Identities[i];

			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					PCGEx::FAttributeIOBase<T>* Reader = static_cast<PCGEx::FAttributeIOBase<T>*>(Readers[i]);
					PCGEx::FAttributeIOBase<T>* Writer = static_cast<PCGEx::FAttributeIOBase<T>*>(Writers[i]);
					Writer->Values[TargetIndex] = Reader->Values[SourceIndex];
				});
		}
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, const FFacade* InTargetDataFacade)
	{
		if (Identities.IsEmpty()) { return; }
		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* SourceAtt = SourceDataFacade->GetIn()->Metadata->GetConstTypedAttribute<T>(Identity.Name);
					InTargetDataFacade->GetOut()->Metadata->DeleteAttribute(Identity.Name);
					FPCGMetadataAttribute<T>* Mark = InTargetDataFacade->GetOut()->Metadata->FindOrCreateAttribute<T>(
						Identity.Name,
						SourceAtt->GetValueFromItemKey(SourceDataFacade->Source->GetInPoint(SourceIndex).MetadataEntry),
						SourceAtt->AllowsInterpolation(), true, true);
				});
		}
	}
}
