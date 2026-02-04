// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExValencySocketCompatibilityCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Core/PCGExValencySocketRules.h"
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

#define LOCTEXT_NAMESPACE "PCGExValencySocketCompatibility"

#pragma region FPCGExValencySocketDefinitionCustomization

TSharedRef<IPropertyTypeCustomization> FPCGExValencySocketDefinitionCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExValencySocketDefinitionCustomization());
}

void FPCGExValencySocketDefinitionCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
}

void FPCGExValencySocketDefinitionCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	UPCGExValencySocketRules* SocketRules = GetOuterSocketRules(PropertyHandle);

	// Get the TypeId for this socket definition
	TSharedPtr<IPropertyHandle> TypeIdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExValencySocketDefinition, TypeId));
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
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FPCGExValencySocketDefinition, TypeId))
		{
			continue;
		}

		// Custom widget for CompatibleTypeIds
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FPCGExValencySocketDefinition, CompatibleTypeIds))
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

UPCGExValencySocketRules* FPCGExValencySocketDefinitionCustomization::GetOuterSocketRules(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	for (UObject* Outer : OuterObjects)
	{
		if (UPCGExValencySocketRules* SocketRules = Cast<UPCGExValencySocketRules>(Outer))
		{
			return SocketRules;
		}
	}

	return nullptr;
}

TSharedRef<SWidget> FPCGExValencySocketDefinitionCustomization::BuildCompatibilityDropdown(
	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
	UPCGExValencySocketRules* SocketRules,
	int32 CurrentTypeId)
{
	if (!SocketRules)
	{
		return SNew(STextBlock).Text(LOCTEXT("NoSocketRules", "No Socket Rules"));
	}

	TSharedRef<SComboButton> ComboButton = SNew(SComboButton)
		.ContentPadding(FMargin(4, 2))
		.HasDownArrow(true)
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
			SNew(SValencySocketCompatibilityDropdown)
			.CompatibleTypeIdsHandle(CompatibleTypeIdsHandle)
			.SocketRules(SocketRules)
			.CurrentTypeId(CurrentTypeId)
		];

	return ComboButton;
}

FText FPCGExValencySocketDefinitionCustomization::GetCompatibilitySummary(
	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
	UPCGExValencySocketRules* SocketRules) const
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
		if (!DisplayName.IsEmpty() && !DisplayName.EqualTo(FText::FromName(NAME_None)))
		{
			TypeNames.Add(DisplayName.ToString());
		}
		else
		{
			// Try to find index for unnamed type
			const int32 TypeIndex = SocketRules->FindSocketTypeIndexById(TypeId);
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

#pragma region SValencySocketCompatibilityDropdown

void SValencySocketCompatibilityDropdown::Construct(const FArguments& InArgs)
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
				UPCGExValencySocketRules* Rules = SocketRulesWeak.Get();
				return (Rules && Rules->SocketTypes.Num() > 16) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.OnTextChanged(this, &SValencySocketCompatibilityDropdown::OnSearchTextChanged)
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

void SValencySocketCompatibilityDropdown::RebuildCheckboxList()
{
	CheckboxContainer->ClearChildren();

	UPCGExValencySocketRules* SocketRules = SocketRulesWeak.Get();
	if (!SocketRules)
	{
		return;
	}

	for (int32 i = 0; i < SocketRules->SocketTypes.Num(); ++i)
	{
		const FPCGExValencySocketDefinition& TypeDef = SocketRules->SocketTypes[i];
		const FString DisplayName = TypeDef.GetDisplayName().ToString();

		// Apply search filter
		if (!SearchFilter.IsEmpty() && !DisplayName.Contains(SearchFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const int32 TypeId = TypeDef.TypeId;
		const bool bIsSelf = (TypeId == CurrentTypeId);
		const int32 TypeIndex = i; // Capture index for lambda

		CheckboxContainer->AddSlot()
		.AutoHeight()
		.Padding(4, 1)
		[
			SNew(SHorizontalBox)
			// Color dot indicator (rounded)
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
					UPCGExValencySocketRules* Rules = SocketRulesWeak.Get();
					if (Rules && Rules->SocketTypes.IsValidIndex(TypeIndex))
					{
						return FSlateColor(Rules->SocketTypes[TypeIndex].DebugColor);
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
			// Fixed-width symbol column for alignment
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			[
				SNew(SBox)
				.WidthOverride(18) // Fixed width for symbol
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this, TypeIndex, bIsSelf]()
					{
						UPCGExValencySocketRules* Rules = SocketRulesWeak.Get();
						if (!Rules || !Rules->SocketTypes.IsValidIndex(TypeIndex))
						{
							return FText::FromString(TEXT(" "));
						}

						const FPCGExValencySocketDefinition& TypeDef = Rules->SocketTypes[TypeIndex];
						const bool bWeConnectToThem = IsTypeCompatible(TypeDef.TypeId);
						const bool bTheyConnectToUs = DoesTypeConnectToUs(TypeDef.TypeId);

						if (bIsSelf)
						{
							return FText::FromString(TEXT("\u25C9")); // ◉ self
						}
						else if (bWeConnectToThem && bTheyConnectToUs)
						{
							return FText::FromString(TEXT("\u2194")); // ↔ bidirectional
						}
						else if (bWeConnectToThem)
						{
							return FText::FromString(TEXT("\u2192")); // → outgoing
						}
						else if (bTheyConnectToUs)
						{
							return FText::FromString(TEXT("\u2190")); // ← incoming
						}
						return FText::FromString(TEXT("\u25CB")); // ○ no connection
					})
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			// Type name
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([this, TypeIndex, bIsSelf]()
				{
					UPCGExValencySocketRules* Rules = SocketRulesWeak.Get();
					if (!Rules || !Rules->SocketTypes.IsValidIndex(TypeIndex))
					{
						return LOCTEXT("InvalidType", "<invalid>");
					}

					const FPCGExValencySocketDefinition& TypeDef = Rules->SocketTypes[TypeIndex];

					// Get display name
					FString Name;
					const FText DisplayNameText = TypeDef.GetDisplayName();
					if (!DisplayNameText.IsEmpty() && !DisplayNameText.EqualTo(FText::FromName(NAME_None)))
					{
						Name = DisplayNameText.ToString();
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
					// Dim self, normal for others
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

void SValencySocketCompatibilityDropdown::OnSearchTextChanged(const FText& NewText)
{
	SearchFilter = NewText.ToString();
	RebuildCheckboxList();
}

bool SValencySocketCompatibilityDropdown::IsTypeCompatible(int32 TypeId) const
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

bool SValencySocketCompatibilityDropdown::DoesTypeConnectToUs(int32 OtherTypeId) const
{
	UPCGExValencySocketRules* SocketRules = SocketRulesWeak.Get();
	if (!SocketRules)
	{
		return false;
	}

	// Find the other type's definition and check if it has us in its compatible list
	for (const FPCGExValencySocketDefinition& TypeDef : SocketRules->SocketTypes)
	{
		if (TypeDef.TypeId == OtherTypeId)
		{
			// Check if CurrentTypeId is in their CompatibleTypeIds
			return TypeDef.CompatibleTypeIds.Contains(CurrentTypeId);
		}
	}

	return false;
}

void SValencySocketCompatibilityDropdown::ToggleTypeCompatibility(int32 TypeId)
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
	if (UPCGExValencySocketRules* SocketRules = SocketRulesWeak.Get())
	{
		SocketRules->Compile();
		SocketRules->MarkPackageDirty();
	}
}

void SValencySocketCompatibilityDropdown::OnSelectAll()
{
	UPCGExValencySocketRules* SocketRules = SocketRulesWeak.Get();
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

	for (const FPCGExValencySocketDefinition& TypeDef : SocketRules->SocketTypes)
	{
		ArrayHandle->AddItem();
		uint32 NumElements = 0;
		ArrayHandle->GetNumElements(NumElements);
		TSharedRef<IPropertyHandle> NewElement = ArrayHandle->GetElement(NumElements - 1);
		NewElement->SetValue(TypeDef.TypeId);
	}

	SocketRules->Compile();
	SocketRules->MarkPackageDirty();

	// Rebuild to refresh checkbox states
	RebuildCheckboxList();
}

void SValencySocketCompatibilityDropdown::OnClearAll()
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

	if (UPCGExValencySocketRules* SocketRules = SocketRulesWeak.Get())
	{
		SocketRules->Compile();
		SocketRules->MarkPackageDirty();
	}

	// Rebuild to refresh checkbox states
	RebuildCheckboxList();
}

void SValencySocketCompatibilityDropdown::OnSelfOnly()
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

	if (UPCGExValencySocketRules* SocketRules = SocketRulesWeak.Get())
	{
		SocketRules->Compile();
		SocketRules->MarkPackageDirty();
	}

	// Rebuild to refresh checkbox states
	RebuildCheckboxList();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
