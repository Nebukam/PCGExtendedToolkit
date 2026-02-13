// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SValencyInspector.h"

#include "Editor.h"
#include "Selection.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Colors/SColorBlock.h"

#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCagePattern.h"
#include "Cages/PCGExValencyCageNull.h"
#include "Cages/PCGExValencyCageOrbital.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Components/PCGExValencyCageConnectorComponent.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Volumes/ValencyContextVolume.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

void SValencyInspector::Construct(const FArguments& InArgs)
{
	EditorMode = InArgs._EditorMode;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("PCGExValency", "InspectorHeader", "Inspector"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ContentArea, SBox)
		]
	];

	// Bind to selection changes (use AddSP for automatic cleanup when widget is destroyed)
	if (GEditor)
	{
		OnSelectionChangedHandle = GEditor->GetSelectedActors()->SelectionChangedEvent.AddSP(
			this, &SValencyInspector::OnSelectionChangedCallback);

		OnComponentSelectionChangedHandle = GEditor->GetSelectedComponents()->SelectionChangedEvent.AddSP(
			this, &SValencyInspector::OnSelectionChangedCallback);
	}

	RefreshContent();
}

void SValencyInspector::RefreshContent()
{
	if (!ContentArea.IsValid())
	{
		return;
	}

	TSharedRef<SWidget> NewContent = BuildSceneStatsContent();
	bool bFoundSpecificContent = false;

	if (GEditor)
	{
		// Check for selected components first (connector)
		if (USelection* CompSelection = GEditor->GetSelectedComponents())
		{
			for (FSelectionIterator It(*CompSelection); It; ++It)
			{
				if (UPCGExValencyCageConnectorComponent* Connector = Cast<UPCGExValencyCageConnectorComponent>(*It))
				{
					NewContent = BuildConnectorContent(Connector);
					bFoundSpecificContent = true;
					break;
				}
			}
		}

		// If no component selected, check actors
		if (!bFoundSpecificContent)
		{
			USelection* Selection = GEditor->GetSelectedActors();
			for (FSelectionIterator It(*Selection); It; ++It)
			{
				if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(*It))
				{
					NewContent = BuildCageContent(Cage);
					break;
				}
				if (AValencyContextVolume* Volume = Cast<AValencyContextVolume>(*It))
				{
					NewContent = BuildVolumeContent(Volume);
					break;
				}
				if (APCGExValencyAssetPalette* Palette = Cast<APCGExValencyAssetPalette>(*It))
				{
					NewContent = BuildPaletteContent(Palette);
					break;
				}
			}
		}
	}

	StaticCastSharedPtr<SBox>(ContentArea)->SetContent(NewContent);
}

void SValencyInspector::OnSelectionChangedCallback(UObject* InObject)
{
	RefreshContent();
}

TSharedRef<SWidget> SValencyInspector::BuildSceneStatsContent()
{
	if (!EditorMode)
	{
		return SNew(STextBlock)
			.Text(NSLOCTEXT("PCGExValency", "NoSelection", "No selection"))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	}

	const int32 CageCount = EditorMode->GetCachedCages().Num();
	const int32 VolumeCount = EditorMode->GetCachedVolumes().Num();
	const int32 PaletteCount = EditorMode->GetCachedPalettes().Num();

	// Count total assets
	int32 TotalAssets = 0;
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : EditorMode->GetCachedCages())
	{
		if (const APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			TotalAssets += Cage->GetAllAssetEntries().Num();
		}
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "StatsScene", "Scene"),
				FText::Format(NSLOCTEXT("PCGExValency", "StatsSceneValue", "{0} cages, {1} volumes, {2} palettes"),
					FText::AsNumber(CageCount), FText::AsNumber(VolumeCount), FText::AsNumber(PaletteCount)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "StatsTotalAssets", "Total Assets"),
				FText::AsNumber(TotalAssets))
		];
}

TSharedRef<SWidget> SValencyInspector::BuildCageContent(APCGExValencyCageBase* Cage)
{
	if (!Cage)
	{
		return BuildSceneStatsContent();
	}

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	// Name and color
	Content->AddSlot().AutoHeight()
	[
		MakeSectionHeader(FText::FromString(Cage->GetCageDisplayName()))
	];

	if (const APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
	{
		Content->AddSlot().AutoHeight()
		[
			MakeLabeledColorRow(NSLOCTEXT("PCGExValency", "CageColor", "Color"), RegularCage->CageColor)
		];
	}

	// Orbital status
	const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();
	int32 ConnectedCount = 0;
	for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		if (Orbital.GetDisplayConnection() != nullptr)
		{
			ConnectedCount++;
		}
	}

	const FLinearColor HealthColor = (ConnectedCount == Orbitals.Num())
		? FLinearColor(0.2f, 0.8f, 0.2f)
		: (ConnectedCount > 0 ? FLinearColor(1.0f, 0.7f, 0.0f) : FLinearColor(0.8f, 0.2f, 0.2f));

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "CageOrbitals", "Orbitals"),
			FText::Format(NSLOCTEXT("PCGExValency", "CageOrbitalsValue", "{0}/{1} connected"),
				FText::AsNumber(ConnectedCount), FText::AsNumber(Orbitals.Num())))
	];

	// Asset count for regular cages
	if (const APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
	{
		Content->AddSlot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "CageAssets", "Assets"),
				FText::AsNumber(RegularCage->GetAllAssetEntries().Num()))
		];
	}

	// Probe radius
	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "CageProbeRadius", "Probe Radius"),
			FText::Format(NSLOCTEXT("PCGExValency", "CageProbeRadiusValue", "{0}"),
				FText::AsNumber(static_cast<int32>(Cage->GetEffectiveProbeRadius()))))
	];

	// Connector Set status
	if (UPCGExValencyConnectorSet* EffectiveSet = Cage->GetEffectiveConnectorSet())
	{
		Content->AddSlot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "CageConnectorSet", "Connector Set"),
				FText::FromString(EffectiveSet->GetName()))
		];
	}

	// Connector components - interactive section
	TArray<UPCGExValencyCageConnectorComponent*> ConnectorComponents;
	Cage->GetConnectorComponents(ConnectorComponents);
	{
		// Header row with connector count and Add button
		Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				MakeSectionHeader(FText::Format(
					NSLOCTEXT("PCGExValency", "CageConnectors", "Connectors ({0})"),
					FText::AsNumber(ConnectorComponents.Num())))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				MakeAddConnectorButton(Cage)
			]
		];

		for (UPCGExValencyCageConnectorComponent* ConnectorComp : ConnectorComponents)
		{
			if (!ConnectorComp) continue;

			Content->AddSlot().AutoHeight()
			[
				MakeConnectorRow(ConnectorComp)
			];
		}
	}

	// Orbital details (compact list)
	if (Orbitals.Num() > 0)
	{
		Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			MakeSectionHeader(NSLOCTEXT("PCGExValency", "CageOrbitalDetails", "Orbital Connections"))
		];

		for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
		{
			const APCGExValencyCageBase* Connected = Orbital.GetDisplayConnection();
			FString OrbitalInfo;

			if (Connected)
			{
				OrbitalInfo = FString::Printf(TEXT("  [%d] %s -> %s"),
					Orbital.OrbitalIndex,
					*Orbital.OrbitalName.ToString(),
					*Connected->GetCageDisplayName());
			}
			else
			{
				OrbitalInfo = FString::Printf(TEXT("  [%d] %s (unconnected)"),
					Orbital.OrbitalIndex,
					*Orbital.OrbitalName.ToString());
			}

			Content->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(OrbitalInfo))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				.ColorAndOpacity(Connected
					? FSlateColor(FLinearColor(0.7f, 0.9f, 0.7f))
					: FSlateColor(FLinearColor(0.7f, 0.5f, 0.5f)))
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SValencyInspector::BuildConnectorContent(UPCGExValencyCageConnectorComponent* Connector)
{
	if (!Connector)
	{
		return BuildSceneStatsContent();
	}

	TWeakObjectPtr<UPCGExValencyCageConnectorComponent> WeakConnector(Connector);
	TWeakObjectPtr<UPCGExValencyCageEditorMode> WeakMode(EditorMode);

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	// Back to Cage button
	Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
	[
		SNew(SButton)
		.Text(NSLOCTEXT("PCGExValency", "BackToCage", "<< Back to Cage"))
		.ToolTipText(NSLOCTEXT("PCGExValency", "BackToCageTip", "Return to the cage connector list"))
		.OnClicked_Lambda([WeakConnector]() -> FReply
		{
			if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
			{
				if (GEditor && S->GetOwner())
				{
					// Deselect component, keep actor selected -> triggers cage view
					GEditor->SelectComponent(S, false, true);
				}
			}
			return FReply::Handled();
		})
	];

	Content->AddSlot().AutoHeight()
	[
		MakeSectionHeader(FText::Format(
			NSLOCTEXT("PCGExValency", "ConnectorHeader", "Connector: {0}"),
			FText::FromName(Connector->ConnectorName)))
	];

	// Owning cage
	if (const APCGExValencyCageBase* OwnerCage = Cast<APCGExValencyCageBase>(Connector->GetOwner()))
	{
		Content->AddSlot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "ConnectorOwner", "Cage"),
				FText::FromString(OwnerCage->GetCageDisplayName()))
		];
	}

	// Editable Name
	Content->AddSlot().AutoHeight().Padding(0, 2)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 1)
		[
			SNew(SBox)
			.WidthOverride(100)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("PCGExValency", "ConnectorName", "Name"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4, 0)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromName(Connector->ConnectorName))
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorNameTip", "Unique connector identifier within this cage"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.OnTextCommitted_Lambda([WeakConnector, WeakMode](const FText& NewText, ETextCommit::Type CommitType)
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "RenameConnector", "Rename Connector"));
					S->Modify();
					S->ConnectorName = FName(*NewText.ToString());
					if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
					{
						Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
					}
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->OnSceneChanged.Broadcast();
					}
				}
			})
		]
	];

	// Editable Type - dropdown when ConnectorSet available, freeform text fallback
	{
		// Get available types from effective ConnectorSet
		UPCGExValencyConnectorSet* EffectiveSet = nullptr;
		if (const APCGExValencyCageBase* OwnerCage = Cast<APCGExValencyCageBase>(Connector->GetOwner()))
		{
			EffectiveSet = OwnerCage->GetEffectiveConnectorSet();
		}

		TSharedRef<SWidget> TypeWidget = SNullWidget::NullWidget;

		if (EffectiveSet && EffectiveSet->ConnectorTypes.Num() > 0)
		{
			// Build options list
			TSharedPtr<TArray<TSharedPtr<FName>>> TypeOptionsPtr = MakeShared<TArray<TSharedPtr<FName>>>();
			TSharedPtr<TArray<FLinearColor>> TypeColorsPtr = MakeShared<TArray<FLinearColor>>();
			TSharedPtr<FName> CurrentSelection;

			for (const FPCGExValencyConnectorEntry& Entry : EffectiveSet->ConnectorTypes)
			{
				TSharedPtr<FName> Option = MakeShared<FName>(Entry.ConnectorType);
				TypeOptionsPtr->Add(Option);
				TypeColorsPtr->Add(Entry.DebugColor);
				if (Entry.ConnectorType == Connector->ConnectorType)
				{
					CurrentSelection = Option;
				}
			}

			TypeWidget = SNew(SHorizontalBox)
				// Color swatch for current type
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					SNew(SColorBlock)
					.Color_Lambda([WeakConnector, EffectiveSet]() -> FLinearColor
					{
						if (const UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
						{
							if (EffectiveSet)
							{
								const int32 Idx = EffectiveSet->FindConnectorTypeIndex(S->ConnectorType);
								if (EffectiveSet->ConnectorTypes.IsValidIndex(Idx))
								{
									return EffectiveSet->ConnectorTypes[Idx].DebugColor;
								}
							}
						}
						return FLinearColor(0.3f, 0.3f, 0.3f);
					})
					.Size(FVector2D(12, 12))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(TypeOptionsPtr.Get())
					.InitiallySelectedItem(CurrentSelection)
					.OnGenerateWidget_Lambda([TypeOptionsPtr, TypeColorsPtr](TSharedPtr<FName> InItem) -> TSharedRef<SWidget>
					{
						// Find color for this item
						FLinearColor ItemColor(0.3f, 0.3f, 0.3f);
						for (int32 i = 0; i < TypeOptionsPtr->Num(); ++i)
						{
							if ((*TypeOptionsPtr)[i] == InItem)
							{
								ItemColor = (*TypeColorsPtr)[i];
								break;
							}
						}

						return SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 4, 0)
							[
								SNew(SColorBlock)
								.Color(ItemColor)
								.Size(FVector2D(10, 10))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromName(*InItem))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							];
					})
					.OnSelectionChanged_Lambda([WeakConnector, WeakMode](TSharedPtr<FName> NewValue, ESelectInfo::Type)
					{
						if (!NewValue.IsValid()) return;
						if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
						{
							if (S->ConnectorType == *NewValue) return;
							FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ChangeConnectorType", "Change Connector Type"));
							S->Modify();
							S->ConnectorType = *NewValue;
							if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
							{
								Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
							}
							if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
							{
								Mode->OnSceneChanged.Broadcast();
							}
						}
					})
					[
						SNew(STextBlock)
						.Text_Lambda([WeakConnector]() -> FText
						{
							if (const UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
							{
								return FText::FromName(S->ConnectorType);
							}
							return FText::GetEmpty();
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					]
				];
		}
		else
		{
			// Fallback: freeform text when no ConnectorSet available
			TypeWidget = SNew(SEditableTextBox)
				.Text(FText::FromName(Connector->ConnectorType))
				.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorTypeTip", "Connector type name \u2014 determines compatibility during solving. Assign a ConnectorSet for type dropdown."))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.OnTextCommitted_Lambda([WeakConnector, WeakMode](const FText& NewText, ETextCommit::Type CommitType)
				{
					if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
					{
						FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ChangeConnectorType", "Change Connector Type"));
						S->Modify();
						S->ConnectorType = FName(*NewText.ToString());
						if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
						{
							Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
						}
						if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
						{
							Mode->OnSceneChanged.Broadcast();
						}
					}
				});
		}

		Content->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 1)
			[
				SNew(SBox)
				.WidthOverride(100)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PCGExValency", "ConnectorType", "Type"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4, 0)
			[
				TypeWidget
			]
		];
	}

	// Polarity cycling
	auto GetPolarityLabel = [](EPCGExConnectorPolarity P) -> FText
	{
		switch (P)
		{
		case EPCGExConnectorPolarity::Universal: return NSLOCTEXT("PCGExValency", "PolarityUniversalDetail", "Universal *");
		case EPCGExConnectorPolarity::Plug:      return NSLOCTEXT("PCGExValency", "PolarityPlugDetail", "Plug >>");
		case EPCGExConnectorPolarity::Port:      return NSLOCTEXT("PCGExValency", "PolarityPortDetail", "<< Port");
		default:                                 return FText::GetEmpty();
		}
	};

	Content->AddSlot().AutoHeight().Padding(0, 2)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 1)
		[
			SNew(SBox)
			.WidthOverride(100)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("PCGExValency", "ConnectorPolarity", "Polarity"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SButton)
			.Text(GetPolarityLabel(Connector->Polarity))
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorPolarityTip", "Cycle polarity: Universal (connects to any), Plug (outward), Port (inward)"))
			.OnClicked_Lambda([WeakConnector, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "CyclePolarity", "Cycle Connector Polarity"));
					S->Modify();
					// Universal -> Plug -> Port -> Universal
					switch (S->Polarity)
					{
					case EPCGExConnectorPolarity::Universal: S->Polarity = EPCGExConnectorPolarity::Plug; break;
					case EPCGExConnectorPolarity::Plug:      S->Polarity = EPCGExConnectorPolarity::Port; break;
					case EPCGExConnectorPolarity::Port:      S->Polarity = EPCGExConnectorPolarity::Universal; break;
					}
					if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
					{
						Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
					}
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->OnSceneChanged.Broadcast();
						Mode->RedrawViewports();
					}
				}
				return FReply::Handled();
			})
		]
	];

	// Enabled checkbox
	Content->AddSlot().AutoHeight().Padding(0, 2)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 1)
		[
			SNew(SBox)
			.WidthOverride(100)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("PCGExValency", "ConnectorEnabled", "Enabled"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SCheckBox)
			.IsChecked(Connector->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorEnabledTip", "Disabled connectors are ignored during compilation"))
			.OnCheckStateChanged_Lambda([WeakConnector, WeakMode](ECheckBoxState NewState)
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleEnabled", "Toggle Connector Enabled"));
					S->Modify();
					S->bEnabled = (NewState == ECheckBoxState::Checked);
					if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
					{
						Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
					}
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->OnSceneChanged.Broadcast();
						Mode->RedrawViewports();
					}
				}
			})
		]
	];

	// Local offset (read-only, edited via viewport gizmo)
	const FTransform ConnectorTransform = Connector->GetRelativeTransform();
	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "ConnectorLocation", "Local Offset"),
			FText::FromString(ConnectorTransform.GetLocation().ToString()))
	];

	// Action buttons
	Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.Text(NSLOCTEXT("PCGExValency", "DuplicateConnector", "Duplicate"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "DuplicateConnectorTip", "Create a copy of this connector with a small offset (Ctrl+D)"))
			.OnClicked_Lambda([WeakConnector, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->DuplicateConnector(S);
					}
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(NSLOCTEXT("PCGExValency", "RemoveConnectorBtn", "Remove"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "RemoveConnectorTip", "Delete this connector from the cage (Delete key)"))
			.OnClicked_Lambda([WeakConnector, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->RemoveConnector(S);
					}
				}
				return FReply::Handled();
			})
		]
	];

	return Content;
}

TSharedRef<SWidget> SValencyInspector::BuildVolumeContent(AValencyContextVolume* Volume)
{
	if (!Volume)
	{
		return BuildSceneStatsContent();
	}

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot().AutoHeight()
	[
		MakeSectionHeader(FText::FromString(Volume->GetActorNameOrLabel()))
	];

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledColorRow(NSLOCTEXT("PCGExValency", "VolumeColor", "Color"), Volume->DebugColor)
	];

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "VolumeProbeRadius", "Default Probe Radius"),
			FText::AsNumber(static_cast<int32>(Volume->DefaultProbeRadius)))
	];

	// Bonding rules
	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "VolumeBondingRules", "Bonding Rules"),
			Volume->BondingRules
				? FText::FromString(Volume->BondingRules->GetName())
				: NSLOCTEXT("PCGExValency", "None", "(none)"))
	];

	// Connector Set
	{
		UPCGExValencyConnectorSet* EffectiveSet = Volume->GetEffectiveConnectorSet();
		Content->AddSlot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "VolumeConnectorSet", "Connector Set"),
				EffectiveSet
					? FText::FromString(EffectiveSet->GetName())
					: NSLOCTEXT("PCGExValency", "VolumeConnectorSetNone", "(none)"))
		];
	}

	// Count contained cages
	TArray<APCGExValencyCageBase*> ContainedCages;
	Volume->CollectContainedCages(ContainedCages);

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "VolumeContainedCages", "Contained Cages"),
			FText::AsNumber(ContainedCages.Num()))
	];

	// List contained cages
	if (ContainedCages.Num() > 0)
	{
		for (APCGExValencyCageBase* Cage : ContainedCages)
		{
			if (!Cage) continue;

			Content->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("  %s"), *Cage->GetCageDisplayName())))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SValencyInspector::BuildPaletteContent(APCGExValencyAssetPalette* Palette)
{
	if (!Palette)
	{
		return BuildSceneStatsContent();
	}

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot().AutoHeight()
	[
		MakeSectionHeader(FText::FromString(Palette->GetPaletteDisplayName()))
	];

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledColorRow(NSLOCTEXT("PCGExValency", "PaletteColor", "Color"), Palette->PaletteColor)
	];

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "PaletteAssets", "Assets"),
			FText::AsNumber(Palette->GetAllAssetEntries().Num()))
	];

	// Mirroring cages
	TArray<APCGExValencyCage*> MirroringCages;
	Palette->FindMirroringCages(MirroringCages);

	if (MirroringCages.Num() > 0)
	{
		Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			MakeSectionHeader(FText::Format(
				NSLOCTEXT("PCGExValency", "PaletteMirroring", "Mirrored by ({0})"),
				FText::AsNumber(MirroringCages.Num())))
		];

		for (APCGExValencyCage* MirrorCage : MirroringCages)
		{
			if (!MirrorCage) continue;

			Content->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("  %s"), *MirrorCage->GetCageDisplayName())))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			];
		}
	}

	return Content;
}

TSharedRef<SWidget> SValencyInspector::MakeConnectorRow(UPCGExValencyCageConnectorComponent* ConnectorComp)
{
	TWeakObjectPtr<UPCGExValencyCageConnectorComponent> WeakConnector(ConnectorComp);
	TWeakObjectPtr<UPCGExValencyCageEditorMode> WeakMode(EditorMode);

	const FLinearColor TextColor = ConnectorComp->bEnabled
		? FLinearColor::White
		: FLinearColor(0.5f, 0.5f, 0.5f);

	// Resolve type color from ConnectorSet
	FLinearColor TypeColor(0.4f, 0.4f, 0.4f);
	if (const APCGExValencyCageBase* OwnerCage = Cast<APCGExValencyCageBase>(ConnectorComp->GetOwner()))
	{
		if (const UPCGExValencyConnectorSet* EffectiveSet = OwnerCage->GetEffectiveConnectorSet())
		{
			const int32 TypeIdx = EffectiveSet->FindConnectorTypeIndex(ConnectorComp->ConnectorType);
			if (EffectiveSet->ConnectorTypes.IsValidIndex(TypeIdx))
			{
				TypeColor = EffectiveSet->ConnectorTypes[TypeIdx].DebugColor;
			}
		}
	}

	auto GetCompactPolarityLabel = [](EPCGExConnectorPolarity P) -> FText
	{
		switch (P)
		{
		case EPCGExConnectorPolarity::Universal: return NSLOCTEXT("PCGExValency", "PolarityUniversalCompact", "*");
		case EPCGExConnectorPolarity::Plug:      return NSLOCTEXT("PCGExValency", "PolarityPlugCompact", ">>");
		case EPCGExConnectorPolarity::Port:      return NSLOCTEXT("PCGExValency", "PolarityPortCompact", "<<");
		default:                                 return FText::GetEmpty();
		}
	};

	return SNew(SHorizontalBox)
		// Type color dot
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0, 0, 0)
		[
			SNew(SColorBlock)
			.Color(TypeColor)
			.Size(FVector2D(8, 8))
		]
		// Clickable connector name - selects the connector in viewport
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(2, 1)
		[
			SNew(SButton)
			.ContentPadding(FMargin(2, 0))
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorRowNameTip", "Click to select this connector in the viewport"))
			.OnClicked_Lambda([WeakConnector]() -> FReply
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					if (GEditor)
					{
						if (AActor* Owner = S->GetOwner())
						{
							GEditor->SelectActor(Owner, true, true);
						}
						GEditor->SelectComponent(S, true, true);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s [%s]"),
					*ConnectorComp->ConnectorName.ToString(),
					*ConnectorComp->ConnectorType.ToString())))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(TextColor))
			]
		]
		// Polarity cycling button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(GetCompactPolarityLabel(ConnectorComp->Polarity))
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorRowPolarityTip", "Cycle polarity: Universal (connects to any), Plug (outward), Port (inward)"))
			.ContentPadding(FMargin(4, 1))
			.OnClicked_Lambda([WeakConnector, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "CyclePolarity", "Cycle Connector Polarity"));
					S->Modify();
					// Universal -> Plug -> Port -> Universal
					switch (S->Polarity)
					{
					case EPCGExConnectorPolarity::Universal: S->Polarity = EPCGExConnectorPolarity::Plug; break;
					case EPCGExConnectorPolarity::Plug:      S->Polarity = EPCGExConnectorPolarity::Port; break;
					case EPCGExConnectorPolarity::Port:      S->Polarity = EPCGExConnectorPolarity::Universal; break;
					}
					if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
					{
						Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
					}
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->OnSceneChanged.Broadcast();
						Mode->RedrawViewports();
					}
				}
				return FReply::Handled();
			})
		]
		// Enabled checkbox
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0)
		[
			SNew(SCheckBox)
			.IsChecked(ConnectorComp->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorRowEnabledTip", "Enable/disable this connector"))
			.OnCheckStateChanged_Lambda([WeakConnector, WeakMode](ECheckBoxState NewState)
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleEnabled", "Toggle Connector Enabled"));
					S->Modify();
					S->bEnabled = (NewState == ECheckBoxState::Checked);
					if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(S->GetOwner()))
					{
						Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
					}
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->OnSceneChanged.Broadcast();
						Mode->RedrawViewports();
					}
				}
			})
		]
		// Remove button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(NSLOCTEXT("PCGExValency", "RemoveX", "x"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "ConnectorRowRemoveTip", "Remove this connector"))
			.ContentPadding(FMargin(4, 1))
			.OnClicked_Lambda([WeakConnector, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageConnectorComponent* S = WeakConnector.Get())
				{
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->RemoveConnector(S);
					}
				}
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> SValencyInspector::MakeAddConnectorButton(APCGExValencyCageBase* Cage)
{
	TWeakObjectPtr<APCGExValencyCageBase> WeakCage(Cage);
	TWeakObjectPtr<UPCGExValencyCageEditorMode> WeakMode(EditorMode);

	// Check if ConnectorSet is available for type picker
	UPCGExValencyConnectorSet* EffectiveSet = Cage->GetEffectiveConnectorSet();

	if (EffectiveSet && EffectiveSet->ConnectorTypes.Num() > 0)
	{
		// Dropdown button with type menu
		return SNew(SComboButton)
			.ContentPadding(FMargin(4, 1))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("PCGExValency", "AddConnector", "+ Add"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			]
			.OnGetMenuContent_Lambda([WeakCage, WeakMode, EffectiveSet]() -> TSharedRef<SWidget>
			{
				FMenuBuilder MenuBuilder(true, nullptr);

				if (EffectiveSet)
				{
					for (const FPCGExValencyConnectorEntry& Entry : EffectiveSet->ConnectorTypes)
					{
						const FName TypeName = Entry.ConnectorType;
						const FLinearColor TypeColor = Entry.DebugColor;

						MenuBuilder.AddMenuEntry(
							FText::FromName(TypeName),
							FText::Format(NSLOCTEXT("PCGExValency", "AddTypedConnectorTip", "Add connector of type '{0}'"), FText::FromName(TypeName)),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([WeakCage, WeakMode, TypeName]()
							{
								if (APCGExValencyCageBase* C = WeakCage.Get())
								{
									if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
									{
										if (UPCGExValencyCageConnectorComponent* NewConn = Mode->AddConnectorToCage(C))
										{
											NewConn->ConnectorType = TypeName;
										}
									}
								}
							}))
						);
					}
				}

				return MenuBuilder.MakeWidget();
			})
			.ToolTipText(NSLOCTEXT("PCGExValency", "AddConnectorTypedTip", "Add a connector with a specific type"));
	}

	// Fallback: plain button when no ConnectorSet available
	return SNew(SButton)
		.Text(NSLOCTEXT("PCGExValency", "AddConnector", "+ Add"))
		.ToolTipText(NSLOCTEXT("PCGExValency", "AddConnectorTip", "Add a new connector to this cage (Ctrl+Shift+A)"))
		.ContentPadding(FMargin(4, 1))
		.OnClicked_Lambda([WeakCage, WeakMode]() -> FReply
		{
			if (APCGExValencyCageBase* C = WeakCage.Get())
			{
				if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
				{
					Mode->AddConnectorToCage(C);
				}
			}
			return FReply::Handled();
		});
}

TSharedRef<SWidget> SValencyInspector::MakeLabeledRow(const FText& Label, const FText& Value)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 1)
		[
			SNew(SBox)
			.WidthOverride(100)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4, 1)
		[
			SNew(STextBlock)
			.Text(Value)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		];
}

TSharedRef<SWidget> SValencyInspector::MakeLabeledColorRow(const FText& Label, const FLinearColor& Color)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 1)
		[
			SNew(SBox)
			.WidthOverride(100)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 1)
		[
			SNew(SColorBlock)
			.Color(Color)
			.Size(FVector2D(16, 16))
		];
}

TSharedRef<SWidget> SValencyInspector::MakeSectionHeader(const FText& Title)
{
	return SNew(STextBlock)
		.Text(Title)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
		.Margin(FMargin(0, 2, 0, 1));
}
