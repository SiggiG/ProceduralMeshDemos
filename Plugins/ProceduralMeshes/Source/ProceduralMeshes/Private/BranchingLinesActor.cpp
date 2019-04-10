// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example branching lines using cylinder strips

#include "ProceduralMeshesPrivatePCH.h"
#include "BranchingLinesActor.h"

ABranchingLinesActor::ABranchingLinesActor()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	MeshComponent->GetOrCreateRuntimeMesh()->SetShouldSerializeMeshData(false);
	MeshComponent->SetupAttachment(RootComponent);

	// Setup random offset directions
	OffsetDirections.Add(FVector(1, 0, 0));
	OffsetDirections.Add(FVector(0, 0, 1));
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ABranchingLinesActor::PostActorCreated()
{
	Super::PostActorCreated();
	PreCacheCrossSection();
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void ABranchingLinesActor::PostLoad()
{
	Super::PostLoad();
	PreCacheCrossSection();
	GenerateMesh();
}

void ABranchingLinesActor::SetupMeshBuffers()
{
	int32 TotalNumberOfVerticesPerSection = RadialSegmentCount * 4; // 4 verts per face 
	int32 TotalNumberOfTrianglesPerSection = TotalNumberOfVerticesPerSection + 2 * RadialSegmentCount;
	Vertices.AddUninitialized(TotalNumberOfVerticesPerSection * Segments.Num());
	Triangles.AddUninitialized(TotalNumberOfTrianglesPerSection * Segments.Num());
}

void ABranchingLinesActor::GenerateMesh()
{
	// -------------------------------------------------------
	// Setup the random number generator and create the branching structure
	RngStream.Initialize(RandomSeed);
	CreateSegments();

	// The number of vertices or polygons wont change at runtime, so we'll just allocate the arrays once
	if (!bHaveBuffersBeenInitialized)
	{
		SetupMeshBuffers();
		bHaveBuffersBeenInitialized = true;
	}

	// -------------------------------------------------------
	// Now lets loop through all the defined segments and create a cylinder for each
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 i = 0; i < Segments.Num(); i++)
	{
		GenerateCylinder(Vertices, Triangles, Segments[i].Start, Segments[i].End, Segments[i].Width, RadialSegmentCount, VertexIndex, TriangleIndex, bSmoothNormals);
	}

	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, GetBounds(), false, EUpdateFrequency::Infrequent);
	MeshComponent->SetMaterial(0, Material);
}

FBox ABranchingLinesActor::GetBounds()
{
	FVector2D RangeX = FVector2D::ZeroVector;
	FVector2D RangeY = FVector2D::ZeroVector;
	FVector2D RangeZ = FVector2D::ZeroVector;

	for (FBranchSegment& Segment : Segments)
	{
		// Start
		if (Segment.Start.X < RangeX.X)
		{
			RangeX.X = Segment.Start.X;
		}
		else if (Segment.Start.X >= RangeX.Y)
		{
			RangeX.Y = Segment.Start.X;
		}

		if (Segment.Start.Y < RangeY.X)
		{
			RangeY.X = Segment.Start.Y;
		}
		else if (Segment.Start.Y >= RangeY.Y)
		{
			RangeY.Y = Segment.Start.Y;
		}

		if (Segment.Start.Z < RangeZ.X)
		{
			RangeZ.X = Segment.Start.Z;
		}
		else if (Segment.Start.Z >= RangeZ.Y)
		{
			RangeZ.Y = Segment.Start.Z;
		}

		// End
		if (Segment.Start.X < RangeX.X)
		{
			RangeX.X = Segment.Start.X;
		}
		else if (Segment.Start.X >= RangeX.Y)
		{
			RangeX.Y = Segment.Start.X;
		}

		if (Segment.End.Y < RangeY.X)
		{
			RangeY.X = Segment.End.Y;
		}
		else if (Segment.End.Y >= RangeY.Y)
		{
			RangeY.Y = Segment.End.Y;
		}

		if (Segment.End.Z < RangeZ.X)
		{
			RangeZ.X = Segment.End.Z;
		}
		else if (Segment.End.Z >= RangeZ.Y)
		{
			RangeZ.Y = Segment.End.Z;
		}
	}

	RangeX.X -= TrunkWidth;
	RangeX.Y += TrunkWidth;
	RangeY.X -= TrunkWidth;
	RangeY.Y += TrunkWidth;
	RangeZ.X -= TrunkWidth;
	RangeZ.Y += TrunkWidth;

	return FBox(FVector(RangeX.X, RangeY.X, RangeZ.X), FVector(RangeX.Y, RangeY.Y, RangeZ.Y));
}

FVector ABranchingLinesActor::RotatePointAroundPivot(FVector InPoint, FVector InPivot, FVector InAngles)
{
	FVector direction = InPoint - InPivot; // get point direction relative to pivot
	direction = FQuat::MakeFromEuler(InAngles) * direction; // rotate it
	return direction + InPivot; // calculate rotated point
}

void ABranchingLinesActor::PreCacheCrossSection()
{
	if (LastCachedCrossSectionCount == RadialSegmentCount)
	{
		return;
	}

	// Generate a cross-section for use in cylinder generation
	const float AngleBetweenQuads = (2.0f / (float)(RadialSegmentCount)) * PI;
	CachedCrossSectionPoints.Empty();

	// Pre-calculate cross section points of a circle, two more than needed
	for (int32 PointIndex = 0; PointIndex < (RadialSegmentCount + 2); PointIndex++)
	{
		float Angle = (float)PointIndex * AngleBetweenQuads;
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
	float ChangeOfFork = FMath::Clamp(ChanceOfForkPercentage, 0.0f, 100.0f) / 100.0f;
	float BranchOffsetReductionEachGeneration = FMath::Clamp(BranchOffsetReductionEachGenerationPercentage, 0.0f, 100.0f) / 100.0f;

	// Add the first segment which is simply between the start and end points
	Segments.Add(FBranchSegment(Start, End, TrunkWidth));

	for (int32 iGen = 0; iGen < Iterations; iGen++)
	{
		TArray<FBranchSegment> newGen;

		for (const FBranchSegment& EachSegment : Segments)
		{
			FVector Midpoint = (EachSegment.End + EachSegment.Start) / 2;

			// Offset the midpoint by a random number along the normal
			FVector normal = FVector::CrossProduct(EachSegment.End - EachSegment.Start, OffsetDirections[RngStream.RandRange(0, 1)]);
			normal.Normalize();
			Midpoint += normal * RngStream.RandRange(-CurrentBranchOffset, CurrentBranchOffset);

			 // Create two new segments
			newGen.Add(FBranchSegment(EachSegment.Start, Midpoint, EachSegment.Width, EachSegment.ForkGeneration));
			newGen.Add(FBranchSegment(Midpoint, EachSegment.End, EachSegment.Width, EachSegment.ForkGeneration));

			// Chance of fork?
			if (RngStream.FRand() > (1 - ChangeOfFork))
			{
				// TODO Normalize the direction vector and calculate a new total length and then subdiv that for X generations
				FVector direction = Midpoint - EachSegment.Start;
				FVector splitEnd = (direction * RngStream.FRandRange(ForkLengthMin, ForkLengthMax)).RotateAngleAxis(RngStream.FRandRange(ForkRotationMin, ForkRotationMax), OffsetDirections[RngStream.RandRange(0, 1)]) + Midpoint;
				newGen.Add(FBranchSegment(Midpoint, splitEnd, EachSegment.Width * WidthReductionOnFork, EachSegment.ForkGeneration + 1));
			}
		}

		Segments.Empty();
		Segments = newGen;

		// Reduce the offset slightly each generation
		CurrentBranchOffset = CurrentBranchOffset * BranchOffsetReductionEachGeneration;
	}
}

void ABranchingLinesActor::GenerateCylinder(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector StartPoint, FVector EndPoint, float InWidth, int32 InCrossSectionCount, int32& InVertexIndex, int32& InTriangleIndex, bool bInSmoothNormals/* = true*/)
{
	// Make a cylinder section
	const float AngleBetweenQuads = (2.0f / (float)(InCrossSectionCount)) * PI;
	const float UMapPerQuad = 1.0f / (float)InCrossSectionCount;

	FVector StartOffset = StartPoint - FVector(0, 0, 0);
	FVector Offset = EndPoint - StartPoint;

	// Find angle between vectors
	FVector LineDirection = (StartPoint - EndPoint);
	LineDirection.Normalize();
	FVector RotationAngle = LineDirection.Rotation().Add(90.f, 0.f, 0.f).Euler();

	// Start by building up vertices that make up the cylinder sides
	for (int32 QuadIndex = 0; QuadIndex < InCrossSectionCount; QuadIndex++)
	{
		float Angle = (float)QuadIndex * AngleBetweenQuads;
		float NextAngle = (float)(QuadIndex + 1) * AngleBetweenQuads;

		// Set up the vertices
		FVector p0 = (CachedCrossSectionPoints[QuadIndex] * InWidth) + StartOffset;
		p0 = RotatePointAroundPivot(p0, StartPoint, RotationAngle);
		FVector p1 = CachedCrossSectionPoints[QuadIndex + 1] * InWidth + StartOffset;
		p1 = RotatePointAroundPivot(p1, StartPoint, RotationAngle);
		FVector p2 = p1 + Offset;
		FVector p3 = p0 + Offset;

		// Set up the quad triangles
		int32 VertIndex1 = InVertexIndex++;
		int32 VertIndex2 = InVertexIndex++;
		int32 VertIndex3 = InVertexIndex++;
		int32 VertIndex4 = InVertexIndex++;

		InVertices[VertIndex1].Position = p0;
		InVertices[VertIndex2].Position = p1;
		InVertices[VertIndex3].Position = p2;
		InVertices[VertIndex4].Position = p3;

		// Now create two triangles from those four vertices
		// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
		InTriangles[InTriangleIndex++] = VertIndex4;
		InTriangles[InTriangleIndex++] = VertIndex3;
		InTriangles[InTriangleIndex++] = VertIndex1;

		InTriangles[InTriangleIndex++] = VertIndex3;
		InTriangles[InTriangleIndex++] = VertIndex2;
		InTriangles[InTriangleIndex++] = VertIndex1;

		// UVs.  Note that Unreal UV origin (0,0) is top left
		InVertices[VertIndex1].UV0 = FVector2D(1.0f - (UMapPerQuad * QuadIndex), 1.0f);
		InVertices[VertIndex2].UV0 = FVector2D(1.0f - (UMapPerQuad * (QuadIndex + 1)), 1.0f);
		InVertices[VertIndex3].UV0 = FVector2D(1.0f - (UMapPerQuad * (QuadIndex + 1)), 0.0f);
		InVertices[VertIndex4].UV0 = FVector2D(1.0f - (UMapPerQuad * QuadIndex), 0.0f);

		// Normals
		FVector NormalCurrent = FVector::CrossProduct(InVertices[VertIndex1].Position - InVertices[VertIndex3].Position, InVertices[VertIndex2].Position - InVertices[VertIndex3].Position).GetSafeNormal();

		if (bInSmoothNormals)
		{
			// To smooth normals we give the vertices different values than the polygon they belong to.
			// GPUs know how to interpolate between those.
			// I do this here as an average between normals of two adjacent polygons
			float NextNextAngle = (float)(QuadIndex + 2) * AngleBetweenQuads;
			FVector p4 = (CachedCrossSectionPoints[QuadIndex + 2] * InWidth) + StartOffset;
			p4 = RotatePointAroundPivot(p4, StartPoint, RotationAngle);

			// p1 to p4 to p2
			FVector NormalNext = FVector::CrossProduct(p1 - p2, p4 - p2).GetSafeNormal();
			FVector AverageNormalRight = (NormalCurrent + NormalNext) / 2;
			AverageNormalRight = AverageNormalRight.GetSafeNormal();

			float PreviousAngle = (float)(QuadIndex - 1) * AngleBetweenQuads;
			FVector pMinus1 = FVector(FMath::Cos(PreviousAngle) * InWidth, FMath::Sin(PreviousAngle) * InWidth, 0.f) + StartOffset;
			pMinus1 = RotatePointAroundPivot(pMinus1, StartPoint, RotationAngle);

			// p0 to p3 to pMinus1
			FVector NormalPrevious = FVector::CrossProduct(p0 - pMinus1, p3 - pMinus1).GetSafeNormal();
			FVector AverageNormalLeft = (NormalCurrent + NormalPrevious) / 2;
			AverageNormalLeft = AverageNormalLeft.GetSafeNormal();

			InVertices[VertIndex1].Normal = FPackedNormal(AverageNormalLeft);
			InVertices[VertIndex2].Normal = FPackedNormal(AverageNormalRight);
			InVertices[VertIndex3].Normal = FPackedNormal(AverageNormalRight);
			InVertices[VertIndex4].Normal = FPackedNormal(AverageNormalLeft);
		}
		else
		{
			// If not smoothing we just set the vertex normal to the same normal as the polygon they belong to
			InVertices[VertIndex1].Normal = InVertices[VertIndex2].Normal = InVertices[VertIndex3].Normal = InVertices[VertIndex4].Normal = FPackedNormal(NormalCurrent);
		}

		// Tangents (perpendicular to the surface)
		FVector SurfaceTangent = p0 - p1;
		SurfaceTangent = SurfaceTangent.GetSafeNormal();
		InVertices[VertIndex1].Tangent = InVertices[VertIndex2].Tangent = InVertices[VertIndex3].Tangent = InVertices[VertIndex4].Tangent = FPackedNormal(SurfaceTangent);
	}
}


#if WITH_EDITOR
void ABranchingLinesActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);

	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, TrunkWidth))
		|| (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, WidthReductionOnFork))
		|| (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, bSmoothNormals))
		|| (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, BranchOffsetReductionEachGenerationPercentage))
		|| (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, ForkLengthMin))
		|| (MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, ForkLengthMax)))
	{
		// Same vert count, so just regen mesh with same buffers
		PreCacheCrossSection();
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ABranchingLinesActor, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
	// Pretty much everything else changes the vert counts
	else
	{
		// Vertice count has changed, so reset buffer and then regen mesh
		PreCacheCrossSection();
		Vertices.Empty();
		Triangles.Empty();
		bHaveBuffersBeenInitialized = false;
		GenerateMesh();
	}
	
}
#endif // WITH_EDITOR
