// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"
#include "PCGExCCPolyline.h"
#include "Details/PCGExCCDetails.h"

namespace PCGExCavalier
{
	namespace Labels
	{
		const FName SourceOperandsLabel = TEXT("Operands");
	}
	
	/** Basic intersection point during offset */
	struct FBasicIntersect
	{
		int32 StartIndex1 = 0;
		int32 StartIndex2 = 0;
		FVector2D Point = FVector2D::ZeroVector;

		FORCEINLINE FBasicIntersect() = default;

		FBasicIntersect(int32 InIdx1, int32 InIdx2, const FVector2D& InPoint)
			: StartIndex1(InIdx1), StartIndex2(InIdx2), Point(InPoint)
		{
		}
	};
}
