// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski tetrahedron

#include "SierpinskiTetrahedron.h"
#include "Providers/RuntimeMeshProviderStatic.h"


ASierpinskiTetrahedron::ASierpinskiTetrahedron()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticProvider = CreateDefaultSubobject<URuntimeMeshProviderStatic>(TEXT("RuntimeMeshProvider-Static"));
	StaticProvider->SetSerializeFlag(false);
}

void ASierpinskiTetrahedron::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateMesh();
}

void ASierpinskiTetrahedron::SetupMeshBuffers()
{
	const int32 TotalNumberOfTetrahedrons = FPlatformMath::RoundToInt(FMath::Pow(4.0f, Iterations + 1));
	const int32 NumberOfVerticesPerTetrahedrons = 4 * 3; // 4 sides of 3 points each
	const int32 NumberOfTriangleIndexesPerTetrahedrons = 4 * 3; // 4 sides of 3 points each
	const int32 VertexCount = TotalNumberOfTetrahedrons * NumberOfVerticesPerTetrahedrons;
	const int32 TriangleCount = TotalNumberOfTetrahedrons * NumberOfTriangleIndexesPerTetrahedrons;

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

void ASierpinskiTetrahedron::GenerateMesh()
{
	GetRuntimeMeshComponent()->Initialize(StaticProvider);
	StaticProvider->ClearSection(0, 0);
	SetupMeshBuffers();

	// -------------------------------------------------------
	// Start by setting the four points that define a tetrahedron
	// 0,0 is center bottom.. so first two are offset half Size to the sides, and the 3rd straight up
	const FVector BottomLeftPoint = FVector(0, -0.5f * Size, 0);
	const FVector BottomRightPoint = FVector(0, 0.5f * Size, 0);
	const float ThirdBasePointDistance = FMath::Sqrt(3) * Size / 2;
	const FVector BottomMiddlePoint = FVector(ThirdBasePointDistance, 0, 0);
	const float CenterPosX = FMath::Tan(FMath::DegreesToRadians(30)) * (Size / 2.0f);
	const FVector TopPoint = FVector(CenterPosX, 0, ThirdBasePointDistance);

	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	// Start by defining the initial tetrahedron and starting the subdivision
	FirstTetrahedron = FTetrahedronStructure(BottomLeftPoint, BottomRightPoint, BottomMiddlePoint, TopPoint);
	PrecalculateTetrahedronSideQuads();
	GenerateTetrahedron(FirstTetrahedron, 0, Positions, Triangles, Normals, Tangents, TexCoords, VertexIndex, TriangleIndex);

	const TArray<FColor> EmptyColors{};
	StaticProvider->CreateSectionFromComponents(0, 0, 0, Positions, Triangles, Normals, TexCoords, EmptyColors, Tangents, ERuntimeMeshUpdateFrequency::Infrequent, false);
	StaticProvider->SetupMaterialSlot(0, TEXT("PyramidMaterial"), Material);
}

void ASierpinskiTetrahedron::GenerateTetrahedron(const FTetrahedronStructure& Tetrahedron, int32 InDepth, TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, int32& VertexIndex, int32& TriangleIndex) const
{
	if (InDepth > Iterations)
	{
		return;
	}

	// Now we subdivide the current tetrahedron into 4 new ones: Front left, Back middle, Front Right and Top.
	// The corners of these are defined by existing points and points midway between those

	// Calculate the mid points between the extents of the current tetrahedron
	const FVector FrontLeftMidPoint = ((Tetrahedron.CornerBottomLeft - Tetrahedron.CornerTop) * 0.5f) + Tetrahedron.CornerTop;
	const FVector FrontRightMidPoint = ((Tetrahedron.CornerBottomRight - Tetrahedron.CornerTop) * 0.5f) + Tetrahedron.CornerTop;
	const FVector FrontBottomMidPoint = ((Tetrahedron.CornerBottomLeft - Tetrahedron.CornerBottomRight) * 0.5f) + Tetrahedron.CornerBottomRight;

	const FVector MiddleMidPointUp = ((Tetrahedron.CornerBottomMiddle - Tetrahedron.CornerTop) * 0.5f) + Tetrahedron.CornerTop;
	const FVector BottomLeftMidPoint = ((Tetrahedron.CornerBottomMiddle - Tetrahedron.CornerBottomLeft) * 0.5f) + Tetrahedron.CornerBottomLeft;
	const FVector BottomRightMidPoint = ((Tetrahedron.CornerBottomMiddle - Tetrahedron.CornerBottomRight) * 0.5f) + Tetrahedron.CornerBottomRight;

	// Define 4x new tetrahedrons:
	// We UV map them by defining a quad for each side with UV coords from 0,0 to 1.1, then projecting each point on to that quad and figuring out its UV based on its position
	// ** Tetrahedron 1 (front left)
	FTetrahedronStructure LeftTetrahedron = FTetrahedronStructure(Tetrahedron.CornerBottomLeft, FrontBottomMidPoint, BottomLeftMidPoint, FrontLeftMidPoint);
	SetTetrahedronUV(LeftTetrahedron);

	// ** Tetrahedron 2 (back middle)
	FTetrahedronStructure MiddleTetrahedron = FTetrahedronStructure(BottomLeftMidPoint, BottomRightMidPoint, Tetrahedron.CornerBottomMiddle, MiddleMidPointUp);
	SetTetrahedronUV(MiddleTetrahedron);

	// ** Tetrahedron 3 (front right)
	FTetrahedronStructure RightTetrahedron = FTetrahedronStructure(FrontBottomMidPoint, Tetrahedron.CornerBottomRight, BottomRightMidPoint, FrontRightMidPoint);
	SetTetrahedronUV(RightTetrahedron);

	// ** Tetrahedron 4 (top)
	FTetrahedronStructure TopTetrahedron = FTetrahedronStructure(FrontLeftMidPoint, FrontRightMidPoint, MiddleMidPointUp, Tetrahedron.CornerTop);
	SetTetrahedronUV(TopTetrahedron);

	if (InDepth == Iterations)
	{
		// Last iteration, here we generate the meshes!
		// ** Tetrahedron 1 (front left)
		AddTetrahedronPolygons(LeftTetrahedron, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
		
		// ** Tetrahedron 2 (back middle)
		AddTetrahedronPolygons(MiddleTetrahedron, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
		
		// ** Tetrahedron 3 (front right)
		AddTetrahedronPolygons(RightTetrahedron, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);

		// ** Tetrahedron 4 (top)
		AddTetrahedronPolygons(TopTetrahedron, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
	}
	else
	{
		// Keep subdividing
		GenerateTetrahedron(LeftTetrahedron, InDepth + 1, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
		GenerateTetrahedron(RightTetrahedron, InDepth + 1, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
		GenerateTetrahedron(MiddleTetrahedron, InDepth + 1, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
		GenerateTetrahedron(TopTetrahedron, InDepth + 1, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
	}
}

void ASierpinskiTetrahedron::AddTetrahedronPolygons(const FTetrahedronStructure& Tetrahedron, TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, int32& VertexIndex, int32& TriangleIndex)
{
	AddPolygon(Tetrahedron.BottomFaceLeftPoint, Tetrahedron.BottomFaceLeftPointUV, Tetrahedron.BottomFaceRightPoint, Tetrahedron.BottomFaceRightPointUV, Tetrahedron.BottomFaceTopPoint, Tetrahedron.BottomFaceTopPointUV, Tetrahedron.BottomFaceNormal, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
	AddPolygon(Tetrahedron.FrontFaceLeftPoint, Tetrahedron.FrontFaceLeftPointUV, Tetrahedron.FrontFaceRightPoint, Tetrahedron.FrontFaceRightPointUV, Tetrahedron.FrontFaceTopPoint, Tetrahedron.FrontFaceTopPointUV, Tetrahedron.FrontFaceNormal, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
	AddPolygon(Tetrahedron.LeftFaceLeftPoint, Tetrahedron.LeftFaceLeftPointUV, Tetrahedron.LeftFaceRightPoint, Tetrahedron.LeftFaceRightPointUV, Tetrahedron.LeftFaceTopPoint, Tetrahedron.LeftFaceTopPointUV, Tetrahedron.LeftFaceNormal, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
	AddPolygon(Tetrahedron.RightFaceLeftPoint, Tetrahedron.RightFaceLeftPointUV, Tetrahedron.RightFaceRightPoint, Tetrahedron.RightFaceRightPointUV, Tetrahedron.RightFaceTopPoint, Tetrahedron.RightFaceTopPointUV, Tetrahedron.RightFaceNormal, InVertices, InTriangles, InNormals, InTangents, InTexCoords, VertexIndex, TriangleIndex);
}

void ASierpinskiTetrahedron::AddPolygon(const FVector& Point1, const FVector2D& Point1UV, const FVector& Point2, const FVector2D& Point2UV, const FVector& Point3, const FVector2D& Point3UV, const FVector FaceNormal, TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, int32& VertexIndex, int32& TriangleIndex)
{
	// Reserve indexes and assign the vertices
	const int32 Point1Index = VertexIndex++;
	const int32 Point2Index = VertexIndex++;
	const int32 Point3Index = VertexIndex++;
	InVertices[Point1Index] = Point1;
	InVertices[Point2Index] = Point2;
	InVertices[Point3Index] = Point3;

	// Triangle
	// Tip: If you add vertices on a polygon in a counter-clockwise order, the polygon will face "towards you".
	InTriangles[TriangleIndex++] = Point1Index;
	InTriangles[TriangleIndex++] = Point2Index;
	InTriangles[TriangleIndex++] = Point3Index;

	// UVs
	InTexCoords[Point1Index] = Point1UV;
	InTexCoords[Point2Index] = Point2UV;
	InTexCoords[Point3Index] = Point3UV;

	// Normals
	InNormals[Point1Index] = InNormals[Point2Index] = InNormals[Point3Index] = FaceNormal;

	// Tangents (perpendicular to the surface)
	const FVector SurfaceTangent = (InVertices[Point1Index] - InVertices[Point2Index]).GetSafeNormal();
	InTangents[Point1Index] = InTangents[Point2Index] = InTangents[Point3Index] = SurfaceTangent;
}

void ASierpinskiTetrahedron::SetTetrahedronUV(FTetrahedronStructure& Tetrahedron) const
{
	Tetrahedron.FrontFaceLeftPointUV = GetUVForSide(Tetrahedron.FrontFaceLeftPoint, ETetrahedronSide::Front);
	Tetrahedron.FrontFaceRightPointUV = GetUVForSide(Tetrahedron.FrontFaceRightPoint, ETetrahedronSide::Front);
	Tetrahedron.FrontFaceTopPointUV = GetUVForSide(Tetrahedron.FrontFaceTopPoint, ETetrahedronSide::Front);
	Tetrahedron.LeftFaceLeftPointUV = GetUVForSide(Tetrahedron.LeftFaceLeftPoint, ETetrahedronSide::Left);
	Tetrahedron.LeftFaceRightPointUV = GetUVForSide(Tetrahedron.LeftFaceRightPoint, ETetrahedronSide::Left);
	Tetrahedron.LeftFaceTopPointUV = GetUVForSide(Tetrahedron.LeftFaceTopPoint, ETetrahedronSide::Left);
	Tetrahedron.RightFaceLeftPointUV = GetUVForSide(Tetrahedron.RightFaceLeftPoint, ETetrahedronSide::Right);
	Tetrahedron.RightFaceRightPointUV = GetUVForSide(Tetrahedron.RightFaceRightPoint, ETetrahedronSide::Right);
	Tetrahedron.RightFaceTopPointUV = GetUVForSide(Tetrahedron.RightFaceTopPoint, ETetrahedronSide::Right);
	Tetrahedron.BottomFaceLeftPointUV = GetUVForSide(Tetrahedron.BottomFaceLeftPoint, ETetrahedronSide::Bottom);
	Tetrahedron.BottomFaceRightPointUV = GetUVForSide(Tetrahedron.BottomFaceRightPoint, ETetrahedronSide::Bottom);
	Tetrahedron.BottomFaceTopPointUV = GetUVForSide(Tetrahedron.BottomFaceTopPoint, ETetrahedronSide::Bottom);
}

FVector2D ASierpinskiTetrahedron::GetUVForSide(const FVector Point, const ETetrahedronSide Side) const
{
	if (Side == ETetrahedronSide::Front)
	{
		const FVector VectorToProject = FrontQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(FrontQuadTopSide).Size() / FrontQuadTopSide.Size(), VectorToProject.ProjectOnTo(FrontQuadLeftSide).Size() / FrontQuadLeftSide.Size());
	}
	else if (Side == ETetrahedronSide::Left)
	{
		const FVector VectorToProject = LeftQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(LeftQuadTopSide).Size() / LeftQuadTopSide.Size(), VectorToProject.ProjectOnTo(LeftQuadLeftSide).Size() / LeftQuadLeftSide.Size());
	}
	else if (Side == ETetrahedronSide::Right)
	{
		const FVector VectorToProject = RightQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(RightQuadTopSide).Size() / RightQuadTopSide.Size(), VectorToProject.ProjectOnTo(RightQuadLeftSide).Size() / RightQuadLeftSide.Size());
	}
	else if (Side == ETetrahedronSide::Bottom)
	{
		const FVector VectorToProject = BottomQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(BottomQuadTopSide).Size() / BottomQuadTopSide.Size(), VectorToProject.ProjectOnTo(BottomQuadLeftSide).Size() / BottomQuadLeftSide.Size());
	}
	else
	{
		return FVector2D(0, 0);
	}
}

void ASierpinskiTetrahedron::PrecalculateTetrahedronSideQuads()
{
	// Normally we would project the target point on to a plane defined by the quad like the commented lines below, but in this case its faster to project the target vectors directly on to vectors that define the edge of the target side quad
	// 		Example: FPlane FrontPlaneLocalSpace = FPlane(FirstTetrahedron.FrontFaceLeftPoint.Position, FirstTetrahedron.FrontFaceRightPoint.Position, FirstTetrahedron.FrontFaceTopPoint.Position);
	// 		Example: FVector ProjectedPointLocal = FVector::PointPlaneProject(Point, FrontPlaneLocalSpace);

	// Front side
	const FVector FrontPlaneBottomLeft = FirstTetrahedron.FrontFaceLeftPoint;
	const FVector FrontPlaneBottomRight = FirstTetrahedron.FrontFaceRightPoint;
	const FVector FrontBottomMidPoint = ((FrontPlaneBottomLeft - FrontPlaneBottomRight) * 0.5f) + FrontPlaneBottomRight;
	const FVector FrontPlaneTopRight = FrontPlaneBottomRight + (FirstTetrahedron.FrontFaceTopPoint - FrontBottomMidPoint);
	FrontQuadTopLeftPoint = FrontPlaneBottomLeft + (FirstTetrahedron.FrontFaceTopPoint - FrontBottomMidPoint);
	FrontQuadTopSide = FrontPlaneTopRight - FrontQuadTopLeftPoint;
	FrontQuadLeftSide = FrontPlaneBottomLeft - FrontQuadTopLeftPoint;

	// Left side
	const FVector LeftPlaneBottomLeft = FirstTetrahedron.LeftFaceLeftPoint;
	const FVector LeftPlaneBottomRight = FirstTetrahedron.LeftFaceRightPoint;
	const FVector LeftBottomMidPoint = ((LeftPlaneBottomLeft - LeftPlaneBottomRight) * 0.5f) + LeftPlaneBottomRight;
	const FVector LeftPlaneTopRight = LeftPlaneBottomRight + (FirstTetrahedron.LeftFaceTopPoint - LeftBottomMidPoint);
	LeftQuadTopLeftPoint = LeftPlaneBottomLeft + (FirstTetrahedron.LeftFaceTopPoint - LeftBottomMidPoint);
	LeftQuadTopSide = LeftPlaneTopRight - LeftQuadTopLeftPoint;
	LeftQuadLeftSide = LeftPlaneBottomLeft - LeftQuadTopLeftPoint;

	// Right side
	const FVector RightPlaneBottomLeft = FirstTetrahedron.RightFaceLeftPoint;
	const FVector RightPlaneBottomRight = FirstTetrahedron.RightFaceRightPoint;
	const FVector RightBottomMidPoint = ((RightPlaneBottomLeft - RightPlaneBottomRight) * 0.5f) + RightPlaneBottomRight;
	const FVector RightPlaneTopRight = RightPlaneBottomRight + (FirstTetrahedron.RightFaceTopPoint - RightBottomMidPoint);
	RightQuadTopLeftPoint = RightPlaneBottomLeft + (FirstTetrahedron.RightFaceTopPoint - RightBottomMidPoint);
	RightQuadTopSide = RightPlaneTopRight - RightQuadTopLeftPoint;
	RightQuadLeftSide = RightPlaneBottomLeft - RightQuadTopLeftPoint;

	// Bottom side
	const FVector BottomPlaneBottomLeft = FirstTetrahedron.BottomFaceLeftPoint;
	const FVector BottomPlaneBottomRight = FirstTetrahedron.BottomFaceRightPoint;
	const FVector BottomBottomMidPoint = ((BottomPlaneBottomLeft - BottomPlaneBottomRight) * 0.5f) + BottomPlaneBottomRight;
	const FVector BottomPlaneTopRight = BottomPlaneBottomRight + (FirstTetrahedron.BottomFaceTopPoint - BottomBottomMidPoint);
	BottomQuadTopLeftPoint = BottomPlaneBottomLeft + (FirstTetrahedron.BottomFaceTopPoint - BottomBottomMidPoint);
	BottomQuadTopSide = BottomPlaneTopRight - BottomQuadTopLeftPoint;
	BottomQuadLeftSide = BottomPlaneBottomLeft - BottomQuadTopLeftPoint;
}
