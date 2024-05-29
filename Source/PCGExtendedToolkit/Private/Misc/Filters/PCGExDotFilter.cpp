// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"

PCGExDataFilter::TFilter* UPCGExDotFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TDotFilter(this);
}

void PCGExPointsFilter::TDotFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	bValid = true;

	OperandA = new PCGEx::FLocalVectorGetter();
	OperandA->Capture(TypedFilterFactory->OperandA);
	OperandA->Grab(*PointIO, false);
	bValid = OperandA->IsUsable(PointIO->GetNum());

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return;
	}

	if (TypedFilterFactory->CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::FLocalVectorGetter();
		OperandB->Capture(TypedFilterFactory->OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}
}

bool PCGExPointsFilter::TDotFilter::Test(const int32 PointIndex) const
{
	const FVector A = OperandA->Values[PointIndex];
	const FVector B = TypedFilterFactory->CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : TypedFilterFactory->OperandBConstant;

	const double Dot = TypedFilterFactory->bUnsignedDot ? FMath::Abs(FVector::DotProduct(A, B)) : FVector::DotProduct(A, B);

	if (TypedFilterFactory->bExcludeAboveDot && Dot > TypedFilterFactory->ExcludeAbove) { return false; }
	if (TypedFilterFactory->bExcludeBelowDot && Dot < TypedFilterFactory->ExcludeBelow) { return false; }

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
