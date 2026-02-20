// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Branching mesh actor with Space Colonization algorithm and Catmull-Rom spline sweep

#include "BranchingMeshActor.h"

ABranchingMeshActor::ABranchingMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void ABranchingMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PreCacheCrossSection();
	GenerateMesh();
}

void ABranchingMeshActor::PostLoad()
{
	Super::PostLoad();
	PreCacheCrossSection();
	GenerateMesh();
}

void ABranchingMeshActor::PreCacheCrossSection()
{
	if (LastCachedCrossSectionCount == RadialSegmentCount)
	{
		return;
	}

	const float AngleBetweenQuads = (2.0f / static_cast<float>(RadialSegmentCount)) * PI;
	CachedCrossSectionPoints.Empty();

	for (int32 PointIndex = 0; PointIndex < (RadialSegmentCount + 2); PointIndex++)
	{
		const float Angle = static_cast<float>(PointIndex) * AngleBetweenQuads;
		CachedCrossSectionPoints.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0));
	}

	LastCachedCrossSectionCount = RadialSegmentCount;
}

// --- Space Colonization Algorithm ---

void ABranchingMeshActor::GenerateAttractors(TArray<FVector>& OutAttractors)
{
	OutAttractors.Empty();
	OutAttractors.Reserve(AttractorCount);

	const FVector CrownCenter = End;
	const float R = FMath::Max(CrownRadius, 1.0f);

	for (int32 i = 0; i < AttractorCount; ++i)
	{
		FVector Point;
		bool bValid = false;

		// Rejection sampling — try up to 100 times per attractor
		for (int32 Attempt = 0; Attempt < 100 && !bValid; ++Attempt)
		{
			// Random point in [-1,1] cube
			const float X = RngStream.FRandRange(-1.0f, 1.0f);
			const float Y = RngStream.FRandRange(-1.0f, 1.0f);
			const float Z = RngStream.FRandRange(-1.0f, 1.0f);

			switch (CrownShape)
			{
			case ECrownShape::Sphere:
				if (X * X + Y * Y + Z * Z <= 1.0f)
				{
					Point = CrownCenter + FVector(X, Y, Z) * R;
					bValid = true;
				}
				break;

			case ECrownShape::Hemisphere:
				if (X * X + Y * Y + Z * Z <= 1.0f && Z >= 0.0f)
				{
					Point = CrownCenter + FVector(X, Y, Z) * R;
					bValid = true;
				}
				break;

			case ECrownShape::Cone:
				// Cone with apex at CrownCenter, expanding downward (negative Z)
				// At height Z (0 to -1), radius = |Z|
				if (Z <= 0.0f)
				{
					const float AllowedR = -Z; // grows from 0 at apex to 1 at base
					if (X * X + Y * Y <= AllowedR * AllowedR)
					{
						Point = CrownCenter + FVector(X, Y, Z) * R;
						bValid = true;
					}
				}
				break;

			case ECrownShape::Cylinder:
				if (X * X + Y * Y <= 1.0f)
				{
					Point = CrownCenter + FVector(X, Y, Z) * R;
					bValid = true;
				}
				break;
			}
		}

		if (bValid)
		{
			OutAttractors.Add(Point);
		}
	}
}

void ABranchingMeshActor::BuildTreeSpaceColonization(TArray<FBranchNode>& OutNodes)
{
	OutNodes.Empty();

	// Generate attractor points in the crown volume
	TArray<FVector> Attractors;
	GenerateAttractors(Attractors);

	if (Attractors.Num() == 0)
	{
		return;
	}

	// Create root node at Start
	auto AddNode = [&OutNodes](const FVector& Pos, int32 Parent) -> int32
	{
		const int32 Idx = OutNodes.Num();
		FBranchNode Node;
		Node.Position = Pos;
		Node.Width = 0.0f;
		Node.ParentIndex = Parent;
		Node.bIsFork = false;
		Node.bIsLeaf = false;
		Node.bIsRoot = (Parent == INDEX_NONE);
		OutNodes.Add(MoveTemp(Node));
		if (Parent != INDEX_NONE)
		{
			OutNodes[Parent].ChildIndices.Add(Idx);
		}
		return Idx;
	};

	// Root node
	AddNode(Start, INDEX_NONE);

	// Grow trunk from Start toward End (crown center) until we're within InfluenceRadius of an attractor
	const float StepLen = FMath::Max(GrowthStepLength, 0.1f);
	const float InfluenceRadSq = InfluenceRadius * InfluenceRadius;
	const float KillDistSq = KillDistance * KillDistance;

	{
		FVector TrunkDir = (End - Start).GetSafeNormal();
		int32 CurrentIdx = 0;
		const float TrunkDist = FVector::Dist(Start, End);
		const int32 MaxTrunkSteps = FMath::CeilToInt(TrunkDist / StepLen) + 1;

		for (int32 Step = 0; Step < MaxTrunkSteps; ++Step)
		{
			const FVector& CurrentPos = OutNodes[CurrentIdx].Position;

			// Check if any attractor is within influence radius
			bool bHasInfluence = false;
			for (const FVector& Attr : Attractors)
			{
				if (FVector::DistSquared(CurrentPos, Attr) <= InfluenceRadSq)
				{
					bHasInfluence = true;
					break;
				}
			}

			if (bHasInfluence)
			{
				break;
			}

			// Step toward crown center
			const FVector NewPos = CurrentPos + TrunkDir * StepLen;
			CurrentIdx = AddNode(NewPos, CurrentIdx);
		}
	}

	// Main space colonization loop
	const int32 MaxIter = FMath::Max(MaxGrowthIterations, 1);

	for (int32 Iter = 0; Iter < MaxIter; ++Iter)
	{
		// For each attractor, find the closest tree node within InfluenceRadius
		// Map: NodeIndex -> list of attractor directions
		TMap<int32, FVector> NodeGrowthDirs;
		TMap<int32, int32> NodeAttractorCounts;

		for (const FVector& Attr : Attractors)
		{
			int32 ClosestNode = INDEX_NONE;
			float ClosestDistSq = InfluenceRadSq;

			for (int32 NodeIdx = 0; NodeIdx < OutNodes.Num(); ++NodeIdx)
			{
				const float DistSq = FVector::DistSquared(OutNodes[NodeIdx].Position, Attr);
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestNode = NodeIdx;
				}
			}

			if (ClosestNode != INDEX_NONE)
			{
				const FVector Dir = (Attr - OutNodes[ClosestNode].Position).GetSafeNormal();
				if (FVector* Existing = NodeGrowthDirs.Find(ClosestNode))
				{
					*Existing += Dir;
					NodeAttractorCounts[ClosestNode]++;
				}
				else
				{
					NodeGrowthDirs.Add(ClosestNode, Dir);
					NodeAttractorCounts.Add(ClosestNode, 1);
				}
			}
		}

		if (NodeGrowthDirs.Num() == 0)
		{
			break; // No attractors influencing any node
		}

		// Create new nodes
		TArray<int32> NewNodeIndices;
		for (auto& Pair : NodeGrowthDirs)
		{
			const int32 ParentIdx = Pair.Key;
			FVector AvgDir = Pair.Value.GetSafeNormal();

			// Add small random jitter for organic feel
			AvgDir += FVector(
				RngStream.FRandRange(-0.1f, 0.1f),
				RngStream.FRandRange(-0.1f, 0.1f),
				RngStream.FRandRange(-0.1f, 0.1f));
			AvgDir = AvgDir.GetSafeNormal();

			const FVector NewPos = OutNodes[ParentIdx].Position + AvgDir * StepLen;

			// Check if a node already exists very close to this position (avoid duplication)
			bool bTooClose = false;
			for (int32 ChildIdx : OutNodes[ParentIdx].ChildIndices)
			{
				if (FVector::DistSquared(OutNodes[ChildIdx].Position, NewPos) < StepLen * StepLen * 0.01f)
				{
					bTooClose = true;
					break;
				}
			}

			if (!bTooClose)
			{
				const int32 NewIdx = AddNode(NewPos, ParentIdx);
				NewNodeIndices.Add(NewIdx);
			}
		}

		if (NewNodeIndices.Num() == 0)
		{
			break; // No new growth
		}

		// Remove attractors within KillDistance of any tree node
		Attractors.RemoveAll([&](const FVector& Attr)
		{
			for (int32 NodeIdx = 0; NodeIdx < OutNodes.Num(); ++NodeIdx)
			{
				if (FVector::DistSquared(OutNodes[NodeIdx].Position, Attr) <= KillDistSq)
				{
					return true;
				}
			}
			return false;
		});

		if (Attractors.Num() == 0)
		{
			break; // All attractors consumed
		}
	}

	// Classify nodes
	for (FBranchNode& Node : OutNodes)
	{
		Node.bIsRoot = (Node.ParentIndex == INDEX_NONE);
		Node.bIsFork = (Node.ChildIndices.Num() > 1);
		Node.bIsLeaf = (Node.ChildIndices.Num() == 0);
	}

	// Compute widths bottom-up using pipe model (Da Vinci rule)
	// Process nodes in reverse order (leaves first, since children always have higher indices)
	const float Exp = FMath::Max(PipeModelExponent, 1.0f);
	const float InvExp = 1.0f / Exp;

	for (int32 i = OutNodes.Num() - 1; i >= 0; --i)
	{
		FBranchNode& Node = OutNodes[i];

		if (Node.bIsLeaf)
		{
			Node.Width = TipWidth;
		}
		else
		{
			float SumPow = 0.0f;
			for (int32 ChildIdx : Node.ChildIndices)
			{
				SumPow += FMath::Pow(OutNodes[ChildIdx].Width, Exp);
			}
			Node.Width = FMath::Pow(SumPow, InvExp);
		}
	}

	// Enforce TrunkWidth along the trunk (root to first fork), tapering smoothly into pipe model widths
	if (OutNodes.Num() > 0)
	{
		// Walk from root down through single-child nodes (the trunk)
		int32 TrunkEnd = 0;
		{
			int32 Idx = 0;
			while (!OutNodes[Idx].bIsFork && !OutNodes[Idx].bIsLeaf && OutNodes[Idx].ChildIndices.Num() == 1)
			{
				Idx = OutNodes[Idx].ChildIndices[0];
			}
			TrunkEnd = Idx;
		}

		// Set trunk nodes to TrunkWidth, then blend into the pipe model width at the first fork
		const float ForkPipeWidth = OutNodes[TrunkEnd].Width;
		int32 Idx = 0;
		int32 StepsFromRoot = 0;
		int32 TrunkSteps = 0;

		// Count trunk steps first
		{
			int32 Cnt = 0;
			while (Cnt != TrunkEnd)
			{
				Cnt = OutNodes[Cnt].ChildIndices[0];
				TrunkSteps++;
			}
		}

		// Apply widths: full TrunkWidth for most of the trunk, blend over the last ~25%
		Idx = 0;
		StepsFromRoot = 0;
		while (true)
		{
			const float BlendStart = FMath::Max(TrunkSteps * 0.75f, TrunkSteps - 5.0f);
			if (StepsFromRoot <= BlendStart)
			{
				OutNodes[Idx].Width = FMath::Max(OutNodes[Idx].Width, TrunkWidth);
			}
			else
			{
				const float T = (static_cast<float>(StepsFromRoot) - BlendStart) / FMath::Max(static_cast<float>(TrunkSteps) - BlendStart, 1.0f);
				const float BlendedWidth = FMath::Lerp(TrunkWidth, ForkPipeWidth, T);
				OutNodes[Idx].Width = FMath::Max(OutNodes[Idx].Width, BlendedWidth);
			}

			if (Idx == TrunkEnd)
			{
				break;
			}
			Idx = OutNodes[Idx].ChildIndices[0];
			StepsFromRoot++;
		}
	}
}

// --- Catmull-Rom spline helpers ---

float ABranchingMeshActor::CatmullRomKnot(float Ti, const FVector& Pi, const FVector& Pj, float Alpha)
{
	const float Dist = FVector::Dist(Pi, Pj);
	return Ti + FMath::Pow(FMath::Max(Dist, KINDA_SMALL_NUMBER), Alpha);
}

FVector ABranchingMeshActor::EvalCatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float T, float Alpha)
{
	// Centripetal Catmull-Rom using Barry and Goldman's formulation
	const float T0 = 0.f;
	const float T1 = CatmullRomKnot(T0, P0, P1, Alpha);
	const float T2 = CatmullRomKnot(T1, P1, P2, Alpha);
	const float T3 = CatmullRomKnot(T2, P2, P3, Alpha);

	// Map input T from [0,1] to [T1,T2]
	const float Kt = FMath::Lerp(T1, T2, T);

	auto SafeDiv = [](float Num, float Den) -> float
	{
		return FMath::Abs(Den) > KINDA_SMALL_NUMBER ? Num / Den : 0.f;
	};

	const FVector A1 = P0 * SafeDiv(T1 - Kt, T1 - T0) + P1 * SafeDiv(Kt - T0, T1 - T0);
	const FVector A2 = P1 * SafeDiv(T2 - Kt, T2 - T1) + P2 * SafeDiv(Kt - T1, T2 - T1);
	const FVector A3 = P2 * SafeDiv(T3 - Kt, T3 - T2) + P3 * SafeDiv(Kt - T2, T3 - T2);

	const FVector B1 = A1 * SafeDiv(T2 - Kt, T2 - T0) + A2 * SafeDiv(Kt - T0, T2 - T0);
	const FVector B2 = A2 * SafeDiv(T3 - Kt, T3 - T1) + A3 * SafeDiv(Kt - T1, T3 - T1);

	return B1 * SafeDiv(T2 - Kt, T2 - T1) + B2 * SafeDiv(Kt - T1, T2 - T1);
}

// --- Extract branch paths between structural points ---

void ABranchingMeshActor::ExtractBranchPaths(const TArray<FBranchNode>& Nodes, TArray<FBranchPath>& OutPaths)
{
	OutPaths.Empty();

	if (Nodes.Num() == 0)
	{
		return;
	}

	// A path starts at a root or fork node and continues until it hits another fork or a leaf
	for (int32 StartNodeIdx = 0; StartNodeIdx < Nodes.Num(); ++StartNodeIdx)
	{
		const FBranchNode& StartNode = Nodes[StartNodeIdx];

		// Only start paths at root or fork nodes
		if (!StartNode.bIsRoot && !StartNode.bIsFork)
		{
			continue;
		}

		// Start a path along each child
		for (int32 ChildIdx : StartNode.ChildIndices)
		{
			FBranchPath Path;
			Path.NodeIndices.Add(StartNodeIdx);
			Path.TotalLength = 0.f;

			int32 Current = ChildIdx;
			while (Current != INDEX_NONE)
			{
				Path.NodeIndices.Add(Current);
				const FBranchNode& CurrNode = Nodes[Current];

				if (CurrNode.bIsFork || CurrNode.bIsLeaf)
				{
					break; // End the path at a fork or leaf
				}

				// Continue to the single child
				Current = (CurrNode.ChildIndices.Num() == 1) ? CurrNode.ChildIndices[0] : INDEX_NONE;
			}

			if (Path.NodeIndices.Num() >= 2)
			{
				OutPaths.Add(MoveTemp(Path));
			}
		}
	}
}

// --- Evaluate Catmull-Rom splines along each path ---

void ABranchingMeshActor::EvaluateSplines(TArray<FBranchPath>& Paths, const TArray<FBranchNode>& Nodes)
{
	const int32 Subdivs = FMath::Clamp(SplineSubdivisions, 1, 32);

	for (FBranchPath& Path : Paths)
	{
		Path.SplinePoints.Empty();
		Path.SplineWidths.Empty();
		Path.SplineDistances.Empty();
		Path.TotalLength = 0.f;

		const int32 NumNodes = Path.NodeIndices.Num();
		if (NumNodes < 2)
		{
			continue;
		}

		// Gather control positions and widths
		TArray<FVector> CtrlPts;
		TArray<float> CtrlWidths;
		CtrlPts.Reserve(NumNodes);
		CtrlWidths.Reserve(NumNodes);

		for (int32 NodeIdx : Path.NodeIndices)
		{
			CtrlPts.Add(Nodes[NodeIdx].Position);
			CtrlWidths.Add(Nodes[NodeIdx].Width);
		}

		const int32 NumSegments = CtrlPts.Num() - 1;

		// Add first point
		Path.SplinePoints.Add(CtrlPts[0]);
		Path.SplineWidths.Add(CtrlWidths[0]);
		Path.SplineDistances.Add(0.f);

		for (int32 Seg = 0; Seg < NumSegments; ++Seg)
		{
			// Get 4 control points (clamp at boundaries by extrapolation)
			const FVector& P1 = CtrlPts[Seg];
			const FVector& P2 = CtrlPts[Seg + 1];
			const FVector P0 = (Seg > 0) ? CtrlPts[Seg - 1] : (P1 + (P1 - P2)); // reflect
			const FVector P3 = (Seg + 2 < CtrlPts.Num()) ? CtrlPts[Seg + 2] : (P2 + (P2 - P1)); // reflect

			const float W1 = CtrlWidths[Seg];
			const float W2 = CtrlWidths[Seg + 1];

			for (int32 Step = 1; Step <= Subdivs; ++Step)
			{
				const float T = static_cast<float>(Step) / static_cast<float>(Subdivs);

				const FVector Pt = EvalCatmullRom(P0, P1, P2, P3, T);
				const float W = FMath::Lerp(W1, W2, T);

				const float Dist = FVector::Dist(Path.SplinePoints.Last(), Pt);
				Path.TotalLength += Dist;

				Path.SplinePoints.Add(Pt);
				Path.SplineWidths.Add(W);
				Path.SplineDistances.Add(Path.TotalLength);
			}
		}
	}
}

// --- Trim spline paths at fork nodes so transitions can bridge the gap ---

void ABranchingMeshActor::TrimPathsAtForks(TArray<FBranchPath>& Paths, const TArray<FBranchNode>& Nodes,
	TMap<int32, FForkTrimInfo>& OutParentTrims, TMap<int32, FForkTrimInfo>& OutChildTrims)
{
	const float HalfTransition = FMath::Max(ForkTransitionLength, 0.1f) * 0.5f;

	OutParentTrims.Empty();
	OutChildTrims.Empty();

	for (FBranchPath& Path : Paths)
	{
		if (Path.SplinePoints.Num() < 3)
		{
			continue;
		}

		const int32 LastNodeIdx = Path.NodeIndices.Last();
		const int32 FirstNodeIdx = Path.NodeIndices[0];
		const bool bTrimEnd = Nodes[LastNodeIdx].bIsFork;
		const bool bTrimStart = Nodes[FirstNodeIdx].bIsFork;

		// Don't trim if path is too short to remain valid after trimming
		const float NeededLength = ((bTrimStart ? 1.f : 0.f) + (bTrimEnd ? 1.f : 0.f)) * HalfTransition * 2.f;
		if (Path.TotalLength <= NeededLength)
		{
			continue;
		}

		// --- Trim end (path ends at a fork) ---
		if (bTrimEnd)
		{
			const float TrimDist = Path.TotalLength - HalfTransition;

			// Find last point to keep (the one at or before TrimDist)
			int32 KeepIdx = Path.SplinePoints.Num() - 1;
			while (KeepIdx > 0 && Path.SplineDistances[KeepIdx] > TrimDist)
			{
				KeepIdx--;
			}

			// Interpolate a new endpoint at exactly TrimDist
			if (KeepIdx < Path.SplinePoints.Num() - 1)
			{
				const float D0 = Path.SplineDistances[KeepIdx];
				const float D1 = Path.SplineDistances[KeepIdx + 1];
				const float T = (D1 > D0) ? (TrimDist - D0) / (D1 - D0) : 0.f;

				const FVector NewPt = FMath::Lerp(Path.SplinePoints[KeepIdx], Path.SplinePoints[KeepIdx + 1], T);
				const float NewWidth = FMath::Lerp(Path.SplineWidths[KeepIdx], Path.SplineWidths[KeepIdx + 1], T);

				Path.SplinePoints.SetNum(KeepIdx + 2);
				Path.SplineWidths.SetNum(KeepIdx + 2);
				Path.SplineDistances.SetNum(KeepIdx + 2);

				Path.SplinePoints[KeepIdx + 1] = NewPt;
				Path.SplineWidths[KeepIdx + 1] = NewWidth;
				Path.SplineDistances[KeepIdx + 1] = TrimDist;
				Path.TotalLength = TrimDist;
			}

			// Record the actual trim position/width/direction for this fork's parent side
			const int32 NumPts = Path.SplinePoints.Num();
			if (NumPts >= 2)
			{
				FForkTrimInfo Info;
				Info.Position = Path.SplinePoints.Last();
				Info.Width = Path.SplineWidths.Last();
				Info.Direction = (Path.SplinePoints[NumPts - 1] - Path.SplinePoints[NumPts - 2]).GetSafeNormal();
				OutParentTrims.Add(LastNodeIdx, Info);
			}
		}

		// --- Trim start (path starts at a fork) ---
		if (bTrimStart)
		{
			// Find first point at or past HalfTransition
			int32 FirstKeep = 0;
			while (FirstKeep < Path.SplinePoints.Num() - 1 && Path.SplineDistances[FirstKeep] < HalfTransition)
			{
				FirstKeep++;
			}

			if (FirstKeep > 0)
			{
				// Interpolate a new start point at exactly HalfTransition
				const float D0 = Path.SplineDistances[FirstKeep - 1];
				const float D1 = Path.SplineDistances[FirstKeep];
				const float T = (D1 > D0) ? (HalfTransition - D0) / (D1 - D0) : 0.f;

				const FVector NewPt = FMath::Lerp(Path.SplinePoints[FirstKeep - 1], Path.SplinePoints[FirstKeep], T);
				const float NewWidth = FMath::Lerp(Path.SplineWidths[FirstKeep - 1], Path.SplineWidths[FirstKeep], T);

				// Replace the point just before FirstKeep with the interpolated point
				const int32 InsertIdx = FirstKeep - 1;
				Path.SplinePoints[InsertIdx] = NewPt;
				Path.SplineWidths[InsertIdx] = NewWidth;
				Path.SplineDistances[InsertIdx] = HalfTransition;

				// Remove everything before InsertIdx
				if (InsertIdx > 0)
				{
					Path.SplinePoints.RemoveAt(0, InsertIdx);
					Path.SplineWidths.RemoveAt(0, InsertIdx);
					Path.SplineDistances.RemoveAt(0, InsertIdx);
				}
			}

			// Record the actual trim position/width/direction for this child side
			if (Path.SplinePoints.Num() >= 2 && Path.NodeIndices.Num() >= 2)
			{
				const int32 ChildNodeIdx = Path.NodeIndices[1];
				FForkTrimInfo Info;
				Info.Position = Path.SplinePoints[0];
				Info.Width = Path.SplineWidths[0];
				Info.Direction = (Path.SplinePoints[1] - Path.SplinePoints[0]).GetSafeNormal();
				OutChildTrims.Add(ChildNodeIdx, Info);
			}
		}
	}
}

// --- Generate tube mesh by sweeping rings along spline paths ---

void ABranchingMeshActor::GenerateTubeMesh(const TArray<FBranchPath>& Paths, int32& VertIdx, int32& TriIdx)
{
	const int32 VertsPerRing = RadialSegmentCount + 1;
	const float UStep = 1.f / static_cast<float>(RadialSegmentCount);

	auto MakeQuat = [](const FVector& Dir) -> FQuat
	{
		return FQuat::FindBetweenNormals(FVector::UpVector, Dir);
	};

	for (const FBranchPath& Path : Paths)
	{
		const int32 NumPts = Path.SplinePoints.Num();
		if (NumPts < 2)
		{
			continue;
		}

		const int32 TubeBaseVert = VertIdx;

		for (int32 RingIdx = 0; RingIdx < NumPts; ++RingIdx)
		{
			// Compute ring direction
			FVector Dir;
			if (RingIdx == 0)
			{
				Dir = (Path.SplinePoints[1] - Path.SplinePoints[0]).GetSafeNormal();
			}
			else if (RingIdx == NumPts - 1)
			{
				Dir = (Path.SplinePoints[NumPts - 1] - Path.SplinePoints[NumPts - 2]).GetSafeNormal();
			}
			else
			{
				Dir = (Path.SplinePoints[RingIdx + 1] - Path.SplinePoints[RingIdx - 1]).GetSafeNormal();
			}

			if (Dir.IsNearlyZero())
			{
				Dir = FVector::UpVector;
			}

			const FQuat Orientation = MakeQuat(Dir);
			const float Width = Path.SplineWidths[RingIdx];
			const float VCoord = Path.SplineDistances[RingIdx]; // World-space V for consistent texture scale

			for (int32 j = 0; j <= RadialSegmentCount; ++j)
			{
				const int32 VI = VertIdx++;
				const FVector LocalPos = CachedCrossSectionPoints[j] * Width;
				const FVector WorldOffset = Orientation.RotateVector(LocalPos);

				Positions[VI] = Path.SplinePoints[RingIdx] + WorldOffset;
				Normals[VI] = WorldOffset.GetSafeNormal();
				Tangents[VI] = FProcMeshTangent(Dir, false);
				TexCoords[VI] = FVector2D(1.f - static_cast<float>(j) * UStep, VCoord);
			}
		}

		// Stitch adjacent rings with quad strips
		for (int32 RingIdx = 0; RingIdx < NumPts - 1; ++RingIdx)
		{
			const int32 Base1 = TubeBaseVert + RingIdx * VertsPerRing;
			const int32 Base2 = TubeBaseVert + (RingIdx + 1) * VertsPerRing;

			for (int32 j = 0; j < RadialSegmentCount; ++j)
			{
				const int32 V0 = Base1 + j;
				const int32 V1 = Base1 + j + 1;
				const int32 V2 = Base2 + j + 1;
				const int32 V3 = Base2 + j;

				Triangles[TriIdx++] = V3;
				Triangles[TriIdx++] = V2;
				Triangles[TriIdx++] = V0;

				Triangles[TriIdx++] = V2;
				Triangles[TriIdx++] = V1;
				Triangles[TriIdx++] = V0;
			}
		}
	}
}

// --- Generate smooth fork transition geometry ---

void ABranchingMeshActor::GenerateForkTransitions(const TArray<FBranchNode>& Nodes, const TArray<FBranchPath>& Paths,
	const TMap<int32, FForkTrimInfo>& ParentTrims, const TMap<int32, FForkTrimInfo>& ChildTrims,
	int32& VertIdx, int32& TriIdx)
{
	const int32 VertsPerRing = RadialSegmentCount + 1;
	const float UStep = 1.f / static_cast<float>(RadialSegmentCount);
	const int32 NumTransitionRings = FMath::Clamp(ForkTransitionRings, 2, 16);
	const float TransitionLen = FMath::Max(ForkTransitionLength, 0.1f);
	const float HalfTransition = TransitionLen * 0.5f;

	auto MakeQuat = [](const FVector& Dir) -> FQuat
	{
		return FQuat::FindBetweenNormals(FVector::UpVector, Dir);
	};

	// Find fork nodes
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
	{
		const FBranchNode& ForkNode = Nodes[NodeIdx];
		if (!ForkNode.bIsFork)
		{
			continue;
		}

		// Determine fallback parent direction from the node graph
		FVector FallbackParentDir = FVector::UpVector;
		if (ForkNode.ParentIndex != INDEX_NONE)
		{
			FallbackParentDir = (ForkNode.Position - Nodes[ForkNode.ParentIndex].Position).GetSafeNormal();
		}

		// Use actual parent trim data if available, otherwise fall back to straight-line estimate
		const FForkTrimInfo* ParentTrim = ParentTrims.Find(NodeIdx);
		const FVector StartPos = ParentTrim ? ParentTrim->Position : (ForkNode.Position - FallbackParentDir * HalfTransition);
		const float StartWidth = ParentTrim ? ParentTrim->Width : ForkNode.Width;
		const FVector StartDir = ParentTrim ? ParentTrim->Direction : FallbackParentDir;

		// For each child, generate a transition tube bridging parent trim to child trim
		for (int32 ChildIdx : ForkNode.ChildIndices)
		{
			const FBranchNode& ChildNode = Nodes[ChildIdx];
			const FVector FallbackChildDir = (ChildNode.Position - ForkNode.Position).GetSafeNormal();

			// Check fork angle — skip transition for near-reversal angles (>150 degrees)
			const float CosAngle = FVector::DotProduct(StartDir, FallbackChildDir);
			if (CosAngle < -0.866f)
			{
				continue;
			}

			// Use actual child trim data if available
			const FForkTrimInfo* ChildTrim = ChildTrims.Find(ChildIdx);
			const FVector EndPos = ChildTrim ? ChildTrim->Position : (ForkNode.Position + FallbackChildDir * HalfTransition);
			const float EndWidth = ChildTrim ? ChildTrim->Width : ChildNode.Width;
			const FVector EndDir = ChildTrim ? ChildTrim->Direction : FallbackChildDir;

			const FQuat StartQ = MakeQuat(StartDir);
			const FQuat EndQ = MakeQuat(EndDir);

			// Compute split offset: push transition center away from the fork axis
			FVector SplitOffset = FVector::ZeroVector;
			if (ForkNode.ChildIndices.Num() == 2)
			{
				const FVector OtherChildDir = (Nodes[ForkNode.ChildIndices[0] == ChildIdx ? ForkNode.ChildIndices[1] : ForkNode.ChildIndices[0]].Position - ForkNode.Position).GetSafeNormal();
				SplitOffset = (FallbackChildDir - OtherChildDir).GetSafeNormal() * EndWidth * 0.3f;
			}
			else if (ForkNode.ChildIndices.Num() > 2)
			{
				SplitOffset = (FallbackChildDir - StartDir).GetSafeNormal() * EndWidth * 0.3f;
			}

			const int32 TubeBaseVert = VertIdx;

			for (int32 RingIdx = 0; RingIdx <= NumTransitionRings; ++RingIdx)
			{
				const float T = static_cast<float>(RingIdx) / static_cast<float>(NumTransitionRings);

				const float SplitBlend = FMath::Sin(T * PI);
				const FVector RingCenter = FMath::Lerp(StartPos, EndPos, T)
					+ SplitOffset * SplitBlend;

				const float RingWidth = FMath::Lerp(StartWidth, EndWidth, T);

				const FQuat RingQ = FQuat::Slerp(StartQ, EndQ, T);
				const FVector RingDir = FMath::Lerp(StartDir, EndDir, T).GetSafeNormal();

				const float VCoord = T * TransitionLen;

				for (int32 j = 0; j <= RadialSegmentCount; ++j)
				{
					const int32 VI = VertIdx++;
					const FVector LocalPos = CachedCrossSectionPoints[j] * RingWidth;
					const FVector WorldOffset = RingQ.RotateVector(LocalPos);

					Positions[VI] = RingCenter + WorldOffset;
					Normals[VI] = WorldOffset.GetSafeNormal();
					Tangents[VI] = FProcMeshTangent(RingDir, false);
					TexCoords[VI] = FVector2D(1.f - static_cast<float>(j) * UStep, VCoord);
				}
			}

			// Stitch rings
			for (int32 RingIdx = 0; RingIdx < NumTransitionRings; ++RingIdx)
			{
				const int32 Base1 = TubeBaseVert + RingIdx * VertsPerRing;
				const int32 Base2 = TubeBaseVert + (RingIdx + 1) * VertsPerRing;

				for (int32 j = 0; j < RadialSegmentCount; ++j)
				{
					const int32 V0 = Base1 + j;
					const int32 V1 = Base1 + j + 1;
					const int32 V2 = Base2 + j + 1;
					const int32 V3 = Base2 + j;

					Triangles[TriIdx++] = V3;
					Triangles[TriIdx++] = V2;
					Triangles[TriIdx++] = V0;

					Triangles[TriIdx++] = V2;
					Triangles[TriIdx++] = V1;
					Triangles[TriIdx++] = V0;
				}
			}
		}
	}
}

// --- End caps ---

void ABranchingMeshActor::GenerateEndCap(const FVector& RingCenter, const FQuat& RingOrientation, const FVector& OutwardDir, const float Width, const float InTaperLength, int32& InVertexIndex, int32& InTriangleIndex)
{
	const FVector TipPos = RingCenter + OutwardDir * InTaperLength;
	const bool bIsTaper = InTaperLength > KINDA_SMALL_NUMBER;
	const float SlantInvLen = bIsTaper
		? 1.f / FMath::Sqrt(Width * Width + InTaperLength * InTaperLength)
		: 0.f;

	const FVector CapTangent = RingOrientation.RotateVector(FVector::ForwardVector);

	// Center/tip vertex
	const int32 TipIdx = InVertexIndex++;
	Positions[TipIdx] = TipPos;
	Normals[TipIdx] = OutwardDir;
	Tangents[TipIdx] = FProcMeshTangent(CapTangent, false);
	TexCoords[TipIdx] = FVector2D(0.5f, 0.5f);

	// Rim vertices
	const int32 RimBaseIdx = InVertexIndex;
	for (int32 j = 0; j <= RadialSegmentCount; ++j)
	{
		const int32 VI = InVertexIndex++;
		const FVector LocalPos = CachedCrossSectionPoints[j] * Width;
		const FVector WorldOffset = RingOrientation.RotateVector(LocalPos);
		Positions[VI] = RingCenter + WorldOffset;

		if (bIsTaper)
		{
			const FVector Radial = WorldOffset.GetSafeNormal();
			Normals[VI] = (Radial * InTaperLength + OutwardDir * Width) * SlantInvLen;
		}
		else
		{
			Normals[VI] = OutwardDir;
		}

		Tangents[VI] = FProcMeshTangent(CapTangent, false);
		TexCoords[VI] = FVector2D(
			(CachedCrossSectionPoints[j].X + 1.f) * 0.5f,
			(CachedCrossSectionPoints[j].Y + 1.f) * 0.5f);
	}

	// Triangle fan from tip to rim
	for (int32 j = 0; j < RadialSegmentCount; ++j)
	{
		Triangles[InTriangleIndex++] = TipIdx;
		Triangles[InTriangleIndex++] = RimBaseIdx + j + 1;
		Triangles[InTriangleIndex++] = RimBaseIdx + j;
	}
}

void ABranchingMeshActor::GenerateEndCaps(const TArray<FBranchNode>& Nodes, const TArray<FBranchPath>& Paths, int32& VertIdx, int32& TriIdx)
{
	if (EndCapType == EBranchEndCapType::None)
	{
		return;
	}

	const float TerminalTaper = (EndCapType == EBranchEndCapType::Taper) ? TaperLength : 0.f;

	auto MakeQuat = [](const FVector& Dir) -> FQuat
	{
		return FQuat::FindBetweenNormals(FVector::UpVector, Dir);
	};

	for (const FBranchPath& Path : Paths)
	{
		if (Path.SplinePoints.Num() < 2)
		{
			continue;
		}

		const int32 FirstNodeIdx = Path.NodeIndices[0];
		const int32 LastNodeIdx = Path.NodeIndices.Last();

		// Cap at root (start of path if the starting node is a root)
		if (Nodes[FirstNodeIdx].bIsRoot)
		{
			const FVector Dir = (Path.SplinePoints[1] - Path.SplinePoints[0]).GetSafeNormal();
			GenerateEndCap(Path.SplinePoints[0], MakeQuat(Dir), -Dir, Path.SplineWidths[0], TerminalTaper, VertIdx, TriIdx);
		}

		// Cap at leaf (end of path if the ending node is a leaf)
		if (Nodes[LastNodeIdx].bIsLeaf)
		{
			const int32 NumPts = Path.SplinePoints.Num();
			const FVector Dir = (Path.SplinePoints[NumPts - 1] - Path.SplinePoints[NumPts - 2]).GetSafeNormal();
			GenerateEndCap(Path.SplinePoints.Last(), MakeQuat(Dir), Dir, Path.SplineWidths.Last(), TerminalTaper, VertIdx, TriIdx);
		}
	}
}

// --- Collision ---

void ABranchingMeshActor::GenerateCollision(const TArray<FBranchPath>& Paths)
{
	if (CollisionType == EBranchCollisionType::None)
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (CollisionType == EBranchCollisionType::ComplexAsSimple)
	{
		MeshComponent->bUseComplexAsSimpleCollision = true;
		return;
	}

	// SimpleCapsules: approximate each branch path with convex hull segments
	MeshComponent->bUseComplexAsSimpleCollision = false;

	for (const FBranchPath& Path : Paths)
	{
		if (Path.SplinePoints.Num() < 2)
		{
			continue;
		}

		// Generate capsule-approximating convex hulls at regular intervals
		const float StepDist = FMath::Max(Path.TotalLength / FMath::Max(1.f, FMath::CeilToFloat(Path.TotalLength / 30.f)), 5.f);
		int32 PtIdx = 0;

		while (PtIdx < Path.SplinePoints.Num() - 1)
		{
			// Find the range of points for this capsule segment
			const int32 StartPt = PtIdx;
			float AccumDist = 0.f;
			while (PtIdx < Path.SplinePoints.Num() - 1 && AccumDist < StepDist)
			{
				AccumDist += FVector::Dist(Path.SplinePoints[PtIdx], Path.SplinePoints[PtIdx + 1]);
				PtIdx++;
			}

			const FVector& SegStart = Path.SplinePoints[StartPt];
			const FVector& SegEnd = Path.SplinePoints[FMath::Min(PtIdx, Path.SplinePoints.Num() - 1)];
			const float SegWidth = FMath::Max(Path.SplineWidths[StartPt], Path.SplineWidths[FMath::Min(PtIdx, Path.SplinePoints.Num() - 1)]);

			const FVector Dir = (SegEnd - SegStart).GetSafeNormal();
			const FQuat Q = FQuat::FindBetweenNormals(FVector::UpVector, Dir);

			// Build a simple 16-vertex convex hull approximating a capsule
			TArray<FVector> ConvexVerts;
			ConvexVerts.Reserve(16);

			for (int32 EndPtIdx = 0; EndPtIdx < 2; ++EndPtIdx)
			{
				const FVector& Center = (EndPtIdx == 0) ? SegStart : SegEnd;
				for (int32 j = 0; j < 8; ++j)
				{
					const float Angle = static_cast<float>(j) * (2.f * PI / 8.f);
					const FVector Local(FMath::Cos(Angle) * SegWidth, FMath::Sin(Angle) * SegWidth, 0.f);
					ConvexVerts.Add(Center + Q.RotateVector(Local));
				}
			}

			MeshComponent->AddCollisionConvexMesh(ConvexVerts);
		}
	}
}

// --- Main mesh generation ---

void ABranchingMeshActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	RngStream.Initialize(RandomSeed);

	MeshComponent->ClearAllMeshSections();

	// Build tree graph using space colonization
	TArray<FBranchNode> TreeNodes;
	BuildTreeSpaceColonization(TreeNodes);

	if (TreeNodes.Num() < 2)
	{
		return;
	}

	// Extract branch paths
	TArray<FBranchPath> BranchPaths;
	ExtractBranchPaths(TreeNodes, BranchPaths);

	// Evaluate splines
	EvaluateSplines(BranchPaths, TreeNodes);

	// Trim tube paths at fork nodes to make room for transitions
	TMap<int32, FForkTrimInfo> ParentTrims, ChildTrims;
	TrimPathsAtForks(BranchPaths, TreeNodes, ParentTrims, ChildTrims);

	// Count fork nodes for transition geometry
	// Must match the skip logic in GenerateForkTransitions exactly (using ParentTrims direction)
	int32 NumForkTransitions = 0;
	for (int32 NodeIdx = 0; NodeIdx < TreeNodes.Num(); ++NodeIdx)
	{
		const FBranchNode& Node = TreeNodes[NodeIdx];
		if (Node.bIsFork)
		{
			FVector FallbackParentDir = FVector::UpVector;
			if (Node.ParentIndex != INDEX_NONE)
			{
				FallbackParentDir = (Node.Position - TreeNodes[Node.ParentIndex].Position).GetSafeNormal();
			}
			const FForkTrimInfo* ParentTrim = ParentTrims.Find(NodeIdx);
			const FVector StartDir = ParentTrim ? ParentTrim->Direction : FallbackParentDir;

			for (int32 ChildIdx : Node.ChildIndices)
			{
				const FVector ChildDir = (TreeNodes[ChildIdx].Position - Node.Position).GetSafeNormal();
				if (FVector::DotProduct(StartDir, ChildDir) >= -0.866f)
				{
					NumForkTransitions++;
				}
			}
		}
	}

	// Count end caps
	int32 NumCaps = 0;
	if (EndCapType != EBranchEndCapType::None)
	{
		for (const FBranchPath& Path : BranchPaths)
		{
			if (Path.SplinePoints.Num() < 2) continue;
			if (TreeNodes[Path.NodeIndices[0]].bIsRoot) NumCaps++;
			if (TreeNodes[Path.NodeIndices.Last()].bIsLeaf) NumCaps++;
		}
	}

	// Calculate buffer sizes
	const int32 VertsPerRing = RadialSegmentCount + 1;
	const int32 NumTransitionRings = FMath::Clamp(ForkTransitionRings, 2, 16);
	const int32 CapVerts = RadialSegmentCount + 2; // 1 tip + (RadialSegmentCount+1) rim
	const int32 CapIndices = RadialSegmentCount * 3;

	int32 TotalVerts = 0;
	int32 TotalIndices = 0;

	// Tube verts/indices
	for (const FBranchPath& Path : BranchPaths)
	{
		const int32 NumPts = Path.SplinePoints.Num();
		if (NumPts < 2) continue;
		TotalVerts += NumPts * VertsPerRing;
		TotalIndices += (NumPts - 1) * RadialSegmentCount * 6;
	}

	// Fork transition verts/indices
	TotalVerts += NumForkTransitions * (NumTransitionRings + 1) * VertsPerRing;
	TotalIndices += NumForkTransitions * NumTransitionRings * RadialSegmentCount * 6;

	// End cap verts/indices
	TotalVerts += NumCaps * CapVerts;
	TotalIndices += NumCaps * CapIndices;

	if (TotalVerts == 0)
	{
		return;
	}

	// Allocate mesh buffers
	Positions.SetNumUninitialized(TotalVerts);
	Normals.SetNumUninitialized(TotalVerts);
	Tangents.SetNumUninitialized(TotalVerts);
	TexCoords.SetNumUninitialized(TotalVerts);
	Triangles.SetNumUninitialized(TotalIndices);

	int32 VertIdx = 0;
	int32 TriIdx = 0;

	// Generate tube meshes
	GenerateTubeMesh(BranchPaths, VertIdx, TriIdx);

	// Generate fork transitions using actual trim positions
	GenerateForkTransitions(TreeNodes, BranchPaths, ParentTrims, ChildTrims, VertIdx, TriIdx);

	// Generate end caps
	GenerateEndCaps(TreeNodes, BranchPaths, VertIdx, TriIdx);

	// Trim buffers in case some fork transitions were skipped (extreme angles)
	if (VertIdx < TotalVerts)
	{
		Positions.SetNum(VertIdx);
		Normals.SetNum(VertIdx);
		Tangents.SetNum(VertIdx);
		TexCoords.SetNum(VertIdx);
	}
	if (TriIdx < TotalIndices)
	{
		Triangles.SetNum(TriIdx);
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	MeshComponent->SetMaterial(0, Material);

	// Collision
	GenerateCollision(BranchPaths);
}
