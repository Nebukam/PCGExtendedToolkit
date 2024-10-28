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
		TSharedPtr<PCGExData::FFacade> InDataFacade;
		TMap<FName, TSharedPtr<PCGExData::FBufferBase>> BufferMap;
		mutable FRWLock BufferLock;

	public:
		explicit FBufferHelper(TSharedRef<FFacade> InDataFacade):
			InDataFacade()
		{
		}

		template <typename T>
		TSharedPtr<PCGExData::TBuffer<T>> GetBuffer(const FName InName)
		{
			{
				FReadScopeLock ReadScopeLock(BufferLock);
				if (TSharedPtr<PCGExData::FBufferBase>* BufferPtr = BufferMap.Find(InName))
				{
					if (!(*BufferPtr)->IsA<T>())
					{
						UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute that already exist, with a different type (%s)"), *InName.ToString())
						return nullptr;
					}

					return StaticCastSharedPtr<PCGExData::TBuffer<T>>(*BufferPtr);
				}
			}
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (PCGEx::IsPCGExAttribute(InName))
				{
					UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute with a protected prefix (%s)"), *InName.ToString())
					return nullptr;
				}
				TSharedPtr<PCGExData::TBuffer<T>> NewBuffer = InDataFacade->GetWritable<T>(InName, true);
				BufferMap.Add(InName, StaticCastSharedPtr<PCGExData::FBufferBase>(NewBuffer));
				return NewBuffer;
			}
		}

		template <typename T>
		TSharedPtr<PCGExData::TBuffer<T>> GetBuffer(const FName InName, const T& DefaultValue)
		{
			{
				FReadScopeLock ReadScopeLock(BufferLock);
				if (TSharedPtr<PCGExData::FBufferBase>* BufferPtr = BufferMap.Find(InName))
				{
					if (!(*BufferPtr)->IsA<T>())
					{
						UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute that already exist, with a different type (%s)"), *InName.ToString())
						return nullptr;
					}

					return StaticCastSharedPtr<PCGExData::TBuffer<T>>(*BufferPtr);
				}
			}
			{
				FWriteScopeLock WriteScopeLock(BufferLock);
				if (PCGEx::IsPCGExAttribute(InName))
				{
					UE_LOG(LogTemp, Error, TEXT("Attempted to create an attribute with a protected prefix (%s)"), *InName.ToString())
					return nullptr;
				}
				TSharedPtr<PCGExData::TBuffer<T>> NewBuffer = InDataFacade->GetWritable<T>(InName, DefaultValue, true, true);
				BufferMap.Add(InName, StaticCastSharedPtr<PCGExData::FBufferBase>(NewBuffer));
				return NewBuffer;
			}
		}
		
		template <typename T>
		FORCEINLINE bool SetValue(const FName& InAttributeName, const int32 InIndex, const T& InValue)
		{
			TSharedPtr<PCGExData::TBuffer<T>> Buffer = GetBuffer<T>(InAttributeName);
			if (!Buffer) { return false; }
			Buffer->GetMutable(InIndex) = InValue;
			return true;
		}
	};
}
