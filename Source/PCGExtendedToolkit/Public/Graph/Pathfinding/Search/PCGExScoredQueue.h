#pragma once
#include "PCGEx.h"

namespace PCGExSearch
{
	class TScoredQueue
	{
		using FElementType = int32;

		struct FScoredNode
		{
			FElementType Id;
			double Score = 0;

			/** Creates and initializes a new node. */
			explicit FScoredNode(const FElementType& InItem, const double InScore)
				: Id(InItem), Score(InScore)
			{
			}

			/** Creates and initializes a new node. */
			explicit FScoredNode(FElementType&& InItem, const double InScore)
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

		TScoredQueue(const int32 Size, const FElementType& Item, const double Score)
		{
			Scores.SetNum(Size);
			Enqueue(Item, Score);
		}

		~TScoredQueue()
		{
			std::priority_queue<FScoredNode, std::vector<FScoredNode>, std::greater<FScoredNode>> EmptyQueue;
			std::swap(InternalQueue, EmptyQueue);
			Scores.Empty();
		}

		void Enqueue(const FElementType& Id, const double Score)
		{
			Scores[Id] = Score;
			InternalQueue.push(FScoredNode(Id, Score));
		}

		bool Dequeue(FElementType& Item, double& OutScore)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ScoredQueue::Dequeue);

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
	};
}
