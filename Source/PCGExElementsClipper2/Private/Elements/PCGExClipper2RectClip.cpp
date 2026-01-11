// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2RectClip.h"

#include "Data/PCGExPointIO.h"
#include "Clipper2Lib/clipper.h"
#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2RectClipElement"
#define PCGEX_NAMESPACE Clipper2RectClip

PCGEX_INITIALIZE_ELEMENT(Clipper2RectClip)

bool UPCGExClipper2RectClipSettings::WantsOperands() const
{
	return BoundsSource == EPCGExRectClipBoundsSource::Operand;
}

FPCGExGeo2DProjectionDetails UPCGExClipper2RectClipSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool UPCGExClipper2RectClipSettings::SupportOpenMainPaths() const
{
	return !bSkipOpenPaths;
}

bool UPCGExClipper2RectClipSettings::SupportOpenOperandPaths() const
{
	return true; // Operands are only used for bounds, so open paths are fine
}

void FPCGExClipper2RectClipContext::ApplyPadding(PCGExClipper2Lib::Rect64& Rect, double Padding, const FVector2D& Scale, int32 Precision)
{
	const int64 PaddingX = static_cast<int64>(Padding * Scale.X * Precision);
	const int64 PaddingY = static_cast<int64>(Padding * Scale.Y * Precision);

	Rect.left -= PaddingX;
	Rect.right += PaddingX;
	Rect.top -= PaddingY;
	Rect.bottom += PaddingY;
}

FBox FPCGExClipper2RectClipContext::ComputeCombinedBounds(const TArray<int32>& Indices)
{
	FBox CombinedBounds(EForceInit::ForceInit);

	for (const int32 Idx : Indices)
	{
		if (Idx < 0 || Idx >= AllOpData->Facades.Num()) { continue; }

		const TSharedPtr<PCGExData::FFacade>& Facade = AllOpData->Facades[Idx];
		const UPCGBasePointData* PointData = Facade->Source->GetIn();

		const FBox DataBounds = PointData->GetBounds();
		if (DataBounds.IsValid)
		{
			CombinedBounds += DataBounds;
		}
	}

	return CombinedBounds;
}

PCGExClipper2Lib::Rect64 FPCGExClipper2RectClipContext::ComputeClipRect(
	const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group,
	const UPCGExClipper2RectClipSettings* Settings)
{
	const int32 Scale = Settings->Precision;

	FBox WorldBounds(EForceInit::ForceInit);

	switch (Settings->BoundsSource)
	{
	case EPCGExRectClipBoundsSource::Operand:
		WorldBounds = ComputeCombinedBounds(Group->OperandIndices);
		break;

	case EPCGExRectClipBoundsSource::Manual:
		WorldBounds = Settings->ManualBounds;
		break;
	}

	// Check if we have valid bounds
	if (!WorldBounds.IsValid)
	{
		return PCGExClipper2Lib::Rect64(); // Return empty rect
	}

	// Project all 8 corners of the 3D bounding box to find the 2D bounds after projection
	TArray<FVector, TInlineAllocator<8>> Corners;
	Corners.Add(FVector(WorldBounds.Min.X, WorldBounds.Min.Y, WorldBounds.Min.Z));
	Corners.Add(FVector(WorldBounds.Max.X, WorldBounds.Min.Y, WorldBounds.Min.Z));
	Corners.Add(FVector(WorldBounds.Min.X, WorldBounds.Max.Y, WorldBounds.Min.Z));
	Corners.Add(FVector(WorldBounds.Max.X, WorldBounds.Max.Y, WorldBounds.Min.Z));
	Corners.Add(FVector(WorldBounds.Min.X, WorldBounds.Min.Y, WorldBounds.Max.Z));
	Corners.Add(FVector(WorldBounds.Max.X, WorldBounds.Min.Y, WorldBounds.Max.Z));
	Corners.Add(FVector(WorldBounds.Min.X, WorldBounds.Max.Y, WorldBounds.Max.Z));
	Corners.Add(FVector(WorldBounds.Max.X, WorldBounds.Max.Y, WorldBounds.Max.Z));

	// Project corners and find 2D min/max
	double MinX = TNumericLimits<double>::Max();
	double MaxX = TNumericLimits<double>::Lowest();
	double MinY = TNumericLimits<double>::Max();
	double MaxY = TNumericLimits<double>::Lowest();

	for (const FVector& Corner : Corners)
	{
		const FVector Projected = ProjectionDetails.Project(Corner, 0);
		MinX = FMath::Min(MinX, Projected.X);
		MaxX = FMath::Max(MaxX, Projected.X);
		MinY = FMath::Min(MinY, Projected.Y);
		MaxY = FMath::Max(MaxY, Projected.Y);
	}

	PCGExClipper2Lib::Rect64 ClipRect;
	ClipRect.left = static_cast<int64_t>(MinX * Scale);
	ClipRect.right = static_cast<int64_t>(MaxX * Scale);
	ClipRect.top = static_cast<int64_t>(MinY * Scale);
	ClipRect.bottom = static_cast<int64_t>(MaxY * Scale);

	// Apply padding
	ApplyPadding(ClipRect, Settings->BoundsPadding, Settings->BoundsPaddingScale, Scale);

	return ClipRect;
}

void FPCGExClipper2RectClipContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2RectClipSettings* Settings = GetInputSettings<UPCGExClipper2RectClipSettings>();

	if (!Group->IsValid()) { return; }

	// Compute the clipping rectangle
	PCGExClipper2Lib::Rect64 ClipRect = ComputeClipRect(Group, Settings);

	// Validate rectangle
	if (ClipRect.IsEmpty())
	{
		PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Computed clip rectangle is empty or invalid. Skipping group."));
		return;
	}

	if (Settings->bInvertClip)
	{
		// For inverted clip, use boolean difference with the rectangle
		// Create rectangle as a path (clockwise winding for positive area)
		PCGExClipper2Lib::Path64 RectPath;
		RectPath.reserve(4);
		RectPath.emplace_back(ClipRect.left, ClipRect.top, 0);
		RectPath.emplace_back(ClipRect.right, ClipRect.top, 0);
		RectPath.emplace_back(ClipRect.right, ClipRect.bottom, 0);
		RectPath.emplace_back(ClipRect.left, ClipRect.bottom, 0);

		PCGExClipper2Lib::Paths64 RectPaths = {RectPath};

		// Perform difference: Subject - Rectangle = Keep what's outside
		PCGExClipper2Lib::Clipper64 Clipper;
		Clipper.SetZCallback(Group->CreateZCallback());

		if (!Group->SubjectPaths.empty()) { Clipper.AddSubject(Group->SubjectPaths); }
		if (!Group->OpenSubjectPaths.empty()) { Clipper.AddOpenSubject(Group->OpenSubjectPaths); }
		Clipper.AddClip(RectPaths);

		PCGExClipper2Lib::Paths64 ClosedResults;
		PCGExClipper2Lib::Paths64 OpenResults;

		if (!Clipper.Execute(PCGExClipper2Lib::ClipType::Difference, PCGExClipper2Lib::FillRule::NonZero, ClosedResults, OpenResults))
		{
			return;
		}

		if (!ClosedResults.empty())
		{
			TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
			OutputPaths64(ClosedResults, Group, OutputPaths, true);
		}

		if (Settings->OpenPathsOutput != EPCGExClipper2OpenPathOutput::Ignore && !OpenResults.empty())
		{
			TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
			OutputPaths64(OpenResults, Group, OutputPaths, false);
		}
	}
	else
	{
		// Normal clip using optimized RectClip64
		PCGExClipper2Lib::Paths64 ClosedResults;
		PCGExClipper2Lib::Paths64 OpenResults;

		// Clip closed paths
		if (!Group->SubjectPaths.empty())
		{
			PCGExClipper2Lib::RectClip64 Clipper(ClipRect);
			ClosedResults = Clipper.Execute(Group->SubjectPaths);
		}

		// Clip open paths
		if (!Group->OpenSubjectPaths.empty())
		{
			if (Settings->bClipOpenPathsAsLines)
			{
				// Use RectClipLines for open paths (keeps them as lines)
				PCGExClipper2Lib::RectClipLines64 LineClipper(ClipRect);
				OpenResults = LineClipper.Execute(Group->OpenSubjectPaths);
			}
			else
			{
				// Treat open paths as closed polygons
				PCGExClipper2Lib::RectClip64 Clipper(ClipRect);
				PCGExClipper2Lib::Paths64 OpenAsClosedResults = Clipper.Execute(Group->OpenSubjectPaths);

				// Add to closed results
				for (auto& Path : OpenAsClosedResults)
				{
					ClosedResults.push_back(std::move(Path));
				}
			}
		}

		// Output results
		// Note: RectClip doesn't use ZCallback, so we use FromSource mode
		if (!ClosedResults.empty())
		{
			TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
			OutputPaths64(ClosedResults, Group, OutputPaths, true, PCGExClipper2::ETransformRestoration::FromSource);
		}

		if (Settings->OpenPathsOutput != EPCGExClipper2OpenPathOutput::Ignore && !OpenResults.empty())
		{
			TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
			OutputPaths64(OpenResults, Group, OutputPaths, false, PCGExClipper2::ETransformRestoration::FromSource);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
