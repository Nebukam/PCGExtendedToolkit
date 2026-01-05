// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Offset.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Clipper2Lib/clipper.h"
#include "Data/PCGExDataTags.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2OffsetElement"
#define PCGEX_NAMESPACE Clipper2Offset

PCGEX_INITIALIZE_ELEMENT(Clipper2Offset)

FPCGExGeo2DProjectionDetails UPCGExClipper2OffsetSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

namespace
{
	PCGExClipper2Lib::JoinType ConvertJoinType(EPCGExClipper2JoinType InType)
	{
		switch (InType)
		{
		case EPCGExClipper2JoinType::Square: return PCGExClipper2Lib::JoinType::Square;
		case EPCGExClipper2JoinType::Round: return PCGExClipper2Lib::JoinType::Round;
		case EPCGExClipper2JoinType::Bevel: return PCGExClipper2Lib::JoinType::Bevel;
		case EPCGExClipper2JoinType::Miter: return PCGExClipper2Lib::JoinType::Miter;
		default: return PCGExClipper2Lib::JoinType::Round;
		}
	}

	PCGExClipper2Lib::EndType ConvertEndType(EPCGExClipper2EndType InType)
	{
		switch (InType)
		{
		case EPCGExClipper2EndType::Polygon: return PCGExClipper2Lib::EndType::Polygon;
		case EPCGExClipper2EndType::Joined: return PCGExClipper2Lib::EndType::Joined;
		case EPCGExClipper2EndType::Butt: return PCGExClipper2Lib::EndType::Butt;
		case EPCGExClipper2EndType::Square: return PCGExClipper2Lib::EndType::Square;
		case EPCGExClipper2EndType::Round: return PCGExClipper2Lib::EndType::Round;
		default: return PCGExClipper2Lib::EndType::Round;
		}
	}
}

void FPCGExClipper2OffsetContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2OffsetSettings* Settings = GetInputSettings<UPCGExClipper2OffsetSettings>();

	if (!Group->IsValid()) { return; }

	const double Scale = static_cast<double>(Settings->Precision);
	const PCGExClipper2Lib::JoinType JoinType = ConvertJoinType(Settings->JoinType);
	const PCGExClipper2Lib::EndType EndType = ConvertEndType(Settings->EndType);

	// Determine which paths to process
	PCGExClipper2Lib::Paths64 PathsToOffset;

	if (Settings->bUnionBeforeOffset && Group->SubjectPaths.size() > 1)
	{
		// Union all paths first - note: this loses per-point Z info for merged vertices
		PathsToOffset = PCGExClipper2Lib::Union(Group->SubjectPaths, PCGExClipper2Lib::FillRule::NonZero);
	}
	else
	{
		PathsToOffset = Group->SubjectPaths;
	}

	if (PathsToOffset.empty()) { return; }

	// Get values from first subject
	const int32 FirstSubjectIdx = Group->SubjectIndices.IsEmpty() ? 0 : Group->SubjectIndices[0];

	bool bDualOffset = true;
	int32 NumIterations = 1;

	if (FirstSubjectIdx < DualOffsetValues.Num() && DualOffsetValues[FirstSubjectIdx])
	{
		bDualOffset = DualOffsetValues[FirstSubjectIdx]->Read(0);
	}

	if (FirstSubjectIdx < IterationValues.Num() && IterationValues[FirstSubjectIdx])
	{
		NumIterations = FMath::Max(1, IterationValues[FirstSubjectIdx]->Read(0));
	}

	// Capture context for the delta callback
	const TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>>& OffsetValuesRef = OffsetValues;
	const TSharedPtr<PCGExClipper2::FOpData>& AllOpDataRef = AllOpData;

	// Helper lambda to create the delta callback with a sign multiplier
	auto CreateDeltaCallback = [&](double SignMultiplier, double IterationMultiplier) -> PCGExClipper2Lib::DeltaCallback64
	{
		return [&, SignMultiplier, IterationMultiplier](
			const PCGExClipper2Lib::Path64& Path, const PCGExClipper2Lib::PathD& PathNormals,
			size_t CurrIdx, size_t PrevIdx) -> double
		{
			// Decode source info from current point's Z value
			uint32 PointIdx, SourceFacadeIdx;
			PCGEx::H64(static_cast<uint64>(Path[CurrIdx].z), PointIdx, SourceFacadeIdx);

			// Find the offset value reader for this source
			const int32 OffsetArrayIdx = AllOpDataRef->FindSourceIndex(SourceFacadeIdx);
			if (OffsetArrayIdx == INDEX_NONE || OffsetArrayIdx >= OffsetValuesRef.Num())
			{
				return 10.0 * Scale * IterationMultiplier * SignMultiplier; // Default fallback
			}

			const TSharedPtr<PCGExDetails::TSettingValue<double>>& OffsetReader = OffsetValuesRef[OffsetArrayIdx];
			if (!OffsetReader)
			{
				return 10.0 * Scale * IterationMultiplier * SignMultiplier;
			}

			// Read the offset for this specific point
			const double PointOffset = OffsetReader->Read(static_cast<int32>(PointIdx));
			return PointOffset * Scale * IterationMultiplier * SignMultiplier;
		};
	};

	// Process iterations
	for (int32 Iteration = 0; Iteration < NumIterations; Iteration++)
	{
		const double IterationMultiplier = static_cast<double>(Iteration + 1);

		// Generate positive offset
		{
			PCGExClipper2Lib::ClipperOffset ClipperOffset(Settings->MiterLimit, 0.0, true, false);
			ClipperOffset.SetZCallback(Group->CreateZCallback());
			ClipperOffset.AddPaths(PathsToOffset, JoinType, EndType);

			PCGExClipper2Lib::Paths64 PositiveOffsetPaths;
			ClipperOffset.Execute(CreateDeltaCallback(1.0, IterationMultiplier), PositiveOffsetPaths);

			if (!PositiveOffsetPaths.empty())
			{
				TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
				OutputPaths64(PositiveOffsetPaths, Group, nullptr, nullptr, OutputPaths);

				if (Settings->bTagIteration)
				{
					for (const TSharedPtr<PCGExData::FPointIO>& Output : OutputPaths)
					{
						Output->Tags->Set<int32>(Settings->IterationTag, Iteration);
					}
				}
			}
		}

		// Generate negative (dual) offset if enabled
		if (bDualOffset)
		{
			PCGExClipper2Lib::ClipperOffset ClipperOffset(Settings->MiterLimit, 0.0, true, false);
			ClipperOffset.SetZCallback(Group->CreateZCallback());
			ClipperOffset.AddPaths(PathsToOffset, JoinType, EndType);

			PCGExClipper2Lib::Paths64 NegativeOffsetPaths;
			ClipperOffset.Execute(CreateDeltaCallback(-1.0, IterationMultiplier), NegativeOffsetPaths);

			if (!NegativeOffsetPaths.empty())
			{
				TArray<TSharedPtr<PCGExData::FPointIO>> DualOutputPaths;
				OutputPaths64(NegativeOffsetPaths, Group, nullptr, nullptr, DualOutputPaths);

				for (const TSharedPtr<PCGExData::FPointIO>& Output : DualOutputPaths)
				{
					if (Settings->bTagDual) { Output->Tags->AddRaw(Settings->DualTag); }
					if (Settings->bTagIteration) { Output->Tags->Set<int32>(Settings->IterationTag, Iteration); }
				}
			}
		}
	}
}

bool FPCGExClipper2OffsetElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Offset)

	const int32 NumFacades = Context->AllOpData->Num();
	Context->DualOffsetValues.SetNum(NumFacades);
	Context->OffsetValues.SetNum(NumFacades);
	Context->IterationValues.SetNum(NumFacades);

	for (int32 i = 0; i < NumFacades; i++)
	{
		const TSharedPtr<PCGExData::FFacade>& Facade = Context->AllOpData->Facades[i];

		auto DualSetting = Settings->DualOffset.GetValueSetting();
		if (DualSetting->Init(Facade)) { Context->DualOffsetValues[i] = DualSetting; }

		auto OffsetSetting = Settings->Offset.GetValueSetting();
		if (OffsetSetting->Init(Facade)) { Context->OffsetValues[i] = OffsetSetting; }

		auto IterationSetting = Settings->Iterations.GetValueSetting();
		if (IterationSetting->Init(Facade)) { Context->IterationValues[i] = IterationSetting; }
	}

	return FPCGExClipper2ProcessorElement::PostBoot(InContext);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE