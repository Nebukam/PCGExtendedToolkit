// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExData.h"
#include "UObject/Object.h"

namespace PCGExData
{
	class FBufferHelper : public TSharedFromThis<FBufferHelper>
	{
		TSharedPtr<FFacade> DataFacade;
		TMap<FName, TSharedPtr<FBufferBase>> BufferMap;
		mutable FRWLock BufferLock;

	public:
		explicit FBufferHelper(const TSharedRef<FFacade>& InDataFacade):
			DataFacade(InDataFacade)
		{
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBuffer(const FName InName)
		{
			{
				FReadScopeLock ReadScopeLock(BufferLock);
				if (TSharedPtr<FBufferBase>* BufferPtr = BufferMap.Find(InName))
				{
					if (!(*BufferPtr)->IsA<T>())
					{
						UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute that already exist, with a different type (%s)"), *InName.ToString())
						return nullptr;
					}

					return StaticCastSharedPtr<TBuffer<T>>(*BufferPtr);
				}
			}
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (PCGEx::IsPCGExAttribute(InName))
				{
					UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute with a protected prefix (%s)"), *InName.ToString())
					return nullptr;
				}
				TSharedPtr<TBuffer<T>> NewBuffer = DataFacade->GetWritable<T>(InName, PCGExData::EBufferInit::New);
				BufferMap.Add(InName, StaticCastSharedPtr<FBufferBase>(NewBuffer));
				return NewBuffer;
			}
		}

		template <typename T>
		TSharedPtr<TBuffer<T>> GetBuffer(const FName InName, const T& DefaultValue)
		{
			{
				FReadScopeLock ReadScopeLock(BufferLock);
				if (TSharedPtr<FBufferBase>* BufferPtr = BufferMap.Find(InName))
				{
					if (!(*BufferPtr)->IsA<T>())
					{
						UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute that already exist, with a different type (%s)"), *InName.ToString())
						return nullptr;
					}

					return StaticCastSharedPtr<TBuffer<T>>(*BufferPtr);
				}
			}
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (PCGEx::IsPCGExAttribute(InName))
				{
					UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute with a protected prefix (%s)"), *InName.ToString())
					return nullptr;
				}
				TSharedPtr<TBuffer<T>> NewBuffer = DataFacade->GetWritable<T>(InName, DefaultValue, true, PCGExData::EBufferInit::New);
				BufferMap.Add(InName, StaticCastSharedPtr<FBufferBase>(NewBuffer));
				return NewBuffer;
			}
		}

		template <typename T>
		FORCEINLINE bool SetValue(const FName& InAttributeName, const int32 InIndex, const T& InValue)
		{
			TSharedPtr<TBuffer<T>> Buffer = GetBuffer<T>(InAttributeName);
			if (!Buffer) { return false; }
			Buffer->GetMutable(InIndex) = InValue;
			return true;
		}
	};
}
