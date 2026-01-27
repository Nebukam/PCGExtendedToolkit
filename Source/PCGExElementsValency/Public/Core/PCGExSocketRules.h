// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "PCGExSocketRules.generated.h"

/**
 * Packed socket reference utilities.
 * Sockets are packed into int64 for attribute storage:
 *   - Bits 0-15:  Socket Rules asset index (in context's asset registry)
 *   - Bits 16-31: Socket entry index within the rules
 *   - Bits 32-63: Reserved for future use
 */
namespace PCGExSocket
{
	/** Invalid socket reference sentinel */
	constexpr int64 INVALID_SOCKET = -1;

	/** Pack a socket reference into int64 */
	FORCEINLINE int64 Pack(uint16 RulesIndex, uint16 SocketIndex)
	{
		return static_cast<int64>(RulesIndex) | (static_cast<int64>(SocketIndex) << 16);
	}

	/** Extract the rules asset index from a packed socket reference */
	FORCEINLINE uint16 GetRulesIndex(int64 Packed)
	{
		return static_cast<uint16>(Packed & 0xFFFF);
	}

	/** Extract the socket entry index from a packed socket reference */
	FORCEINLINE uint16 GetSocketIndex(int64 Packed)
	{
		return static_cast<uint16>((Packed >> 16) & 0xFFFF);
	}

	/** Check if a packed reference is valid */
	FORCEINLINE bool IsValid(int64 Packed)
	{
		return Packed != INVALID_SOCKET;
	}
}

/**
 * A single socket type definition.
 * Sockets are abstract connection points - they ARE orbitals with non-directional identity.
 * Unlike directional orbitals, sockets are matched by type rather than spatial direction.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExSocketDefinition
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	/**
	 * Stable identity for this socket type.
	 * Auto-generated, preserved through name changes.
	 * Used by CompatibleTypeIds for rename-safe references.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Internal", meta = (IgnoreForMemberInitializationTest))
	int32 TypeId = 0;
#endif

	/** Socket type name - used for compatibility matrix lookup and matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName SocketType;

	/** Display name for UI (defaults to SocketType if empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FText DisplayName;

#if WITH_EDITORONLY_DATA
	/**
	 * Socket types this type is compatible with (stored by TypeId).
	 * Displayed as type names in the editor via custom UI.
	 * Compiled to CompatibilityMatrix bitmask at runtime.
	 */
	UPROPERTY(EditAnywhere, Category = "Compatibility")
	TArray<int32> CompatibleTypeIds;
#endif

	/** Default transform offset relative to module origin (can be overridden per-module) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FTransform DefaultOffset = FTransform::Identity;

	/** Debug visualization color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FLinearColor DebugColor = FLinearColor::White;

	/** Bit index in compatibility mask (0-63, assigned at compile time) */
	int32 BitIndex = -1;

	FPCGExSocketDefinition()
	{
#if WITH_EDITOR
		TypeId = GetTypeHash(FGuid::NewGuid());
#endif
	}

	/** Get the display name, falling back to socket type if empty */
	FText GetDisplayName() const
	{
		return DisplayName.IsEmpty() ? FText::FromName(SocketType) : DisplayName;
	}

	bool operator==(const FPCGExSocketDefinition& Other) const
	{
		return SocketType == Other.SocketType;
	}
};

/**
 * Data asset defining socket types and their compatibility rules.
 * Analogous to UPCGExValencyOrbitalSet but for non-directional sockets.
 *
 * Sockets enable:
 * - Chained solving: Solve A -> WriteModuleSockets -> Solve B with different rules
 * - Mesh socket extraction: UStaticMesh sockets as connection points
 * - Non-directional module connections
 */
UCLASS(BlueprintType, DisplayName="[PCGEx] Socket Rules")
class PCGEXELEMENTSVALENCY_API UPCGExSocketRules : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Layer name - for multi-layer socket systems, determines attribute naming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName LayerName = FName("Main");

	/**
	 * Socket type definitions (max 64).
	 * Each socket type can be marked compatible with other types via the compatibility matrix.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (TitleProperty = "{SocketType}"))
	TArray<FPCGExSocketDefinition> SocketTypes;

	/**
	 * Compatibility matrix as bitmasks.
	 * Index = SocketTypeIndex, Value = bitmask of compatible socket types.
	 * If bit N is set in CompatibilityMatrix[M], socket type M can connect to socket type N.
	 *
	 * Example: If SocketTypes = ["Input", "Output", "BiDir"]
	 *   CompatibilityMatrix[0] = 0b010  // Input compatible with Output
	 *   CompatibilityMatrix[1] = 0b001  // Output compatible with Input
	 *   CompatibilityMatrix[2] = 0b100  // BiDir compatible with itself
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<int64> CompatibilityMatrix;

	/** Get the number of socket types */
	int32 Num() const { return SocketTypes.Num(); }

	/** Check if an index is valid */
	bool IsValidIndex(int32 Index) const { return SocketTypes.IsValidIndex(Index); }

	/** Get the attribute name for socket references on edges */
	FName GetSocketAttributeName() const
	{
		return FName(FString::Printf(TEXT("PCGEx/V/Socket/%s"), *LayerName.ToString()));
	}

	/**
	 * Find socket type index by name.
	 * @param SocketType The socket type name to find
	 * @return Index in SocketTypes array, or INDEX_NONE if not found
	 */
	int32 FindSocketTypeIndex(FName SocketType) const;

	/**
	 * Check if two socket types are compatible.
	 * @param TypeIndexA First socket type index
	 * @param TypeIndexB Second socket type index
	 * @return True if the types can connect
	 */
	bool AreTypesCompatible(int32 TypeIndexA, int32 TypeIndexB) const;

	/**
	 * Get the compatibility mask for a socket type.
	 * @param TypeIndex Socket type index
	 * @return Bitmask of compatible types (bit N set = compatible with type N)
	 */
	int64 GetCompatibilityMask(int32 TypeIndex) const;

	/**
	 * Compile socket definitions - assigns BitIndex to each socket type.
	 * Called automatically when needed, but can be called explicitly after modifications.
	 */
	void Compile();

	/**
	 * Validate the socket rules - checks for duplicates and valid configuration.
	 * @param OutErrors Array to receive error messages
	 * @return True if validation passed
	 */
	bool Validate(TArray<FText>& OutErrors) const;

	/**
	 * Set compatibility between two socket types.
	 * @param TypeIndexA First socket type index
	 * @param TypeIndexB Second socket type index
	 * @param bBidirectional If true, sets compatibility in both directions
	 */
	void SetCompatibility(int32 TypeIndexA, int32 TypeIndexB, bool bBidirectional = true);

	/**
	 * Clear all compatibility rules.
	 */
	void ClearCompatibility();

	/**
	 * Initialize the compatibility matrix with self-compatible types.
	 * Each socket type is compatible only with itself.
	 */
	void InitializeSelfCompatible();

#if WITH_EDITOR
	/**
	 * Find socket type index by TypeId.
	 * @param TypeId The stable type identifier
	 * @return Index in SocketTypes array, or INDEX_NONE if not found
	 */
	int32 FindSocketTypeIndexById(int32 TypeId) const;

	/**
	 * Get socket type name by TypeId.
	 * @param TypeId The stable type identifier
	 * @return Socket type name, or NAME_None if not found
	 */
	FName GetSocketTypeNameById(int32 TypeId) const;

	/**
	 * Get socket type display name by TypeId.
	 * @param TypeId The stable type identifier
	 * @return Display name text, or empty if not found
	 */
	FText GetSocketTypeDisplayNameById(int32 TypeId) const;

	/**
	 * Build compatibility matrix from CompatibleTypeIds on each socket definition.
	 * Called during Compile() to convert user-friendly data to runtime bitmask.
	 */
	void BuildCompatibilityMatrixFromTypeIds();

	/**
	 * Initialize all socket types as self-compatible using CompatibleTypeIds.
	 * Sets each type's CompatibleTypeIds to contain only its own TypeId.
	 */
	void InitializeSelfCompatibleTypeIds();

	/**
	 * Make all socket types compatible with each other using CompatibleTypeIds.
	 * Sets each type's CompatibleTypeIds to contain all TypeIds.
	 */
	void InitializeAllCompatibleTypeIds();
#endif

	/**
	 * Find a matching socket type for a mesh socket based on name and tag.
	 * Used for auto-extraction of sockets from static meshes.
	 *
	 * Matching priority:
	 * 1. If MeshSocketTag exactly matches a SocketType name -> return that type
	 * 2. If MeshSocketName exactly matches a SocketType name -> return that type
	 * 3. If MeshSocketName starts with a SocketType name (e.g., "Door_Left" matches "Door") -> return that type
	 *
	 * @param MeshSocketName The UStaticMeshSocket's SocketName
	 * @param MeshSocketTag The UStaticMeshSocket's Tag (optional)
	 * @return Matching socket type name, or NAME_None if no match
	 */
	FName FindMatchingSocketType(const FName& MeshSocketName, const FString& MeshSocketTag) const;

	/**
	 * Check if a mesh socket name/tag matches any socket type definition.
	 * @param MeshSocketName The UStaticMeshSocket's SocketName
	 * @param MeshSocketTag The UStaticMeshSocket's Tag (optional)
	 * @return True if a matching socket type exists
	 */
	bool MatchesMeshSocket(const FName& MeshSocketName, const FString& MeshSocketTag) const
	{
		return !FindMatchingSocketType(MeshSocketName, MeshSocketTag).IsNone();
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/**
 * A socket instance on a module.
 * Represents a specific socket connection point with optional transform override.
 *
 * Sockets map to orbital indices during compilation:
 * - Each unique socket type gets an orbital index (0-63)
 * - The solver then works identically to directional orbitals
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExModuleSocket
{
	GENERATED_BODY()

	/** Socket instance name (for identification/debugging, unique per module) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName SocketName;

	/** Socket type (references SocketRules.SocketTypes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName SocketType;

	/** Transform offset relative to module (used if bOverrideOffset is true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bOverrideOffset"))
	FTransform LocalOffset = FTransform::Identity;

	/** Whether LocalOffset overrides the SocketRules default offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bOverrideOffset = false;

	/** Whether this is an output socket (for chaining to next solve) vs input (receiving connections) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bIsOutputSocket = false;

	/** Orbital index this socket maps to (assigned during compilation, runtime only) */
	int32 OrbitalIndex = -1;

	/** Check if this socket has been assigned an orbital index */
	bool IsCompiled() const { return OrbitalIndex >= 0; }

	/**
	 * Get the effective transform offset.
	 * @param SocketRules The socket rules asset containing default offsets
	 * @return LocalOffset if bOverrideOffset is true, otherwise the default from SocketRules
	 */
	FTransform GetEffectiveOffset(const UPCGExSocketRules* SocketRules) const;

	bool operator==(const FPCGExModuleSocket& Other) const
	{
		return SocketName == Other.SocketName && SocketType == Other.SocketType;
	}
};

/** Hash function for FPCGExModuleSocket */
FORCEINLINE uint32 GetTypeHash(const FPCGExModuleSocket& Socket)
{
	return HashCombine(GetTypeHash(Socket.SocketName), GetTypeHash(Socket.SocketType));
}
