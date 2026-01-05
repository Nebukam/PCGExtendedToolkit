// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExClipper2Inflate.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Clipper2Lib/clipper.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2InflateElement"
#define PCGEX_NAMESPACE Clipper2Inflate

PCGEX_INITIALIZE_ELEMENT(Clipper2Inflate)

FPCGExGeo2DProjectionDetails UPCGExClipper2InflateSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

namespace PCGExClipper2
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

void FPCGExClipper2InflateContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2InflateSettings* Settings = GetInputSettings<UPCGExClipper2InflateSettings>();

	if (!Group->IsValid()) { return; }

	const double Scale = static_cast<double>(Settings->Precision);
	const PCGExClipper2Lib::JoinType JoinType = PCGExClipper2::ConvertJoinType(Settings->JoinType);
	const PCGExClipper2Lib::EndType EndType = PCGExClipper2::ConvertEndType(Settings->EndType);

	if (Group->SubjectPaths.empty() && Group->OpenSubjectPaths.empty()) { return; }

	// Get settings values
	int32 NumIterations = 1;
	const double DefaultOffset = 10;

	// Read max iterations from subjects
	for (const int32 SubjectIdx : Group->SubjectIndices) { NumIterations = FMath::Max(NumIterations, IterationValues[SubjectIdx]->Read(0)); }

	// Capture values by copy for lambda safety
	// Since Facade->Idx == ArrayIndex, we can use SourceIdx from Z directly as array index
	const TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> OffsetValuesCopy = OffsetValues;
	const int32 NumFacades = AllOpData->Facades.Num();

	// Process iterations
	for (int32 Iteration = 0; Iteration < NumIterations; Iteration++)
	{
		const double IterationMultiplier = static_cast<double>(Iteration + 1);

		// Create ClipperOffset with ZCallback for tracking
		PCGExClipper2Lib::ClipperOffset ClipperOffset(Settings->MiterLimit, 0.0, true, false);
		ClipperOffset.SetZCallback(Group->CreateZCallback());

		// Add paths
		if (!Group->SubjectPaths.empty()) { ClipperOffset.AddPaths(Group->SubjectPaths, JoinType, PCGExClipper2Lib::EndType::Joined); }
		if (!Group->OpenSubjectPaths.empty()) { ClipperOffset.AddPaths(Group->OpenSubjectPaths, JoinType, EndType); }

		// Execute with per-point delta callback
		PCGExClipper2Lib::Paths64 InflatedPaths;

		ClipperOffset.Execute(
			[Scale, &OffsetValuesCopy, NumFacades, DefaultOffset, IterationMultiplier](
			const PCGExClipper2Lib::Path64& Path, const PCGExClipper2Lib::PathD& PathNormals,
			size_t CurrIdx, size_t PrevIdx) -> double
			{
				// Decode source info from current point's Z value
				uint32 PointIdx, SourceIdx;
				PCGEx::H64(static_cast<uint64>(Path[CurrIdx].z), PointIdx, SourceIdx);

				// SourceIdx is now directly the array index (Facade->Idx == ArrayIndex)
				const int32 ArrayIdx = static_cast<int32>(SourceIdx);

				if (ArrayIdx < 0 || ArrayIdx >= NumFacades || ArrayIdx >= OffsetValuesCopy.Num())
				{
					return DefaultOffset * Scale * IterationMultiplier;
				}

				const TSharedPtr<PCGExDetails::TSettingValue<double>>& OffsetReader = OffsetValuesCopy[ArrayIdx];
				if (!OffsetReader)
				{
					return DefaultOffset * Scale * IterationMultiplier;
				}

				// Read the offset for this specific point
				const double PointOffset = OffsetReader->Read(static_cast<int32>(PointIdx));
				return PointOffset * Scale * IterationMultiplier;
			},
			InflatedPaths
		);

		if (InflatedPaths.empty()) { continue; }

		// Output the inflated paths
		TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;

		// Use Unproject mode since inflate changes positions
		OutputPaths64(InflatedPaths, Group, nullptr, nullptr, OutputPaths, PCGExClipper2::ETransformRestoration::Unproject);

		// Tag with iteration number if requested
		if (Settings->bTagIteration)
		{
			for (const TSharedPtr<PCGExData::FPointIO>& Output : OutputPaths)
			{
				Output->Tags->Set<int32>(Settings->IterationTag, Iteration);
			}
		}

		// Write iteration attribute if bWriteIteration is true
		if (Settings->bWriteIteration)
		{
			for (const TSharedPtr<PCGExData::FPointIO>& Output : OutputPaths)
			{
				PCGExData::Helpers::SetDataValue(Output->GetOut(), Settings->IterationAttributeName, Iteration);
			}
		}
	}
}

bool FPCGExClipper2InflateElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Inflate)

	const int32 NumFacades = Context->AllOpData->Num();
	Context->OffsetValues.SetNum(NumFacades);
	Context->IterationValues.SetNum(NumFacades);

	for (int32 i = 0; i < NumFacades; i++)
	{
		const TSharedPtr<PCGExData::FFacade>& Facade = Context->AllOpData->Facades[i];

		auto OffsetSetting = Settings->Offset.GetValueSetting();
		if (OffsetSetting->Init(Facade)) { Context->OffsetValues[i] = OffsetSetting; }

		auto IterationSetting = Settings->Iterations.GetValueSetting();
		if (IterationSetting->Init(Facade)) { Context->IterationValues[i] = IterationSetting; }
	}

	return FPCGExClipper2ProcessorElement::PostBoot(InContext);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
