// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Most helpers here are Copyright Epic Games, Inc. All Rights Reserved, cherry picked for 5.3

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGExMacros.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"
#include "Data/PCGSpatialData.h"
#include "Engine/AssetManager.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Async/Async.h"

#include "PCGExHelpers.generated.h"

UENUM()
enum class EPCGExPointPropertyOutput : uint8
{
	None      = 0 UMETA(DisplayName = "None", Tooltip="..."),
	Density   = 1 UMETA(DisplayName = "Density", Tooltip="..."),
	Steepness = 2 UMETA(DisplayName = "Steepness", Tooltip="..."),
	ColorR    = 3 UMETA(DisplayName = "R Channel", Tooltip="..."),
	ColorG    = 4 UMETA(DisplayName = "G Channel", Tooltip="..."),
	ColorB    = 5 UMETA(DisplayName = "B Channel", Tooltip="..."),
	ColorA    = 6 UMETA(DisplayName = "A Channel", Tooltip="..."),
};


UINTERFACE(MinimalAPI)
class UPCGExManagedObjectInterface : public UInterface
{
	GENERATED_BODY()
};

class IPCGExManagedObjectInterface
{
	GENERATED_BODY()

public:
	virtual void Cleanup() = 0;
};

UINTERFACE(MinimalAPI)
class UPCGExManagedComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class IPCGExManagedComponentInterface
{
	GENERATED_BODY()

public:
	virtual void SetManagedComponent(UPCGManagedComponent* InManagedComponent) = 0;
	virtual UPCGManagedComponent* GetManagedComponent() = 0;
};

namespace PCGExHelpers
{
	template <typename T>
	static TObjectPtr<T> LoadBlocking_AnyThread(const TSoftObjectPtr<T>& SoftObjectPtr, const FSoftObjectPath& FallbackPath = nullptr)
	{
		// If the requested object is valid and loaded, early exit
		TObjectPtr<T> LoadedObject = SoftObjectPtr.Get();
		if (LoadedObject) { return LoadedObject; }

		// If not, make sure it's a valid path, and if not fallback to the fallback path
		FSoftObjectPath ToBeLoaded = SoftObjectPtr.ToSoftObjectPath().IsValid() ? SoftObjectPtr.ToSoftObjectPath() : FallbackPath;

		// Make sure we have a valid path at all
		if (!ToBeLoaded.IsValid()) { return nullptr; }

		// Check if the fallback path is loaded, early exit
		LoadedObject = TSoftObjectPtr<T>(ToBeLoaded).Get();
		if (LoadedObject) { return LoadedObject; }


		if (IsInGameThread())
		{
			// We're in the game thread, request synchronous load
			UAssetManager::GetStreamableManager().RequestSyncLoad(ToBeLoaded);
		}
		else
		{
			// We're not in the game thread, we need to dispatch loading to the main thread
			// and wait in the current one
			FEvent* BlockingEvent = FPlatformProcess::GetSynchEventFromPool();
			AsyncTask(
				ENamedThreads::GameThread, [BlockingEvent, ToBeLoaded]()
				{
					const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						ToBeLoaded, [BlockingEvent]() { BlockingEvent->Trigger(); });

					if (!Handle || !Handle->IsActive()) { BlockingEvent->Trigger(); }
				});

			BlockingEvent->Wait();
			FPlatformProcess::ReturnSynchEventToPool(BlockingEvent);
		}

		return TSoftObjectPtr<T>(ToBeLoaded).Get();
	}

	template <typename T>
	static void LoadBlocking_AnyThread(const TSharedPtr<TSet<FSoftObjectPath>>& Paths)
	{
		if (IsInGameThread())
		{
			UAssetManager::GetStreamableManager().RequestSyncLoad(Paths->Array());
		}
		else
		{
			TWeakPtr<TSet<FSoftObjectPath>> WeakPaths = Paths;
			FEvent* BlockingEvent = FPlatformProcess::GetSynchEventFromPool();
			AsyncTask(
				ENamedThreads::GameThread, [BlockingEvent, WeakPaths]()
				{
					const TSharedPtr<TSet<FSoftObjectPath>> ToBeLoaded = WeakPaths.Pin();
					if (!ToBeLoaded)
					{
						BlockingEvent->Trigger();
						return;
					}

					const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
						ToBeLoaded->Array(), [BlockingEvent]() { BlockingEvent->Trigger(); });

					if (!Handle || !Handle->IsActive()) { BlockingEvent->Trigger(); }
				});

			BlockingEvent->Wait();
			FPlatformProcess::ReturnSynchEventToPool(BlockingEvent);
		}
	}

	static void CopyStructProperties(const void* SourceStruct, void* TargetStruct, const UStruct* SourceStructType, const UStruct* TargetStructType)
	{
		for (TFieldIterator<FProperty> SourceIt(SourceStructType); SourceIt; ++SourceIt)
		{
			const FProperty* SourceProperty = *SourceIt;
			const FProperty* TargetProperty = TargetStructType->FindPropertyByName(SourceProperty->GetFName());
			if (!TargetProperty || SourceProperty->GetClass() != TargetProperty->GetClass()) { continue; }

			if (SourceProperty->SameType(TargetProperty))
			{
				const void* SourceValue = SourceProperty->ContainerPtrToValuePtr<void>(SourceStruct);
				void* TargetValue = TargetProperty->ContainerPtrToValuePtr<void>(TargetStruct);

				SourceProperty->CopyCompleteValue(TargetValue, SourceValue);
			}
		}
	}

	static void SetPointProperty(FPCGPoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty)
	{
		switch (InProperty)
		{
		default:
		case EPCGExPointPropertyOutput::None:
			break;
		case EPCGExPointPropertyOutput::Density:
			InPoint.Density = InValue;
			break;
		case EPCGExPointPropertyOutput::Steepness:
			InPoint.Steepness = InValue;
			break;
		case EPCGExPointPropertyOutput::ColorR:
			InPoint.Color.Component(0) = InValue;
			break;
		case EPCGExPointPropertyOutput::ColorG:
			InPoint.Color.Component(1) = InValue;
			break;
		case EPCGExPointPropertyOutput::ColorB:
			InPoint.Color.Component(2) = InValue;
			break;
		case EPCGExPointPropertyOutput::ColorA:
			InPoint.Color.Component(3) = InValue;
			break;
		}
	}

	static TArray<FString> GetStringArrayFromCommaSeparatedList(const FString& InCommaSeparatedString)
	{
		TArray<FString> Result;
		InCommaSeparatedString.ParseIntoArray(Result, TEXT(","));
		// Trim leading and trailing spaces
		for (int i = 0; i < Result.Num(); i++)
		{
			FString& String = Result[i];
			String.TrimStartAndEndInline();
			if (String.IsEmpty())
			{
				Result.RemoveAt(i);
				i--;
			}
		}

		return Result;
	}

	static TArray<UFunction*> FindUserFunctions(const TSubclassOf<AActor>& ActorClass, const TArray<FName>& FunctionNames, const TArray<const UFunction*>& FunctionPrototypes, const FPCGContext* InContext)
	{
		TArray<UFunction*> Functions;

		if (!ActorClass)
		{
			return Functions;
		}

		for (FName FunctionName : FunctionNames)
		{
			if (FunctionName == NAME_None)
			{
				continue;
			}

			if (UFunction* Function = ActorClass->FindFunctionByName(FunctionName))
			{
#if WITH_EDITOR
				if (!Function->GetBoolMetaData(TEXT("CallInEditor")))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT( "Function '{0}' in class '{1}' requires CallInEditor to be true while in-editor."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
					continue;
				}
#endif
				for (const UFunction* Prototype : FunctionPrototypes)
				{
					if (Function->IsSignatureCompatibleWith(Prototype))
					{
						Functions.Add(Function);
						break;
					}
				}

				if (Functions.IsEmpty() || Functions.Last() != Function)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Function '{0}' in class '{1}' has incorrect parameters."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
				}
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Function '{0}' was not found in class '{1}'."), FText::FromName(FunctionName), FText::FromName(ActorClass->GetFName())));
			}
		}

		return Functions;
	}
}

/** Holds function prototypes used to match against actor function signatures. */
UCLASS(MinimalAPI)
class UPCGExFunctionPrototypes : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UFunction* GetPrototypeWithNoParams() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithNoParams")); }
	static UFunction* GetPrototypeWithPointAndMetadata() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithPointAndMetadata")); }

private:
	UFUNCTION()
	void PrototypeWithNoParams()
	{
	}

	UFUNCTION()
	void PrototypeWithPointAndMetadata(FPCGPoint Point, const UPCGMetadata* Metadata)
	{
	}
};

namespace PCGEx
{
	class FLifecycle final : public TSharedFromThis<FLifecycle>
	{
	public:
		FLifecycle() = default;
		~FLifecycle() = default;

		void Terminate() { bAlive = false; }
		bool IsAlive() const { return bAlive; }

	private:
		std::atomic<bool> bAlive{true};
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FManagedObjects
	{
		mutable FRWLock ManagedObjectLock;
		mutable FRWLock DuplicatedObjectLock;

		FPCGContext* Context = nullptr;
		TSharedPtr<FLifecycle> Lifecycle;
		TSet<UObject*> ManagedObjects;

		bool IsFlushing() const { return static_cast<bool>(bFlushing); }

		explicit FManagedObjects(FPCGContext* InContext, const TSharedPtr<FLifecycle>& InLifecycle):
			Context(InContext), Lifecycle(InLifecycle)
		{
		}

		~FManagedObjects();

		void Flush();

		void Add(UObject* InObject);
		bool Remove(UObject* InObject);

		template <class T, typename... Args>
		T* New(Args&&... InArgs)
		{
			check(Lifecycle->IsAlive())
			check(!IsFlushing())

			T* Object = nullptr;
			if (!IsInGameThread())
			{
				{
					FGCScopeGuard Scope;
					Object = NewObject<T>(std::forward<Args>(InArgs)...);
				}
				check(Object);
			}
			else
			{
				Object = NewObject<T>(std::forward<Args>(InArgs)...);
			}

			Add(Object);
			return Object;
		}

		template <class T>
		T* Duplicate(const UPCGData* InData)
		{
			check(Lifecycle->IsAlive())
			check(!IsFlushing())

			T* Object = nullptr;

#if PCGEX_ENGINE_VERSION >= 505

			if (!IsInGameThread())
			{
				FWriteScopeLock WriteScopeLock(ManagedObjectLock);

				// Ensure PCG AsyncState is up to date
				bool bRestoreTo = Context->AsyncState.bIsRunningOnMainThread;
				Context->AsyncState.bIsRunningOnMainThread = false;

				// Do the duplicate (uses AnyThread that requires bIsRunningOnMainThread to be up-to-date)
				Object = Cast<T>(InData->DuplicateData(Context, true));

				Context->AsyncState.bIsRunningOnMainThread = bRestoreTo;

				check(Object);
				{
					FWriteScopeLock DupeLock(DuplicatedObjectLock);
					DuplicateObjects.Add(Object);
				}
			}
			else
			{
				FWriteScopeLock WriteScopeLock(ManagedObjectLock);
				Object = Cast<T>(InData->DuplicateData(Context, true));
				check(Object);
				{
					FWriteScopeLock DupeLock(DuplicatedObjectLock);
					DuplicateObjects.Add(Object);
				}
			}


#elif PCGEX_ENGINE_VERSION == 504
			if (!IsInGameThread())
			{
				{
					FGCScopeGuard Scope;
					FWriteScopeLock WriteScopeLock(ManagedObjectLock);
					Object = Cast<T>(InData->DuplicateData(true));
				}
				check(Object);
			}
			else
			{
				FWriteScopeLock WriteScopeLock(ManagedObjectLock);
				Object = Cast<T>(InData->DuplicateData(true));
			}

#else
			
			const UPCGSpatialData* AsSpatialData = Cast<UPCGSpatialData>(InData);
			check(AsSpatialData)
			
			if (!IsInGameThread())
			{
				{
					FGCScopeGuard Scope;
					FWriteScopeLock WriteScopeLock(ManagedObjectLock);
					Object = Cast<T>(AsSpatialData->DuplicateData(true));
				}
				check(Object);
			}
			else
			{
				FWriteScopeLock WriteScopeLock(ManagedObjectLock);
				Object = Cast<T>(AsSpatialData->DuplicateData(true));
			}
			
#endif

			Add(Object);
			return Object;
		}

		void Destroy(UObject* InObject);

	protected:
		TSet<UObject*> DuplicateObjects;
		void RecursivelyClearAsyncFlagUnsafe(UObject* InObject) const;

	private:
		int8 bFlushing = 0;
	};

	static FVector GetPointsCentroid(const TArray<FPCGPoint>& InPoints)
	{
		FVector Position = FVector::ZeroVector;
		for (const FPCGPoint& Pt : InPoints) { Position += Pt.Transform.GetLocation(); }
		return Position / static_cast<double>(InPoints.Num());
	}

#pragma region Metadata Type

	template <typename T>
	FORCEINLINE static EPCGMetadataTypes GetMetadataType()
	{
		if constexpr (std::is_same_v<T, bool>) { return EPCGMetadataTypes::Boolean; }
		else if constexpr (std::is_same_v<T, int32>) { return EPCGMetadataTypes::Integer32; }
		else if constexpr (std::is_same_v<T, int64>) { return EPCGMetadataTypes::Integer64; }
		else if constexpr (std::is_same_v<T, float>) { return EPCGMetadataTypes::Float; }
		else if constexpr (std::is_same_v<T, double>) { return EPCGMetadataTypes::Double; }
		else if constexpr (std::is_same_v<T, FVector2D>) { return EPCGMetadataTypes::Vector2; }
		else if constexpr (std::is_same_v<T, FVector>) { return EPCGMetadataTypes::Vector; }
		else if constexpr (std::is_same_v<T, FVector4>) { return EPCGMetadataTypes::Vector4; }
		else if constexpr (std::is_same_v<T, FQuat>) { return EPCGMetadataTypes::Quaternion; }
		else if constexpr (std::is_same_v<T, FRotator>) { return EPCGMetadataTypes::Rotator; }
		else if constexpr (std::is_same_v<T, FTransform>) { return EPCGMetadataTypes::Transform; }
		else if constexpr (std::is_same_v<T, FString>) { return EPCGMetadataTypes::String; }
		else if constexpr (std::is_same_v<T, FName>) { return EPCGMetadataTypes::Name; }
#if PCGEX_ENGINE_VERSION > 503
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { return EPCGMetadataTypes::SoftClassPath; }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { return EPCGMetadataTypes::SoftObjectPath; }
#endif
		else { return EPCGMetadataTypes::Unknown; }
	}

	FORCEINLINE static EPCGMetadataTypes GetPropertyType(const EPCGPointProperties Property)
	{
		switch (Property)
		{
		case EPCGPointProperties::Density:
			return EPCGMetadataTypes::Float;
		case EPCGPointProperties::BoundsMin:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::BoundsMax:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Extents:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Color:
			return EPCGMetadataTypes::Vector4;
		case EPCGPointProperties::Position:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Rotation:
			return EPCGMetadataTypes::Rotator;
		case EPCGPointProperties::Scale:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Transform:
			return EPCGMetadataTypes::Transform;
		case EPCGPointProperties::Steepness:
			return EPCGMetadataTypes::Float;
		case EPCGPointProperties::LocalCenter:
			return EPCGMetadataTypes::Vector;
		case EPCGPointProperties::Seed:
			return EPCGMetadataTypes::Integer32;
		default:
			return EPCGMetadataTypes::Unknown;
		}
	}

	constexpr bool DummyBoolean = bool{};
	constexpr int32 DummyInteger32 = int32{};
	constexpr int64 DummyInteger64 = int64{};
	constexpr float DummyFloat = float{};
	constexpr double DummyDouble = double{};
	const FVector2D DummyVector2 = FVector2D::ZeroVector;
	const FVector DummyVector = FVector::ZeroVector;
	const FVector4 DummyVector4 = FVector4::Zero();
	const FQuat DummyQuaternion = FQuat::Identity;
	const FRotator DummyRotator = FRotator::ZeroRotator;
	const FTransform DummyTransform = FTransform::Identity;
	const FString DummyString = TEXT("");
	const FName DummyName = NAME_None;
	const FSoftClassPath DummySoftClassPath = FSoftClassPath{};
	const FSoftObjectPath DummySoftObjectPath = FSoftObjectPath{};

	template <typename T, typename Func>
	FORCEINLINE static void ExecuteWithRightType(Func&& Callback)
	{
		if constexpr (std::is_same_v<T, bool>) { Callback(DummyBoolean); }
		else if constexpr (std::is_same_v<T, int32>) { Callback(DummyInteger32); }
		else if constexpr (std::is_same_v<T, int64>) { Callback(DummyInteger64); }
		else if constexpr (std::is_same_v<T, float>) { Callback(DummyFloat); }
		else if constexpr (std::is_same_v<T, double>) { Callback(DummyDouble); }
		else if constexpr (std::is_same_v<T, FVector2D>) { Callback(DummyVector2); }
		else if constexpr (std::is_same_v<T, FVector>) { Callback(DummyVector); }
		else if constexpr (std::is_same_v<T, FVector4>) { Callback(DummyVector4); }
		else if constexpr (std::is_same_v<T, FQuat>) { Callback(DummyQuaternion); }
		else if constexpr (std::is_same_v<T, FRotator>) { Callback(DummyRotator); }
		else if constexpr (std::is_same_v<T, FTransform>) { Callback(DummyTransform); }
		else if constexpr (std::is_same_v<T, FString>) { Callback(DummyString); }
		else if constexpr (std::is_same_v<T, FName>) { Callback(DummyName); }
#if PCGEX_ENGINE_VERSION > 503
		else if constexpr (std::is_same_v<T, FSoftClassPath>) { Callback(DummySoftClassPath); }
		else if constexpr (std::is_same_v<T, FSoftObjectPath>) { Callback(DummySoftObjectPath); }
#endif
		else { static_assert("Unsupported type"); }
	}

	template <typename Func>
	FORCEINLINE static void ExecuteWithRightType(const EPCGMetadataTypes Type, Func&& Callback)
	{
#define PCGEX_EXECUTE_WITH_TYPE(_TYPE, _ID, ...) case EPCGMetadataTypes::_ID : ExecuteWithRightType<_TYPE>(Callback); break;

		switch (Type)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXECUTE_WITH_TYPE)
		default: ;
		}

#undef PCGEX_EXECUTE_WITH_TYPE
	}

	template <typename Func>
	FORCEINLINE static void ExecuteWithRightType(const int16 Type, Func&& Callback)
	{
		ExecuteWithRightType(static_cast<EPCGMetadataTypes>(Type), Callback);
	}

	template <typename T>
	FORCEINLINE static void InitArray(TArray<T>& InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray.SetNumUninitialized(Num); }
		else { InArray.SetNum(Num); }
	}

	template <typename T>
	FORCEINLINE static void InitArray(TSharedPtr<TArray<T>>& InArray, const int32 Num)
	{
		if (!InArray) { InArray = MakeShared<TArray<T>>(); }
		if constexpr (std::is_trivially_copyable_v<T>) { InArray->SetNumUninitialized(Num); }
		else { InArray->SetNum(Num); }
	}

	template <typename T>
	FORCEINLINE static void InitArray(TSharedRef<TArray<T>> InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray.SetNumUninitialized(Num); }
		else { InArray.SetNum(Num); }
	}

	template <typename T>
	FORCEINLINE static void InitArray(TArray<T>* InArray, const int32 Num)
	{
		if constexpr (std::is_trivially_copyable_v<T>) { InArray->SetNumUninitialized(Num); }
		else { InArray->SetNum(Num); }
	}

#pragma endregion
}
