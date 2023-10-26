// PriorityQueue.h

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "UObject/NoExportTypes.h"

template <typename T>
class PCGEXTENDEDTOOLKIT_API PCGExPriorityQueue
{
private:
    // Internal storage for the heap
    TArray<TPair<T, double>> Heap;

    // Function to compare the priorities of two elements
    bool ShouldSwap(int32 IndexA, int32 IndexB) const
    {
        return Heap[IndexA].Value < Heap[IndexB].Value;
    }

    // Function to swap two elements in the heap
    void Swap(int32 IndexA, int32 IndexB)
    {
        TPair<T, double> Temp = Heap[IndexA];
        Heap[IndexA] = Heap[IndexB];
        Heap[IndexB] = Temp;
    }

    // Recursive function to heapify a subtree rooted at given index
    void Heapify(int32 Index)
    {
        int32 LeftChild = (2 * Index) + 1;
        int32 RightChild = (2 * Index) + 2;
        int32 Smallest = Index;

        if (LeftChild < Heap.Num() && ShouldSwap(LeftChild, Smallest))
        {
            Smallest = LeftChild;
        }

        if (RightChild < Heap.Num() && ShouldSwap(RightChild, Smallest))
        {
            Smallest = RightChild;
        }

        if (Smallest != Index)
        {
            Swap(Index, Smallest);
            Heapify(Smallest);
        }
    }

public:
    // Function to add an element to the heap
    void Enqueue(T Element, double Priority)
    {
        Heap.Add(TPair<T, double>(Element, Priority));

        // Bubble up
        int32 Index = Heap.Num() - 1;
        while (Index != 0 && ShouldSwap(Index, (Index - 1) / 2))
        {
            Swap(Index, (Index - 1) / 2);
            Index = (Index - 1) / 2;
        }
    }

    // Function to remove and return the highest priority element from the heap
    T Dequeue()
    {
        T Element = Heap[0].Key;

        // Replace the root of the heap with the last element
        Heap[0] = Heap.Last();
        Heap.RemoveAt(Heap.Num() - 1);

        // Heapify the root element
        Heapify(0);

        return Element;
    }

    // Function to check if the heap is empty
    bool IsEmpty() const
    {
        return Heap.Num() == 0;
    }
};
