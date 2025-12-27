// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCTypes.h"

#include "Data/PCGExPointIO.h"
#include "Math/PCGExProjectionDetails.h"
#include "Paths/PCGExPathsHelpers.h"

namespace PCGExCavalier
{
	FRootPath::FRootPath(const TSharedPtr<PCGExData::FPointIO> InIO, const FPCGExGeo2DProjectionDetails& InProjectionDetails)
		: PathId(InIO->IOIndex), bIsClosed(PCGExPaths::Helpers::GetClosedLoop(InIO))
	{
		TConstPCGValueRange<FTransform> InTransforms = InIO->GetIn()->GetConstTransformValueRange();
		const int32 InNumVertices = InTransforms.Num();

		Reserve(InNumVertices);

		for (int i = 0; i < InNumVertices; i++) { AddPoint(InProjectionDetails.Project(InTransforms[i], i)); }
	}
}
