// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	/// Used to effectively store vertices beyond.
	template <typename T>
	class TVertexBuffer
	{
	public:
		TArray<T*> Items;

		TVertexBuffer()
		{
		}

		explicit TVertexBuffer(int32 InCapacity)
		{
			Items.Reserve(InCapacity);
		}

		int32 Num() { return Items.Num(); }

		T* operator[](int32 Index) const { return Items[Index]; }

		/// Adds a vertex to the buffer.
		void Add(T* Vtx) { Items.Add(Vtx); }

		/// Sets the Count to 0, otherwise does nothing.
		void Clear() { Items.Empty(); }
	};

	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TSimplexWrap : public TFSimplex<DIMENSIONS, VECTOR_TYPE>
	{
	public:
		/// Gets or sets the vertices beyond.
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* VerticesBeyond = nullptr;

		/// The furthest vertex.
		TFVtx<DIMENSIONS, VECTOR_TYPE>* FurthestVertex = nullptr;

		/// Prev node in the list.
		TSimplexWrap* Previous = nullptr;

		/// Next node in the list.
		TSimplexWrap* Next = nullptr;

		/// Is it present in the list.
		bool bInList = false;

		explicit TSimplexWrap(TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* InBeyondList)
		{
			Clear();
			VerticesBeyond = InBeyondList;
		}

		void Clear()
		{
			Previous = nullptr;
			Next = nullptr;

			for (int i = 0; i < DIMENSIONS; i++)
			{
				this->AdjacentFaces[i] = nullptr;
				this->Vertices[i] = nullptr;
			}
		}

	};

	/// For deferred face addition.
	template <typename T>
	class TDeferredSimplex
	{
	public:
		T* Face = nullptr;
		T* Pivot = nullptr;
		T* OldFace = nullptr;
		int32 FaceIndex = -1;
		int32 PivotIndex = -1;

		void Clear()
		{
			Face = nullptr;
			Pivot = nullptr;
			OldFace = nullptr;
			FaceIndex = -1;
			PivotIndex = -1;
		}
	};

	/// A helper class used to connect faces.
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TSimplexConnector
	{
	public:
		/// The face.
		TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face = nullptr;

		/// The edge to be connected.
		int32 EdgeIndex = -1;

		/// The vertex indices.
		int32 Vertices[DIMENSIONS - 1];

		/// The hash code computed from indices.
		uint64 HashCode = 0;

		/// Prev node in the list.
		TSimplexConnector* Previous = nullptr;

		/// Next node in the list.
		TSimplexConnector* Next = nullptr;

		TSimplexConnector()
		{
			Clear();
		}

		/// Updates the connector.
		void Update(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* InFace, int InEdgeIndex)
		{
			Face = InFace;
			EdgeIndex = InEdgeIndex;

			uint64 TempHashCode = 31;

			for (int i = 0, c = 0; i < DIMENSIONS; i++)
			{
				if (i == InEdgeIndex) { continue; }
				int v = InFace.Vertices[i].Id;
				Vertices[c++] = v;
				TempHashCode += 23 * TempHashCode + static_cast<uint64>(v);
			}

			HashCode = TempHashCode;
		}

		/// Can two faces be connected.
		static bool AreConnectable(TSimplexConnector* A, TSimplexConnector* B)
		{
			if (A.HashCode != B.HashCode) return false;
			for (int i = 0; i < DIMENSIONS - 1; i++) { if (A.Vertices[i] != B.Vertices[i]) { return false; } }
			return true;
		}

		/// Connect two faces.
		static void Connect(TSimplexConnector* A, TSimplexConnector* B)
		{
			A.Face.AdjacentFaces[A.EdgeIndex] = B.Face;
			B.Face.AdjacentFaces[B.EdgeIndex] = A.Face;
		}

		void Clear()
		{
			Face = nullptr;
			EdgeIndex = -1;
			HashCode = 0;

			Previous = nullptr;
			Next = nullptr;

			for (int i = 0; i < DIMENSIONS - 1; i++) { Vertices[i] = -1; }
		}
	};

	/// Connector list.
	template <typename T>
	class ConnectorList
	{
		T* First = nullptr;
		T* Last = nullptr;

		void Clear()
		{
			First = nullptr;
			Last = nullptr;
		}

		/// Adds the element to the beginning.
		void AddFirst(T* connector)
		{
			this.First.Previous = connector;
			connector.Next = this.First;
			this.First = connector;
		}

		/// Adds a face to the list.
		void Add(T* element)
		{
			if (this.Last) { this.Last.Next = element; }
			element.Previous = this.Last;
			this.Last = element;
			if (!this.First) { this.First = element; }
		}

		/// <summary>
		/// Removes the element from the list.
		/// </summary>
		void Remove(T* connector)
		{
			if (connector.Previous) { connector.Previous.Next = connector.Next; }
			else if (!connector.Previous) { this.First = connector.Next; }

			if (connector.Next) { connector.Next.Previous = connector.Previous; }
			else if (!connector.Next) { this.Last = connector.Previous; }

			connector.Next = nullptr;
			connector.Previous = nullptr;
		}
	};

	template <typename T>
	class TSimplexList
	{
	public:
		T* First = nullptr;
		T* Last = nullptr;

		void Clear()
		{
			//TODO: Destroy
			First = nullptr;
			Last = nullptr;
		}

		/// Adds the element to the beginning.
		void AddFirst(T* Face)
		{
			Face.bInList = true;
			First.Previous = Face;
			Face.Next = First;
			First = Face;
		}

		/// Adds a face to the list.
		void Add(T* Face)
		{
			if (Face.bInList)
			{
				if (First.VerticesBeyond.Num() < Face.VerticesBeyond.Num())
				{
					Remove(Face);
					AddFirst(Face);
				}
				return;
			}

			Face.bInList = true;

			if (First && First.VerticesBeyond.Num() < Face.VerticesBeyond.Num())
			{
				First.Previous = Face;
				Face.Next = First;
				First = Face;
			}
			else
			{
				if (Last) { Last.Next = Face; }
				Face.Previous = Last;
				Last = Face;
				if (!First) { First = Face; }
			}
		}

		/// Removes the element from the list.
		void Remove(T* Face)
		{
			if (!Face.bInList) { return; }

			Face.bInList = false;

			if (Face.Previous) { Face.Previous.Next = Face.Next; }
			else if (!Face.Previous) { First = Face.Next; }

			if (Face.Next) { Face.Next.Previous = Face.Previous; }
			else if (!Face.Next) { Last = Face.Previous; }

			Face.Next = nullptr;
			Face.Previous = nullptr;
		}
	};

	/// A helper class for object allocation/storage. 
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TObjectManager
	{
	protected:
		TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* NULL_WRAP = nullptr;
		TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* NULL_CONNECTOR = nullptr;
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* NULL_BUFFER = nullptr;
		TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* NULL_DEFERRED = nullptr;

	public:
		TQueue<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> RecycledFaceStack;
		TQueue<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>*> ConnectorStack;
		TQueue<TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>*> EmptyBufferStack;
		TQueue<TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>*> DeferredSimplexStack;

		TObjectManager()
		{
			NULL_WRAP = new TSimplexWrap<DIMENSIONS, VECTOR_TYPE>(nullptr);
			NULL_CONNECTOR = new TSimplexConnector<DIMENSIONS, VECTOR_TYPE>();
			NULL_BUFFER = new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();
			NULL_DEFERRED = new TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>();
		}

		~TObjectManager()
		{
			PCGEX_DELETE(NULL_WRAP);
			PCGEX_DELETE(NULL_CONNECTOR);
			PCGEX_DELETE(NULL_BUFFER);
			PCGEX_DELETE(NULL_DEFERRED);
		}

		void Clear()
		{
			RecycledFaceStack.Empty();
			ConnectorStack.Clear();
			EmptyBufferStack.Clear();
			DeferredSimplexStack.Clear();
		}

		void DepositFace(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face)
		{
			Face->Clear();
			RecycledFaceStack.Enqueue(Face);
		}

		TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* GetFace()
		{
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>& OutFace = *NULL_WRAP;
			if (RecycledFaceStack.Dequeue(OutFace)) { return &OutFace; }
			return new TSimplexWrap<DIMENSIONS, VECTOR_TYPE>(GetVertexBuffer());
		}

		void DepositConnector(TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* Connector)
		{
			Connector->Clear();
			ConnectorStack.Enqueue(Connector);
		}

		TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* GetConnector()
		{
			TSimplexConnector<DIMENSIONS, VECTOR_TYPE>& OutConnector = *NULL_CONNECTOR;
			if (ConnectorStack.Dequeue(OutConnector)) { return &OutConnector; }
			return new TSimplexConnector<DIMENSIONS, VECTOR_TYPE>();
		}

		void DepositVertexBuffer(TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* Buffer)
		{
			Buffer.Clear();
			EmptyBufferStack.Enqueue(Buffer);
		}

		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* GetVertexBuffer()
		{
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>& OutBuffer = *NULL_BUFFER;
			if (EmptyBufferStack.Dequeue(OutBuffer)) { return &OutBuffer; }
			return new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();
		}

		void DepositDeferredSimplex(TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* Face)
		{
			Face->Clear();
			DeferredSimplexStack.Enqueue(Face);
		}

		TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* GetDeferredSimplex()
		{
			TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>& OutDeferredSimplex = *NULL_DEFERRED;
			if (DeferredSimplexStack.Dequeue(OutDeferredSimplex)) { return &OutDeferredSimplex; }
			return new TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>();
		}
	};

	/// Holds all the objects needed to create a convex hull.
	/// Helps keep the Convex hull class clean and could maybe recyle them.
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TObjectBuffer
	{
	public:
		static constexpr int32 CONNECTOR_TABLE_SIZE = 2017;

		float MaxDistance;

		TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*> InputVertices;
		TArray<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> ConvexSimplexs;

		TFVtx<DIMENSIONS, VECTOR_TYPE>* CurrentVertex = nullptr;
		TFVtx<DIMENSIONS, VECTOR_TYPE>* FurthestVertex = nullptr;

		TObjectManager<DIMENSIONS, VECTOR_TYPE>* ObjectManager = nullptr;

		TSimplexList<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* UnprocessedFaces = nullptr;
		TArray<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>*> AffectedFaceBuffer;
		TQueue<TFSimplex<DIMENSIONS, VECTOR_TYPE>*> TraverseStack;
		TSet<TFVtx<DIMENSIONS, VECTOR_TYPE>*> SingularVertices;

		TArray<TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>*> ConeFaceBuffer;

		TFSimplex<DIMENSIONS, VECTOR_TYPE>* UpdateBuffer[DIMENSIONS];
		int32 UpdateIndices[DIMENSIONS];

		ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* ConnectorTable[CONNECTOR_TABLE_SIZE];
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* EmptyBuffer = nullptr;
		TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* BeyondBuffer = nullptr;

		TObjectBuffer()
		{
			Reset();
		}

		void Reset()
		{
			MaxDistance = TNumericLimits<double>::Min();

			InputVertices.Empty();
			ConvexSimplexs.Empty();

			PCGEX_DELETE(UnprocessedFaces)
			UnprocessedFaces = new TSimplexList<TFVtx<DIMENSIONS, VECTOR_TYPE>>();
			AffectedFaceBuffer.Empty();
			TraverseStack.Empty();
			SingularVertices.Empty();

			ConeFaceBuffer.Empty();

			for (int i = 0; i < DIMENSIONS; i++)
			{
				UpdateBuffer[i] = nullptr;
				UpdateIndices[i] = -1;
			}

			PCGEX_DELETE(ObjectManager)
			ObjectManager = new TObjectManager<DIMENSIONS, VECTOR_TYPE>();

			PCGEX_DELETE(EmptyBuffer)
			EmptyBuffer = new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();

			PCGEX_DELETE(BeyondBuffer)
			BeyondBuffer = new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>();

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++)
			{
				ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* CList = ConnectorTable[i];
				PCGEX_DELETE(CList)
				ConnectorTable[i] = new ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>();
			}
		}

		void Clear()
		{
			Reset();
		}

		~TObjectBuffer()
		{
			InputVertices.Empty();
			ConvexSimplexs.Empty();

			PCGEX_DELETE(UnprocessedFaces)

			AffectedFaceBuffer.Empty();
			TraverseStack.Empty();
			SingularVertices.Empty();

			ConeFaceBuffer.Empty();

			for (int i = 0; i < DIMENSIONS; i++)
			{
				UpdateBuffer[i] = nullptr;
				UpdateIndices[i] = -1;
			}

			PCGEX_DELETE(ObjectManager)
			PCGEX_DELETE(EmptyBuffer)
			PCGEX_DELETE(BeyondBuffer)

			for (int i = 0; i < CONNECTOR_TABLE_SIZE; i++)
			{
				ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* CList = ConnectorTable[i];
				PCGEX_DELETE(CList)
			}
		}

		void InitInput(TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& Input, bool bAssignIds)
		{
			InputVertices.Empty(Input.Num());
			InputVertices.Append(Input);
			if (bAssignIds) { for (int i = 0; i < InputVertices.Num(); i++) { InputVertices[i]->Id = i; } }
		}
	};
}
