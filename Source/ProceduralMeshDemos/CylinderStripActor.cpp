// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cylinder strip mesh

#include "CylinderStripActor.h"

ACylinderStripActor::ACylinderStripActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void ACylinderStripActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PreCacheCrossSection();
	GenerateMesh();
}

void ACylinderStripActor::PostLoad()
{
	Super::PostLoad();
	PreCacheCrossSection();
	GenerateMesh();
}

void ACylinderStripActor::SetupMeshBuffers()
{
	const int32 TotalNumberOfVerticesPerSection = RadialSegmentCount * 4; // 4 verts per face 
	const int32 TotalNumberOfTrianglesPerSection = TotalNumberOfVerticesPerSection + 2 * RadialSegmentCount;
	const int32 NumberOfSections = LinePoints.Num() - 1;
	const int32 VertexCount = TotalNumberOfVerticesPerSection * NumberOfSections;
	const int32 TriangleCount = TotalNumberOfTrianglesPerSection * NumberOfSections;

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

void ACylinderStripActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->ClearAllMeshSections();

	if (LinePoints.Num() < 2)
	{
		return;
	}

	SetupMeshBuffers();

	// Now lets loop through all the defined points and create a cylinder between each two defined points
	const int32 NumberOfSections = LinePoints.Num() - 1;
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 i = 0; i < NumberOfSections; i++)
	{
		GenerateCylinder(Positions, Triangles, Normals, Tangents, TexCoords, LinePoints[i], LinePoints[i + 1], Radius, RadialSegmentCount, VertexIndex, TriangleIndex, bSmoothNormals);
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	MeshComponent->SetMaterial(0, Material);
}

FVector ACylinderStripActor::RotatePointAroundPivot(const FVector InPoint, const FVector InPivot, const FVector InAngles)
{
	FVector Direction = InPoint - InPivot; // get point direction relative to pivot
	Direction = FQuat::MakeFromEuler(InAngles) * Direction; // rotate it
	return Direction + InPivot; // calculate rotated point
}

void ACylinderStripActor::PreCacheCrossSection()
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
		const float Angle = (float)PointIndex * AngleBetweenQuads;
		CachedCrossSectionPoints.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0));
	}

	LastCachedCrossSectionCount = RadialSegmentCount;
}

void ACylinderStripActor::GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FProcMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector StartPoint, const FVector EndPoint, const float InWidth, const int32 InCrossSectionCount, int32& InVertexIndex, int32& InTriangleIndex, const bool bInSmoothNormals/* = true*/)
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
