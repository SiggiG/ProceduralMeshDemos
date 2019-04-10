// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski tetrahedron

#include "ProceduralMeshesPrivatePCH.h"
#include "SierpinskiTetrahedron.h"


ASierpinskiTetrahedron::ASierpinskiTetrahedron()
{
	RootNode = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = RootNode;

	MeshComponent = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("ProceduralMesh"));
	MeshComponent->GetOrCreateRuntimeMesh()->SetShouldSerializeMeshData(false);
	MeshComponent->SetupAttachment(RootComponent);
}

// This is called when actor is spawned (at runtime or when you drop it into the world in editor)
void ASierpinskiTetrahedron::PostActorCreated()
{
	Super::PostActorCreated();
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void ASierpinskiTetrahedron::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
}

void ASierpinskiTetrahedron::SetupMeshBuffers()
{
	int32 TotalNumberOfTetrahedrons = FPlatformMath::RoundToInt(FMath::Pow(4.0f, Iterations + 1));
	int32 NumberOfVerticesPerTetrahedrons = 4 * 3; // 4 sides of 3 points each
	int32 NumberOfTriangleIndexesPerTetrahedrons = 4 * 3; // 4 sides of 3 points each
	Vertices.AddUninitialized(TotalNumberOfTetrahedrons * NumberOfVerticesPerTetrahedrons);
	Triangles.AddUninitialized(TotalNumberOfTetrahedrons * NumberOfTriangleIndexesPerTetrahedrons);
}

void ASierpinskiTetrahedron::GenerateMesh()
{
	// The number of vertices or polygons wont change at runtime, so we'll just allocate the arrays once
	if (!bHaveBuffersBeenInitialized)
	{
		SetupMeshBuffers();
		bHaveBuffersBeenInitialized = true;
	}

	// -------------------------------------------------------
	// Start by setting the four points that define a tetrahedron
	// 0,0 is center bottom.. so first two are offset half Size to the sides, and the 3rd straight up
	FVector BottomLeftPoint = FVector(0, -0.5f * Size, 0);
	FVector BottomRightPoint = FVector(0, 0.5f * Size, 0);
	float ThirdBasePointDistance = FMath::Sqrt(3) * Size / 2;
	FVector BottomMiddlePoint = FVector(ThirdBasePointDistance, 0, 0);
	float CenterPosX = FMath::Tan(FMath::DegreesToRadians(30)) * (Size / 2.0f);
	FVector TopPoint = FVector(CenterPosX, 0, ThirdBasePointDistance);

	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	// Start by defining the initial tetrahedron and starting the subdivision
	FirstTetrahedron = FTetrahedronStructure(BottomLeftPoint, BottomRightPoint, BottomMiddlePoint, TopPoint);
	PrecalculateTetrahedronSideQuads();
	GenerateTetrahedron(FirstTetrahedron, 0, Vertices, Triangles, VertexIndex, TriangleIndex);

	// Find bounding box
	float Height = FMath::Sqrt(3) * Size / 2;
	float HalfSize = Size / 2;
	FBox BoundingBox = FBox(FVector(0, -HalfSize, 0), FVector(Height, HalfSize, Height));

	MeshComponent->ClearAllMeshSections();
	MeshComponent->CreateMeshSection(0, Vertices, Triangles, BoundingBox, false, EUpdateFrequency::Infrequent);
	MeshComponent->SetMaterial(0, Material);
}

void ASierpinskiTetrahedron::GenerateTetrahedron(const FTetrahedronStructure& Tetrahedron, int32 InDepth, TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, int32& VertexIndex, int32& TriangleIndex)
{
	if (InDepth > Iterations)
	{
		return;
	}

	// Now we subdivide the current tetrahedron into 4 new ones: Front left, Back middle, Front Right and Top.
	// The corners of these are defined by existing points and points midway between those

	// Calculate the mid points between the extents of the current tetrahedron
	FVector FrontLeftMidPoint = ((Tetrahedron.CornerBottomLeft - Tetrahedron.CornerTop) * 0.5f) + Tetrahedron.CornerTop;
	FVector FrontRightMidPoint = ((Tetrahedron.CornerBottomRight - Tetrahedron.CornerTop) * 0.5f) + Tetrahedron.CornerTop;
	FVector FrontBottomMidPoint = ((Tetrahedron.CornerBottomLeft - Tetrahedron.CornerBottomRight) * 0.5f) + Tetrahedron.CornerBottomRight;

	FVector MiddleMidPointUp = ((Tetrahedron.CornerBottomMiddle - Tetrahedron.CornerTop) * 0.5f) + Tetrahedron.CornerTop;
	FVector BottomLeftMidPoint = ((Tetrahedron.CornerBottomMiddle - Tetrahedron.CornerBottomLeft) * 0.5f) + Tetrahedron.CornerBottomLeft;
	FVector BottomRightMidPoint = ((Tetrahedron.CornerBottomMiddle - Tetrahedron.CornerBottomRight) * 0.5f) + Tetrahedron.CornerBottomRight;

	// Define 4x new tetrahedrons:
	// We UV map them by defining a quad for each side with UV coords from 0,0 to 1.1, then projecting each point on to that quad and figuring out its UV based on its position
	// ** Tetrahedron 1 (front left)
	FTetrahedronStructure LeftTetrahedron = FTetrahedronStructure(Tetrahedron.CornerBottomLeft, FrontBottomMidPoint, BottomLeftMidPoint, FrontLeftMidPoint);
	LeftTetrahedron.FrontFaceLeftPoint.UV0 = GetUVForSide(LeftTetrahedron.FrontFaceLeftPoint.Position, ETetrahedronSide::Front);
	LeftTetrahedron.FrontFaceRightPoint.UV0 = GetUVForSide(LeftTetrahedron.FrontFaceRightPoint.Position, ETetrahedronSide::Front);
	LeftTetrahedron.FrontFaceTopPoint.UV0 = GetUVForSide(LeftTetrahedron.FrontFaceTopPoint.Position, ETetrahedronSide::Front);
	LeftTetrahedron.LeftFaceLeftPoint.UV0 = GetUVForSide(LeftTetrahedron.LeftFaceLeftPoint.Position, ETetrahedronSide::Left);
	LeftTetrahedron.LeftFaceRightPoint.UV0 = GetUVForSide(LeftTetrahedron.LeftFaceRightPoint.Position, ETetrahedronSide::Left);
	LeftTetrahedron.LeftFaceTopPoint.UV0 = GetUVForSide(LeftTetrahedron.LeftFaceTopPoint.Position, ETetrahedronSide::Left);
	LeftTetrahedron.RightFaceLeftPoint.UV0 = GetUVForSide(LeftTetrahedron.RightFaceLeftPoint.Position, ETetrahedronSide::Right);
	LeftTetrahedron.RightFaceRightPoint.UV0 = GetUVForSide(LeftTetrahedron.RightFaceRightPoint.Position, ETetrahedronSide::Right);
	LeftTetrahedron.RightFaceTopPoint.UV0 = GetUVForSide(LeftTetrahedron.RightFaceTopPoint.Position, ETetrahedronSide::Right);
	LeftTetrahedron.BottomFaceLeftPoint.UV0 = GetUVForSide(LeftTetrahedron.BottomFaceLeftPoint.Position, ETetrahedronSide::Bottom);
	LeftTetrahedron.BottomFaceRightPoint.UV0 = GetUVForSide(LeftTetrahedron.BottomFaceRightPoint.Position, ETetrahedronSide::Bottom);
	LeftTetrahedron.BottomFaceTopPoint.UV0 = GetUVForSide(LeftTetrahedron.BottomFaceTopPoint.Position, ETetrahedronSide::Bottom);

	// ** Tetrahedron 2 (back middle)
	FTetrahedronStructure MiddleTetrahedron = FTetrahedronStructure(BottomLeftMidPoint, BottomRightMidPoint, Tetrahedron.CornerBottomMiddle, MiddleMidPointUp);
	MiddleTetrahedron.FrontFaceLeftPoint.UV0 = GetUVForSide(MiddleTetrahedron.FrontFaceLeftPoint.Position, ETetrahedronSide::Front);
	MiddleTetrahedron.FrontFaceRightPoint.UV0 = GetUVForSide(MiddleTetrahedron.FrontFaceRightPoint.Position, ETetrahedronSide::Front);
	MiddleTetrahedron.FrontFaceTopPoint.UV0 = GetUVForSide(MiddleTetrahedron.FrontFaceTopPoint.Position, ETetrahedronSide::Front);
	MiddleTetrahedron.LeftFaceLeftPoint.UV0 = GetUVForSide(MiddleTetrahedron.LeftFaceLeftPoint.Position, ETetrahedronSide::Left);
	MiddleTetrahedron.LeftFaceRightPoint.UV0 = GetUVForSide(MiddleTetrahedron.LeftFaceRightPoint.Position, ETetrahedronSide::Left);
	MiddleTetrahedron.LeftFaceTopPoint.UV0 = GetUVForSide(MiddleTetrahedron.LeftFaceTopPoint.Position, ETetrahedronSide::Left);
	MiddleTetrahedron.RightFaceLeftPoint.UV0 = GetUVForSide(MiddleTetrahedron.RightFaceLeftPoint.Position, ETetrahedronSide::Right);
	MiddleTetrahedron.RightFaceRightPoint.UV0 = GetUVForSide(MiddleTetrahedron.RightFaceRightPoint.Position, ETetrahedronSide::Right);
	MiddleTetrahedron.RightFaceTopPoint.UV0 = GetUVForSide(MiddleTetrahedron.RightFaceTopPoint.Position, ETetrahedronSide::Right);
	MiddleTetrahedron.BottomFaceLeftPoint.UV0 = GetUVForSide(MiddleTetrahedron.BottomFaceLeftPoint.Position, ETetrahedronSide::Bottom);
	MiddleTetrahedron.BottomFaceRightPoint.UV0 = GetUVForSide(MiddleTetrahedron.BottomFaceRightPoint.Position, ETetrahedronSide::Bottom);
	MiddleTetrahedron.BottomFaceTopPoint.UV0 = GetUVForSide(MiddleTetrahedron.BottomFaceTopPoint.Position, ETetrahedronSide::Bottom);

	// ** Tetrahedron 3 (front right)
	FTetrahedronStructure RightTetrahedron = FTetrahedronStructure(FrontBottomMidPoint, Tetrahedron.CornerBottomRight, BottomRightMidPoint, FrontRightMidPoint);
	RightTetrahedron.FrontFaceLeftPoint.UV0 = GetUVForSide(RightTetrahedron.FrontFaceLeftPoint.Position, ETetrahedronSide::Front);
	RightTetrahedron.FrontFaceRightPoint.UV0 = GetUVForSide(RightTetrahedron.FrontFaceRightPoint.Position, ETetrahedronSide::Front);
	RightTetrahedron.FrontFaceTopPoint.UV0 = GetUVForSide(RightTetrahedron.FrontFaceTopPoint.Position, ETetrahedronSide::Front);
	RightTetrahedron.LeftFaceLeftPoint.UV0 = GetUVForSide(RightTetrahedron.LeftFaceLeftPoint.Position, ETetrahedronSide::Left);
	RightTetrahedron.LeftFaceRightPoint.UV0 = GetUVForSide(RightTetrahedron.LeftFaceRightPoint.Position, ETetrahedronSide::Left);
	RightTetrahedron.LeftFaceTopPoint.UV0 = GetUVForSide(RightTetrahedron.LeftFaceTopPoint.Position, ETetrahedronSide::Left);
	RightTetrahedron.RightFaceLeftPoint.UV0 = GetUVForSide(RightTetrahedron.RightFaceLeftPoint.Position, ETetrahedronSide::Right);
	RightTetrahedron.RightFaceRightPoint.UV0 = GetUVForSide(RightTetrahedron.RightFaceRightPoint.Position, ETetrahedronSide::Right);
	RightTetrahedron.RightFaceTopPoint.UV0 = GetUVForSide(RightTetrahedron.RightFaceTopPoint.Position, ETetrahedronSide::Right);
	RightTetrahedron.BottomFaceLeftPoint.UV0 = GetUVForSide(RightTetrahedron.BottomFaceLeftPoint.Position, ETetrahedronSide::Bottom);
	RightTetrahedron.BottomFaceRightPoint.UV0 = GetUVForSide(RightTetrahedron.BottomFaceRightPoint.Position, ETetrahedronSide::Bottom);
	RightTetrahedron.BottomFaceTopPoint.UV0 = GetUVForSide(RightTetrahedron.BottomFaceTopPoint.Position, ETetrahedronSide::Bottom);

	// ** Tetrahedron 4 (top)
	FTetrahedronStructure TopTetrahedron = FTetrahedronStructure(FrontLeftMidPoint, FrontRightMidPoint, MiddleMidPointUp, Tetrahedron.CornerTop);
	TopTetrahedron.FrontFaceLeftPoint.UV0 = GetUVForSide(TopTetrahedron.FrontFaceLeftPoint.Position, ETetrahedronSide::Front);
	TopTetrahedron.FrontFaceRightPoint.UV0 = GetUVForSide(TopTetrahedron.FrontFaceRightPoint.Position, ETetrahedronSide::Front);
	TopTetrahedron.FrontFaceTopPoint.UV0 = GetUVForSide(TopTetrahedron.FrontFaceTopPoint.Position, ETetrahedronSide::Front);
	TopTetrahedron.LeftFaceLeftPoint.UV0 = GetUVForSide(TopTetrahedron.LeftFaceLeftPoint.Position, ETetrahedronSide::Left);
	TopTetrahedron.LeftFaceRightPoint.UV0 = GetUVForSide(TopTetrahedron.LeftFaceRightPoint.Position, ETetrahedronSide::Left);
	TopTetrahedron.LeftFaceTopPoint.UV0 = GetUVForSide(TopTetrahedron.LeftFaceTopPoint.Position, ETetrahedronSide::Left);
	TopTetrahedron.RightFaceLeftPoint.UV0 = GetUVForSide(TopTetrahedron.RightFaceLeftPoint.Position, ETetrahedronSide::Right);
	TopTetrahedron.RightFaceRightPoint.UV0 = GetUVForSide(TopTetrahedron.RightFaceRightPoint.Position, ETetrahedronSide::Right);
	TopTetrahedron.RightFaceTopPoint.UV0 = GetUVForSide(TopTetrahedron.RightFaceTopPoint.Position, ETetrahedronSide::Right);
	TopTetrahedron.BottomFaceLeftPoint.UV0 = GetUVForSide(TopTetrahedron.BottomFaceLeftPoint.Position, ETetrahedronSide::Bottom);
	TopTetrahedron.BottomFaceRightPoint.UV0 = GetUVForSide(TopTetrahedron.BottomFaceRightPoint.Position, ETetrahedronSide::Bottom);
	TopTetrahedron.BottomFaceTopPoint.UV0 = GetUVForSide(TopTetrahedron.BottomFaceTopPoint.Position, ETetrahedronSide::Bottom);

	if (InDepth == Iterations)
	{
		// Last iteration, here we generate the meshes!
		// ** Tetrahedron 1 (front left)
		AddPolygon(LeftTetrahedron.BottomFaceLeftPoint, LeftTetrahedron.BottomFaceRightPoint, LeftTetrahedron.BottomFaceTopPoint, LeftTetrahedron.BottomFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(LeftTetrahedron.FrontFaceLeftPoint, LeftTetrahedron.FrontFaceRightPoint, LeftTetrahedron.FrontFaceTopPoint, LeftTetrahedron.FrontFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(LeftTetrahedron.LeftFaceLeftPoint, LeftTetrahedron.LeftFaceRightPoint, LeftTetrahedron.LeftFaceTopPoint, LeftTetrahedron.LeftFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(LeftTetrahedron.RightFaceLeftPoint, LeftTetrahedron.RightFaceRightPoint, LeftTetrahedron.RightFaceTopPoint, LeftTetrahedron.RightFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);

		// ** Tetrahedron 2 (back middle)
		AddPolygon(MiddleTetrahedron.BottomFaceLeftPoint, MiddleTetrahedron.BottomFaceRightPoint, MiddleTetrahedron.BottomFaceTopPoint, MiddleTetrahedron.BottomFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(MiddleTetrahedron.FrontFaceLeftPoint, MiddleTetrahedron.FrontFaceRightPoint, MiddleTetrahedron.FrontFaceTopPoint, MiddleTetrahedron.FrontFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(MiddleTetrahedron.LeftFaceLeftPoint, MiddleTetrahedron.LeftFaceRightPoint, MiddleTetrahedron.LeftFaceTopPoint, MiddleTetrahedron.LeftFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(MiddleTetrahedron.RightFaceLeftPoint, MiddleTetrahedron.RightFaceRightPoint, MiddleTetrahedron.RightFaceTopPoint, MiddleTetrahedron.RightFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);

		// ** Tetrahedron 3 (front right)
		AddPolygon(RightTetrahedron.BottomFaceLeftPoint, RightTetrahedron.BottomFaceRightPoint, RightTetrahedron.BottomFaceTopPoint, RightTetrahedron.BottomFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(RightTetrahedron.FrontFaceLeftPoint, RightTetrahedron.FrontFaceRightPoint, RightTetrahedron.FrontFaceTopPoint, RightTetrahedron.FrontFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(RightTetrahedron.LeftFaceLeftPoint, RightTetrahedron.LeftFaceRightPoint, RightTetrahedron.LeftFaceTopPoint, RightTetrahedron.LeftFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(RightTetrahedron.RightFaceLeftPoint, RightTetrahedron.RightFaceRightPoint, RightTetrahedron.RightFaceTopPoint, RightTetrahedron.RightFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);

		// ** Tetrahedron 4 (top)
		AddPolygon(TopTetrahedron.BottomFaceLeftPoint, TopTetrahedron.BottomFaceRightPoint, TopTetrahedron.BottomFaceTopPoint, TopTetrahedron.BottomFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(TopTetrahedron.FrontFaceLeftPoint, TopTetrahedron.FrontFaceRightPoint, TopTetrahedron.FrontFaceTopPoint, TopTetrahedron.FrontFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(TopTetrahedron.LeftFaceLeftPoint, TopTetrahedron.LeftFaceRightPoint, TopTetrahedron.LeftFaceTopPoint, TopTetrahedron.LeftFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
		AddPolygon(TopTetrahedron.RightFaceLeftPoint, TopTetrahedron.RightFaceRightPoint, TopTetrahedron.RightFaceTopPoint, TopTetrahedron.RightFaceNormal, InVertices, InTriangles, VertexIndex, TriangleIndex);
	}
	else
	{
		// Keep subdividing
		GenerateTetrahedron(LeftTetrahedron, InDepth + 1, InVertices, InTriangles, VertexIndex, TriangleIndex);
		GenerateTetrahedron(RightTetrahedron, InDepth + 1, InVertices, InTriangles, VertexIndex, TriangleIndex);
		GenerateTetrahedron(MiddleTetrahedron, InDepth + 1, InVertices, InTriangles, VertexIndex, TriangleIndex);
		GenerateTetrahedron(TopTetrahedron, InDepth + 1, InVertices, InTriangles, VertexIndex, TriangleIndex);
	}
}

void ASierpinskiTetrahedron::AddPolygon(const FRuntimeMeshVertexSimple& Point1, const FRuntimeMeshVertexSimple& Point2, const FRuntimeMeshVertexSimple& Point3, FVector FaceNormal, TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, int32& VertexIndex, int32& TriangleIndex)
{
	int32 Point1Index, Point2Index, Point3Index;

	// Reserve indexes and assign the vertices
	Point1Index = VertexIndex++;
	Point2Index = VertexIndex++;
	Point3Index = VertexIndex++;
	InVertices[Point1Index] = Point1;
	InVertices[Point2Index] = Point2;
	InVertices[Point3Index] = Point3;

	// Triangle
	// Tip: If you add vertices on a polygon in a counter-clockwise order, the polygon will face "towards you".
	InTriangles[TriangleIndex++] = Point1Index;
	InTriangles[TriangleIndex++] = Point2Index;
	InTriangles[TriangleIndex++] = Point3Index;

	// Normals
	InVertices[Point1Index].Normal = InVertices[Point2Index].Normal = InVertices[Point3Index].Normal = FPackedNormal(FaceNormal);

	// Tangents (perpendicular to the surface)
	FVector SurfaceTangent = InVertices[Point1Index].Position - InVertices[Point2Index].Position;
	SurfaceTangent = SurfaceTangent.GetSafeNormal();
	InVertices[Point1Index].Tangent = InVertices[Point2Index].Tangent = InVertices[Point3Index].Tangent = FPackedNormal(SurfaceTangent);
}

FVector2D ASierpinskiTetrahedron::GetUVForSide(FVector Point, ETetrahedronSide Side)
{
	if (Side == ETetrahedronSide::Front)
	{
		FVector VectorToProject = FrontQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(FrontQuadTopSide).Size() / FrontQuadTopSide.Size(), VectorToProject.ProjectOnTo(FrontQuadLeftSide).Size() / FrontQuadLeftSide.Size());
	}
	else if (Side == ETetrahedronSide::Left)
	{
		FVector VectorToProject = LeftQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(LeftQuadTopSide).Size() / LeftQuadTopSide.Size(), VectorToProject.ProjectOnTo(LeftQuadLeftSide).Size() / LeftQuadLeftSide.Size());
	}
	else if (Side == ETetrahedronSide::Right)
	{
		FVector VectorToProject = RightQuadTopLeftPoint - Point;
		return FVector2D(VectorToProject.ProjectOnTo(RightQuadTopSide).Size() / RightQuadTopSide.Size(), VectorToProject.ProjectOnTo(RightQuadLeftSide).Size() / RightQuadLeftSide.Size());
	}
	else if (Side == ETetrahedronSide::Bottom)
	{
		FVector VectorToProject = BottomQuadTopLeftPoint - Point;
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
	FVector FrontPlaneBottomLeft = FirstTetrahedron.FrontFaceLeftPoint.Position;
	FVector FrontPlaneBottomRight = FirstTetrahedron.FrontFaceRightPoint.Position;
	FVector FrontBottomMidPoint = ((FrontPlaneBottomLeft - FrontPlaneBottomRight) * 0.5f) + FrontPlaneBottomRight;
	FVector FrontPlaneTopRight = FrontPlaneBottomRight + (FirstTetrahedron.FrontFaceTopPoint.Position - FrontBottomMidPoint);
	FrontQuadTopLeftPoint = FrontPlaneBottomLeft + (FirstTetrahedron.FrontFaceTopPoint.Position - FrontBottomMidPoint);
	FrontQuadTopSide = FrontPlaneTopRight - FrontQuadTopLeftPoint;
	FrontQuadLeftSide = FrontPlaneBottomLeft - FrontQuadTopLeftPoint;

	// Left side
	FVector LeftPlaneBottomLeft = FirstTetrahedron.LeftFaceLeftPoint.Position;
	FVector LeftPlaneBottomRight = FirstTetrahedron.LeftFaceRightPoint.Position;
	FVector LeftBottomMidPoint = ((LeftPlaneBottomLeft - LeftPlaneBottomRight) * 0.5f) + LeftPlaneBottomRight;
	FVector LeftPlaneTopRight = LeftPlaneBottomRight + (FirstTetrahedron.LeftFaceTopPoint.Position - LeftBottomMidPoint);
	LeftQuadTopLeftPoint = LeftPlaneBottomLeft + (FirstTetrahedron.LeftFaceTopPoint.Position - LeftBottomMidPoint);
	LeftQuadTopSide = LeftPlaneTopRight - LeftQuadTopLeftPoint;
	LeftQuadLeftSide = LeftPlaneBottomLeft - LeftQuadTopLeftPoint;

	// Right side
	FVector RightPlaneBottomLeft = FirstTetrahedron.RightFaceLeftPoint.Position;
	FVector RightPlaneBottomRight = FirstTetrahedron.RightFaceRightPoint.Position;
	FVector RightBottomMidPoint = ((RightPlaneBottomLeft - RightPlaneBottomRight) * 0.5f) + RightPlaneBottomRight;
	FVector RightPlaneTopRight = RightPlaneBottomRight + (FirstTetrahedron.RightFaceTopPoint.Position - RightBottomMidPoint);
	RightQuadTopLeftPoint = RightPlaneBottomLeft + (FirstTetrahedron.RightFaceTopPoint.Position - RightBottomMidPoint);
	RightQuadTopSide = RightPlaneTopRight - RightQuadTopLeftPoint;
	RightQuadLeftSide = RightPlaneBottomLeft - RightQuadTopLeftPoint;

	// Bottom side
	FVector BottomPlaneBottomLeft = FirstTetrahedron.BottomFaceLeftPoint.Position;
	FVector BottomPlaneBottomRight = FirstTetrahedron.BottomFaceRightPoint.Position;
	FVector BottomBottomMidPoint = ((BottomPlaneBottomLeft - BottomPlaneBottomRight) * 0.5f) + BottomPlaneBottomRight;
	FVector BottomPlaneTopRight = BottomPlaneBottomRight + (FirstTetrahedron.BottomFaceTopPoint.Position - BottomBottomMidPoint);
	BottomQuadTopLeftPoint = BottomPlaneBottomLeft + (FirstTetrahedron.BottomFaceTopPoint.Position - BottomBottomMidPoint);
	BottomQuadTopSide = BottomPlaneTopRight - BottomQuadTopLeftPoint;
	BottomQuadLeftSide = BottomPlaneBottomLeft - BottomQuadTopLeftPoint;
}

#if WITH_EDITOR
void ASierpinskiTetrahedron::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

 	FName MemberPropertyChanged = (PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None);
 
 	if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASierpinskiTetrahedron, Size)))
 	{
 		// Same vert count, so just regen mesh with same buffers
 		GenerateMesh();
 	}
 	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASierpinskiTetrahedron, Iterations)))
	{
		// Vertice count has changed, so reset buffer and then regen mesh
		Vertices.Empty();
		Triangles.Empty();
		bHaveBuffersBeenInitialized = false;
		GenerateMesh();
	}
	else if ((MemberPropertyChanged == GET_MEMBER_NAME_CHECKED(ASierpinskiTetrahedron, Material)))
	{
		MeshComponent->SetMaterial(0, Material);
	}
}
#endif // WITH_EDITOR
