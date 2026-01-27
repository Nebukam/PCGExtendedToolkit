// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExValencyCageSocket.generated.h"

class UPCGExSocketRules;

/**
 * Represents a socket on a Valency cage.
 * Sockets are non-directional connection points that map to orbitals during compilation.
 * Unlike orbitals (which are direction-based), sockets have explicit transforms and type identity.
 *
 * Use cases:
 * - Chained solving: Output sockets connect to next solve's input sockets
 * - Mesh socket extraction: UStaticMesh sockets as connection points
 * - Non-directional module connections
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyCageSocket
{
	GENERATED_BODY()

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
	 * Local transform offset relative to the cage origin.
	 * The socket's world position is computed as: CageTransform * LocalOffset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FTransform LocalOffset = FTransform::Identity;

	/**
	 * Whether this is an output socket (for chaining to next solve).
	 * Input sockets receive connections, output sockets provide them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	bool bIsOutputSocket = false;

	/**
	 * Debug visualization color override.
	 * If (0,0,0,0), uses the color from SocketRules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket|Visualization", AdvancedDisplay)
	FLinearColor DebugColorOverride = FLinearColor(0, 0, 0, 0);

	/** Whether this socket is enabled (disabled sockets are ignored during compilation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	bool bEnabled = true;

	FPCGExValencyCageSocket() = default;

	FPCGExValencyCageSocket(const FName& InName, const FName& InType, bool bOutput = false)
		: SocketName(InName)
		, SocketType(InType)
		, bIsOutputSocket(bOutput)
	{
	}

	FPCGExValencyCageSocket(const FName& InName, const FName& InType, const FTransform& InOffset, bool bOutput = false)
		: SocketName(InName)
		, SocketType(InType)
		, LocalOffset(InOffset)
		, bIsOutputSocket(bOutput)
	{
	}

	/** Get the world transform for this socket given a cage transform */
	FTransform GetWorldTransform(const FTransform& CageTransform) const
	{
		return LocalOffset * CageTransform;
	}

	/**
	 * Get the effective debug color.
	 * @param SocketRules Optional socket rules to get default color from
	 * @return DebugColorOverride if set, otherwise color from SocketRules or white
	 */
	FLinearColor GetEffectiveDebugColor(const UPCGExSocketRules* SocketRules) const;

	bool operator==(const FPCGExValencyCageSocket& Other) const
	{
		return SocketName == Other.SocketName && SocketType == Other.SocketType;
	}
};

/** Hash function for FPCGExValencyCageSocket */
FORCEINLINE uint32 GetTypeHash(const FPCGExValencyCageSocket& Socket)
{
	return HashCombine(GetTypeHash(Socket.SocketName), GetTypeHash(Socket.SocketType));
}
