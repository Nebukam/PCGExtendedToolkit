// Implementation

#include "Elements/Decomposition/PCGExSimpleConvexDecomposer.h"

namespace PCGExClusters
{
    bool FSimpleConvexDecomposer::Decompose(
        const FCluster* Cluster,
        FConvexDecomposition& OutResult,
        const FPCGExConvexDecompositionDetails& Settings)
    {
        OutResult.Clear();
        
        if (!Cluster || Cluster->Nodes->Num() < 4)
        {
            return false;
        }
        
        TArray<int32> AllNodes;
        AllNodes.Reserve(Cluster->Nodes->Num());
        
        for (int32 i = 0; i < Cluster->Nodes->Num(); i++)
        {
            if (Cluster->GetNode(i)->bValid)
            {
                AllNodes.Add(i);
            }
        }
        
        return DecomposeSubset(Cluster, AllNodes, OutResult, Settings);
    }

    bool FSimpleConvexDecomposer::DecomposeSubset(
        const FCluster* Cluster,
        const TArray<int32>& NodeIndices,
        FConvexDecomposition& OutResult,
        const FPCGExConvexDecompositionDetails& Settings)
    {
        OutResult.Clear();
        
        if (NodeIndices.Num() < Settings.MinNodesPerCell)
        {
            FConvexCell3D Cell;
            Cell.NodeIndices = NodeIndices;
            Cell.ComputeBounds(Cluster);
            OutResult.Cells.Add(MoveTemp(Cell));
            return true;
        }
        
        DecomposeRecursive(Cluster, NodeIndices, Settings, OutResult.Cells, 0);
        
        return OutResult.Cells.Num() > 0;
    }

    double FSimpleConvexDecomposer::ComputeConvexityRatio(
        const TArray<FVector>& Positions) const
    {
        if (Positions.Num() <= 4)
        {
            return 0.0; // 4 or fewer points are always convex hull
        }
        
        TArray<int32> HullIndices;
        ComputeConvexHull(Positions, HullIndices);
        
        if (HullIndices.Num() == 0)
        {
            return 1.0;
        }
        
        // Ratio of points NOT on hull
        int32 InteriorCount = Positions.Num() - HullIndices.Num();
        return static_cast<double>(InteriorCount) / Positions.Num();
    }

    bool FSimpleConvexDecomposer::FindSplitPlane(
        const TArray<FVector>& Positions,
        FVector& OutPlaneOrigin,
        FVector& OutPlaneNormal) const
    {
        if (Positions.Num() < 2)
        {
            return false;
        }
        
        // Compute centroid
        FVector Centroid = FVector::ZeroVector;
        for (const FVector& P : Positions)
        {
            Centroid += P;
        }
        Centroid /= Positions.Num();
        
        // Compute covariance matrix for PCA
        double Cov[3][3] = {{0}};
        
        for (const FVector& P : Positions)
        {
            FVector D = P - Centroid;
            Cov[0][0] += D.X * D.X;
            Cov[0][1] += D.X * D.Y;
            Cov[0][2] += D.X * D.Z;
            Cov[1][1] += D.Y * D.Y;
            Cov[1][2] += D.Y * D.Z;
            Cov[2][2] += D.Z * D.Z;
        }
        Cov[1][0] = Cov[0][1];
        Cov[2][0] = Cov[0][2];
        Cov[2][1] = Cov[1][2];
        
        // Power iteration to find principal eigenvector (largest spread direction)
        FVector Axis = FVector(1, 0, 0);
        
        for (int32 Iter = 0; Iter < 50; Iter++)
        {
            FVector NewAxis;
            NewAxis.X = Cov[0][0] * Axis.X + Cov[0][1] * Axis.Y + Cov[0][2] * Axis.Z;
            NewAxis.Y = Cov[1][0] * Axis.X + Cov[1][1] * Axis.Y + Cov[1][2] * Axis.Z;
            NewAxis.Z = Cov[2][0] * Axis.X + Cov[2][1] * Axis.Y + Cov[2][2] * Axis.Z;
            
            double Len = NewAxis.Size();
            if (Len > KINDA_SMALL_NUMBER)
            {
                Axis = NewAxis / Len;
            }
        }
        
        // Split perpendicular to the principal axis, through centroid
        OutPlaneOrigin = Centroid;
        OutPlaneNormal = Axis.GetSafeNormal();
        
        if (OutPlaneNormal.IsNearlyZero())
        {
            OutPlaneNormal = FVector::UpVector;
        }
        
        return true;
    }

    void FSimpleConvexDecomposer::DecomposeRecursive(
        const FCluster* Cluster,
        const TArray<int32>& NodeIndices,
        const FPCGExConvexDecompositionDetails& Settings,
        TArray<FConvexCell3D>& OutCells,
        int32 Depth)
    {
        // Gather positions
        TArray<FVector> Positions;
        Positions.SetNum(NodeIndices.Num());
        for (int32 i = 0; i < NodeIndices.Num(); i++)
        {
            Positions[i] = Cluster->GetPos(NodeIndices[i]);
        }
        
        // Check termination conditions
        bool bShouldTerminate = false;
        
        if (Depth >= Settings.MaxDepth)
        {
            bShouldTerminate = true;
        }
        else if (OutCells.Num() >= Settings.MaxCells)
        {
            bShouldTerminate = true;
        }
        else if (NodeIndices.Num() <= Settings.MinNodesPerCell)
        {
            bShouldTerminate = true;
        }
        else
        {
            double ConvexityRatio = ComputeConvexityRatio(Positions);
            if (ConvexityRatio <= Settings.MaxConcavityRatio)
            {
                bShouldTerminate = true;
            }
        }
        
        if (bShouldTerminate)
        {
            FConvexCell3D Cell;
            Cell.NodeIndices = NodeIndices;
            Cell.ComputeBounds(Cluster);
            OutCells.Add(MoveTemp(Cell));
            return;
        }
        
        // Find split plane
        FVector PlaneOrigin, PlaneNormal;
        if (!FindSplitPlane(Positions, PlaneOrigin, PlaneNormal))
        {
            FConvexCell3D Cell;
            Cell.NodeIndices = NodeIndices;
            Cell.ComputeBounds(Cluster);
            OutCells.Add(MoveTemp(Cell));
            return;
        }
        
        // Split nodes by plane
        TArray<int32> FrontNodes, BackNodes;
        
        for (int32 i = 0; i < NodeIndices.Num(); i++)
        {
            double Dist = FVector::DotProduct(Positions[i] - PlaneOrigin, PlaneNormal);
            
            if (Dist >= 0)
            {
                FrontNodes.Add(NodeIndices[i]);
            }
            else
            {
                BackNodes.Add(NodeIndices[i]);
            }
        }
        
        // Check if split is valid
        if (FrontNodes.Num() < Settings.MinNodesPerCell || 
            BackNodes.Num() < Settings.MinNodesPerCell)
        {
            // Try splitting along different axis
            // Rotate the plane normal
            FVector AltNormals[] = {
                FVector::CrossProduct(PlaneNormal, FVector::UpVector).GetSafeNormal(),
                FVector::CrossProduct(PlaneNormal, FVector::RightVector).GetSafeNormal(),
                FVector::CrossProduct(PlaneNormal, FVector::ForwardVector).GetSafeNormal()
            };
            
            bool bFoundValidSplit = false;
            
            for (const FVector& AltNormal : AltNormals)
            {
                if (AltNormal.IsNearlyZero()) continue;
                
                FrontNodes.Empty();
                BackNodes.Empty();
                
                for (int32 i = 0; i < NodeIndices.Num(); i++)
                {
                    double Dist = FVector::DotProduct(Positions[i] - PlaneOrigin, AltNormal);
                    
                    if (Dist >= 0)
                    {
                        FrontNodes.Add(NodeIndices[i]);
                    }
                    else
                    {
                        BackNodes.Add(NodeIndices[i]);
                    }
                }
                
                if (FrontNodes.Num() >= Settings.MinNodesPerCell && 
                    BackNodes.Num() >= Settings.MinNodesPerCell)
                {
                    bFoundValidSplit = true;
                    break;
                }
            }
            
            if (!bFoundValidSplit)
            {
                // Cannot split further
                FConvexCell3D Cell;
                Cell.NodeIndices = NodeIndices;
                Cell.ComputeBounds(Cluster);
                OutCells.Add(MoveTemp(Cell));
                return;
            }
        }
        
        // Recurse
        DecomposeRecursive(Cluster, FrontNodes, Settings, OutCells, Depth + 1);
        DecomposeRecursive(Cluster, BackNodes, Settings, OutCells, Depth + 1);
    }

    void FSimpleConvexDecomposer::ComputeConvexHull(
        const TArray<FVector>& Points,
        TArray<int32>& OutHullIndices) const
    {
        OutHullIndices.Empty();
        
        const int32 NumPoints = Points.Num();
        if (NumPoints < 4)
        {
            for (int32 i = 0; i < NumPoints; i++)
            {
                OutHullIndices.Add(i);
            }
            return;
        }
        
        // Find extreme points to form initial tetrahedron
        int32 MinX = 0, MaxX = 0;
        for (int32 i = 1; i < NumPoints; i++)
        {
            if (Points[i].X < Points[MinX].X) MinX = i;
            if (Points[i].X > Points[MaxX].X) MaxX = i;
        }
        
        if (MinX == MaxX)
        {
            // Degenerate case - all same X
            for (int32 i = 0; i < NumPoints; i++)
            {
                OutHullIndices.Add(i);
            }
            return;
        }
        
        // Find point furthest from line MinX-MaxX
        FVector LineDir = (Points[MaxX] - Points[MinX]).GetSafeNormal();
        double MaxLineDist = 0;
        int32 ThirdPoint = -1;
        
        for (int32 i = 0; i < NumPoints; i++)
        {
            if (i == MinX || i == MaxX) continue;
            
            FVector ToPoint = Points[i] - Points[MinX];
            FVector Projected = Points[MinX] + LineDir * FVector::DotProduct(ToPoint, LineDir);
            double DistSq = FVector::DistSquared(Points[i], Projected);
            
            if (DistSq > MaxLineDist)
            {
                MaxLineDist = DistSq;
                ThirdPoint = i;
            }
        }
        
        if (ThirdPoint < 0)
        {
            OutHullIndices.Add(MinX);
            OutHullIndices.Add(MaxX);
            return;
        }
        
        // Find point furthest from plane
        FVector PlaneNormal = FVector::CrossProduct(
            Points[MaxX] - Points[MinX],
            Points[ThirdPoint] - Points[MinX]).GetSafeNormal();
        
        double MaxPlaneDist = 0;
        int32 FourthPoint = -1;
        
        for (int32 i = 0; i < NumPoints; i++)
        {
            if (i == MinX || i == MaxX || i == ThirdPoint) continue;
            
            double Dist = FMath::Abs(FVector::DotProduct(Points[i] - Points[MinX], PlaneNormal));
            if (Dist > MaxPlaneDist)
            {
                MaxPlaneDist = Dist;
                FourthPoint = i;
            }
        }
        
        if (FourthPoint < 0 || MaxPlaneDist < KINDA_SMALL_NUMBER)
        {
            // Coplanar - return triangle
            OutHullIndices.Add(MinX);
            OutHullIndices.Add(MaxX);
            OutHullIndices.Add(ThirdPoint);
            return;
        }
        
        // We have initial tetrahedron - now incrementally add points
        TSet<int32> HullSet;
        HullSet.Add(MinX);
        HullSet.Add(MaxX);
        HullSet.Add(ThirdPoint);
        HullSet.Add(FourthPoint);
        
        // Simple approach: for each remaining point, check if it's outside current hull
        // If so, it must be on the hull
        
        struct FFace
        {
            int32 A, B, C;
            FVector Normal;
            double D;
            
            void ComputePlane(const TArray<FVector>& Pts)
            {
                Normal = FVector::CrossProduct(Pts[B] - Pts[A], Pts[C] - Pts[A]).GetSafeNormal();
                D = -FVector::DotProduct(Normal, Pts[A]);
            }
            
            double SignedDist(const FVector& P) const
            {
                return FVector::DotProduct(Normal, P) + D;
            }
        };
        
        // Build initial faces
        TArray<FFace> Faces;
        
        auto AddFace = [&](int32 A, int32 B, int32 C)
        {
            FFace F;
            F.A = A; F.B = B; F.C = C;
            F.ComputePlane(Points);
            
            // Orient outward (centroid should be behind all faces)
            FVector Centroid = (Points[MinX] + Points[MaxX] + Points[ThirdPoint] + Points[FourthPoint]) / 4.0;
            if (F.SignedDist(Centroid) > 0)
            {
                F.Normal = -F.Normal;
                F.D = -F.D;
                Swap(F.B, F.C);
            }
            
            Faces.Add(F);
        };
        
        AddFace(MinX, MaxX, ThirdPoint);
        AddFace(MinX, ThirdPoint, FourthPoint);
        AddFace(MinX, FourthPoint, MaxX);
        AddFace(MaxX, FourthPoint, ThirdPoint);
        
        // Check remaining points
        for (int32 i = 0; i < NumPoints; i++)
        {
            if (HullSet.Contains(i)) continue;
            
            // Check if point is outside hull
            bool bOutside = false;
            for (const FFace& Face : Faces)
            {
                if (Face.SignedDist(Points[i]) > KINDA_SMALL_NUMBER)
                {
                    bOutside = true;
                    break;
                }
            }
            
            if (bOutside)
            {
                HullSet.Add(i);
            }
        }
        
        OutHullIndices = HullSet.Array();
    }
}