// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Blending/PCGExDataBlending.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExFactoryProvider.h"

#include "PCGExDataFilter.generated.h"

namespace PCGExDataFilter
{
	class TFilter;
	
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Operand Type"))
enum class EPCGExOperandType : uint8
{
	Attribute UMETA(DisplayName = "Attribute", ToolTip="Use a local attribute value."),
	Constant UMETA(DisplayName = "Constant", ToolTip="Use a constant, static value."),
};

namespace PCGExDataFilter
{
	enum class EType : uint8
	{
		Default = 0,
		Cluster
	};

}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	FORCEINLINE virtual PCGExFactories::EType GetFactoryType() const override;

	int32 Priority = 0;
	virtual PCGExDataFilter::TFilter* CreateFilter() const;
};

namespace PCGExDataFilter
{
	constexpr PCGExMT::AsyncState State_FilteringPoints = __COUNTER__;

	const FName OutputFilterLabel = TEXT("Filter");
	const FName SourceFiltersLabel = TEXT("Filters");
	const FName OutputInsideFiltersLabel = TEXT("Inside");
	const FName OutputOutsideFiltersLabel = TEXT("Outside");

	class PCGEXTENDEDTOOLKIT_API TFilter
	{
	public:
		explicit TFilter(const UPCGExFilterFactoryBase* InFactory):
			Factory(InFactory)
		{
		}

		const UPCGExFilterFactoryBase* Factory;
		TArray<bool> Results;

		int32 Index = 0;
		bool bValid = true;

		FORCEINLINE virtual EType GetFilterType() const;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO);
		FORCEINLINE virtual bool Test(const int32 PointIndex) const;
		virtual void PrepareForTesting(const PCGExData::FPointIO* PointIO);
		virtual void PrepareForTesting(const PCGExData::FPointIO* PointIO, const TArrayView<int32>& PointIndices);

		virtual ~TFilter()
		{
			Results.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API TFilterManager
	{
	public:
		explicit TFilterManager(const PCGExData::FPointIO* InPointIO);

		TArray<TFilter*> Handlers;
		bool bValid = false;

		const PCGExData::FPointIO* PointIO = nullptr;

		template <typename T_DEF>
		void Register(const FPCGContext* InContext, const TArray<T_DEF*>& InDefinitions, const PCGExData::FPointIO* InPointIO)
		{
			RegisterAndCapture(InContext, InDefinitions, [&](TFilter* Handler) { Handler->Capture(InContext, InPointIO); });
		}

		template <typename T_DEF, class CaptureFunc>
		void RegisterAndCapture(const FPCGContext* InContext, const TArray<T_DEF*>& InFactories, CaptureFunc&& InCaptureFn)
		{
			for (T_DEF* Factory : InFactories)
			{
				TFilter* Handler = Factory->CreateFilter();
				InCaptureFn(Handler);

				if (!Handler->bValid)
				{
					delete Handler;
					continue;
				}

				Handlers.Add(Handler);
			}

			bValid = !Handlers.IsEmpty();

			if (!bValid) { return; }

			// Sort mappings so higher priorities come last, as they have to potential to override values.
			Handlers.Sort([&](const TFilter& A, const TFilter& B) { return A.Factory->Priority < B.Factory->Priority; });

			// Update index & partials
			for (int i = 0; i < Handlers.Num(); i++)
			{
				Handlers[i]->Index = i;
				PostProcessHandler(Handlers[i]);
			}
		}

		virtual void PrepareForTesting();
		virtual void PrepareForTesting(const TArrayView<int32>& PointIndices);

		virtual void Test(const int32 PointIndex);

		virtual ~TFilterManager()
		{
			PCGEX_DELETE_TARRAY(Handlers)
		}

	protected:
		virtual void PostProcessHandler(TFilter* Handler);
	};

	class PCGEXTENDEDTOOLKIT_API TEarlyExitFilterManager : public TFilterManager
	{
	public:
		explicit TEarlyExitFilterManager(const PCGExData::FPointIO* InPointIO);

		TArray<bool> Results;

		virtual void Test(const int32 PointIndex) override;
		virtual void PrepareForTesting() override;
	};

	template <typename T_DEF>
	static bool GetInputFactories(FPCGContext* InContext, const FName InLabel, TArray<T_DEF*>& OutFactories, const TSet<PCGExFactories::EType>& Types, const bool bThrowError = true)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);

		TSet<FName> UniqueStatesNames;
		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			if (const T_DEF* State = Cast<T_DEF>(TaggedData.Data))
			{
				if (!Types.Contains(State->GetFactoryType()))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Input '{0}' is not supported."), FText::FromString(State->GetClass()->GetName())));
					continue;
				}

				OutFactories.AddUnique(const_cast<T_DEF*>(State));
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Input '{0}' is not supported."), FText::FromString(TaggedData.Data->GetClass()->GetName())));
			}
		}

		UniqueStatesNames.Empty();

		if (OutFactories.IsEmpty())
		{
			if (bThrowError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing valid filters.")); }
			return false;
		}

		return true;
	}
}
