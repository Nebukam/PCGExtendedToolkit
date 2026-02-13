// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SValencySceneOverview.h"

#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Images/SImage.h"

#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCagePattern.h"
#include "Cages/PCGExValencyCageNull.h"
#include "Cages/PCGExValencyCageOrbital.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"

void SValencySceneOverview::Construct(const FArguments& InArgs)
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
			.Text(NSLOCTEXT("PCGExValency", "SceneOverviewHeader", "Scene"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.MaxHeight(300.0f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FValencySceneEntry>>)
			.ListItemsSource(&Entries)
			.OnGenerateRow(this, &SValencySceneOverview::OnGenerateRow)
			.OnSelectionChanged(this, &SValencySceneOverview::OnSelectionChanged)
			.OnMouseButtonDoubleClick(this, &SValencySceneOverview::OnDoubleClick)
			.SelectionMode(ESelectionMode::Single)
		]
	];

	// Build initial list
	RebuildList();

	// Bind to scene changes
	if (EditorMode)
	{
		OnSceneChangedHandle = EditorMode->OnSceneChanged.AddSP(this, &SValencySceneOverview::RebuildList);
	}
}

void SValencySceneOverview::RebuildList()
{
	Entries.Empty();

	if (!EditorMode)
	{
		if (ListView.IsValid())
		{
			ListView->RequestListRefresh();
		}
		return;
	}

	// Volumes section
	const auto& CachedVolumes = EditorMode->GetCachedVolumes();
	if (CachedVolumes.Num() > 0)
	{
		auto Header = MakeShared<FValencySceneEntry>();
		Header->Type = FValencySceneEntry::EType::GroupHeader;
		Header->DisplayName = FString::Printf(TEXT("Volumes (%d)"), CachedVolumes.Num());
		Entries.Add(Header);

		for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : CachedVolumes)
		{
			if (AValencyContextVolume* Volume = VolumePtr.Get())
			{
				auto Entry = MakeShared<FValencySceneEntry>();
				Entry->Type = FValencySceneEntry::EType::Volume;
				Entry->Actor = Volume;
				Entry->DisplayName = Volume->GetActorNameOrLabel();
				Entry->IconColor = Volume->DebugColor;
				Entries.Add(Entry);
			}
		}
	}

	// Cages section
	const auto& CachedCages = EditorMode->GetCachedCages();
	if (CachedCages.Num() > 0)
	{
		auto Header = MakeShared<FValencySceneEntry>();
		Header->Type = FValencySceneEntry::EType::GroupHeader;
		Header->DisplayName = FString::Printf(TEXT("Cages (%d)"), CachedCages.Num());
		Entries.Add(Header);

		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
		{
			if (APCGExValencyCageBase* Cage = CagePtr.Get())
			{
				auto Entry = MakeShared<FValencySceneEntry>();
				Entry->Type = FValencySceneEntry::EType::Cage;
				Entry->Actor = Cage;
				Entry->DisplayName = Cage->GetCageDisplayName();

				// Count connected orbitals
				const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();
				Entry->TotalOrbitals = Orbitals.Num();
				Entry->ConnectedOrbitals = 0;
				for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
				{
					if (Orbital.GetDisplayConnection() != nullptr)
					{
						Entry->ConnectedOrbitals++;
					}
				}

				// Asset count for regular cages
				if (const APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
				{
					Entry->AssetCount = RegularCage->GetAllAssetEntries().Num();
					Entry->IconColor = RegularCage->CageColor;
				}
				else
				{
					Entry->IconColor = FLinearColor::White;
				}

				// Warning if unconnected orbitals or no assets
				Entry->bHasWarnings = (Entry->ConnectedOrbitals < Entry->TotalOrbitals) ||
					(Cast<APCGExValencyCage>(Cage) && Entry->AssetCount == 0);

				Entries.Add(Entry);
			}
		}
	}

	// Palettes section
	const auto& CachedPalettes = EditorMode->GetCachedPalettes();
	if (CachedPalettes.Num() > 0)
	{
		auto Header = MakeShared<FValencySceneEntry>();
		Header->Type = FValencySceneEntry::EType::GroupHeader;
		Header->DisplayName = FString::Printf(TEXT("Palettes (%d)"), CachedPalettes.Num());
		Entries.Add(Header);

		for (const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr : CachedPalettes)
		{
			if (APCGExValencyAssetPalette* Palette = PalettePtr.Get())
			{
				auto Entry = MakeShared<FValencySceneEntry>();
				Entry->Type = FValencySceneEntry::EType::Palette;
				Entry->Actor = Palette;
				Entry->DisplayName = Palette->GetPaletteDisplayName();
				Entry->IconColor = Palette->PaletteColor;
				Entry->AssetCount = Palette->GetAllAssetEntries().Num();
				Entries.Add(Entry);
			}
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SValencySceneOverview::OnGenerateRow(TSharedPtr<FValencySceneEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Item->Type == FValencySceneEntry::EType::GroupHeader)
	{
		return SNew(STableRow<TSharedPtr<FValencySceneEntry>>, OwnerTable)
			.IsEnabled(false)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayName))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
			];
	}

	// Build status text
	FString StatusText;
	if (Item->Type == FValencySceneEntry::EType::Cage)
	{
		if (Item->AssetCount > 0)
		{
			StatusText = FString::Printf(TEXT("%d assets, %d/%d orbs"),
				Item->AssetCount, Item->ConnectedOrbitals, Item->TotalOrbitals);
		}
		else if (Item->TotalOrbitals > 0)
		{
			StatusText = FString::Printf(TEXT("%d/%d orbs"),
				Item->ConnectedOrbitals, Item->TotalOrbitals);
		}
	}
	else if (Item->Type == FValencySceneEntry::EType::Palette)
	{
		StatusText = FString::Printf(TEXT("%d assets"), Item->AssetCount);
	}

	return SNew(STableRow<TSharedPtr<FValencySceneEntry>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			// Color indicator
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 4, 0)
			[
				SNew(SBorder)
				.BorderBackgroundColor(Item->IconColor)
				.Padding(FMargin(4.0f))
				[
					SNew(SSpacer)
					.Size(FVector2D(8, 8))
				]
			]
			// Name
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->DisplayName))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			]
			// Status
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(StatusText))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
			// Warning indicator
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2, 0)
			[
				SNew(STextBlock)
				.Text(Item->bHasWarnings ? FText::FromString(TEXT("!")) : FText::GetEmpty())
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f)))
			]
		];
}

void SValencySceneOverview::OnSelectionChanged(TSharedPtr<FValencySceneEntry> Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid() || !Item->Actor.IsValid() || !GEditor)
	{
		return;
	}

	if (SelectInfo == ESelectInfo::Direct)
	{
		return; // Avoid recursive selection
	}

	// Select the actor in the level editor
	GEditor->SelectNone(true, true);
	GEditor->SelectActor(Item->Actor.Get(), true, true);
}

void SValencySceneOverview::OnDoubleClick(TSharedPtr<FValencySceneEntry> Item)
{
	if (!Item.IsValid() || !Item->Actor.IsValid() || !GEditor)
	{
		return;
	}

	// Focus the viewport on the actor
	GEditor->MoveViewportCamerasToActor(*Item->Actor.Get(), false);
}
