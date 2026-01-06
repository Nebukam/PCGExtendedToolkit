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

#if WITH_EDITOR
TArray<FPCGPreConfiguredSettingsInfo> UPCGExClipper2OffsetSettings::GetPreconfiguredInfo() const
{
	const TSet<EPCGExClipper2OffsetType> ValuesToSkip = {};
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExClipper2OffsetType>(ValuesToSkip, FTEXT("Clipper2 Offset : {0}"));
}
#endif

void UPCGExClipper2OffsetSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	Super::ApplyPreconfiguredSettings(PreconfigureInfo);
	if (const UEnum* EnumPtr = StaticEnum<EPCGExClipper2OffsetType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			OffsetType = static_cast<EPCGExClipper2OffsetType>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

FPCGExGeo2DProjectionDetails UPCGExClipper2OffsetSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool UPCGExClipper2OffsetSettings::SupportOpenMainPaths() const
{
	return Super::SupportOpenMainPaths() && OffsetType == EPCGExClipper2OffsetType::Inflate;
}

#if WITH_EDITOR
FString UPCGExClipper2OffsetSettings::GetDisplayName() const
{
	switch (OffsetType)
	{
	default:
	case EPCGExClipper2OffsetType::Offset: return TEXT("PCGEx | Clipper2 : Offset");
	case EPCGExClipper2OffsetType::Inflate: return TEXT("PCGEx | Clipper2 : Inflate");
	}
}
#endif


void FPCGExClipper2OffsetContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	const UPCGExClipper2OffsetSettings* Settings = GetInputSettings<UPCGExClipper2OffsetSettings>();

	if (!Group->IsValid()) { return; }

	const double Scale = static_cast<double>(Settings->Precision);
	const PCGExClipper2Lib::JoinType JoinType = PCGExClipper2::ConvertJoinType(Settings->JoinType);
	const PCGExClipper2Lib::EndType EndType = PCGExClipper2::ConvertEndType(Settings->EndType);

	if (Group->SubjectPaths.empty() && Group->OpenSubjectPaths.empty()) { return; }

	// Get settings values - use defaults from settings, override from first subject if available
	bool bDualOffset = Settings->bDualOffset && Settings->OffsetType == EPCGExClipper2OffsetType::Offset;
	PCGExClipper2Lib::EndType PolygonEndType = Settings->OffsetType == EPCGExClipper2OffsetType::Inflate ? PCGExClipper2Lib::EndType::Joined : PCGExClipper2Lib::EndType::Polygon;

	int32 NumIterations = 1;
	const double DefaultOffset = 10;

	// Read values from subjects (take max iterations across all)
	for (const int32 SubjectIdx : Group->SubjectIndices) { NumIterations = FMath::Max(NumIterations, IterationValues[SubjectIdx]->Read(0)); }

	// Capture values by copy for lambda safety
	// Since Facade->Idx == ArrayIndex, we can use SourceIdx from Z directly as array index
	const TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> OffsetValuesCopy = OffsetValues;
	const int32 NumFacades = AllOpData->Facades.Num();

	// Helper lambda to create the delta callback with a sign multiplier
	auto CreateDeltaCallback = [Scale, &OffsetValuesCopy, NumFacades, DefaultOffset](double SignMultiplier, double IterationMultiplier) -> PCGExClipper2Lib::DeltaCallback64
	{
		return [Scale, &OffsetValuesCopy, NumFacades, DefaultOffset, SignMultiplier, IterationMultiplier](
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
				return DefaultOffset * Scale * IterationMultiplier * SignMultiplier;
			}

			const TSharedPtr<PCGExDetails::TSettingValue<double>>& OffsetReader = OffsetValuesCopy[ArrayIdx];
			if (!OffsetReader)
			{
				return DefaultOffset * Scale * IterationMultiplier * SignMultiplier;
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
			PCGExClipper2Lib::ClipperOffset ClipperOffset(Settings->MiterLimit, Settings->GetArcTolerance(), Settings->bPreserveCollinear, false);
			ClipperOffset.SetZCallback(Group->CreateZCallback());

			if (!Group->SubjectPaths.empty()) { ClipperOffset.AddPaths(Group->SubjectPaths, JoinType, PolygonEndType); }
			if (!Group->OpenSubjectPaths.empty()) { ClipperOffset.AddPaths(Group->OpenSubjectPaths, JoinType, EndType); }

			PCGExClipper2Lib::Paths64 PositiveOffsetPaths;
			ClipperOffset.Execute(CreateDeltaCallback(1.0 * Settings->OffsetScale, IterationMultiplier), PositiveOffsetPaths);

			if (!PositiveOffsetPaths.empty())
			{
				TArray<TSharedPtr<PCGExData::FPointIO>> OutputPaths;
				// Use Unproject mode since offset changes positions
				OutputPaths64(PositiveOffsetPaths, Group, OutputPaths, true, PCGExClipper2::ETransformRestoration::Unproject);

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
			PCGExClipper2Lib::ClipperOffset ClipperOffset(Settings->MiterLimit, Settings->GetArcTolerance(), Settings->bPreserveCollinear, false);
			ClipperOffset.SetZCallback(Group->CreateZCallback());

			if (!Group->SubjectPaths.empty()) { ClipperOffset.AddPaths(Group->SubjectPaths, JoinType, PolygonEndType); }
			if (!Group->OpenSubjectPaths.empty()) { ClipperOffset.AddPaths(Group->OpenSubjectPaths, JoinType, EndType); }

			PCGExClipper2Lib::Paths64 NegativeOffsetPaths;
			ClipperOffset.Execute(CreateDeltaCallback(-1.0 * Settings->OffsetScale, IterationMultiplier), NegativeOffsetPaths);

			if (!NegativeOffsetPaths.empty())
			{
				TArray<TSharedPtr<PCGExData::FPointIO>> DualOutputPaths;
				// Use Unproject mode since offset changes positions
				OutputPaths64(NegativeOffsetPaths, Group, DualOutputPaths, true, PCGExClipper2::ETransformRestoration::Unproject);

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
