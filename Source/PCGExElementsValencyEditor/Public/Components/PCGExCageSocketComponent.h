// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "PCGExCageSocketComponent.generated.h"

class UPCGExSocketRules;
class UStaticMesh;

/**
 * A socket component attached to a Valency cage.
 * Sockets are non-directional connection points that map to orbitals during compilation.
 * Unlike orbitals (which are direction-based), sockets have explicit transforms and type identity.
 *
 * Use cases:
 * - Chained solving: Output sockets connect to next solve's input sockets
 * - Mesh socket extraction: UStaticMesh sockets as connection points
 * - Non-directional module connections
 *
 * Being a USceneComponent enables:
 * - Visual placement in viewport with transform gizmos
 * - Blueprint spawnable components
 * - Actor component architecture (attach to any cage)
 */
UCLASS(BlueprintType, ClassGroup = PCGEx, meta = (BlueprintSpawnableComponent), HideCategories = (Mobility, LOD, Collision, Physics, Rendering, Navigation, Cooking))
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExCageSocketComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPCGExCageSocketComponent();

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UActorComponent Interface

	// ========== Socket Properties ==========

	/**
	 * Socket instance name (unique per cage).
	 * Used for identification and debugging.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName SocketName;

	/**
	 * Socket type (references SocketRules.SocketTypes).
	 * Determines compatibility with other sockets during solving.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName SocketType;

	/**
	 * Whether this is an output socket (for chaining to next solve).
	 * Input sockets receive connections, output sockets provide them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	bool bIsOutputSocket = false;

	/** Whether this socket is enabled (disabled sockets are ignored during compilation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	bool bEnabled = true;

	// ========== Mesh Integration ==========

	/**
	 * Optional reference to a mesh socket name to inherit transform from.
	 * When set, the component will attempt to sync its transform with the named socket
	 * from the owning cage's static mesh (if applicable).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket|Mesh Integration", AdvancedDisplay)
	FName MeshSocketName;

	/**
	 * If enabled, automatically match and inherit transform from a mesh socket
	 * with the same name as this component's SocketName.
	 * The mesh is searched in the owning cage's effective assets.
	 * This is evaluated at compile time during BuildFromCages.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket|Mesh Integration", AdvancedDisplay)
	bool bMatchMeshSocketTransform = false;

	/**
	 * If enabled, this socket component overrides any auto-extracted socket
	 * with the same name (from bReadSocketsFromAssets).
	 * If disabled, auto-extracted sockets take precedence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket|Mesh Integration", AdvancedDisplay)
	bool bOverrideAutoExtracted = true;

	// ========== Visualization ==========

	/**
	 * Debug visualization color override.
	 * If (0,0,0,0), uses the color from SocketRules based on SocketType.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket|Visualization", AdvancedDisplay)
	FLinearColor DebugColorOverride = FLinearColor(0, 0, 0, 0);

	// ========== Methods ==========

	/**
	 * Get the effective debug color.
	 * @param SocketRules Optional socket rules to get default color from
	 * @return DebugColorOverride if set (non-zero alpha), otherwise color from SocketRules or white
	 */
	FLinearColor GetEffectiveDebugColor(const UPCGExSocketRules* SocketRules) const;

	/**
	 * Get the socket's world transform (same as component world transform).
	 */
	FTransform GetSocketWorldTransform() const { return GetComponentTransform(); }

	/**
	 * Get the socket's local transform relative to parent (same as component relative transform).
	 */
	FTransform GetSocketLocalTransform() const { return GetRelativeTransform(); }

	/**
	 * Sync transform from a mesh socket if MeshSocketName is set.
	 * @param Mesh The static mesh containing the socket
	 * @return true if transform was synced, false if mesh socket not found
	 */
	bool SyncTransformFromMeshSocket(UStaticMesh* Mesh);

protected:
	/**
	 * Generate a unique socket name based on owning actor.
	 * Called during registration if SocketName is None.
	 */
	void GenerateDefaultSocketName();

	/**
	 * Request a rebuild of the owning cage's containing volumes.
	 */
	void RequestCageRebuild();
};
