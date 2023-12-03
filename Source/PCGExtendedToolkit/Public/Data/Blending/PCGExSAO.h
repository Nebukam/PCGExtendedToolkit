// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExSAOMin.h"
#include "PCGExSAOMax.h"
#include "PCGExSAOAverage.h"
#include "PCGExSAOWeight.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExSAO.generated.h"

UENUM(BlueprintType)
enum class EPCGExOperationType : uint8
{
	Average UMETA(DisplayName = "Average", ToolTip="Average values"),
	Weight UMETA(DisplayName = "Weight", ToolTip="TBD"),
	Min UMETA(DisplayName = "Min", ToolTip="TBD"),
	Max UMETA(DisplayName = "Max", ToolTip="TBD"),
};

namespace PCGExSAO
{
	static UPCGExSingleAttributeOperation* Get(EPCGExOperationType Type, PCGEx::FAttributeIdentity Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : SAO = NewObject<UPCGExSAO##_ID##_NAME>(); break;

		UPCGExSingleAttributeOperation* SAO;
		switch (Type)
		{
		case EPCGExOperationType::Average:
#define PCGEX_SAO_AVG(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Average)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_AVG) }
#undef PCGEX_SAO_AVG
			break;
		case EPCGExOperationType::Weight:
#define PCGEX_SAO_WHT(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Weight)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_WHT) }
#undef PCGEX_SAO_WHT
			break;
		case EPCGExOperationType::Min:
#define PCGEX_SAO_MIN(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Min)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_MIN) }
#undef PCGEX_SAO_MIN
			break;
		case EPCGExOperationType::Max:
#define PCGEX_SAO_MAX(_TYPE, _NAME) PCGEX_SAO_NEW(_TYPE, _NAME, Max)
			switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_MAX) }
#undef PCGEX_SAO_MAX
			break;
		default: ;
		}
#undef PCGEX_SAO_NEW
		return SAO;
	}
}

#undef PCGEX_SAO_CLASS
