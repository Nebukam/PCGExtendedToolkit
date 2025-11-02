// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsFiltering.h"

#include "PCGEx.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"

namespace PCGEx
{
	void TagsToData(UPCGData* Data, const TSharedPtr<PCGExData::FTags>& Tags, const EPCGExTagsToDataAction Action)
	{
		if (Action == EPCGExTagsToDataAction::Ignore) { return; }

		if (Action == EPCGExTagsToDataAction::ToData)
		{
			for (const TPair<FString, TSharedPtr<PCGExData::IDataValue>>& ValueTag : Tags->ValueTags)
			{
				PCGEx::ExecuteWithRightType(
					ValueTag.Value->UnderlyingType, [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						TSharedPtr<PCGExData::TDataValue<T>> TypedValue = StaticCastSharedPtr<PCGExData::TDataValue<T>>(ValueTag.Value);
						PCGExDataHelpers::SetDataValue<T>(Data, FName(ValueTag.Key), TypedValue->Value);
					});
			}
		}
		else if (Action == EPCGExTagsToDataAction::ToElements)
		{
			for (const TPair<FString, TSharedPtr<PCGExData::IDataValue>>& ValueTag : Tags->ValueTags)
			{
				PCGEx::ExecuteWithRightType(
					ValueTag.Value->UnderlyingType, [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						TSharedPtr<PCGExData::TDataValue<T>> TypedValue = StaticCastSharedPtr<PCGExData::TDataValue<T>>(ValueTag.Value);
						Data->MutableMetadata()->FindOrCreateAttribute<T>(FName(ValueTag.Key), TypedValue->Value);
					});
			}
		}
	}

	void TagsToData(const TSharedPtr<PCGExData::FPointIO>& Data, const EPCGExTagsToDataAction Action)
	{
		if (Action == EPCGExTagsToDataAction::Ignore) { return; }
		TagsToData(Data->GetOut(), Data->Tags, Action);
	}
}

FPCGExFilterResultDetails::FPCGExFilterResultDetails(bool bTogglable, bool InEnabled)
	: bOptional(bTogglable), bEnabled(InEnabled)
{
}

bool FPCGExFilterResultDetails::Validate(FPCGExContext* InContext) const
{
	if (!bEnabled) { return true; }
	PCGEX_VALIDATE_NAME_C(InContext, ResultAttributeName);
	return true;
}

void FPCGExFilterResultDetails::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (bResultAsIncrement) { IncrementBuffer = InDataFacade->GetWritable<double>(ResultAttributeName, 0, true, PCGExData::EBufferInit::Inherit); }
	else { BoolBuffer = InDataFacade->GetWritable<bool>(ResultAttributeName, false, true, PCGExData::EBufferInit::New); }
}

void FPCGExFilterResultDetails::Write(const int32 Index, bool bPass) const
{
	if (bResultAsIncrement) { IncrementBuffer->SetValue(Index, IncrementBuffer->GetValue(Index) + (bPass ? PassIncrement : FailIncrement)); }
	else { BoolBuffer->SetValue(Index, bPass); }
}

void FPCGExFilterResultDetails::Write(const PCGExMT::FScope& Scope, const TArray<int8>& Results) const
{
	if (bResultAsIncrement)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			IncrementBuffer->SetValue(Index, IncrementBuffer->GetValue(Index) + (Results[Index] ? PassIncrement : FailIncrement));
		}
	}
	else
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			BoolBuffer->SetValue(Index, static_cast<bool>(Results[Index]));
		}
	}
}

void FPCGExFilterResultDetails::Write(const PCGExMT::FScope& Scope, const TBitArray<>& Results) const
{
	if (bResultAsIncrement)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			IncrementBuffer->SetValue(Index, IncrementBuffer->GetValue(Index) + (Results[Index] ? PassIncrement : FailIncrement));
		}
	}
	else
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			BoolBuffer->SetValue(Index, Results[Index]);
		}
	}
}
