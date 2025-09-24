﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExValueHashFilter.h"

#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExValueHashFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

PCGExFactories::EPreparationResult UPCGExValueHashFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGExFactories::EPreparationResult Result = Super::Prepare(InContext, AsyncManager);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	PCGExData::TryGetFacades(InContext, FName("Sets"), SetSources, false, true);

	if (SetSources.IsEmpty())
	{
		if (MissingDataHandling == EPCGExFilterNoDataFallback::Error) { if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No valid set found")); } }
		return PCGExFactories::EPreparationResult::MissingData;
	}

	PCGEx::InitArray(Hashes, SetSources.Num());

	TWeakPtr<FPCGContextHandle> CtxHandle = InContext->GetOrCreateHandle();
	PCGEX_ASYNC_GROUP_CHKD_CUSTOM(AsyncManager, GrabUniqueValues, PCGExFactories::EPreparationResult::Fail)

	GrabUniqueValues->OnCompleteCallback =
		[CtxHandle, this]()
		{
			PCGEX_SHARED_CONTEXT_VOID(CtxHandle)

			if (Config.Mode == EPCGExValueHashMode::Merged)
			{
				TSet<uint32>& MergedSet = Hashes[0];
				for (TSet<uint32>& H : Hashes) { MergedSet.Append(H); }
				Hashes.SetNum(1);

				if (Hashes[0].IsEmpty())
				{
					if (MissingDataHandling == EPCGExFilterNoDataFallback::Error) { if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, SharedContext.Get(), FTEXT("Merged sets are empty")); } }
					PrepResult = PCGExFactories::EPreparationResult::MissingData;
				}
			}
			else
			{
				int32 WriteIndex = 0;
				for (TSet<uint32>& H : Hashes)
				{
					if (H.IsEmpty()) { continue; }
					Hashes[WriteIndex++] = MoveTemp(H);
				}

				Hashes.SetNum(WriteIndex);

				if (Hashes.IsEmpty())
				{
					if (MissingDataHandling == EPCGExFilterNoDataFallback::Error) { if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, SharedContext.Get(), FTEXT("Merged sets are empty")); } }
					PrepResult = PCGExFactories::EPreparationResult::MissingData;
				}
			}

			SetSources.Empty();
		};

	GrabUniqueValues->OnIterationCallback =
		[CtxHandle, this](const int32 Index, const PCGExMT::FScope& Scope)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPolyPathFilterFactory::CreatePolyPath);

			PCGEX_SHARED_CONTEXT_VOID(CtxHandle)

			Hashes[Index] = TSet<uint32>();
			TSet<uint32>& UniqueValues = Hashes[Index];

			TSharedPtr<PCGExData::FFacade> SourceFacade = SetSources[Index];

			FPCGAttributeIdentifier Identifier;

			if (Config.SetAttributeName.IsNone())
			{
				TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(SourceFacade->GetIn()->Metadata);
				if (Infos->Attributes.IsEmpty()) { return; }

				Identifier = Infos->Identities[0].Identifier;
			}
			else
			{
				Identifier = PCGEx::GetAttributeIdentifier(Config.SetAttributeName, SourceFacade->GetIn());
			}

			TSharedPtr<PCGExData::IBuffer> Buffer = SourceFacade->GetDefaultReadable(Identifier, PCGExData::EIOSide::In, false);

			if (!Buffer)
			{
				PCGEX_LOG_INVALID_ATTR_C(SharedContext.Get(), SetAttributeName, Identifier.Name)
				return;
			}

			const int32 NumValues = Buffer->GetNumValues(PCGExData::EIOSide::In);
			for (int i = 0; i < NumValues; i++) { UniqueValues.Add(Buffer->ReadValueHash(i)); }
		};

	GrabUniqueValues->StartIterations(SetSources.Num(), 1);

	return Result;
}

bool UPCGExValueHashFilterFactory::DomainCheck()
{
	return PCGExHelpers::IsDataDomainAttribute(Config.OperandA);
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExValueHashFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FValueHashFilter>(this);
}

void UPCGExValueHashFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	FacadePreloader.Register<double>(InContext, Config.OperandA);
}

bool UPCGExValueHashFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }
	InContext->AddConsumableAttributeName(Config.OperandA);
	return true;
}

bool PCGExPointFilter::FValueHashFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	bInvert = TypedFilterFactory->Config.bInvert;
	bAnyPass = TypedFilterFactory->Config.Mode == EPCGExValueHashMode::Individual ? TypedFilterFactory->Config.Inclusion == EPCGExValueHashSetInclusionMode::Any : true;

	FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(TypedFilterFactory->Config.OperandA, PointDataFacade->GetIn());
	OperandA = InPointDataFacade->GetDefaultReadable(Identifier, PCGExData::EIOSide::In, true);

	if (!OperandA)
	{
		PCGEX_LOG_INVALID_ATTR_C(InContext, Operand A, TypedFilterFactory->Config.OperandA)
		return false;
	}

	return true;
}

bool PCGExPointFilter::FValueHashFilter::Test(const int32 PointIndex) const
{
	const double H = OperandA->ReadValueHash(PointIndex);
	bool bPass = false;
	if (bAnyPass)
	{
		for (const TSet<uint32>& HashSet : *Hashes)
		{
			if (HashSet.Contains(H))
			{
				bPass = true;
				break;
			}
		}
	}
	else
	{
		bPass = true;
		for (const TSet<uint32>& HashSet : *Hashes)
		{
			if (!HashSet.Contains(H))
			{
				bPass = false;
				break;
			}
		}
	}

	return bPass ? !bInvert : bInvert;
}

bool PCGExPointFilter::FValueHashFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	double H = 0;

	if (!PCGExDataHelpers::TryReadDataValue(IO, TypedFilterFactory->Config.OperandA, H)) { return bInvert; }

	bool bPass = false;
	if (bAnyPass)
	{
		for (const TSet<uint32>& HashSet : *Hashes)
		{
			if (HashSet.Contains(H))
			{
				bPass = true;
				break;
			}
		}
	}
	else
	{
		bPass = true;
		for (const TSet<uint32>& HashSet : *Hashes)
		{
			if (!HashSet.Contains(H))
			{
				bPass = false;
				break;
			}
		}
	}

	return bPass ? !bInvert : bInvert;
}

TArray<FPCGPinProperties> UPCGExValueHashFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(FName("Sets"), "Data from value set will be extracted", Required)
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(ValueHash)

#if WITH_EDITOR
FString UPCGExValueHashFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Is ") + Config.OperandA.ToString();
	if (Config.Mode == EPCGExValueHashMode::Individual || Config.Inclusion == EPCGExValueHashSetInclusionMode::Any)
	{
		DisplayName += TEXT(" in any set");
	}
	else
	{
		DisplayName += TEXT(" in all set");
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
