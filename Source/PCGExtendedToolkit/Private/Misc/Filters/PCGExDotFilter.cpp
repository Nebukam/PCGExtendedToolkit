// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"

PCGExDataFilter::TFilter* UPCGExDotFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TDotFilter(this);
}

void PCGExPointsFilter::TDotFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	TFilter::Capture(InContext, PointIO);

	OperandA = new PCGEx::FLocalVectorGetter();
	OperandA->Capture(TypedFilterFactory->Descriptor.OperandA);
	OperandA->Grab(*PointIO, false);
	bValid = OperandA->IsUsable(PointIO->GetNum());

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return;
	}

	if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::FLocalVectorGetter();
		OperandB->Capture(TypedFilterFactory->Descriptor.OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}
}

bool PCGExPointsFilter::TDotFilter::Test(const int32 PointIndex) const
{
	FVector A = OperandA->Values[PointIndex];
	FVector B = TypedFilterFactory->Descriptor.CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : TypedFilterFactory->Descriptor.OperandBConstant;

	if (TypedFilterFactory->Descriptor.bTransformOperandA || TypedFilterFactory->Descriptor.bTransformOperandB)
	{
		const FTransform PtTransform = FilteredIO->GetInPoint(PointIndex).Transform;
		if (TypedFilterFactory->Descriptor.bTransformOperandA) { A = PtTransform.TransformVector(A); }
		if (TypedFilterFactory->Descriptor.bTransformOperandB) { B = PtTransform.TransformVector(B); }
	}

	const double Dot = TypedFilterFactory->Descriptor.bUnsignedDot ? FMath::Abs(FVector::DotProduct(A, B)) : FVector::DotProduct(A, B);

	if (TypedFilterFactory->Descriptor.bDoExcludeAboveDot && Dot > TypedFilterFactory->Descriptor.ExcludeAbove) { return false; }
	if (TypedFilterFactory->Descriptor.bDoExcludeBelowDot && Dot < TypedFilterFactory->Descriptor.ExcludeBelow) { return false; }

	return true;
}

#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExDotFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Dot)

FString UPCGExDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Descriptor.OperandA.GetName().ToString() + " . ";

	if (Descriptor.CompareAgainst == EPCGExOperandType::Attribute) { DisplayName += Descriptor.OperandB.GetName().ToString(); }
	else { DisplayName += " (Constant)"; }

	return DisplayName;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
