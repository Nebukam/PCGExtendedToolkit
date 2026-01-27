// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExSocketCompatibilityCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Core/PCGExSocketRules.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PCGExSocketCompatibility"

#pragma region FPCGExSocketDefinitionCustomization

TSharedRef<IPropertyTypeCustomization> FPCGExSocketDefinitionCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExSocketDefinitionCustomization());
}

void FPCGExSocketDefinitionCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
}

void FPCGExSocketDefinitionCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	UPCGExSocketRules* SocketRules = GetOuterSocketRules(PropertyHandle);

	// Get the TypeId for this socket definition
	TSharedPtr<IPropertyHandle> TypeIdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExSocketDefinition, TypeId));
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

		// Hide TypeId (internal)
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FPCGExSocketDefinition, TypeId))
		{
			continue;
		}

		// Custom widget for CompatibleTypeIds
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FPCGExSocketDefinition, CompatibleTypeIds))
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
					BuildCompatibilityDropdown(ChildHandle, SocketRules, CurrentTypeId)
				];
		}
		else
		{
			// Default display for other properties
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

UPCGExSocketRules* FPCGExSocketDefinitionCustomization::GetOuterSocketRules(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	for (UObject* Outer : OuterObjects)
	{
		if (UPCGExSocketRules* SocketRules = Cast<UPCGExSocketRules>(Outer))
		{
			return SocketRules;
		}
	}

	return nullptr;
}

TSharedRef<SWidget> FPCGExSocketDefinitionCustomization::BuildCompatibilityDropdown(
	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
	UPCGExSocketRules* SocketRules,
	int32 CurrentTypeId)
{
	if (!SocketRules)
	{
		return SNew(STextBlock).Text(LOCTEXT("NoSocketRules", "No Socket Rules"));
	}

	TSharedRef<SComboButton> ComboButton = SNew(SComboButton)
		.ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
		.ForegroundColor(FAppStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
		.ContentPadding(FMargin(2, 2, 2, 1))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([this, CompatibleTypeIdsHandle, SocketRules]()
			{
				return GetCompatibilitySummary(CompatibleTypeIdsHandle, SocketRules);
			})
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.MenuContent()
		[
			SNew(SSocketCompatibilityDropdown)
			.CompatibleTypeIdsHandle(CompatibleTypeIdsHandle)
			.SocketRules(SocketRules)
			.CurrentTypeId(CurrentTypeId)
		];

	return ComboButton;
}

FText FPCGExSocketDefinitionCustomization::GetCompatibilitySummary(
	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
	UPCGExSocketRules* SocketRules) const
{
	if (!CompatibleTypeIdsHandle.IsValid() || !SocketRules)
	{
		return LOCTEXT("None", "None");
	}

	// Get the array of compatible type IDs
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

	// Build summary string
	TArray<FString> TypeNames;
	for (uint32 i = 0; i < NumElements && i < 3; ++i) // Show max 3 names
	{
		TSharedRef<IPropertyHandle> ElementHandle = ArrayHandle->GetElement(i);
		int32 TypeId = 0;
		ElementHandle->GetValue(TypeId);

		const FText DisplayName = SocketRules->GetSocketTypeDisplayNameById(TypeId);
		if (!DisplayName.IsEmpty())
		{
			TypeNames.Add(DisplayName.ToString());
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

#pragma region SSocketCompatibilityDropdown

void SSocketCompatibilityDropdown::Construct(const FArguments& InArgs)
{
	CompatibleTypeIdsHandle = InArgs._CompatibleTypeIdsHandle;
	SocketRulesWeak = InArgs._SocketRules;
	CurrentTypeId = InArgs._CurrentTypeId;

	ChildSlot
	[
		SNew(SVerticalBox)
		// Search box (shown if >16 types)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SNew(SSearchBox)
			.Visibility_Lambda([this]()
			{
				UPCGExSocketRules* Rules = SocketRulesWeak.Get();
				return (Rules && Rules->SocketTypes.Num() > 16) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.OnTextChanged(this, &SSocketCompatibilityDropdown::OnSearchTextChanged)
		]
		// Quick action buttons
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
		// Separator
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SNew(SSeparator)
		]
		// Scrollable checkbox list
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

void SSocketCompatibilityDropdown::RebuildCheckboxList()
{
	CheckboxContainer->ClearChildren();

	UPCGExSocketRules* SocketRules = SocketRulesWeak.Get();
	if (!SocketRules)
	{
		return;
	}

	for (int32 i = 0; i < SocketRules->SocketTypes.Num(); ++i)
	{
		const FPCGExSocketDefinition& TypeDef = SocketRules->SocketTypes[i];
		const FString DisplayName = TypeDef.GetDisplayName().ToString();

		// Apply search filter
		if (!SearchFilter.IsEmpty() && !DisplayName.Contains(SearchFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const int32 TypeId = TypeDef.TypeId;
		const bool bIsSelf = (TypeId == CurrentTypeId);

		CheckboxContainer->AddSlot()
		.AutoHeight()
		.Padding(4, 1)
		[
			SNew(SHorizontalBox)
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
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(DisplayName))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(bIsSelf ? FSlateColor(FLinearColor::Yellow) : FSlateColor::UseForeground())
			]
		];
	}
}

void SSocketCompatibilityDropdown::OnSearchTextChanged(const FText& NewText)
{
	SearchFilter = NewText.ToString();
	RebuildCheckboxList();
}

bool SSocketCompatibilityDropdown::IsTypeCompatible(int32 TypeId) const
{
	if (!CompatibleTypeIdsHandle.IsValid())
	{
		return false;
	}

	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid())
	{
		return false;
	}

	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);

	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedRef<IPropertyHandle> ElementHandle = ArrayHandle->GetElement(i);
		int32 StoredTypeId = 0;
		ElementHandle->GetValue(StoredTypeId);
		if (StoredTypeId == TypeId)
		{
			return true;
		}
	}

	return false;
}

void SSocketCompatibilityDropdown::ToggleTypeCompatibility(int32 TypeId)
{
	if (!CompatibleTypeIdsHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid())
	{
		return;
	}

	// Check if already in list
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
		// Remove from list
		ArrayHandle->DeleteItem(FoundIndex);
	}
	else
	{
		// Add to list
		ArrayHandle->AddItem();
		ArrayHandle->GetNumElements(NumElements);
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(NumElements - 1);
		NewElement->SetValue(TypeId);
	}

	// Trigger compile on the outer object
	if (UPCGExSocketRules* SocketRules = SocketRulesWeak.Get())
	{
		SocketRules->Compile();
		SocketRules->MarkPackageDirty();
	}
}

void SSocketCompatibilityDropdown::OnSelectAll()
{
	UPCGExSocketRules* SocketRules = SocketRulesWeak.Get();
	if (!SocketRules || !CompatibleTypeIdsHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid())
	{
		return;
	}

	// Clear and add all type IDs
	ArrayHandle->EmptyArray();

	for (const FPCGExSocketDefinition& TypeDef : SocketRules->SocketTypes)
	{
		ArrayHandle->AddItem();
		uint32 NumElements = 0;
		ArrayHandle->GetNumElements(NumElements);
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(NumElements - 1);
		NewElement->SetValue(TypeDef.TypeId);
	}

	SocketRules->Compile();
	SocketRules->MarkPackageDirty();
}

void SSocketCompatibilityDropdown::OnClearAll()
{
	if (!CompatibleTypeIdsHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (ArrayHandle.IsValid())
	{
		ArrayHandle->EmptyArray();
	}

	if (UPCGExSocketRules* SocketRules = SocketRulesWeak.Get())
	{
		SocketRules->Compile();
		SocketRules->MarkPackageDirty();
	}
}

void SSocketCompatibilityDropdown::OnSelfOnly()
{
	if (!CompatibleTypeIdsHandle.IsValid())
	{
		return;
	}

	TSharedPtr<IPropertyHandleArray> ArrayHandle = CompatibleTypeIdsHandle->AsArray();
	if (!ArrayHandle.IsValid())
	{
		return;
	}

	// Clear and add only self
	ArrayHandle->EmptyArray();
	ArrayHandle->AddItem();

	uint32 NumElements = 0;
	ArrayHandle->GetNumElements(NumElements);
	if (NumElements > 0)
	{
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(0);
		NewElement->SetValue(CurrentTypeId);
	}

	if (UPCGExSocketRules* SocketRules = SocketRulesWeak.Get())
	{
		SocketRules->Compile();
		SocketRules->MarkPackageDirty();
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
