// SPDX-License-Identifier: LGPL-2.1-or-later

#include <gtest/gtest.h>
#include "PartTestHelpers.h"

#include <src/App/InitApplication.h>
#include <App/Datums.h>
#include <App/Document.h>
#include <Mod/Part/App/Attacher.h>

#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>

using namespace Part;
using namespace Attacher;
using namespace PartTestHelpers;

/*
 * Testing note:  It looks like there are about 45 different attachment modes, and these tests
 * mostly only look at some of them - to prove that adding elementMap code doesn't break anything.
 * While a trivial bounding box test is used to ensure no hard crashes in any of the modes, any
 * mode that requires additional shapes beyond a couple of boxes would need a more comprehensive
 * test.
 */

class AttacherTest: public ::testing::Test, public PartTestHelpers::PartTestHelperClass
{
protected:
    static void SetUpTestSuite()
    {
        tests::initApplication();
    }

    void SetUp() override
    {
        createTestDoc();
        _boxes[1]->MapReversed.setValue(false);
        _boxes[1]->AttachmentSupport.setValue(_boxes[0]);
        _boxes[1]->MapPathParameter.setValue(0.0);
        _boxes[1]->MapMode.setValue("ObjectXY");  // There are lots of attachment modes!
        _boxes[1]->recomputeFeature();
    }

    void TearDown() override
    {
        App::GetApplication().closeDocument(_docName.c_str());
    }

    App::Document* getDocument() const
    {
        return _doc;
    }

protected:
};

TEST_F(AttacherTest, TestSetReferences)
{
    auto& attacher = _boxes[1]->attacher();
    EXPECT_EQ(attacher.getRefObjects().size(), 1);
}

TEST_F(AttacherTest, TestSuggestMapModes)
{
    auto& attacher = _boxes[1]->attacher();
    SuggestResult result;
    attacher.suggestMapModes(result);
    EXPECT_EQ(result.allApplicableModes.size(), 4);
    EXPECT_EQ(result.allApplicableModes[0], mmObjectXY);
    EXPECT_EQ(result.allApplicableModes[1], mmObjectXZ);
    EXPECT_EQ(result.allApplicableModes[2], mmObjectYZ);
    EXPECT_EQ(result.allApplicableModes[3], mmInertialCS);
}

TEST_F(AttacherTest, TestGetShapeType)
{
    auto& attacher = _boxes[1]->attacher();
    auto subObjects = _boxes[1]->getSubObjects();
    auto shapeType = attacher.getShapeType(_boxes[1], "Vertex2");
    EXPECT_EQ(shapeType, TopAbs_COMPSOLID);
}

TEST_F(AttacherTest, TestGetInertialPropsOfShape)
{
    auto& attacher = _boxes[1]->attacher();
    std::vector<const TopoShape*> result;
    auto faces = _boxes[1]->Shape.getShape().getSubTopoShapes(TopAbs_FACE);
    result.emplace_back(&faces[0]);
    auto shapeType = attacher.getInertialPropsOfShape(result);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(shapeType.Mass(), 6.0);
}

TEST_F(AttacherTest, TestGetRefObjects)
{
    auto& attacher = _boxes[1]->attacher();
    auto docObjects = attacher.getRefObjects();
    EXPECT_EQ(docObjects.size(), 1);
    EXPECT_STREQ(docObjects.front()->getNameInDocument(), "Part__Box");
}

TEST_F(AttacherTest, TestCalculateAttachedPlacement)
{
    auto& attacher = _boxes[1]->attacher();
    const Base::Placement orig;
    auto placement = attacher.calculateAttachedPlacement(orig);
    EXPECT_EQ(orig.getPosition().x, 0);
    EXPECT_EQ(orig.getPosition().y, 0);
    EXPECT_EQ(orig.getPosition().z, 0);
    EXPECT_EQ(placement.getPosition().x, 0);
    EXPECT_EQ(placement.getPosition().y, 0);
    EXPECT_EQ(placement.getPosition().z, 0);
}

TEST_F(AttacherTest, TestAllStringModesValid)
{
    // Arrange
    const char* modes[] = {
        "Deactivated",
        "Translate",
        "ObjectXY",
        "ObjectXZ",
        "ObjectYZ",
        "FlatFace",
        "TangentPlane",
        "NormalToEdge",
        "FrenetNB",
        "FrenetTN",
        "FrenetTB",
        "Concentric",
        "SectionOfRevolution",
        "ThreePointsPlane",
        "ThreePointsNormal",
        "Folding",

        "ObjectX",
        "ObjectY",
        "ObjectZ",
        "AxisOfCurvature",
        "Directrix1",
        "Directrix2",
        "Asymptote1",
        "Asymptote2",
        "Tangent",
        "Normal",
        "Binormal",
        "TangentU",
        "TangentV",
        "TwoPointLine",
        "IntersectionLine",
        "ProximityLine",

        "ObjectOrigin",
        "Focus1",
        "Focus2",
        "OnEdge",
        "CenterOfCurvature",
        "CenterOfMass",
        "IntersectionPoint",
        "Vertex",
        "ProximityPoint1",
        "ProximityPoint2",

        "AxisOfInertia1",
        "AxisOfInertia2",
        "AxisOfInertia3",

        "InertialCS",

        "FaceNormal",

        "OZX",
        "OZY",
        "OXY",
        "OXZ",
        "OYZ",
        "OYX",

        "ParallelPlane",
        "MidPoint",
        "MidPlane",
        "PlaneThroughLine",
    };
    int index = 0;
    for (auto mode : modes) {
        _boxes[1]->MapMode.setValue(mode);  // There are lots of attachment modes!
        _boxes[1]->recomputeFeature();
        EXPECT_STREQ(_boxes[1]->MapMode.getValueAsString(), mode);
        EXPECT_EQ(_boxes[1]->MapMode.getValue(), index);
        index++;
    }
}

TEST_F(AttacherTest, TestAllModesBoundaries)
{
    _boxes[1]->MapMode.setValue(mmTranslate);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 1, 2, 3)));
    _boxes[1]->MapMode.setValue(mmObjectXY);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 1, 2, 3)));
    _boxes[1]->MapMode.setValue(mmObjectXZ);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, -3, 0, 1, 0, 2)));
    _boxes[1]->MapMode.setValue(mmObjectYZ);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));

    _boxes[1]->MapMode.setValue(mmParallelPlane);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmFlatFace);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmTangentPlane);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Normal);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));

    _boxes[1]->MapMode.setValue(mmFrenetNB);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmFrenetTN);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmFrenetTB);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmConcentric);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmRevolutionSection);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmThreePointsNormal);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmThreePointsPlane);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mmFolding);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));

    _boxes[1]->MapMode.setValue(mm1AxisX);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1AxisY);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1AxisZ);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1AxisCurv);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Directrix1);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Directrix2);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Asymptote1);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Asymptote2);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Tangent);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1TangentU);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1TangentV);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1TwoPoints);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Intersection);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Proximity);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));

    _boxes[1]->MapMode.setValue(mm0Origin);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0Focus1);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0Focus2);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0OnEdge);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0CenterOfCurvature);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0CenterOfMass);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1Intersection);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0Vertex);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0ProximityPoint1);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm0ProximityPoint2);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));

    _boxes[1]->MapMode.setValue(mm1AxisInertia1);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1AxisInertia2);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));
    _boxes[1]->MapMode.setValue(mm1AxisInertia3);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0, 0, 0, 3, 1, 2)));

    _boxes[1]->MapMode.setValue(mmInertialCS);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );

    _boxes[1]->MapMode.setValue(mm1FaceNormal);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );

    _boxes[1]->MapMode.setValue(mmOZX);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );
    _boxes[1]->MapMode.setValue(mmOZY);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );
    _boxes[1]->MapMode.setValue(mmOXY);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );
    _boxes[1]->MapMode.setValue(mmOXZ);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );
    _boxes[1]->MapMode.setValue(mmOYZ);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );
    _boxes[1]->MapMode.setValue(mmOYX);
    _boxes[1]->recomputeFeature();
    EXPECT_TRUE(
        boxesMatch(_boxes[1]->Shape.getBoundingBox(), Base::BoundBox3d(0.5, 1, 1.5, 3.5, 2, 3.5))
    );
}

namespace
{
// Distance of a point from the XY plane of the placement
double distanceFromPlane(const Base::Placement& plm, const gp_Pnt& pnt)
{
    Base::Vector3d normal;
    plm.getRotation().multVec(Base::Vector3d(0, 0, 1), normal);
    Base::Vector3d point(pnt.X(), pnt.Y(), pnt.Z());
    return (point - plm.getPosition()).Dot(normal);
}
}  // namespace

TEST_F(AttacherTest, TestThreePointsPlaneThroughCornerEdges)
{
    // Arrange: all ordered pairs of box edges sharing a vertex
    const TopoShape& box = _boxes[0]->Shape.getShape();
    int edgeCount = box.countSubShapes(TopAbs_EDGE);
    int pairs = 0;
    for (int i = 1; i <= edgeCount; ++i) {
        for (int j = 1; j <= edgeCount; ++j) {
            std::string first = "Edge" + std::to_string(i);
            std::string second = "Edge" + std::to_string(j);
            TopoDS_Edge edge1 = TopoDS::Edge(box.getSubShape(first.c_str()));
            TopoDS_Edge edge2 = TopoDS::Edge(box.getSubShape(second.c_str()));
            TopoDS_Vertex common;
            if (i == j || !TopExp::CommonVertex(edge1, edge2, common)) {
                continue;
            }
            ++pairs;
            _boxes[1]->AttachmentSupport.setValue(_boxes[0], std::vector<std::string> {first, second});
            _boxes[1]->MapMode.setValue(mmThreePointsPlane);

            // Act
            _boxes[1]->recomputeFeature();

            // Assert: both edges lie in the plane
            EXPECT_FALSE(_boxes[1]->isError()) << first << " " << second;
            Base::Placement plm = _boxes[1]->Placement.getValue();
            for (const auto& edge : {edge1, edge2}) {
                for (const auto& vertex : {TopExp::FirstVertex(edge), TopExp::LastVertex(edge)}) {
                    EXPECT_NEAR(distanceFromPlane(plm, BRep_Tool::Pnt(vertex)), 0, 1e-7)
                        << first << " " << second;
                }
            }
        }
    }
    EXPECT_EQ(pairs, 48);  // 8 corners with 3 edges, 6 ordered pairs each
}

TEST_F(AttacherTest, TestThreePointsPlaneUnchangedForThreeVertices)
{
    // Arrange
    _boxes[1]->AttachmentSupport.setValue(
        _boxes[0],
        std::vector<std::string> {"Vertex1", "Vertex3", "Vertex5"}
    );
    _boxes[1]->MapMode.setValue(mmThreePointsPlane);

    // Act
    _boxes[1]->recomputeFeature();

    // Assert: the base point is the centroid of the points
    auto p0 = BRep_Tool::Pnt(TopoDS::Vertex(_boxes[0]->Shape.getShape().getSubShape("Vertex1")));
    auto p1 = BRep_Tool::Pnt(TopoDS::Vertex(_boxes[0]->Shape.getShape().getSubShape("Vertex3")));
    auto p2 = BRep_Tool::Pnt(TopoDS::Vertex(_boxes[0]->Shape.getShape().getSubShape("Vertex5")));
    Base::Vector3d centroid((p0.X() + p1.X() + p2.X()) / 3,
                            (p0.Y() + p1.Y() + p2.Y()) / 3,
                            (p0.Z() + p1.Z() + p2.Z()) / 3);
    EXPECT_FALSE(_boxes[1]->isError());
    EXPECT_TRUE(_boxes[1]->Placement.getValue().getPosition().IsEqual(centroid, 1e-7));
}

TEST_F(AttacherTest, TestMidPlaneOfParallelFaces)
{
    // Arrange: Face1 and Face2 of the 1 x 2 x 3 box are its faces at x = 0 and x = 1
    _boxes[1]->AttachmentSupport.setValue(_boxes[0], std::vector<std::string> {"Face1", "Face2"});
    _boxes[1]->MapMode.setValue(mmMidPlane);

    // Act
    _boxes[1]->recomputeFeature();

    // Assert
    Base::Placement plm = _boxes[1]->Placement.getValue();
    Base::Vector3d normal;
    plm.getRotation().multVec(Base::Vector3d(0, 0, 1), normal);
    EXPECT_FALSE(_boxes[1]->isError());
    EXPECT_NEAR(std::abs(normal.x), 1, 1e-7);
    EXPECT_NEAR(plm.getPosition().x, 0.5, 1e-7);
}

namespace
{
Base::Vector3d planeNormal(const Base::Placement& plm)
{
    Base::Vector3d normal;
    plm.getRotation().multVec(Base::Vector3d(0, 0, 1), normal);
    return normal;
}

// Name of the box edge from (0, 0, 0) to (0, 0, 3)
std::string verticalEdgeAtOrigin(const TopoShape& box)
{
    for (int i = 1; i <= box.countSubShapes(TopAbs_EDGE); ++i) {
        std::string name = "Edge" + std::to_string(i);
        TopoDS_Edge edge = TopoDS::Edge(box.getSubShape(name.c_str()));
        gp_Pnt p1 = BRep_Tool::Pnt(TopExp::FirstVertex(edge));
        gp_Pnt p2 = BRep_Tool::Pnt(TopExp::LastVertex(edge));
        if (p1.X() == 0 && p1.Y() == 0 && p2.X() == 0 && p2.Y() == 0) {
            return name;
        }
    }
    return {};
}
}  // namespace

TEST_F(AttacherTest, TestPlaneThroughBoxEdge)
{
    // Arrange
    std::string edge = verticalEdgeAtOrigin(_boxes[0]->Shape.getShape());
    ASSERT_FALSE(edge.empty());
    _boxes[1]->AttachmentSupport.setValue(_boxes[0], std::vector<std::string> {edge});
    _boxes[1]->MapMode.setValue(mmPlaneThroughLine);

    // Act
    _boxes[1]->recomputeFeature();

    // Assert: the plane contains the edge and is parallel to an adjacent face (x = 0 or y = 0)
    Base::Placement plm = _boxes[1]->Placement.getValue();
    Base::Vector3d normal = planeNormal(plm);
    EXPECT_FALSE(_boxes[1]->isError());
    EXPECT_TRUE(plm.getPosition().IsEqual(Base::Vector3d(0, 0, 1.5), 1e-7));
    EXPECT_NEAR(std::abs(normal.x) + std::abs(normal.y), 1, 1e-7);
    EXPECT_NEAR(normal.z, 0, 1e-7);
    Base::Vector3d xAxis;
    plm.getRotation().multVec(Base::Vector3d(1, 0, 0), xAxis);
    EXPECT_NEAR(std::abs(xAxis.z), 1, 1e-7);

    // Act: rotate about the line
    _boxes[1]->AttachmentOffset.setValue(
        Base::Placement(Base::Vector3d(), Base::Rotation(Base::Vector3d(1, 0, 0), M_PI / 2))
    );
    _boxes[1]->recomputeFeature();

    // Assert: still containing the edge, perpendicular to the plane at zero angle
    Base::Placement rotated = _boxes[1]->Placement.getValue();
    EXPECT_TRUE(rotated.getPosition().IsEqual(Base::Vector3d(0, 0, 1.5), 1e-7));
    EXPECT_NEAR(planeNormal(rotated).Dot(normal), 0, 1e-7);
    EXPECT_NEAR(planeNormal(rotated).z, 0, 1e-7);
}

TEST_F(AttacherTest, TestPlaneThroughLineParallelToFace)
{
    // Arrange: the top face of the box (z = 3) defines the plane at zero angle
    _boxes[1]->AttachmentSupport.setValues(
        std::vector<App::DocumentObject*> {_boxes[2], _boxes[0]},
        std::vector<std::string> {"Edge1", "Face6"}
    );
    _boxes[1]->MapMode.setValue(mmPlaneThroughLine);
    TopoDS_Edge line = TopoDS::Edge(_boxes[2]->Shape.getShape().getSubShape("Edge1"));
    gp_Pnt p1 = BRep_Tool::Pnt(TopExp::FirstVertex(line));
    gp_Pnt p2 = BRep_Tool::Pnt(TopExp::LastVertex(line));

    // Act
    _boxes[1]->recomputeFeature();

    // Assert
    Base::Placement plm = _boxes[1]->Placement.getValue();
    Base::Vector3d normal = planeNormal(plm);
    Base::Vector3d lineDir(p2.X() - p1.X(), p2.Y() - p1.Y(), p2.Z() - p1.Z());
    EXPECT_FALSE(_boxes[1]->isError());
    EXPECT_NEAR(normal.Dot(lineDir), 0, 1e-7);
    EXPECT_NEAR(distanceFromPlane(plm, p1), 0, 1e-7);
    EXPECT_NEAR(distanceFromPlane(plm, p2), 0, 1e-7);
}

TEST_F(AttacherTest, TestPlaneThroughDatumLine)
{
    // Arrange: a datum line along the global Y axis
    auto line = dynamic_cast<App::Line*>(getDocument()->addObject("App::Line", "Line"));
    ASSERT_TRUE(line);
    line->Placement.setValue(
        Base::Placement(Base::Vector3d(1, 0, 0), Base::Rotation(Base::Vector3d(0, 0, 1), M_PI / 2))
    );
    _boxes[1]->AttachmentSupport.setValue(line);
    _boxes[1]->MapMode.setValue(mmPlaneThroughLine);

    // Act
    _boxes[1]->recomputeFeature();

    // Assert: at zero angle the plane is the XY plane of the line, through its origin
    Base::Placement plm = _boxes[1]->Placement.getValue();
    EXPECT_FALSE(_boxes[1]->isError()) << _boxes[1]->getStatusString();
    EXPECT_TRUE(plm.getPosition().IsEqual(Base::Vector3d(1, 0, 0), 1e-7));
    EXPECT_NEAR(std::abs(planeNormal(plm).z), 1, 1e-7);
}

TEST_F(AttacherTest, TestSuggestModeForEdgeUnchanged)
{
    // Arrange
    std::string edge = verticalEdgeAtOrigin(_boxes[0]->Shape.getShape());
    _boxes[1]->AttachmentSupport.setValue(_boxes[0], std::vector<std::string> {edge});
    auto& attacher = _boxes[1]->attacher();
    SuggestResult result;

    // Act
    attacher.suggestMapModes(result);

    // Assert
    EXPECT_EQ(result.bestFitMode, mmNormalToPath);
    EXPECT_NE(
        std::find(result.allApplicableModes.begin(), result.allApplicableModes.end(), mmPlaneThroughLine),
        result.allApplicableModes.end()
    );
}
