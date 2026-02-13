// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExValencyEditorActorBase.h"

#include "PCGExValencyEditorSettings.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "EditorMode/PCGExValencyDirtyState.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorModeManager.h"
#endif

APCGExValencyEditorActorBase::APCGExValencyEditorActorBase()
{
}

void APCGExValencyEditorActorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// ========== Meta Tag Handling ==========

	// Check PCGEX_ValencyGhostRefresh meta on any property in the change chain
	{
		bool bGhostRefresh = false;
		if (const FProperty* Property = PropertyChangedEvent.Property)
		{
			bGhostRefresh = Property->HasMetaData(TEXT("PCGEX_ValencyGhostRefresh"));
		}
		if (!bGhostRefresh && PropertyChangedEvent.MemberProperty)
		{
			bGhostRefresh = PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyGhostRefresh"));
		}

		if (bGhostRefresh)
		{
			OnGhostRefreshRequested();
		}
	}

	// Check PCGEX_ValencyRebuild meta on any property in the change chain
	{
		bool bShouldRebuild = false;
		if (const FProperty* Property = PropertyChangedEvent.Property)
		{
			if (Property->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
			{
				bShouldRebuild = true;
			}
		}
		if (!bShouldRebuild && PropertyChangedEvent.MemberProperty)
		{
			if (PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
			{
				bShouldRebuild = true;
			}
		}

		// Debounce interactive changes (dragging sliders) to prevent spam
		if (bShouldRebuild && !UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			bShouldRebuild = false;
		}

		if (bShouldRebuild)
		{
			OnRebuildMetaTagTriggered();
		}
	}

	// Subclass hook for class-specific property handling
	OnPostEditChangeProperty(PropertyChangedEvent);
}

FValencyDirtyStateManager* APCGExValencyEditorActorBase::GetActiveDirtyStateManager()
{
#if WITH_EDITOR
	if (GEditor)
	{
		if (GLevelEditorModeTools().IsModeActive(UPCGExValencyCageEditorMode::ModeID))
		{
			if (UPCGExValencyCageEditorMode* Mode = Cast<UPCGExValencyCageEditorMode>(
				GLevelEditorModeTools().GetActiveScriptableMode(UPCGExValencyCageEditorMode::ModeID)))
			{
				return &Mode->GetDirtyStateManager();
			}
		}
	}
#endif
	return nullptr;
}
