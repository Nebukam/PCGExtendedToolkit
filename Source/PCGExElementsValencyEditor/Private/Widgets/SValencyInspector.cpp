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
#include "Components/PCGExValencyCageSocketComponent.h"
#include "Volumes/ValencyContextVolume.h"

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
		// Check for selected components first (socket)
		if (USelection* CompSelection = GEditor->GetSelectedComponents())
		{
			for (FSelectionIterator It(*CompSelection); It; ++It)
			{
				if (UPCGExValencyCageSocketComponent* Socket = Cast<UPCGExValencyCageSocketComponent>(*It))
				{
					NewContent = BuildSocketContent(Socket);
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

	// Socket components - interactive section
	TArray<UPCGExValencyCageSocketComponent*> SocketComponents;
	Cage->GetSocketComponents(SocketComponents);
	{
		// Header row with socket count and Add button
		Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				MakeSectionHeader(FText::Format(
					NSLOCTEXT("PCGExValency", "CageSockets", "Sockets ({0})"),
					FText::AsNumber(SocketComponents.Num())))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				MakeAddSocketButton(Cage)
			]
		];

		for (UPCGExValencyCageSocketComponent* Socket : SocketComponents)
		{
			if (!Socket) continue;

			Content->AddSlot().AutoHeight()
			[
				MakeSocketRow(Socket)
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

TSharedRef<SWidget> SValencyInspector::BuildSocketContent(UPCGExValencyCageSocketComponent* Socket)
{
	if (!Socket)
	{
		return BuildSceneStatsContent();
	}

	TWeakObjectPtr<UPCGExValencyCageSocketComponent> WeakSocket(Socket);
	TWeakObjectPtr<UPCGExValencyCageEditorMode> WeakMode(EditorMode);

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	// Back to Cage button
	Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
	[
		SNew(SButton)
		.Text(NSLOCTEXT("PCGExValency", "BackToCage", "<< Back to Cage"))
		.ToolTipText(NSLOCTEXT("PCGExValency", "BackToCageTip", "Return to the cage socket list"))
		.OnClicked_Lambda([WeakSocket]() -> FReply
		{
			if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
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
			NSLOCTEXT("PCGExValency", "SocketHeader", "Socket: {0}"),
			FText::FromName(Socket->SocketName)))
	];

	// Owning cage
	if (const APCGExValencyCageBase* OwnerCage = Cast<APCGExValencyCageBase>(Socket->GetOwner()))
	{
		Content->AddSlot().AutoHeight()
		[
			MakeLabeledRow(
				NSLOCTEXT("PCGExValency", "SocketOwner", "Cage"),
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
				.Text(NSLOCTEXT("PCGExValency", "SocketName", "Name"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4, 0)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromName(Socket->SocketName))
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketNameTip", "Unique socket identifier within this cage"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.OnTextCommitted_Lambda([WeakSocket, WeakMode](const FText& NewText, ETextCommit::Type CommitType)
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "RenameSocket", "Rename Socket"));
					S->Modify();
					S->SocketName = FName(*NewText.ToString());
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

	// Editable Type
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
				.Text(NSLOCTEXT("PCGExValency", "SocketType", "Type"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4, 0)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromName(Socket->SocketType))
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketTypeTip", "Socket type name \u2014 determines compatibility during solving. Must match a type defined in the cage's Socket Rules."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.OnTextCommitted_Lambda([WeakSocket, WeakMode](const FText& NewText, ETextCommit::Type CommitType)
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ChangeSocketType", "Change Socket Type"));
					S->Modify();
					S->SocketType = FName(*NewText.ToString());
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

	// Direction toggle
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
				.Text(NSLOCTEXT("PCGExValency", "SocketDirection", "Direction"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SButton)
			.Text(Socket->bIsOutputSocket
				? NSLOCTEXT("PCGExValency", "SocketOutArrow", "Outward >>")
				: NSLOCTEXT("PCGExValency", "SocketInArrow", "<< Inward"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketDirTip", "Toggle between providing connections (arrow outward) and receiving connections (arrow inward)"))
			.OnClicked_Lambda([WeakSocket, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleDir", "Toggle Socket Direction"));
					S->Modify();
					S->bIsOutputSocket = !S->bIsOutputSocket;
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
				.Text(NSLOCTEXT("PCGExValency", "SocketEnabled", "Enabled"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SCheckBox)
			.IsChecked(Socket->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketEnabledTip", "Disabled sockets are ignored during compilation"))
			.OnCheckStateChanged_Lambda([WeakSocket, WeakMode](ECheckBoxState NewState)
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleEnabled", "Toggle Socket Enabled"));
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
	const FTransform SocketTransform = Socket->GetRelativeTransform();
	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "SocketLocation", "Local Offset"),
			FText::FromString(SocketTransform.GetLocation().ToString()))
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
			.Text(NSLOCTEXT("PCGExValency", "DuplicateSocket", "Duplicate"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "DuplicateSocketTip", "Create a copy of this socket with a small offset (Ctrl+D)"))
			.OnClicked_Lambda([WeakSocket, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->DuplicateSocket(S);
					}
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.Text(NSLOCTEXT("PCGExValency", "RemoveSocketBtn", "Remove"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "RemoveSocketTip", "Delete this socket from the cage (Delete key)"))
			.OnClicked_Lambda([WeakSocket, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->RemoveSocket(S);
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

TSharedRef<SWidget> SValencyInspector::MakeSocketRow(UPCGExValencyCageSocketComponent* Socket)
{
	TWeakObjectPtr<UPCGExValencyCageSocketComponent> WeakSocket(Socket);
	TWeakObjectPtr<UPCGExValencyCageEditorMode> WeakMode(EditorMode);

	const FLinearColor TextColor = Socket->bEnabled
		? FLinearColor::White
		: FLinearColor(0.5f, 0.5f, 0.5f);

	return SNew(SHorizontalBox)
		// Clickable socket name - selects the socket in viewport
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(2, 1)
		[
			SNew(SButton)
			.ContentPadding(FMargin(2, 0))
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketRowNameTip", "Click to select this socket in the viewport"))
			.OnClicked_Lambda([WeakSocket]() -> FReply
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
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
					*Socket->SocketName.ToString(),
					*Socket->SocketType.ToString())))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(TextColor))
			]
		]
		// Direction toggle button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 0)
		[
			SNew(SButton)
			.Text(Socket->bIsOutputSocket
				? NSLOCTEXT("PCGExValency", "OutArrow", ">>")
				: NSLOCTEXT("PCGExValency", "InArrow", "<<"))
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketRowDirTip", "Toggle between providing connections (arrow outward) and receiving connections (arrow inward)"))
			.ContentPadding(FMargin(4, 1))
			.OnClicked_Lambda([WeakSocket, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleDir", "Toggle Socket Direction"));
					S->Modify();
					S->bIsOutputSocket = !S->bIsOutputSocket;
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
			.IsChecked(Socket->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketRowEnabledTip", "Enable/disable this socket"))
			.OnCheckStateChanged_Lambda([WeakSocket, WeakMode](ECheckBoxState NewState)
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleEnabled", "Toggle Socket Enabled"));
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
			.ToolTipText(NSLOCTEXT("PCGExValency", "SocketRowRemoveTip", "Remove this socket"))
			.ContentPadding(FMargin(4, 1))
			.OnClicked_Lambda([WeakSocket, WeakMode]() -> FReply
			{
				if (UPCGExValencyCageSocketComponent* S = WeakSocket.Get())
				{
					if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
					{
						Mode->RemoveSocket(S);
					}
				}
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> SValencyInspector::MakeAddSocketButton(APCGExValencyCageBase* Cage)
{
	TWeakObjectPtr<APCGExValencyCageBase> WeakCage(Cage);
	TWeakObjectPtr<UPCGExValencyCageEditorMode> WeakMode(EditorMode);

	return SNew(SButton)
		.Text(NSLOCTEXT("PCGExValency", "AddSocket", "+ Add"))
		.ToolTipText(NSLOCTEXT("PCGExValency", "AddSocketTip", "Add a new socket to this cage (Ctrl+Shift+A)"))
		.ContentPadding(FMargin(4, 1))
		.OnClicked_Lambda([WeakCage, WeakMode]() -> FReply
		{
			if (APCGExValencyCageBase* C = WeakCage.Get())
			{
				if (UPCGExValencyCageEditorMode* Mode = WeakMode.Get())
				{
					Mode->AddSocketToCage(C);
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
