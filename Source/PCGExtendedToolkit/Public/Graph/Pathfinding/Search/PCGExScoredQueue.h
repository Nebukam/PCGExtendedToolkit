﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <queue>
#include <vector>

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Math/NumericLimits.h"
#include "Templates/UnrealTemplate.h"

namespace PCGExSearch
{
	class FScoredQueue
	{
		struct FScoredNode
		{
			int32 Id;
			double Score = 0;

			/** Creates and initializes a new node. */
			explicit FScoredNode(const int32 InItem, const double InScore)
				: Id(InItem), Score(InScore)
			{
			}

			/** Creates and initializes a new node. */
			explicit FScoredNode(int32&& InItem, const double InScore)
				: Id(MoveTemp(InItem)), Score(InScore)
			{
			}

			bool operator<(const FScoredNode& Other) const { return Score < Other.Score; }
			bool operator>(const FScoredNode& Other) const { return Score > Other.Score; }
		};

	protected:
		std::priority_queue<FScoredNode, std::vector<FScoredNode>, std::greater<FScoredNode>> InternalQueue;

	public:
		TArray<double> Scores;

		FScoredQueue(const int32 Size) { Scores.Init(MAX_dbl, Size); }

		~FScoredQueue()
		{
			std::priority_queue<FScoredNode, std::vector<FScoredNode>, std::greater<FScoredNode>> EmptyQueue;
			std::swap(InternalQueue, EmptyQueue);
		}

		bool Enqueue(const int32 Index, const double InScore)
		{
			double& RegisteredScore = Scores[Index];
			if (RegisteredScore <= InScore) { return false; }

			RegisteredScore = InScore;
			InternalQueue.push(FScoredNode(Index, InScore));
			return true;
		}

		bool Dequeue(int32& Item, double& OutScore)
		{
			//TRACE_CPUPROFILER_EVENT_SCOPE(ScoredQueue::Dequeue);

			while (!InternalQueue.empty())
			{
				const FScoredNode TopNode = InternalQueue.top();
				InternalQueue.pop();

				if (TopNode.Score == Scores[TopNode.Id])
				{
					Item = TopNode.Id;
					OutScore = TopNode.Score;
					return true;
				}
			}

			return false;
		}

		void Reset()
		{
			InternalQueue = decltype(InternalQueue)();
			for (double& Score : Scores) { Score = MAX_dbl; }
		}
	};
}
