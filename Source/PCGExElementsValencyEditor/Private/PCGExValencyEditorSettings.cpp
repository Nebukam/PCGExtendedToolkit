// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExValencyEditorSettings.h"

#include "Materials/MaterialInterface.h"
#include "HAL/PlatformTime.h"

UPCGExValencyEditorSettings::UPCGExValencyEditorSettings()
{
	// Default values are set in the header via UPROPERTY initializers
}

const UPCGExValencyEditorSettings* UPCGExValencyEditorSettings::Get()
{
	return GetDefault<UPCGExValencyEditorSettings>();
}

UMaterialInterface* UPCGExValencyEditorSettings::GetGhostMaterial() const
{
	// Try to load the configured ghost material
	if (!GhostMaterial.IsNull())
	{
		if (UMaterialInterface* LoadedMaterial = GhostMaterial.LoadSynchronous())
		{
			return LoadedMaterial;
		}
	}

	// Fallback to plugin's default ghost material
	static UMaterialInterface* DefaultGhostMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/PCGExtendedToolkit/Data/Materials/M_ValencyAssetGhost.M_ValencyAssetGhost"));

	return DefaultGhostMaterial;
}

bool UPCGExValencyEditorSettings::ShouldAllowRebuild(EPropertyChangeType::Type ChangeType)
{
	// Non-interactive changes always allowed
	if (ChangeType != EPropertyChangeType::Interactive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Valency] ShouldAllowRebuild: Non-interactive change type %d - ALLOWED"), (int32)ChangeType);
		return true;
	}

	// Interactive change - check settings
	const UPCGExValencyEditorSettings* Settings = Get();
	UE_LOG(LogTemp, Warning, TEXT("[Valency] ShouldAllowRebuild: Interactive change, Settings=%p, bRebuildDuring=%s"),
		Settings, Settings ? (Settings->bRebuildDuringInteractiveChanges ? TEXT("true") : TEXT("false")) : TEXT("N/A"));

	if (Settings && Settings->bRebuildDuringInteractiveChanges)
	{
		// User opted into continuous rebuilds during dragging
		return true;
	}

	// Debounce: only allow if enough time has passed since last interactive rebuild
	static double LastInteractiveRebuildTime = 0.0;
	const double CurrentTime = FPlatformTime::Seconds();
	constexpr double DebounceThreshold = 0.2; // 200ms

	const bool bAllowed = (CurrentTime - LastInteractiveRebuildTime >= DebounceThreshold);
	UE_LOG(LogTemp, Warning, TEXT("[Valency] ShouldAllowRebuild: Debounce check - elapsed=%.3f, threshold=%.3f, allowed=%s"),
		CurrentTime - LastInteractiveRebuildTime, DebounceThreshold, bAllowed ? TEXT("true") : TEXT("false"));

	if (!bAllowed)
	{
		return false;
	}

	LastInteractiveRebuildTime = CurrentTime;
	return true;
}
