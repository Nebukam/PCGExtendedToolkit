// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsFiltering.h"

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
						Data->MutableMetadata()->FindOrCreateAttribute<T>(FName(ValueTag.Key),TypedValue->Value);
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
