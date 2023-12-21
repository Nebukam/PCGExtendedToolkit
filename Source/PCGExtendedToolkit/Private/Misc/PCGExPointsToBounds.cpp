// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

#define PCGEX_WRITE_MARK(_NAME, _VALUE)\
if(Settings->bWrite##_NAME){if(FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName)){\
PCGExData::WriteMark(OutData->Metadata, FName(Settings->_NAME##AttributeName), _VALUE);\
}else{PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid attribute name "#_NAME));}}

PCGExData::EInit UPCGExPointsToBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExPointsToBoundsContext::~FPCGExPointsToBoundsContext()
{
	AttributesBlendingOverrides.Empty();
	OutPoints = nullptr;
	PCGEX_DELETE(MetadataBlender)
	PCGEX_DELETE(PropertyBlender)
}

UPCGExPointsToBoundsSettings::UPCGExPointsToBoundsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExPointsToBoundsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FPCGElementPtr UPCGExPointsToBoundsSettings::CreateElement() const { return MakeShared<FPCGExPointsToBoundsElement>(); }

PCGEX_INITIALIZE_CONTEXT(PointsToBounds)

bool FPCGExPointsToBoundsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	Context->AttributesBlendingOverrides = Settings->BlendingSettings.AttributesOverrides;
	Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(Settings->BlendingSettings.DefaultBlending);
	Context->PropertyBlender = new PCGExDataBlending::FPropertiesBlender(Settings->BlendingSettings);

	return true;
}

bool FPCGExPointsToBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsToBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();
		UPCGPointData* OutData = Context->GetCurrentOut();
		TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
		MutablePoints.Add(InPoints[0]);

		Context->MetadataBlender->PrepareForData(*Context->CurrentIO, Context->AttributesBlendingOverrides);
		const double AverageDivider = InPoints.Num();

		FBox Box = FBox(ForceInit);
		for (int i = 0; i < AverageDivider; i++) { Box += InPoints[i].Transform.GetLocation(); }

		const FVector Center = Box.GetCenter();
		const double SqrDist = Box.GetExtent().SquaredLength();

		MutablePoints[0].Transform.SetLocation(Center);
		MutablePoints[0].BoundsMin = Box.Min - Center;
		MutablePoints[0].BoundsMax = Box.Max - Center;

		FPCGPoint& ConsolidatedPoint = Context->CurrentIO->GetMutablePoint(0);
		Context->MetadataBlender->PrepareForBlending(0);
		PCGExDataBlending::FPropertiesBlender PropertiesBlender = PCGExDataBlending::FPropertiesBlender(*Context->PropertyBlender);
		if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.PrepareBlending(ConsolidatedPoint, ConsolidatedPoint); }

		for (int i = 0; i < AverageDivider; i++)
		{
			FVector Location = InPoints[i].Transform.GetLocation();
			const double Weight = FVector::DistSquared(Center, Location) / SqrDist;
			Context->MetadataBlender->Blend(0, i, i, Weight);
		}

		if (PropertiesBlender.bRequiresPrepare) { PropertiesBlender.CompleteBlending(ConsolidatedPoint); }
		Context->MetadataBlender->CompleteBlending(0, AverageDivider);

		MutablePoints[0].Transform.SetLocation(Center);
		MutablePoints[0].BoundsMin = Box.Min - Center;
		MutablePoints[0].BoundsMax = Box.Max - Center;

		PCGEX_WRITE_MARK(PointsCount, AverageDivider)

		Context->MetadataBlender->Write();

		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);

	}

	return Context->IsDone();
}
#undef PCGEX_WRITE_MARK

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
