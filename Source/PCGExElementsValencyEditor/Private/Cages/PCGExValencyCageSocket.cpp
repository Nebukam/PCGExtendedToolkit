// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageSocket.h"
#include "Core/PCGExSocketRules.h"

FLinearColor FPCGExValencyCageSocket::GetEffectiveDebugColor(const UPCGExSocketRules* SocketRules) const
{
	// Use override if set (non-zero alpha indicates intentional color)
	if (DebugColorOverride.A > 0.0f)
	{
		return DebugColorOverride;
	}

	// Try to get color from socket rules
	if (SocketRules)
	{
		const int32 TypeIndex = SocketRules->FindSocketTypeIndex(SocketType);
		if (SocketRules->SocketTypes.IsValidIndex(TypeIndex))
		{
			return SocketRules->SocketTypes[TypeIndex].DebugColor;
		}
	}

	// Default to white
	return FLinearColor::White;
}
