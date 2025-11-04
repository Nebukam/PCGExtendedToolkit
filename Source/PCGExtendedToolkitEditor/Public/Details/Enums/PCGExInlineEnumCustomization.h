// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExInlineEnumCustomization : public IPropertyTypeCustomization
{
public:
	explicit FPCGExInlineEnumCustomization(const FString& InEnumName);

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

protected:
	FString EnumName = TEXT("");
	TSharedPtr<IPropertyHandle> EnumHandle;

	virtual TSharedRef<SWidget> GenerateEnumButtons(UEnum* Enum);
};

#pragma region Simple enums

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExInputValueTypeCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExInputValueTypeCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExInputValueTypeCustomization(TEXT("EPCGExInputValueType")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExDataInputValueTypeCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExDataInputValueTypeCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExDataInputValueTypeCustomization(TEXT("EPCGExDataInputValueType")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExApplyAxisFlagCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExApplyAxisFlagCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExApplyAxisFlagCustomization(TEXT("EPCGExApplySampledComponentFlags")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExOptionStateCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExOptionStateCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExOptionStateCustomization(TEXT("EPCGExOptionState")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExFilterNoDataFallbackCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExFilterNoDataFallbackCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExFilterNoDataFallbackCustomization(TEXT("EPCGExFilterNoDataFallback")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExBoundsSourceCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExBoundsSourceCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExBoundsSourceCustomization(TEXT("EPCGExPointBoundsSource")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExDistanceCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExDistanceCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExDistanceCustomization(TEXT("EPCGExDistance")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExClusterElementCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExClusterElementCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExClusterElementCustomization(TEXT("EPCGExClusterElement")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExAttributeFilterCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExAttributeFilterCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExAttributeFilterCustomization(TEXT("EPCGExAttributeFilter")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExScaleToFitCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExScaleToFitCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExScaleToFitCustomization(TEXT("EPCGExScaleToFit")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExJustifyFromCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExJustifyFromCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExJustifyFromCustomization(TEXT("EPCGExJustifyFrom")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExJustifyToCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExJustifyToCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExJustifyToCustomization(TEXT("EPCGExJustifyTo")));
	}
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExFitModeCustomization final : public FPCGExInlineEnumCustomization
{
public:
	explicit FPCGExFitModeCustomization(const FString& InEnumName) : FPCGExInlineEnumCustomization(InEnumName)
	{
	}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPCGExFitModeCustomization(TEXT("EPCGExFitMode")));
	}
};
#pragma endregion
