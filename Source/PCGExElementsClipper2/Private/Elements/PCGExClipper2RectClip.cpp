// Copyright 2026 Timothé Lapetite and contributors
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

bool UPCGExClipper2RectClipSettings::OperandsAsBounds() const
{
	return true;
}

namespace PCGExClipper2RectClip
{
	// Helper to check if point P lies on segment AB (within tolerance)
	// Returns alpha (0-1) if on segment, -1 if not
	double PointOnSegment(
		int64_t Px, int64_t Py,
		int64_t Ax, int64_t Ay,
		int64_t Bx, int64_t By,
		int64_t Tolerance)
	{
		// Vector AB
		const double ABx = static_cast<double>(Bx - Ax);
		const double ABy = static_cast<double>(By - Ay);

		// Vector AP
		const double APx = static_cast<double>(Px - Ax);
		const double APy = static_cast<double>(Py - Ay);

		// Length squared of AB
		const double ABLenSq = ABx * ABx + ABy * ABy;
		if (ABLenSq < 1.0)
		{
			return -1.0; // Degenerate segment
		}

		// Project P onto line AB, get parameter t
		const double t = (APx * ABx + APy * ABy) / ABLenSq;

		// Check if t is in valid range [0, 1]
		if (t < -0.001 || t > 1.001)
		{
			return -1.0;
		}

		// Calculate closest point on segment
		const double ClosestX = Ax + t * ABx;
		const double ClosestY = Ay + t * ABy;

		// Check distance from P to closest point
		const double DistX = Px - ClosestX;
		const double DistY = Py - ClosestY;
		const double DistSq = DistX * DistX + DistY * DistY;

		// Use tolerance squared for comparison
		const double TolSq = static_cast<double>(Tolerance) * static_cast<double>(Tolerance);
		if (DistSq > TolSq)
		{
			return -1.0;
		}

		return FMath::Clamp(t, 0.0, 1.0);
	}

	/**
	 * Restore Z values for paths output by RectClip64.
	 * 
	 * RectClip64 sets Z=0 for all intersection points (see GetLineIntersectPt in clipper_core.h).
	 * This function restores proper Z values by:
	 * 1. Matching output points to original source points by X/Y coordinates (exact match)
	 * 2. For intersection points, finding which SOURCE EDGE they lie on and interpolating Z
	 * 
	 * @param OutPaths - The paths output by RectClip64 (modified in place)
	 * @param SourcePaths - The original source paths with valid Z encodings
	 * @param Tolerance - Distance tolerance for matching points (in Clipper2 integer units)
	 */
	void RestoreZValuesForRectClipResults(
		PCGExClipper2Lib::Paths64& OutPaths,
		const PCGExClipper2Lib::Paths64& SourcePaths,
		int64_t Tolerance = 2)
	{
		// Build a map from (X,Y) to Z for all source points (for exact matches)
		TMap<uint64, int64_t> SourcePointZMap;
		for (const PCGExClipper2Lib::Path64& SrcPath : SourcePaths)
		{
			for (const PCGExClipper2Lib::Point64& SrcPt : SrcPath)
			{
				const uint64 Key = PCGEx::H64(
					static_cast<uint32>(SrcPt.x & 0xFFFFFFFF),
					static_cast<uint32>(SrcPt.y & 0xFFFFFFFF));
				SourcePointZMap.Add(Key, SrcPt.z);
			}
		}

		// Process each output path
		for (PCGExClipper2Lib::Path64& OutPath : OutPaths)
		{
			const size_t NumPoints = OutPath.size();
			if (NumPoints == 0)
			{
				continue;
			}

			for (size_t i = 0; i < NumPoints; i++)
			{
				PCGExClipper2Lib::Point64& Pt = OutPath[i];

				// First, try exact match with source points
				const uint64 Key = PCGEx::H64(
					static_cast<uint32>(Pt.x & 0xFFFFFFFF),
					static_cast<uint32>(Pt.y & 0xFFFFFFFF));

				if (const int64_t* FoundZ = SourcePointZMap.Find(Key))
				{
					// Exact match found - restore original Z
					Pt.z = *FoundZ;
					continue;
				}

				// No exact match - this is an intersection point
				// Find which source edge this point lies on
				bool bFoundEdge = false;

				for (const PCGExClipper2Lib::Path64& SrcPath : SourcePaths)
				{
					if (bFoundEdge)
					{
						break;
					}

					const size_t SrcNumPts = SrcPath.size();
					if (SrcNumPts < 2)
					{
						continue;
					}

					// Check each edge in the source path
					for (size_t j = 0; j < SrcNumPts; j++)
					{
						const size_t NextJ = (j + 1) % SrcNumPts;
						const PCGExClipper2Lib::Point64& A = SrcPath[j];
						const PCGExClipper2Lib::Point64& B = SrcPath[NextJ];

						// Check if Pt lies on edge A->B
						const double Alpha = PointOnSegment(
							Pt.x, Pt.y,
							A.x, A.y,
							B.x, B.y,
							Tolerance);

						if (Alpha >= 0.0)
						{
							// Point lies on this edge! Interpolate Z
							// Since Z encodes (PointIndex, SourceIndex), we pick the closer endpoint's Z
							// This ensures we reference a valid source point for transform lookup
							if (Alpha <= 0.5)
							{
								Pt.z = A.z;
							}
							else
							{
								Pt.z = B.z;
							}
							bFoundEdge = true;
							break;
						}
					}
				}

				// If still not found (shouldn't happen normally), leave Z as 0
				// The output code will need to handle this gracefully
			}
		}
	}
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
	FBox CombinedBounds(ForceInit);

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

	FBox WorldBounds(ForceInit);

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
		// (This path uses ZCallback and doesn't need the fix)
		PCGExClipper2Lib::Path64 RectPath;
		RectPath.reserve(4);
		RectPath.emplace_back(ClipRect.left, ClipRect.top, 0);
		RectPath.emplace_back(ClipRect.right, ClipRect.top, 0);
		RectPath.emplace_back(ClipRect.right, ClipRect.bottom, 0);
		RectPath.emplace_back(ClipRect.left, ClipRect.bottom, 0);

		PCGExClipper2Lib::Paths64 RectPaths = {RectPath};

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
			if (Settings->bClipAsLines)
			{
				for (const PCGExClipper2Lib::Path64& SrcPath : Group->SubjectPaths)
				{
					PCGExClipper2Lib::Rect64 PathBounds = PCGExClipper2Lib::GetBounds(SrcPath);

					const bool bEntirelyInside =
						PathBounds.left >= ClipRect.left &&
						PathBounds.right <= ClipRect.right &&
						PathBounds.top >= ClipRect.top &&
						PathBounds.bottom <= ClipRect.bottom;

					if (bEntirelyInside)
					{
						ClosedResults.push_back(SrcPath);
					}
					else
					{
						// For closed paths, explicitly close the loop by appending V0
						// RectClipLines64 doesn't process the edge from last vertex back to V0 otherwise
						PCGExClipper2Lib::Path64 ExplicitlyClosed = SrcPath;
						if (!SrcPath.empty())
						{
							ExplicitlyClosed.push_back(SrcPath[0]);
						}

						PCGExClipper2Lib::RectClipLines64 LineClipper(ClipRect);
						PCGExClipper2Lib::Paths64 SinglePath = {ExplicitlyClosed};
						PCGExClipper2Lib::Paths64 ClippedResults = LineClipper.Execute(SinglePath);

						// Restore Z values using original source paths
						PCGExClipper2RectClip::RestoreZValuesForRectClipResults(ClippedResults, Group->SubjectPaths);

						// Now that the path is explicitly closed, segments that should connect at V0 will exist
						// Find and merge them
						if (ClippedResults.size() >= 2 && !SrcPath.empty())
						{
							const PCGExClipper2Lib::Point64& V0 = SrcPath[0];

							int SegmentStartingAtV0 = -1;
							int SegmentEndingAtV0 = -1;

							for (int s = 0; s < static_cast<int>(ClippedResults.size()); s++)
							{
								const PCGExClipper2Lib::Path64& Seg = ClippedResults[s];
								if (Seg.empty())
								{
									continue;
								}

								if (Seg.front().x == V0.x && Seg.front().y == V0.y)
								{
									SegmentStartingAtV0 = s;
								}
								if (Seg.back().x == V0.x && Seg.back().y == V0.y)
								{
									SegmentEndingAtV0 = s;
								}
							}

							if (SegmentStartingAtV0 >= 0 && SegmentEndingAtV0 >= 0 &&
								SegmentStartingAtV0 != SegmentEndingAtV0)
							{
								PCGExClipper2Lib::Path64& EndingSeg = ClippedResults[SegmentEndingAtV0];
								PCGExClipper2Lib::Path64& StartingSeg = ClippedResults[SegmentStartingAtV0];

								// Append StartingSeg onto EndingSeg (skip duplicate V0)
								EndingSeg.reserve(EndingSeg.size() + StartingSeg.size() - 1);
								for (size_t k = 1; k < StartingSeg.size(); k++)
								{
									EndingSeg.push_back(StartingSeg[k]);
								}

								StartingSeg.clear();
							}
						}

						for (auto& Path : ClippedResults)
						{
							if (!Path.empty())
							{
								OpenResults.push_back(std::move(Path));
							}
						}
					}
				}
			}
			else
			{
				// Normal polygon clipping
				PCGExClipper2Lib::RectClip64 Clipper(ClipRect);
				ClosedResults = Clipper.Execute(Group->SubjectPaths);

				PCGExClipper2RectClip::RestoreZValuesForRectClipResults(ClosedResults, Group->SubjectPaths);
			}
		}

		// Clip open paths
		if (!Group->OpenSubjectPaths.empty())
		{
			if (Settings->bClipOpenPathsAsLines || Settings->bClipAsLines)
			{
				// Use RectClipLines for open paths (or when bClipAsLines is enabled)
				PCGExClipper2Lib::RectClipLines64 LineClipper(ClipRect);
				PCGExClipper2Lib::Paths64 OpenLinesResults = LineClipper.Execute(Group->OpenSubjectPaths);

				PCGExClipper2RectClip::RestoreZValuesForRectClipResults(OpenLinesResults, Group->OpenSubjectPaths);

				for (auto& Path : OpenLinesResults)
				{
					OpenResults.push_back(std::move(Path));
				}
			}
			else
			{
				// Treat open paths as closed polygons
				PCGExClipper2Lib::RectClip64 Clipper(ClipRect);
				PCGExClipper2Lib::Paths64 OpenAsClosedResults = Clipper.Execute(Group->OpenSubjectPaths);

				PCGExClipper2RectClip::RestoreZValuesForRectClipResults(OpenAsClosedResults, Group->OpenSubjectPaths);

				for (auto& Path : OpenAsClosedResults)
				{
					ClosedResults.push_back(std::move(Path));
				}
			}
		}

		// Output results
		if (!ClosedResults.empty())
		{
			TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
			OutputPaths64(ClosedResults, Group, OutputPaths, true, PCGExClipper2::ETransformRestoration::Unproject);
		}

		if (Settings->OpenPathsOutput != EPCGExClipper2OpenPathOutput::Ignore && !OpenResults.empty())
		{
			TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
			OutputPaths64(OpenResults, Group, OutputPaths, false, PCGExClipper2::ETransformRestoration::Unproject);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
