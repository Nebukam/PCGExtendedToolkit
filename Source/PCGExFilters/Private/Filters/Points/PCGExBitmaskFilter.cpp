// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExBitmaskFilter.h"


#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "Details/PCGExSettingsDetails.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExBitmaskFilterConfig, Bitmask, int64, MaskInput, BitmaskAttribute, Bitmask)

bool UPCGExBitmaskFilterFactory::DomainCheck()
{
	return (Config.MaskInput == EPCGExInputValueType::Constant || PCGExMetaHelpers::IsDataDomainAttribute(Config.BitmaskAttribute)) && PCGExMetaHelpers::IsDataDomainAttribute(Config.FlagsAttribute);
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
		PCGEX_LOG_INVALID_ATTR_HANDLED_C(InContext, Flags, TypedFilterFactory->Config.FlagsAttribute)
		return false;
	}

	MaskReader = TypedFilterFactory->Config.GetValueSettingBitmask(PCGEX_QUIET_HANDLING);
	if (!MaskReader->Init(PointDataFacade)) { return false; }

	Compositions.Reserve(TypedFilterFactory->Config.Compositions.Num());
	for (const FPCGExBitmaskRef& Ref : TypedFilterFactory->Config.Compositions) { Compositions.Add(Ref.GetSimpleBitmask()); }

	return true;
}

bool PCGExPointFilter::FBitmaskFilter::Test(const int32 PointIndex) const
{
	int64 OutMask = MaskReader->Read(PointIndex);
	for (const FPCGExSimpleBitmask& Comp : Compositions) { Comp.Mutate(OutMask); }
	const bool Result = PCGExBitmask::Compare(TypedFilterFactory->Config.Comparison, FlagsReader->Read(PointIndex), OutMask);
	return TypedFilterFactory->Config.bInvertResult ? !Result : Result;
}

bool PCGExPointFilter::FBitmaskFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	int64 OutFlags = 0;
	int64 OutMask = 0;

	if (!PCGExData::Helpers::TryReadDataValue(IO, TypedFilterFactory->Config.FlagsAttribute, OutFlags, PCGEX_QUIET_HANDLING)) { PCGEX_QUIET_HANDLING_RET }

	if (!PCGExData::Helpers::TryGetSettingDataValue(IO, TypedFilterFactory->Config.MaskInput, TypedFilterFactory->Config.BitmaskAttribute, TypedFilterFactory->Config.Bitmask, OutMask, PCGEX_QUIET_HANDLING)) { PCGEX_QUIET_HANDLING_RET }

	for (const FPCGExSimpleBitmask& Comp : Compositions) { Comp.Mutate(OutMask); }

	const bool Result = PCGExBitmask::Compare(TypedFilterFactory->Config.Comparison, OutFlags, OutMask);
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
	case EPCGExBitflagComparison::MatchPartial: DisplayName = TEXT("Contains Any");
		//DisplayName = TEXT("A & B != 0");
		//DisplayName = A + " & " + B + TEXT(" != 0");
		break;
	case EPCGExBitflagComparison::MatchFull: DisplayName = TEXT("Contains All");
		//DisplayName = TEXT("A & B == B");
		//DisplayName = A + " Any " + B + TEXT(" == B");
		break;
	case EPCGExBitflagComparison::MatchStrict: DisplayName = TEXT("Is Exactly");
		//DisplayName = TEXT("A == B");
		//DisplayName = A + " == " + B;
		break;
	case EPCGExBitflagComparison::NoMatchPartial: DisplayName = TEXT("Not Contains Any");
		//DisplayName = TEXT("A & B == 0");
		//DisplayName = A + " & " + B + TEXT(" == 0");
		break;
	case EPCGExBitflagComparison::NoMatchFull: DisplayName = TEXT("Not Contains All");
		//DisplayName = TEXT("A & B != B");
		//DisplayName = A + " & " + B + TEXT(" != B");
		break;
	default: DisplayName = " ?? ";
		break;
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
