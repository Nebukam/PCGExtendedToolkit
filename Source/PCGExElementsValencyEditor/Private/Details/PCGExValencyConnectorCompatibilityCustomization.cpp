// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExValencyConnectorCompatibilityCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "PCGExValencyConnectorCompatibility"

#pragma region FPCGExValencyConnectorEntryCustomization

TSharedRef<IPropertyTypeCustomization> FPCGExValencyConnectorEntryCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExValencyConnectorEntryCustomization());
}

void FPCGExValencyConnectorEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
}

void FPCGExValencyConnectorEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	UPCGExValencyConnectorSet* ConnectorSet = GetOuterConnectorSet(PropertyHandle);

	TSharedPtr<IPropertyHandle> TypeIdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExValencyConnectorEntry, TypeId));
	int32 CurrentTypeId = 0;
	if (TypeIdHandle.IsValid())
	{
		TypeIdHandle->GetValue(CurrentTypeId);
	}

	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(i);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FPCGExValencyConnectorEntry, TypeId))
		{
			continue;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FPCGExValencyConnectorEntry, CompatibleTypeIds))
		{
			ChildBuilder.AddCustomRow(LOCTEXT("CompatibleWith", "Compatible With"))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CompatibleWithLabel", "Compatible With"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				.MinDesiredWidth(200)
				[
					BuildCompatibilityDropdown(ChildHandle, ConnectorSet, CurrentTypeId)
				];
		}
		else
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

UPCGExValencyConnectorSet* FPCGExValencyConnectorEntryCustomization::GetOuterConnectorSet(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	for (UObject* Outer : OuterObjects)
	{
		if (UPCGExValencyConnectorSet* ConnectorSet = Cast<UPCGExValencyConnectorSet>(Outer))
		{
			return ConnectorSet;
		}
	}

	return nullptr;
}

TSharedRef<SWidget> FPCGExValencyConnectorEntryCustomization::BuildCompatibilityDropdown(
	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
	UPCGExValencyConnectorSet* ConnectorSet,
	int32 CurrentTypeId)
{
	if (!ConnectorSet)
	{
		return SNew(STextBlock).Text(LOCTEXT("NoConnectorSet", "No Connector Set"));
	}

	TSharedRef<SComboButton> ComboButton = SNew(SComboButton)
		.ContentPadding(FMargin(4, 2))
		.HasDownArrow(true)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([this, CompatibleTypeIdsHandle, ConnectorSet]()
			{
				return GetCompatibilitySummary(CompatibleTypeIdsHandle, ConnectorSet);
			})
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.MenuContent()
		[
			SNew(SValencyConnectorCompatibilityDropdown)
			.CompatibleTypeIdsHandle(CompatibleTypeIdsHandle)
			.ConnectorSet(ConnectorSet)
			.CurrentTypeId(CurrentTypeId)
		];

	return ComboButton;
}

FText FPCGExValencyConnectorEntryCustomization::GetCompatibilitySummary(
	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
	UPCGExValencyConnectorSet* ConnectorSet) const
{
	if (!CompatibleTypeIdsHandle.IsValid() || !ConnectorSet)
	{
		return LOCTEXT("None", "None");
	}

	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid())
	{
		return LOCTEXT("None", "None");
	}

	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);

	if (NumElements == 0)
	{
		return LOCTEXT("None", "None");
	}

	TArray<FString> TypeNames;
	for (uint32 i = 0; i < NumElements && i < 3; ++i)
	{
		TSharedRef<IPropertyHandle> ElementHandle = ArrayHandle->GetElement(i);
		int32 TypeId = 0;
		ElementHandle->GetValue(TypeId);

		const FName TypeName = ConnectorSet->GetConnectorTypeNameById(TypeId);
		if (!TypeName.IsNone())
		{
			TypeNames.Add(TypeName.ToString());
		}
		else
		{
			const int32 TypeIndex = ConnectorSet->FindConnectorTypeIndexById(TypeId);
			if (TypeIndex != INDEX_NONE)
			{
				TypeNames.Add(FString::Printf(TEXT("Type %d"), TypeIndex));
			}
		}
	}

	if (TypeNames.Num() == 0)
	{
		return FText::Format(LOCTEXT("CountOnly", "{0} types"), FText::AsNumber(NumElements));
	}

	FString Summary = FString::Join(TypeNames, TEXT(", "));
	if (NumElements > 3)
	{
		Summary += FString::Printf(TEXT(" (+%d more)"), NumElements - 3);
	}

	return FText::FromString(Summary);
}

#pragma endregion

#pragma region SValencyConnectorCompatibilityDropdown

void SValencyConnectorCompatibilityDropdown::Construct(const FArguments& InArgs)
{
	CompatibleTypeIdsHandle = InArgs._CompatibleTypeIdsHandle;
	ConnectorSetWeak = InArgs._ConnectorSet;
	CurrentTypeId = InArgs._CurrentTypeId;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SNew(SSearchBox)
			.Visibility_Lambda([this]()
			{
				UPCGExValencyConnectorSet* Rules = ConnectorSetWeak.Get();
				return (Rules && Rules->ConnectorTypes.Num() > 16) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.OnTextChanged(this, &SValencyConnectorCompatibilityDropdown::OnSearchTextChanged)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Self", "Self"))
				.ToolTipText(LOCTEXT("SelfTooltip", "Only compatible with itself"))
				.OnClicked_Lambda([this]() { OnSelfOnly(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("All", "All"))
				.ToolTipText(LOCTEXT("AllTooltip", "Compatible with all types"))
				.OnClicked_Lambda([this]() { OnSelectAll(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Clear", "Clear"))
				.ToolTipText(LOCTEXT("ClearTooltip", "Not compatible with any type"))
				.OnClicked_Lambda([this]() { OnClearAll(); return FReply::Handled(); })
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.MaxHeight(300)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(CheckboxContainer, SVerticalBox)
			]
		]
	];

	RebuildCheckboxList();
}

void SValencyConnectorCompatibilityDropdown::RebuildCheckboxList()
{
	CheckboxContainer->ClearChildren();

	UPCGExValencyConnectorSet* ConnectorSet = ConnectorSetWeak.Get();
	if (!ConnectorSet)
	{
		return;
	}

	for (int32 i = 0; i < ConnectorSet->ConnectorTypes.Num(); ++i)
	{
		const FPCGExValencyConnectorEntry& TypeDef = ConnectorSet->ConnectorTypes[i];
		const FString TypeNameStr = TypeDef.ConnectorType.ToString();

		if (!SearchFilter.IsEmpty() && !TypeNameStr.Contains(SearchFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const int32 TypeId = TypeDef.TypeId;
		const bool bIsSelf = (TypeId == CurrentTypeId);
		const int32 TypeIndex = i;

		CheckboxContainer->AddSlot()
		.AutoHeight()
		.Padding(4, 1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
				.DesiredSizeOverride(FVector2D(10, 10))
				.ColorAndOpacity_Lambda([this, TypeIndex]()
				{
					UPCGExValencyConnectorSet* Rules = ConnectorSetWeak.Get();
					if (Rules && Rules->ConnectorTypes.IsValidIndex(TypeIndex))
					{
						return FSlateColor(Rules->ConnectorTypes[TypeIndex].DebugColor);
					}
					return FSlateColor(FLinearColor::White);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this, TypeId]()
				{
					return IsTypeCompatible(TypeId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, TypeId](ECheckBoxState NewState)
				{
					ToggleTypeCompatibility(TypeId);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			[
				SNew(SBox)
				.WidthOverride(18)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this, TypeIndex, bIsSelf]()
					{
						UPCGExValencyConnectorSet* Rules = ConnectorSetWeak.Get();
						if (!Rules || !Rules->ConnectorTypes.IsValidIndex(TypeIndex))
						{
							return FText::FromString(TEXT(" "));
						}

						const FPCGExValencyConnectorEntry& TypeDef2 = Rules->ConnectorTypes[TypeIndex];
						const bool bWeConnectToThem = IsTypeCompatible(TypeDef2.TypeId);
						const bool bTheyConnectToUs = DoesTypeConnectToUs(TypeDef2.TypeId);

						if (bIsSelf)
						{
							return FText::FromString(TEXT("\u25C9"));
						}
						else if (bWeConnectToThem && bTheyConnectToUs)
						{
							return FText::FromString(TEXT("\u2194"));
						}
						else if (bWeConnectToThem)
						{
							return FText::FromString(TEXT("\u2192"));
						}
						else if (bTheyConnectToUs)
						{
							return FText::FromString(TEXT("\u2190"));
						}
						return FText::FromString(TEXT("\u25CB"));
					})
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this, TypeIndex, bIsSelf]()
				{
					UPCGExValencyConnectorSet* Rules = ConnectorSetWeak.Get();
					if (!Rules || !Rules->ConnectorTypes.IsValidIndex(TypeIndex))
					{
						return LOCTEXT("InvalidType", "<invalid>");
					}

					const FPCGExValencyConnectorEntry& TypeDef2 = Rules->ConnectorTypes[TypeIndex];
					FString Name;
					if (!TypeDef2.ConnectorType.IsNone())
					{
						Name = TypeDef2.ConnectorType.ToString();
					}
					else
					{
						Name = FString::Printf(TEXT("Type %d"), TypeIndex);
					}
					return FText::FromString(Name);
				})
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity_Lambda([bIsSelf]()
				{
					if (bIsSelf)
					{
						return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
					}
					return FSlateColor::UseForeground();
				})
			]
		];
	}
}

void SValencyConnectorCompatibilityDropdown::OnSearchTextChanged(const FText& NewText)
{
	SearchFilter = NewText.ToString();
	RebuildCheckboxList();
}

bool SValencyConnectorCompatibilityDropdown::IsTypeCompatible(int32 TypeId) const
{
	if (!CompatibleTypeIdsHandle.IsValid()) return false;
	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid()) return false;

	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);
	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedRef<IPropertyHandle> ElementHandle = ArrayHandle->GetElement(i);
		int32 StoredTypeId = 0;
		ElementHandle->GetValue(StoredTypeId);
		if (StoredTypeId == TypeId) return true;
	}
	return false;
}

bool SValencyConnectorCompatibilityDropdown::DoesTypeConnectToUs(int32 OtherTypeId) const
{
	UPCGExValencyConnectorSet* ConnectorSet = ConnectorSetWeak.Get();
	if (!ConnectorSet) return false;
	for (const FPCGExValencyConnectorEntry& TypeDef : ConnectorSet->ConnectorTypes)
	{
		if (TypeDef.TypeId == OtherTypeId)
		{
			return TypeDef.CompatibleTypeIds.Contains(CurrentTypeId);
		}
	}
	return false;
}

void SValencyConnectorCompatibilityDropdown::ToggleTypeCompatibility(int32 TypeId)
{
	if (!CompatibleTypeIdsHandle.IsValid()) return;
	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid()) return;

	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);

	int32 FoundIndex = INDEX_NONE;
	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedRef<IPropertyHandle> ElementHandle = ArrayHandle->GetElement(i);
		int32 StoredTypeId = 0;
		ElementHandle->GetValue(StoredTypeId);
		if (StoredTypeId == TypeId)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex != INDEX_NONE)
	{
		ArrayHandle->DeleteItem(FoundIndex);
	}
	else
	{
		ArrayHandle->AddItem();
		ArrayHandle->GetNumElements(NumElements);
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(NumElements - 1);
		NewElement->SetValue(TypeId);
	}

	if (UPCGExValencyConnectorSet* ConnectorSet = ConnectorSetWeak.Get())
	{
		ConnectorSet->Compile();
		ConnectorSet->MarkPackageDirty();
	}
}

void SValencyConnectorCompatibilityDropdown::OnSelectAll()
{
	UPCGExValencyConnectorSet* ConnectorSet = ConnectorSetWeak.Get();
	if (!ConnectorSet || !CompatibleTypeIdsHandle.IsValid()) return;
	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid()) return;

	ArrayHandle->EmptyArray();
	for (const FPCGExValencyConnectorEntry& TypeDef : ConnectorSet->ConnectorTypes)
	{
		ArrayHandle->AddItem();
		uint32 NumElements = 0;
		ArrayHandle->GetNumElements(NumElements);
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(NumElements - 1);
		NewElement->SetValue(TypeDef.TypeId);
	}
	ConnectorSet->Compile();
	ConnectorSet->MarkPackageDirty();
	RebuildCheckboxList();
}

void SValencyConnectorCompatibilityDropdown::OnClearAll()
{
	if (!CompatibleTypeIdsHandle.IsValid()) return;
	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (ArrayHandle.IsValid()) ArrayHandle->EmptyArray();
	if (UPCGExValencyConnectorSet* ConnectorSet = ConnectorSetWeak.Get())
	{
		ConnectorSet->Compile();
		ConnectorSet->MarkPackageDirty();
	}
	RebuildCheckboxList();
}

void SValencyConnectorCompatibilityDropdown::OnSelfOnly()
{
	if (!CompatibleTypeIdsHandle.IsValid()) return;
	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid()) return;

	ArrayHandle->EmptyArray();
	ArrayHandle->AddItem();
	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);
	if (NumElements > 0)
	{
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(0);
		NewElement->SetValue(CurrentTypeId);
	}
	if (UPCGExValencyConnectorSet* ConnectorSet = ConnectorSetWeak.Get())
	{
		ConnectorSet->Compile();
		ConnectorSet->MarkPackageDirty();
	}
	RebuildCheckboxList();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
