// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski pyramid using cylinder lines

#include "SierpinskiLineActor.h"
#include "Providers/RuntimeMeshProviderStatic.h"

ASierpinskiLineActor::ASierpinskiLineActor()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticProvider = CreateDefaultSubobject<URuntimeMeshProviderStatic>(TEXT("RuntimeMeshProvider-Static"));
	StaticProvider->SetSerializeFlag(false);
}

void ASierpinskiLineActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PreCacheCrossSection();
	GenerateLines();
	GenerateMesh();
}

void ASierpinskiLineActor::SetupMeshBuffers()
{
	const int32 TotalNumberOfVerticesPerSection = RadialSegmentCount * 4; // 4 verts per face 
	const int32 TotalNumberOfTrianglesPerSection = TotalNumberOfVerticesPerSection + 2 * RadialSegmentCount;
	const int32 VertexCount = TotalNumberOfVerticesPerSection * Lines.Num();
	const int32 TriangleCount = TotalNumberOfTrianglesPerSection * Lines.Num();
	
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

void ASierpinskiLineActor::GenerateMesh()
{
	GetRuntimeMeshComponent()->Initialize(StaticProvider);
	StaticProvider->ClearSection(0, 0);
	SetupMeshBuffers();

	// -------------------------------------------------------
	// Now lets loop through all the defined lines of the pyramid and create a cylinder for each
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 i = 0; i < Lines.Num(); i++)
	{
		GenerateCylinder(Positions, Triangles, Normals, Tangents, TexCoords, Lines[i].Start, Lines[i].End, Lines[i].Width, RadialSegmentCount, VertexIndex, TriangleIndex, bSmoothNormals);
	}
	
	const TArray<FColor> EmptyColors{};
	StaticProvider->CreateSectionFromComponents(0, 0, 0, Positions, Triangles, Normals, TexCoords, EmptyColors, Tangents, ERuntimeMeshUpdateFrequency::Infrequent, false);
	StaticProvider->SetupMaterialSlot(0, TEXT("PyramidMaterial"), Material);
}

FVector ASierpinskiLineActor::RotatePointAroundPivot(const FVector InPoint, const FVector InPivot, const FVector InAngles)
{
	FVector Direction = InPoint - InPivot; // get point direction relative to pivot
	Direction = FQuat::MakeFromEuler(InAngles) * Direction; // rotate it
	return Direction + InPivot; // calculate rotated point
}

void ASierpinskiLineActor::PreCacheCrossSection()
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

void ASierpinskiLineActor::GenerateLines()
{
	Lines.Empty();

	// -------------------------------------------------------
	// Start by setting the four points that define a pyramid
	// 0,0 is center bottom.. so first two are offset half Size to the sides, and the 3rd straight up
	const FVector BottomLeftPoint = FVector(0, -0.5f * Size, 0);
	const FVector BottomRightPoint = FVector(0, 0.5f * Size, 0);
	const float ThirdBasePointDistance = FMath::Sqrt(3) * Size / 2;
	const FVector BottomMiddlePoint = FVector(ThirdBasePointDistance, 0, 0);
	const float CenterPosX = FMath::Tan(FMath::DegreesToRadians(30)) * (Size / 2.0f);
	const FVector TopPoint = FVector(CenterPosX, 0, ThirdBasePointDistance);

	// Then create all the lines between those 4 points
	Lines.Add(FPyramidLine(BottomLeftPoint, BottomRightPoint, LineThickness));
	Lines.Add(FPyramidLine(BottomRightPoint, TopPoint, LineThickness));
	Lines.Add(FPyramidLine(TopPoint, BottomLeftPoint, LineThickness));

	Lines.Add(FPyramidLine(BottomLeftPoint, BottomMiddlePoint, LineThickness));
	Lines.Add(FPyramidLine(BottomMiddlePoint, BottomRightPoint, LineThickness));
	Lines.Add(FPyramidLine(BottomMiddlePoint, TopPoint, LineThickness));

	// -------------------------------------------------------
	// Create the rest of the lines through recursion
	AddSection(BottomLeftPoint, TopPoint, BottomRightPoint, BottomMiddlePoint, 1);
}

void ASierpinskiLineActor::AddSection(const FVector InBottomLeftPoint, const FVector InTopPoint, const FVector InBottomRightPoint, const FVector InBottomMiddlePoint, const int32 InDepth)
{
	if (InDepth > Iterations)
	{
		return;
	}

	// First side
	FVector Side1LeftPoint = InTopPoint - InBottomLeftPoint;
	Side1LeftPoint = (Side1LeftPoint * 0.5f) + InBottomLeftPoint;
	FVector Side1RightPoint = InBottomRightPoint - InTopPoint;
	Side1RightPoint = (Side1RightPoint * 0.5f) + InTopPoint;
	FVector Side1BottomPoint = InBottomLeftPoint - InBottomRightPoint;
	Side1BottomPoint = (Side1BottomPoint * 0.5f) + InBottomRightPoint;

	// Points Towards Middle
	FVector MiddlePointUp = InBottomMiddlePoint - InTopPoint;
	MiddlePointUp = (MiddlePointUp * 0.5f) + InTopPoint;
	FVector BottomLeftPoint = InBottomMiddlePoint - InBottomLeftPoint;
	BottomLeftPoint = (BottomLeftPoint * 0.5f) + InBottomLeftPoint;
	FVector BottomRightPoint = InBottomMiddlePoint - InBottomRightPoint;
	BottomRightPoint = (BottomRightPoint * 0.5f) + InBottomRightPoint;

	// Find new thickness
	const float NewThickness = LineThickness * FMath::Pow(ThicknessMultiplierPerGeneration, InDepth);

	// First side
	Lines.Add(FPyramidLine(Side1LeftPoint, Side1RightPoint, NewThickness));
	Lines.Add(FPyramidLine(Side1RightPoint, Side1BottomPoint, NewThickness));
	Lines.Add(FPyramidLine(Side1BottomPoint, Side1LeftPoint, NewThickness));

	// Second side
	Lines.Add(FPyramidLine(BottomLeftPoint, Side1LeftPoint, NewThickness));
	Lines.Add(FPyramidLine(BottomLeftPoint, MiddlePointUp, NewThickness));
	Lines.Add(FPyramidLine(Side1LeftPoint, MiddlePointUp, NewThickness));

	// Third side
	Lines.Add(FPyramidLine(BottomRightPoint, Side1RightPoint, NewThickness));
	Lines.Add(FPyramidLine(BottomRightPoint, MiddlePointUp, NewThickness));
	Lines.Add(FPyramidLine(Side1RightPoint, MiddlePointUp, NewThickness));

	// Fourth side (bottom)
	Lines.Add(FPyramidLine(Side1BottomPoint, BottomLeftPoint, NewThickness));
	Lines.Add(FPyramidLine(Side1BottomPoint, BottomRightPoint, NewThickness));
	Lines.Add(FPyramidLine(BottomLeftPoint, BottomRightPoint, NewThickness));

	AddSection(InBottomLeftPoint, Side1LeftPoint, Side1BottomPoint, BottomLeftPoint, InDepth + 1); // Lower left pyramid
	AddSection(Side1LeftPoint, InTopPoint, Side1RightPoint, MiddlePointUp, InDepth + 1); // Top pyramid
	AddSection(Side1BottomPoint, Side1RightPoint, InBottomRightPoint, BottomRightPoint, InDepth + 1); // Lower right pyramid
	AddSection(BottomLeftPoint, MiddlePointUp, BottomRightPoint, InBottomMiddlePoint, InDepth + 1); // Lower middle pyramid
}

void ASierpinskiLineActor::GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector StartPoint, const FVector EndPoint, const float InWidth, const int32 InCrossSectionCount, int32& VertexIndex, int32& TriangleIndex, const bool bInSmoothNormals/* = true*/)
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
		const int32 VertIndex1 = VertexIndex++;
		const int32 VertIndex2 = VertexIndex++;
		const int32 VertIndex3 = VertexIndex++;
		const int32 VertIndex4 = VertexIndex++;

		InVertices[VertIndex1] = P0;
		InVertices[VertIndex2] = P1;
		InVertices[VertIndex3] = P2;
		InVertices[VertIndex4] = P3;

		// Now create two triangles from those four vertices
		// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
		InTriangles[TriangleIndex++] = VertIndex4;
		InTriangles[TriangleIndex++] = VertIndex3;
		InTriangles[TriangleIndex++] = VertIndex1;

		InTriangles[TriangleIndex++] = VertIndex3;
		InTriangles[TriangleIndex++] = VertIndex2;
		InTriangles[TriangleIndex++] = VertIndex1;

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
		const FVector SurfaceTangent = (P0 - P1).GetSafeNormal();
		InTangents[VertIndex1] = InTangents[VertIndex2] = InTangents[VertIndex3] = InTangents[VertIndex4] = SurfaceTangent;
	}
}
