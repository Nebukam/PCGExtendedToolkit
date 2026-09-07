// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGExAssemblyRoot.h"

#include "PCGExAssemblyRootActor.generated.h"

class UBillboardComponent;

/**
 * Barebone assembly root: whatever is attached under it is exported as if it were a level, relative to
 * this actor. Point a PCGDataAsset collection entry (Source == Actor) at it, or drop it inside a
 * Valency cage, which registers it as one DataAsset module. Authors nothing itself and spawns nothing.
 *
 * In the editor it selects as a unit: clicking any attached descendant selects the root first. Once the
 * root (or one of its descendants) is selected, clicks drill into the content. The editor module drives
 * that through the sub-selection latch below.
 */
UCLASS(BlueprintType, Blueprintable, DisplayName = "[PCGEx] Assembly Root", meta=(PCGExNodeLibraryDoc="staging/collections/pcg-data-asset-collection/assembly-root"))
class PCGEXCOLLECTIONS_API APCGExAssemblyRootActor : public AActor, public IPCGExAssemblyRoot
{
	GENERATED_BODY()

public:
	APCGExAssemblyRootActor();

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	//~ Begin AActor Interface
	/** Engine-level select-as-unit: attached descendants redirect to this root unless the latch is set. */
	virtual bool IsSelectionParentOfAttachedActors() const override;
	//~ End AActor Interface

#if WITH_EDITOR
	/**
	 * Sub-selection latch. Transient and editor-owned: the editor selection tracker sets it before a
	 * selection change while this root or its content is selected, and clears it once nothing inside is.
	 * Never serialized, never set from actor code.
	 */
	void SetEditorSubSelectionLatch(const bool bInLatched) { bEditorSubSelectionLatch = bInLatched; }
	bool GetEditorSubSelectionLatch() const { return bEditorSubSelectionLatch; }
#endif

private:
#if WITH_EDITORONLY_DATA
	bool bEditorSubSelectionLatch = false;
#endif
};
