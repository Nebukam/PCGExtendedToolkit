// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataCommon.generated.h"

namespace PCGExData
{
	template <typename T>
	class TDataValue;
}

using PCGExValueHash = uint32;
using PCGExDataId = TSharedPtr<PCGExData::TDataValue<int64>>;

UENUM()
enum class EPCGExInputValueType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Use a constant, user-defined value.", ActionIcon="Constant"),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Read the value from the input data.", ActionIcon="Attribute"),
};

UENUM()
enum class EPCGExInputValueToggle : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Use a constant, user-defined value.", ActionIcon="Constant"),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Read the value from the input data.", ActionIcon="Attribute"),
	Disabled  = 2 UMETA(DisplayName = "Disabled", Tooltip="Disabled", ActionIcon="STF_None"),
};

UENUM()
enum class EPCGExDataInputValueType : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant.", ActionIcon="Constant"),
	Attribute = 1 UMETA(DisplayName = "@Data", Tooltip="Attribute. Can only read from @Data domain.", ActionIcon="DataAttribute"),
};

UENUM(BlueprintType)
enum class EPCGExNumericOutput : uint8
{
	Double = 0,
	Float  = 1,
	Int32  = 2,
	Int64  = 3,
};

namespace PCGExData
{
	enum class EIOInit : uint8
	{
		// No Output
		NoInit,
		// Create Empty Output Object
		New,
		// Duplicate Input Object
		Duplicate,
		//Forward Input Object
		Forward,
	};

	enum class EIOSide : uint8
	{
		In,
		Out
	};

	enum class EStaging : uint8
	{
		None              = 0,
		Managed           = 1 << 0,
		Mutable           = 1 << 1,
		Pinless           = 1 << 2,
		MutableAndManaged = Managed | Mutable
	};

	ENUM_CLASS_FLAGS(EStaging)
}
