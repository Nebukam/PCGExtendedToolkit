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

	auto ExitFail = [&]()
	{
		bValid = false;

		DotComparison.Cleanup();
		PCGEX_DELETE(OperandA)
		PCGEX_DELETE(OperandB)
	};

	OperandA = new PCGEx::FLocalVectorGetter();
	OperandA->Capture(TypedFilterFactory->Descriptor.OperandA);

	if (!OperandA->Grab(PointIO, false))
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
		return ExitFail();
	}


	OperandB = new PCGEx::FLocalVectorGetter();
	if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB->Capture(TypedFilterFactory->Descriptor.OperandB);
		if (!OperandB->Grab(PointIO, false))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			return ExitFail();
		}
	}
}

bool PCGExPointsFilter::TDotFilter::Test(const int32 PointIndex) const
{
	const FPCGPoint& Point = FilteredIO->GetInPoint(PointIndex);

	const FVector A = TypedFilterFactory->Descriptor.bTransformOperandA ?
		                  OperandA->Values[PointIndex] :
		                  Point.Transform.TransformVectorNoScale(OperandA->Values[PointIndex]);

	const FVector B = TypedFilterFactory->Descriptor.bTransformOperandB ?
		                  OperandB->SafeGet(PointIndex, TypedFilterFactory->Descriptor.OperandBConstant) :
		                  Point.Transform.TransformVectorNoScale(OperandB->SafeGet(PointIndex, TypedFilterFactory->Descriptor.OperandBConstant));

	const double Dot = DotComparison.bUnsignedDot ? FMath::Abs(FVector::DotProduct(A, B)) : FVector::DotProduct(A, B);
	return DotComparison.Test(Dot, DotComparison.GetDot(FilteredIO->GetInPoint(PointIndex)));
}

#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExDotFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Dot)

FString UPCGExDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Descriptor.OperandA.GetName().ToString() + " . ";

	if (Descriptor.CompareAgainst == EPCGExFetchType::Attribute) { DisplayName += Descriptor.OperandB.GetName().ToString(); }
	else { DisplayName += " (Constant)"; }

	return DisplayName;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
