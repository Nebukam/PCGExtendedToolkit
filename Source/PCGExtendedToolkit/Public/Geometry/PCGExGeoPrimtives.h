// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExGeo
{
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class PCGEXTENDEDTOOLKIT_API TFVtx
	{
	public:
		int32 Id = 0;
		int32 Tag = 0;
		VECTOR_TYPE Position = VECTOR_TYPE{};
		static int Dimensions() { return DIMENSIONS; }

		TFVtx()
		{
		}

		TFVtx(int InId)
			: Id(InId)
		{
		}

		~TFVtx()
		{
		}

		double SqrMagnitude()
		{
			float sum = 0.0f;
			for (int i = 0; i < DIMENSIONS; i++) { sum += Position[i] * Position[i]; }
			return sum;
		}
	};

	template <int DIMENSIONS, typename VECTOR_TYPE>
	class PCGEXTENDEDTOOLKIT_API TFSimplex
	{
	public:
		VECTOR_TYPE Normal = VECTOR_TYPE{};
		VECTOR_TYPE Centroid = VECTOR_TYPE{};

		/// The vertices that make up the simplex.
		/// For 2D a face will be 2 vertices making a line.
		/// For 3D a face will be 3 vertices making a triangle.
		TFVtx<DIMENSIONS, VECTOR_TYPE>* Vertices[DIMENSIONS];

		/// The simplexs adjacent to this simplex
		/// For 2D a simplex will be a segment and it with have two adjacent segments joining it.
		/// For 3D a simplex will be a triangle and it with have three adjacent triangles joining it.
		TFSimplex* AdjacentFaces[DIMENSIONS];

		double Offset = 0;
		int32 Tag = 0;
		bool bIsNormalFlipped = false;

		TFSimplex()
		{
			for (int i = 0; i < DIMENSIONS; i++)
			{
				AdjacentFaces[i] = nullptr;
				Vertices[i] = nullptr;
			}
		}

		virtual ~TFSimplex()
		{
		}

		double Dot(TFVtx<DIMENSIONS, VECTOR_TYPE>* V)
		{
			if (!V) { return 0.0f; }

			double Dp = 0.0f;
			for (int i = 0; i < DIMENSIONS; i++) { Dp += Normal[i] * V.Position[i]; }
			return Dp;
		}

		bool Remove(TFSimplex* OtherSimplex)
		{
			if (!OtherSimplex) { return false; }
			for (int i = 0; i < DIMENSIONS; i++)
			{
				if (AdjacentFaces[i] == OtherSimplex)
				{
					AdjacentFaces[i] = nullptr;
					return true;
				}
			}
			return false;
		}

		void UpdateNormal() { CalculateNormal(Normal); }

		void CalculateNormal(VECTOR_TYPE& InNormal)
		{
			if (DIMENSIONS == 3)
			{
				const VECTOR_TYPE Sub = Vertices[0]->Position - Vertices[1]->Position;
				const double NX = -Sub[1];
				const double NY = Sub[0];

				const float f = 1.0f / FMath::Sqrt(NX * NX + NY * NY);
				InNormal[0] = f * NX;
				InNormal[1] = f * NY;
			}
			else if (DIMENSIONS == 2)
			{
				const VECTOR_TYPE x = Vertices[1]->Position - Vertices[0]->Position;
				const VECTOR_TYPE y = Vertices[2]->Position - Vertices[1]->Position;

				const double NX = x[1] * y[2] - x[2] * y[1];
				const double NY = x[2] * y[0] - x[0] * y[2];
				const double NZ = x[0] * y[1] - x[1] * y[0];

				float f = 1.0f / FMath::Sqrt(NX * NX + NY * NY + NZ * NZ);
				InNormal[0] = f * NX;
				InNormal[1] = f * NY;
				InNormal[2] = f * NZ;
			}
			else if (DIMENSIONS == 4)
			{
				VECTOR_TYPE X = Vertices[1]->Position - Vertices[0]->Position;
				VECTOR_TYPE Y = Vertices[2]->Position - Vertices[1]->Position;
				VECTOR_TYPE Z = Vertices[3]->Position - Vertices[2]->Position;

				// This was generated using Mathematica
				const double Nx = X[3] * (Y[2] * Z[1] - Y[1] * Z[2])
					+ X[2] * (Y[1] * Z[3] - Y[3] * Z[1])
					+ X[1] * (Y[3] * Z[2] - Y[2] * Z[3]);
				const double Ny = X[3] * (Y[0] * Z[2] - Y[2] * Z[0])
					+ X[2] * (Y[3] * Z[0] - Y[0] * Z[3])
					+ X[0] * (Y[2] * Z[3] - Y[3] * Z[2]);
				const double Nz = X[3] * (Y[1] * Z[0] - Y[0] * Z[1])
					+ X[1] * (Y[0] * Z[3] - Y[3] * Z[0])
					+ X[0] * (Y[3] * Z[1] - Y[1] * Z[3]);
				const double Nw = X[2] * (Y[0] * Z[1] - Y[1] * Z[0])
					+ X[1] * (Y[2] * Z[0] - Y[0] * Z[2])
					+ X[0] * (Y[1] * Z[2] - Y[2] * Z[1]);

				const float F = 1.0f / FMath::Sqrt(Nx * Nx + Ny * Ny + Nz * Nz + Nw * Nw);
				InNormal[0] = F * Nx;
				InNormal[1] = F * Ny;
				InNormal[2] = F * Nz;
				InNormal[3] = F * Nw;
			}
		}

		void UpdateCentroid()
		{
			Centroid = VECTOR_TYPE{};
			for (int i = 0; i < DIMENSIONS; i++) { Centroid += Vertices[i]->Position; }
			Centroid /= static_cast<double>(DIMENSIONS);
		};

		void UpdateAdjacency(TFSimplex* OtherSimplex)
		{
			int i;

			// reset marks on the 1st face & mark 2nd
			for (i = 0; i < DIMENSIONS; i++)
			{
				Vertices[i]->Tag = 0;
				OtherSimplex->Vertices[i]->Tag = 1;
			}

			// find the 1st false index
			for (i = 0; i < DIMENSIONS; i++) { if (Vertices[i]->Tag == 0) { break; } }

			// no vertex was marked
			if (i == DIMENSIONS) { return; }

			// check if only 1 vertex wasn't marked
			for (int j = i + 1; j < DIMENSIONS; j++) { if (Vertices[j]->Tag == 0) { return; } }

			// if we are here, the two faces share an edge
			AdjacentFaces[i] = OtherSimplex;

			// update the adj. face on the other face - find the vertex that remains marked
			for (i = 0; i < DIMENSIONS; i++) { Vertices[i]->Tag = 0; }
			for (i = 0; i < DIMENSIONS; i++) { if (OtherSimplex->Vertices[i]->Tag == 1) { break; } }
			
			OtherSimplex->AdjacentFaces[i] = this;
		}

		bool HasNullAdjacency() const
		{
			for (int i = 0; i < DIMENSIONS; i++) { if (!AdjacentFaces[i]) return true; }
			return false;
		}

		bool HasAdjacency() const
		{
			for (int i = 0; i < DIMENSIONS; i++) { if (AdjacentFaces[i]) return true; }
			return false;
		}

		/// <summary>
		/// Check if the vertex is "visible" from the face.
		/// The vertex is "over face" if the return value is > Constants.PlaneDistanceTolerance.
		/// </summary>
		/// <returns>The vertex is "over face" if the result is positive.</returns>
		double GetVertexDistance(TFVtx<DIMENSIONS, VECTOR_TYPE>* V)
		{
			float Distance = Offset;
			for (int i = 0; i < DIMENSIONS; i++) Distance += Normal[i] * V->Position[i];
			return Distance;
		}
	};
}
