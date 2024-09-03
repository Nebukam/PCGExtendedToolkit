// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExAttributeRemap.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExComponentRemapRule
{
	GENERATED_BODY()

	FPCGExComponentRemapRule()
	{
	}

	FPCGExComponentRemapRule(const FPCGExComponentRemapRule& Other):
		InputClampDetails(Other.InputClampDetails),
		RemapDetails(Other.RemapDetails),
		OutputClampDetails(Other.OutputClampDetails)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExClampDetails InputClampDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExRemapDetails RemapDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExClampDetails OutputClampDetails;

	TArray<double> MinCache;
	TArray<double> MaxCache;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributeRemapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AttributeRemap, "Attribute Remap", "Remap a single property or attribute.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Source attribute to remap */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName SourceAttributeName;

	/** Target attribute to write remapped value to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName TargetAttributeName;

	/** The default remap rule, used for single component values, or first component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Remap (Default)"))
	FPCGExComponentRemapRule BaseRemap;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideComponent2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideComponent2", DisplayName="Remap (2nd Component)"))
	FPCGExComponentRemapRule Component2RemapOverride;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideComponent3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideComponent3", DisplayName="Remap (3rd Component)"))
	FPCGExComponentRemapRule Component3RemapOverride;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOverrideComponent4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOverrideComponent4", DisplayName="Remap (4th Component)"))
	FPCGExComponentRemapRule Component4RemapOverride;

private:
	friend class FPCGExAttributeRemapElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeRemapContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExAttributeRemapElement;

	virtual ~FPCGExAttributeRemapContext() override;

	FPCGExComponentRemapRule RemapSettings[4];
	int32 RemapIndices[4];
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeRemapElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAttributeRemap
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExAttributeRemapContext* LocalTypedContext = nullptr;
		const UPCGExAttributeRemapSettings* LocalSettings = nullptr;

		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		int32 Dimensions = 0;

		TArray<FPCGExComponentRemapRule> Rules;

		PCGEx::FAttributeIOBase* CacheWriter = nullptr;
		PCGEx::FAttributeIOBase* CacheReader = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;

		template <typename T>
		void RemapRange(const int32 StartIndex, const int32 Count, T DummyValue)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::RemapRange);

			PCGEx::TAttributeWriter<T>* Writer = static_cast<PCGEx::TAttributeWriter<T>*>(CacheWriter);

			for (int d = 0; d < Dimensions; d++)
			{
				FPCGExComponentRemapRule& Rule = Rules[d];

				double VAL;

				if (Rule.RemapDetails.bUseAbsoluteRange)
				{
					if (Rule.RemapDetails.bPreserveSign)
					{
						for (int i = StartIndex; i < StartIndex + Count; i++)
						{
							T& V = Writer->Values[i];
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(FMath::Abs(VAL)) * PCGExMath::SignPlus(VAL);
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
					else
					{
						for (int i = StartIndex; i < StartIndex + Count; i++)
						{
							T& V = Writer->Values[i];
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(FMath::Abs(VAL));
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
				}
				else
				{
					if (Rule.RemapDetails.bPreserveSign)
					{
						for (int i = StartIndex; i < StartIndex + Count; i++)
						{
							T& V = Writer->Values[i];
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(VAL);
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
					else
					{
						for (int i = StartIndex; i < StartIndex + Count; i++)
						{
							T& V = Writer->Values[i];
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(FMath::Abs(VAL));
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
				}
			}
		}

		void OnPreparationComplete();

		virtual void CompleteWork() override;
	};
}
