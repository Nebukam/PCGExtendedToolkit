// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SValencyInspector.h"

#include "Editor.h"
#include "Selection.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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

	// Socket components
	TArray<UPCGExValencyCageSocketComponent*> SocketComponents;
	Cage->GetSocketComponents(SocketComponents);
	if (SocketComponents.Num() > 0)
	{
		Content->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			MakeSectionHeader(FText::Format(
				NSLOCTEXT("PCGExValency", "CageSockets", "Sockets ({0})"),
				FText::AsNumber(SocketComponents.Num())))
		];

		for (UPCGExValencyCageSocketComponent* Socket : SocketComponents)
		{
			if (!Socket) continue;

			const FString SocketInfo = FString::Printf(TEXT("  %s [%s] %s"),
				*Socket->SocketName.ToString(),
				*Socket->SocketType.ToString(),
				Socket->bIsOutputSocket ? TEXT("(Out)") : TEXT("(In)"));

			Content->AddSlot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(SocketInfo))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(Socket->bEnabled
					? FSlateColor(FLinearColor::White)
					: FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
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

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

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

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "SocketType", "Type"),
			FText::FromName(Socket->SocketType))
	];

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "SocketDirection", "Direction"),
			Socket->bIsOutputSocket
				? NSLOCTEXT("PCGExValency", "SocketOutput", "Output")
				: NSLOCTEXT("PCGExValency", "SocketInput", "Input"))
	];

	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "SocketEnabled", "Enabled"),
			Socket->bEnabled
				? NSLOCTEXT("PCGExValency", "Yes", "Yes")
				: NSLOCTEXT("PCGExValency", "No", "No"))
	];

	// Transform
	const FTransform SocketTransform = Socket->GetRelativeTransform();
	Content->AddSlot().AutoHeight()
	[
		MakeLabeledRow(
			NSLOCTEXT("PCGExValency", "SocketLocation", "Local Offset"),
			FText::FromString(SocketTransform.GetLocation().ToString()))
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
