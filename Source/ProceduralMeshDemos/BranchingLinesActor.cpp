// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example branching lines using cylinder strips

#include "BranchingLinesActor.h"

ABranchingLinesActor::ABranchingLinesActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);

	// Setup random offset directions
	OffsetDirections.Add(FVector(1, 0, 0));
	OffsetDirections.Add(FVector(0, 0, 1));
}

void ABranchingLinesActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PreCacheCrossSection();
	GenerateMesh();
}

void ABranchingLinesActor::PostLoad()
{
	Super::PostLoad();
	PreCacheCrossSection();
	GenerateMesh();
}

void ABranchingLinesActor::SetupMeshBuffers()
{
	const int32 TotalNumberOfVerticesPerSection = RadialSegmentCount * 4; // 4 verts per face 
	const int32 TotalNumberOfTrianglesPerSection = TotalNumberOfVerticesPerSection + 2 * RadialSegmentCount;
	const int32 VertexCount = TotalNumberOfVerticesPerSection * Segments.Num();
	const int32 TriangleCount = TotalNumberOfTrianglesPerSection * Segments.Num();
		
	if (VertexCount != Positions.Num())
	{
		Positions.Empty();
		Positions.AddUninitialized(VertexCount);
		Normals.Empty();
		Normals.AddUninitialized(VertexCount);
		Tangents.Empty();
		Tangents.AddUninitialized(VertexCount);
		TexCoords.Empty();
		TexCoords.AddUninitialized(VertexCount);
	}

	if (TriangleCount != Triangles.Num())
	{
		Triangles.Empty();
		Triangles.AddUninitialized(TriangleCount);
	}
}

void ABranchingLinesActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	// -------------------------------------------------------
	// Setup the random number generator and create the branching structure
	RngStream.Initialize(RandomSeed);
	CreateSegments();

	MeshComponent->ClearAllMeshSections();

	if (bSmoothJoints)
	{
		GenerateSmoothMesh();
		return;
	}

	// -------------------------------------------------------
	// Identify terminal endpoints (roots and leaves) for end caps
	auto Quantize = [](const FVector& V) -> FIntVector
	{
		return FIntVector(
			FMath::RoundToInt(V.X * 100.f),
			FMath::RoundToInt(V.Y * 100.f),
			FMath::RoundToInt(V.Z * 100.f)
		);
	};

	TSet<FIntVector> AllStartPoints, AllEndPoints;
	int32 NumCaps = 0;

	if (EndCapType != EBranchEndCapType::None)
	{
		for (const FBranchSegment& Seg : Segments)
		{
			AllStartPoints.Add(Quantize(Seg.Start));
			AllEndPoints.Add(Quantize(Seg.End));
		}

		for (const FBranchSegment& Seg : Segments)
		{
			if (!AllStartPoints.Contains(Quantize(Seg.End))) NumCaps++;   // leaf
			if (!AllEndPoints.Contains(Quantize(Seg.Start))) NumCaps++;   // root
		}
	}

	SetupMeshBuffers();

	// Extend buffers for end cap geometry
	if (NumCaps > 0)
	{
		const int32 CapVerts = RadialSegmentCount + 2; // 1 tip + (RadialSegmentCount+1) rim
		const int32 CapIndices = RadialSegmentCount * 3;
		Positions.AddUninitialized(NumCaps * CapVerts);
		Normals.AddUninitialized(NumCaps * CapVerts);
		Tangents.AddUninitialized(NumCaps * CapVerts);
		TexCoords.AddUninitialized(NumCaps * CapVerts);
		Triangles.AddUninitialized(NumCaps * CapIndices);
	}

	// -------------------------------------------------------
	// Now lets loop through all the defined segments and create a cylinder for each
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 i = 0; i < Segments.Num(); i++)
	{
		GenerateCylinder(Positions, Triangles, Normals, Tangents, TexCoords, Segments[i].Start, Segments[i].End, Segments[i].Width, RadialSegmentCount, VertexIndex, TriangleIndex, bSmoothNormals);
	}

	// Generate end caps at terminal endpoints
	if (EndCapType != EBranchEndCapType::None)
	{
		const float CapTaperLen = (EndCapType == EBranchEndCapType::Taper) ? TaperLength : 0.f;

		for (const FBranchSegment& Seg : Segments)
		{
			// Compute the rotation quaternion matching GenerateCylinder's Euler rotation
			const FVector LineDir = (Seg.Start - Seg.End).GetSafeNormal();
			const FQuat Q = FQuat::MakeFromEuler(LineDir.Rotation().Add(90.f, 0.f, 0.f).Euler());

			// Leaf cap at segment End
			if (!AllStartPoints.Contains(Quantize(Seg.End)))
			{
				GenerateEndCap(Seg.End, Q, -LineDir, Seg.Width, CapTaperLen, VertexIndex, TriangleIndex);
			}

			// Root cap at segment Start
			if (!AllEndPoints.Contains(Quantize(Seg.Start)))
			{
				GenerateEndCap(Seg.Start, Q, LineDir, Seg.Width, CapTaperLen, VertexIndex, TriangleIndex);
			}
		}
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	MeshComponent->SetMaterial(0, Material);
}

void ABranchingLinesActor::GenerateSmoothMesh()
{
	if (Segments.Num() == 0)
	{
		return;
	}

	// --- Step 1: Build polyline chains from the flat segment list ---
	// Segments form a tree (from subdivision + forks). We trace chains of connected
	// segments, breaking at fork points (where multiple segments share a start point)
	// and at leaves (where no continuation exists).

	// Quantize points to integer keys for reliable lookup
	auto Quantize = [](const FVector& V) -> FIntVector
	{
		return FIntVector(
			FMath::RoundToInt(V.X * 100.f),
			FMath::RoundToInt(V.Y * 100.f),
			FMath::RoundToInt(V.Z * 100.f)
		);
	};

	// Map: quantized start point -> list of segment indices starting there
	TMap<FIntVector, TArray<int32>> StartMap;
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		StartMap.FindOrAdd(Quantize(Segments[i].Start)).Add(i);
	}

	// Set of all quantized end points
	TSet<FIntVector> EndPointSet;
	for (const FBranchSegment& Seg : Segments)
	{
		EndPointSet.Add(Quantize(Seg.End));
	}

	// Trace chains
	struct FChain
	{
		TArray<FVector> Points;
		float Width;
	};

	TArray<FChain> Chains;
	TSet<int32> Visited;

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		if (Visited.Contains(i))
		{
			continue;
		}

		const FIntVector StartKey = Quantize(Segments[i].Start);

		// A segment starts a new chain if its Start point is either:
		// - Not the End of any segment (root of the tree)
		// - At a fork point (multiple segments start from the same point)
		const bool bIsRoot = !EndPointSet.Contains(StartKey);
		const TArray<int32>* StartsHere = StartMap.Find(StartKey);
		const bool bIsFork = StartsHere && StartsHere->Num() > 1;

		if (!bIsRoot && !bIsFork)
		{
			continue;
		}

		// Trace chain from this segment
		FChain Chain;
		Chain.Width = Segments[i].Width;
		Chain.Points.Add(Segments[i].Start);

		int32 Current = i;
		while (Current >= 0 && !Visited.Contains(Current))
		{
			Visited.Add(Current);
			Chain.Points.Add(Segments[Current].End);

			// Find continuation: exactly one unvisited segment starting at this End point
			const FIntVector EndKey = Quantize(Segments[Current].End);
			const TArray<int32>* NextStarts = StartMap.Find(EndKey);

			if (!NextStarts || NextStarts->Num() != 1)
			{
				break; // fork or leaf — end chain
			}

			const int32 Next = (*NextStarts)[0];
			if (Visited.Contains(Next))
			{
				break;
			}

			Current = Next;
		}

		Chains.Add(MoveTemp(Chain));
	}

	// --- Step 2: Build rings for each chain and compute buffer sizes ---
	auto MakeQuat = [](const FVector& Dir) -> FQuat
	{
		return FQuat::FindBetweenNormals(FVector::UpVector, Dir);
	};

	struct FRing
	{
		FVector Center;
		FQuat Orientation;
		float V;
	};

	struct FTubeData
	{
		TArray<FRing> Rings;
		float Width;
	};

	const float AngleThreshold = FMath::DegreesToRadians(2.f);

	TArray<FTubeData> Tubes;
	Tubes.Reserve(Chains.Num());

	// Collect end cap info while building tubes
	struct FCapInfo
	{
		FVector Center;
		FQuat Orientation;
		FVector OutwardDir;
		float Width;
	};
	TArray<FCapInfo> Caps;

	for (const FChain& Chain : Chains)
	{
		if (Chain.Points.Num() < 2)
		{
			continue;
		}

		const int32 NumSeg = Chain.Points.Num() - 1;

		// Compute segment directions
		TArray<FVector> Dirs;
		Dirs.Reserve(NumSeg);
		for (int32 s = 0; s < NumSeg; ++s)
		{
			const FVector Delta = Chain.Points[s + 1] - Chain.Points[s];
			const float Len = Delta.Size();
			Dirs.Add(Len > KINDA_SMALL_NUMBER ? Delta / Len : FVector::UpVector);
		}

		FTubeData Tube;
		Tube.Width = Chain.Width;

		// First endpoint ring
		Tube.Rings.Add({Chain.Points[0], MakeQuat(Dirs[0]), 1.f});

		// Interior joints
		for (int32 p = 0; p < NumSeg - 1; ++p)
		{
			const FVector& DirIn = Dirs[p];
			const FVector& DirOut = Dirs[p + 1];
			const FVector& JointPt = Chain.Points[p + 1];

			const float CosAlpha = FMath::Clamp(FVector::DotProduct(DirIn, DirOut), -1.f, 1.f);
			const float Alpha = FMath::Acos(CosAlpha);

			if (Alpha < AngleThreshold)
			{
				// Nearly straight — two coincident rings to reset V
				Tube.Rings.Add({JointPt, MakeQuat(DirIn), 0.f});
				Tube.Rings.Add({JointPt, MakeQuat(DirOut), 1.f});
			}
			else if (JointSegments <= 0 || Alpha > PI - AngleThreshold)
			{
				// Sharp miter
				FVector MiterDir = (DirIn + DirOut);
				if (MiterDir.SizeSquared() < KINDA_SMALL_NUMBER)
				{
					MiterDir = DirOut;
				}
				const FQuat MiterQ = MakeQuat(MiterDir.GetSafeNormal());
				Tube.Rings.Add({JointPt, MiterQ, 0.f});
				Tube.Rings.Add({JointPt, MiterQ, 1.f});
			}
			else
			{
				// Smooth spherical joint: slerp from incoming to outgoing
				const FQuat QIn = MakeQuat(DirIn);
				const FQuat QOut = MakeQuat(DirOut);

				for (int32 j = 0; j <= JointSegments; ++j)
				{
					const float T = static_cast<float>(j) / static_cast<float>(JointSegments);
					Tube.Rings.Add({JointPt, FQuat::Slerp(QIn, QOut, T), T});
				}
			}
		}

		// Last endpoint ring
		Tube.Rings.Add({Chain.Points.Last(), MakeQuat(Dirs.Last()), 0.f});

		// Check for terminal endpoints that need caps
		if (EndCapType != EBranchEndCapType::None)
		{
			// Root: first point not any segment's End → tree root
			if (!EndPointSet.Contains(Quantize(Chain.Points[0])))
			{
				Caps.Add({Chain.Points[0], MakeQuat(Dirs[0]), -Dirs[0], Chain.Width});
			}

			// Leaf: last point not any segment's Start → branch tip
			if (!StartMap.Contains(Quantize(Chain.Points.Last())))
			{
				Caps.Add({Chain.Points.Last(), MakeQuat(Dirs.Last()), Dirs.Last(), Chain.Width});
			}
		}

		Tubes.Add(MoveTemp(Tube));
	}

	// --- Step 3: Allocate mesh buffers ---
	const int32 VertsPerRing = RadialSegmentCount + 1;
	const int32 CapVerts = RadialSegmentCount + 2;    // 1 tip + (RadialSegmentCount+1) rim
	const int32 CapIndices = RadialSegmentCount * 3;
	int32 TotalVerts = 0;
	int32 TotalIndices = 0;

	for (const FTubeData& Tube : Tubes)
	{
		TotalVerts += Tube.Rings.Num() * VertsPerRing;
		TotalIndices += (Tube.Rings.Num() - 1) * RadialSegmentCount * 6;
	}

	TotalVerts += Caps.Num() * CapVerts;
	TotalIndices += Caps.Num() * CapIndices;

	if (TotalVerts == 0)
	{
		return;
	}

	Positions.SetNumUninitialized(TotalVerts);
	Normals.SetNumUninitialized(TotalVerts);
	Tangents.SetNumUninitialized(TotalVerts);
	TexCoords.SetNumUninitialized(TotalVerts);
	Triangles.SetNumUninitialized(TotalIndices);

	// --- Step 4: Fill mesh buffers ---
	const float UStep = 1.f / static_cast<float>(RadialSegmentCount);
	int32 VertIdx = 0;
	int32 TriIdx = 0;

	for (const FTubeData& Tube : Tubes)
	{
		const int32 NumRings = Tube.Rings.Num();
		const int32 TubeBaseVert = VertIdx;

		// Fill vertex data
		for (int32 RingIdx = 0; RingIdx < NumRings; ++RingIdx)
		{
			const FRing& Ring = Tube.Rings[RingIdx];
			const FVector TubeDir = Ring.Orientation.RotateVector(FVector::UpVector);

			for (int32 j = 0; j <= RadialSegmentCount; ++j)
			{
				const int32 VI = VertIdx++;
				const FVector LocalPos = CachedCrossSectionPoints[j] * Tube.Width;
				const FVector WorldOffset = Ring.Orientation.RotateVector(LocalPos);

				Positions[VI] = Ring.Center + WorldOffset;
				Normals[VI] = WorldOffset.GetSafeNormal();
				Tangents[VI] = FProcMeshTangent(TubeDir, false);
				TexCoords[VI] = FVector2D(1.f - static_cast<float>(j) * UStep, Ring.V);
			}
		}

		// Fill index data — connect adjacent rings within this tube
		for (int32 RingIdx = 0; RingIdx < NumRings - 1; ++RingIdx)
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

	// --- Step 5: Generate end caps ---
	const float CapTaperLen = (EndCapType == EBranchEndCapType::Taper) ? TaperLength : 0.f;
	for (const FCapInfo& Cap : Caps)
	{
		GenerateEndCap(Cap.Center, Cap.Orientation, Cap.OutwardDir, Cap.Width, CapTaperLen, VertIdx, TriIdx);
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	MeshComponent->SetMaterial(0, Material);
}

void ABranchingLinesActor::GenerateEndCap(const FVector& RingCenter, const FQuat& RingOrientation, const FVector& OutwardDir, const float Width, const float InTaperLength, int32& InVertexIndex, int32& InTriangleIndex)
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
			// Cone surface normal: perpendicular to the slant surface, pointing outward
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

FVector ABranchingLinesActor::RotatePointAroundPivot(const FVector InPoint, const FVector InPivot, const FVector InAngles)
{
	FVector Direction = InPoint - InPivot; // get point direction relative to pivot
	Direction = FQuat::MakeFromEuler(InAngles) * Direction; // rotate it
	return Direction + InPivot; // calculate rotated point
}

void ABranchingLinesActor::PreCacheCrossSection()
{
	if (LastCachedCrossSectionCount == RadialSegmentCount)
	{
		return;
	}

	// Generate a cross-section for use in cylinder generation
	const float AngleBetweenQuads = (2.0f / static_cast<float>(RadialSegmentCount)) * PI;
	CachedCrossSectionPoints.Empty();

	// Pre-calculate cross section points of a circle, two more than needed
	for (int32 PointIndex = 0; PointIndex < (RadialSegmentCount + 2); PointIndex++)
	{
		const float Angle = static_cast<float>(PointIndex) * AngleBetweenQuads;
		CachedCrossSectionPoints.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0));
	}

	LastCachedCrossSectionCount = RadialSegmentCount;
}

void ABranchingLinesActor::CreateSegments()
{
	// We create the branching structure by constantly subdividing a line between two points by creating a new point in the middle.
	// We then take that point and offset it in a random direction, by a random amount defined within limits.
	// Next we take both of the newly created line halves, and subdivide them the same way.
	// Each new midpoint also has a chance to create a new branch
	// TODO This should really be recursive
	Segments.Empty();
	float CurrentBranchOffset = MaxBranchOffset;

	if (bMaxBranchOffsetAsPercentageOfLength)
	{
		CurrentBranchOffset = (Start - End).Size() * (FMath::Clamp(MaxBranchOffset, 0.1f, 100.0f) / 100.0f);
	}

	// Pre-calc a few floats from percentages
	const float ChangeOfFork = FMath::Clamp(ChanceOfForkPercentage, 0.0f, 100.0f) / 100.0f;
	const float BranchOffsetReductionEachGeneration = FMath::Clamp(BranchOffsetReductionEachGenerationPercentage, 0.0f, 100.0f) / 100.0f;

	// Add the first segment which is simply between the start and end points
	Segments.Add(FBranchSegment(Start, End, TrunkWidth));

	for (int32 iGen = 0; iGen < Iterations; iGen++)
	{
		TArray<FBranchSegment> NewGen;

		for (const FBranchSegment& EachSegment : Segments)
		{
			FVector Midpoint = (EachSegment.End + EachSegment.Start) / 2;

			// Offset the midpoint by a random number along the normal
			const FVector Normal = FVector::CrossProduct(EachSegment.End - EachSegment.Start, OffsetDirections[RngStream.RandRange(0, 1)]).GetSafeNormal();
			Midpoint += Normal * RngStream.RandRange(-CurrentBranchOffset, CurrentBranchOffset);

			 // Create two new segments
			NewGen.Add(FBranchSegment(EachSegment.Start, Midpoint, EachSegment.Width, EachSegment.ForkGeneration));
			NewGen.Add(FBranchSegment(Midpoint, EachSegment.End, EachSegment.Width, EachSegment.ForkGeneration));

			// Chance of fork?
			if (RngStream.FRand() > (1 - ChangeOfFork))
			{
				// TODO Normalize the direction vector and calculate a new total length and then subdiv that for X generations
				const FVector Direction = Midpoint - EachSegment.Start;
				const FVector SplitEnd = (Direction * RngStream.FRandRange(ForkLengthMin, ForkLengthMax)).RotateAngleAxis(RngStream.FRandRange(ForkRotationMin, ForkRotationMax), OffsetDirections[RngStream.RandRange(0, 1)]) + Midpoint;
				NewGen.Add(FBranchSegment(Midpoint, SplitEnd, EachSegment.Width * WidthReductionOnFork, EachSegment.ForkGeneration + 1));
			}
		}

		Segments.Empty();
		Segments = NewGen;

		// Reduce the offset slightly each generation
		CurrentBranchOffset = CurrentBranchOffset * BranchOffsetReductionEachGeneration;
	}
}

void ABranchingLinesActor::GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FProcMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector StartPoint, const FVector EndPoint, const float InWidth, const int32 InCrossSectionCount, int32& InVertexIndex, int32& InTriangleIndex, const bool bInSmoothNormals/* = true*/)
{
	// Make a cylinder section
	const float AngleBetweenQuads = (2.0f / static_cast<float>(InCrossSectionCount)) * PI;
	const float UMapPerQuad = 1.0f / static_cast<float>(InCrossSectionCount);

	const FVector StartOffset = StartPoint - FVector(0, 0, 0);
	const FVector Offset = EndPoint - StartPoint;

	// Find angle between vectors
	const FVector LineDirection = (StartPoint - EndPoint).GetSafeNormal();
	const FVector RotationAngle = LineDirection.Rotation().Add(90.f, 0.f, 0.f).Euler();

	// Start by building up vertices that make up the cylinder sides
	for (int32 QuadIndex = 0; QuadIndex < InCrossSectionCount; QuadIndex++)
	{
		//float Angle = static_cast<float>(QuadIndex) * AngleBetweenQuads;
		//float NextAngle = static_cast<float>(QuadIndex + 1) * AngleBetweenQuads;

		// Set up the vertices
		FVector P0 = (CachedCrossSectionPoints[QuadIndex] * InWidth) + StartOffset;
		P0 = RotatePointAroundPivot(P0, StartPoint, RotationAngle);
		FVector P1 = CachedCrossSectionPoints[QuadIndex + 1] * InWidth + StartOffset;
		P1 = RotatePointAroundPivot(P1, StartPoint, RotationAngle);
		const FVector P2 = P1 + Offset;
		const FVector P3 = P0 + Offset;

		// Set up the quad triangles
		const int32 VertIndex1 = InVertexIndex++;
		const int32 VertIndex2 = InVertexIndex++;
		const int32 VertIndex3 = InVertexIndex++;
		const int32 VertIndex4 = InVertexIndex++;

		InVertices[VertIndex1] = P0;
		InVertices[VertIndex2] = P1;
		InVertices[VertIndex3] = P2;
		InVertices[VertIndex4] = P3;

		// Now create two triangles from those four vertices
		// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
		InTriangles[InTriangleIndex++] = VertIndex4;
		InTriangles[InTriangleIndex++] = VertIndex3;
		InTriangles[InTriangleIndex++] = VertIndex1;

		InTriangles[InTriangleIndex++] = VertIndex3;
		InTriangles[InTriangleIndex++] = VertIndex2;
		InTriangles[InTriangleIndex++] = VertIndex1;

		// UVs.  Note that Unreal UV origin (0,0) is top left
		InTexCoords[VertIndex1] = FVector2D(1.0f - (UMapPerQuad * QuadIndex), 1.0f);
		InTexCoords[VertIndex2] = FVector2D(1.0f - (UMapPerQuad * (QuadIndex + 1)), 1.0f);
		InTexCoords[VertIndex3] = FVector2D(1.0f - (UMapPerQuad * (QuadIndex + 1)), 0.0f);
		InTexCoords[VertIndex4] = FVector2D(1.0f - (UMapPerQuad * QuadIndex), 0.0f);

		// Normals
		const FVector NormalCurrent = FVector::CrossProduct(InVertices[VertIndex1] - InVertices[VertIndex3], InVertices[VertIndex2] - InVertices[VertIndex3]).GetSafeNormal();

		if (bInSmoothNormals)
		{
			// To smooth normals we give the vertices different values than the polygon they belong to.
			// GPUs know how to interpolate between those.
			// I do this here as an average between normals of two adjacent polygons
			//float NextNextAngle = static_cast<float>(QuadIndex + 2) * AngleBetweenQuads;
			FVector P4 = (CachedCrossSectionPoints[QuadIndex + 2] * InWidth) + StartOffset;
			P4 = RotatePointAroundPivot(P4, StartPoint, RotationAngle);

			// p1 to p4 to p2
			const FVector NormalNext = FVector::CrossProduct(P1 - P2, P4 - P2).GetSafeNormal();
			const FVector AverageNormalRight = ((NormalCurrent + NormalNext) / 2).GetSafeNormal();

			const float PreviousAngle = static_cast<float>(QuadIndex - 1) * AngleBetweenQuads;
			FVector PMinus1 = FVector(FMath::Cos(PreviousAngle) * InWidth, FMath::Sin(PreviousAngle) * InWidth, 0.f) + StartOffset;
			PMinus1 = RotatePointAroundPivot(PMinus1, StartPoint, RotationAngle);

			// p0 to p3 to pMinus1
			const FVector NormalPrevious = FVector::CrossProduct(P0 - PMinus1, P3 - PMinus1).GetSafeNormal();
			const FVector AverageNormalLeft = ((NormalCurrent + NormalPrevious) / 2).GetSafeNormal();

			InNormals[VertIndex1] = AverageNormalLeft;
			InNormals[VertIndex2] = AverageNormalRight;
			InNormals[VertIndex3] = AverageNormalRight;
			InNormals[VertIndex4] = AverageNormalLeft;
		}
		else
		{
			// If not smoothing we just set the vertex normal to the same normal as the polygon they belong to
			InNormals[VertIndex1] = InNormals[VertIndex2] = InNormals[VertIndex3] = InNormals[VertIndex4] = NormalCurrent;
		}

		// Tangents (perpendicular to the surface)
		const FVector SurfaceTangentVec = (P0 - P1).GetSafeNormal();
		const FProcMeshTangent SurfaceTangent(SurfaceTangentVec, /*bFlipTangentY=*/ false);
		InTangents[VertIndex1] = InTangents[VertIndex2] = InTangents[VertIndex3] = InTangents[VertIndex4] = SurfaceTangent;
	}
}
