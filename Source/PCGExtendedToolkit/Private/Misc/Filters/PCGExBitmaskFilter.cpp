﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBitmaskFilter.h"

#include "PCGExHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataPreloader.h"
#include "Details/PCGExDetailsSettings.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExBitmaskFilterConfig, Bitmask, int64, MaskInput, BitmaskAttribute, Bitmask)

bool UPCGExBitmaskFilterFactory::DomainCheck()
{
	return
		(Config.MaskInput == EPCGExInputValueType::Constant || PCGExHelpers::IsDataDomainAttribute(Config.BitmaskAttribute)) &&
		PCGExHelpers::IsDataDomainAttribute(Config.FlagsAttribute);
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExBitmaskFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FBitmaskFilter>(this);
}

void UPCGExBitmaskFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	FacadePreloader.Register<int64>(InContext, Config.FlagsAttribute);
	if (Config.MaskInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<int64>(InContext, Config.BitmaskAttribute); }
}

bool UPCGExBitmaskFilterFactory::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	if (!Super::RegisterConsumableAttributes(InContext)) { return false; }

	InContext->AddConsumableAttributeName(Config.FlagsAttribute);
	InContext->AddConsumableAttributeName(Config.BitmaskAttribute);

	return true;
}

bool PCGExPointFilter::FBitmaskFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	FlagsReader = PointDataFacade->GetReadable<int64>(TypedFilterFactory->Config.FlagsAttribute, PCGExData::EIOSide::In, true);

	if (!FlagsReader)
	{
		PCGEX_LOG_INVALID_ATTR_C(InContext, Flags, TypedFilterFactory->Config.FlagsAttribute)
		return false;
	}

	MaskReader = TypedFilterFactory->Config.GetValueSettingBitmask();
	if (!MaskReader->Init(PointDataFacade)) { return false; }

	return true;
}

bool PCGExPointFilter::FBitmaskFilter::Test(const int32 PointIndex) const
{
	const bool Result = PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FlagsReader->Read(PointIndex), MaskReader->Read(PointIndex));
	return TypedFilterFactory->Config.bInvertResult ? !Result : Result;
}

bool PCGExPointFilter::FBitmaskFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	int64 OutFlags = 0;
	int64 OutMask = 0;

	if (!PCGExDataHelpers::TryReadDataValue(IO, TypedFilterFactory->Config.FlagsAttribute, OutFlags)) { return false; }
	if (!PCGExDataHelpers::TryGetSettingDataValue(IO, TypedFilterFactory->Config.MaskInput, TypedFilterFactory->Config.BitmaskAttribute, TypedFilterFactory->Config.Bitmask, OutMask)) { return false; }

	const bool Result = PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, OutFlags, OutMask);
	return TypedFilterFactory->Config.bInvertResult ? !Result : Result;
}

PCGEX_CREATE_FILTER_FACTORY(Bitmask)

#if WITH_EDITOR
FString UPCGExBitmaskFilterProviderSettings::GetDisplayName() const
{
	FString A = Config.MaskInput == EPCGExInputValueType::Attribute ? Config.BitmaskAttribute.ToString() : TEXT("(Const)");
	FString B = Config.FlagsAttribute.ToString();
	FString DisplayName;

	switch (Config.Comparison)
	{
	case EPCGExBitflagComparison::MatchPartial:
		DisplayName = TEXT("Contains Any");
		//DisplayName = TEXT("A & B != 0");
		//DisplayName = A + " & " + B + TEXT(" != 0");
		break;
	case EPCGExBitflagComparison::MatchFull:
		DisplayName = TEXT("Contains All");
		//DisplayName = TEXT("A & B == B");
		//DisplayName = A + " Any " + B + TEXT(" == B");
		break;
	case EPCGExBitflagComparison::MatchStrict:
		DisplayName = TEXT("Is Exactly");
		//DisplayName = TEXT("A == B");
		//DisplayName = A + " == " + B;
		break;
	case EPCGExBitflagComparison::NoMatchPartial:
		DisplayName = TEXT("Not Contains Any");
		//DisplayName = TEXT("A & B == 0");
		//DisplayName = A + " & " + B + TEXT(" == 0");
		break;
	case EPCGExBitflagComparison::NoMatchFull:
		DisplayName = TEXT("Not Contains All");
		//DisplayName = TEXT("A & B != B");
		//DisplayName = A + " & " + B + TEXT(" != B");
		break;
	default:
		DisplayName = " ?? ";
		break;
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
