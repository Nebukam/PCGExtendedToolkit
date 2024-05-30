#pragma once

#include <queue>
#include <vector>

namespace PCGExSearch
{
	class TScoredQueue
	{
		struct FScoredNode
		{
			int32 Id;
			double Score = 0;

			/** Creates and initializes a new node. */
			explicit FScoredNode(const int32& InItem, const double InScore)
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

		TScoredQueue(const int32 Size, const int32& Item, const double Score)
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

		FORCEINLINE void Enqueue(const int32& Id, const double Score)
		{
			Scores[Id] = Score;
			InternalQueue.push(FScoredNode(Id, Score));
		}

		FORCEINLINE bool Dequeue(int32& Item, double& OutScore)
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
	};
}
