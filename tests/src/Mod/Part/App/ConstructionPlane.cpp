// SPDX-License-Identifier: LGPL-2.1-or-later

#include <gtest/gtest.h>
#include "PartTestHelpers.h"

#include <src/App/InitApplication.h>
#include <App/Document.h>
#include <Mod/Part/App/ConstructionPlane.h>
#include <Mod/Part/App/PrimitiveFeature.h>

using namespace Attacher;
using namespace Part::ConstructionPlane;

namespace
{
const eRefType plane = eRefType(rtFlatFace | rtFlagHasPlacement);
const eRefType datumLine = eRefType(rtLine | rtFlagHasPlacement);
const eRefType sketch = eRefType(rtWire | rtFlagHasPlacement);
}  // namespace

TEST(ConstructionPlane, ClassifyOneReference)
{
    EXPECT_EQ(classify({rtFlatFace}), Type::Offset);
    EXPECT_EQ(classify({plane}), Type::Offset);
    EXPECT_EQ(classify({sketch}), Type::Offset);
    EXPECT_EQ(classify({rtLine}), Type::Angle);
    EXPECT_EQ(classify({datumLine}), Type::Angle);
    EXPECT_EQ(classify({rtCylindricalFace}), Type::Tangent);
    EXPECT_EQ(classify({rtConicalFace}), Type::Tangent);
    EXPECT_EQ(classify({rtCircle}), Type::AlongPath);
    EXPECT_EQ(classify({rtVertex}), std::nullopt);
    EXPECT_EQ(classify({rtSphericalFace}), std::nullopt);
}

TEST(ConstructionPlane, ClassifySeveralReferences)
{
    EXPECT_EQ(classify({rtFlatFace, plane}), Type::MidPlane);
    EXPECT_EQ(classify({rtLine, rtLine}), Type::TwoEdges);
    EXPECT_EQ(classify({rtFlatFace, rtLine}), Type::Angle);
    EXPECT_EQ(classify({rtFlatFace, rtCylindricalFace}), Type::Tangent);
    EXPECT_EQ(classify({rtCylindricalFace, rtVertex}), Type::TangentAtPoint);
    EXPECT_EQ(classify({rtVertex, rtFlatFace}), Type::TangentAtPoint);
    EXPECT_EQ(classify({rtCurve, rtVertex}), Type::AlongPath);
    EXPECT_EQ(classify({rtVertex, rtVertex, rtVertex}), Type::ThreePoints);
    EXPECT_EQ(classify({rtVertex, rtVertex}), std::nullopt);
    EXPECT_EQ(classify({rtVertex, rtVertex, rtVertex, rtVertex}), std::nullopt);
    EXPECT_EQ(classify({}), std::nullopt);
}

TEST(ConstructionPlane, ReferenceOrder)
{
    EXPECT_EQ(referenceOrder(Type::Angle, {rtFlatFace, rtLine}), (std::vector<int> {1, 0}));
    EXPECT_EQ(referenceOrder(Type::Angle, {rtLine, rtFlatFace}), (std::vector<int> {0, 1}));
    EXPECT_EQ(referenceOrder(Type::Tangent, {plane, rtConicalFace}), (std::vector<int> {1, 0}));
    EXPECT_EQ(referenceOrder(Type::AlongPath, {rtVertex, rtCurve}), (std::vector<int> {1, 0}));
    EXPECT_EQ(referenceOrder(Type::MidPlane, {rtFlatFace, plane}), (std::vector<int> {0, 1}));
}

TEST(ConstructionPlane, MapModeAndTypeMatch)
{
    const std::vector<std::vector<eRefType>> examples {
        {rtFlatFace},
        {plane},
        {rtLine},
        {rtLine, rtFlatFace},
        {rtCylindricalFace},
        {rtFlatFace, rtFlatFace},
        {rtLine, rtLine},
        {rtVertex, rtVertex, rtVertex},
        {rtFlatFace, rtVertex},
        {rtCurve},
    };
    for (const auto& refs : examples) {
        auto type = classify(refs);
        ASSERT_TRUE(type);
        EXPECT_EQ(typeOf(mapMode(*type, refs), refs), type);
        EXPECT_LE(static_cast<int>(refs.size()), maxReferences(*type));
        for (auto ref : refs) {
            EXPECT_TRUE(acceptsReference(*type, ref));
        }
    }
    EXPECT_EQ(mapMode(Type::Offset, {rtFlatFace}), mmFlatFace);
    EXPECT_EQ(mapMode(Type::Offset, {plane}), mmObjectXY);
    EXPECT_EQ(typeOf(mmFolding, {rtLine, rtLine, rtLine, rtLine}), std::nullopt);
}

TEST(ConstructionPlane, AcceptsReference)
{
    EXPECT_FALSE(acceptsReference(Type::Offset, rtLine));
    EXPECT_FALSE(acceptsReference(Type::Offset, datumLine));
    EXPECT_FALSE(acceptsReference(Type::MidPlane, rtCylindricalFace));
    EXPECT_FALSE(acceptsReference(Type::TwoEdges, rtCircle));
    EXPECT_TRUE(acceptsReference(Type::TangentAtPoint, rtSphericalFace));
}

class ConstructionPlaneShapes: public ::testing::Test, public PartTestHelpers::PartTestHelperClass
{
protected:
    static void SetUpTestSuite()
    {
        tests::initApplication();
    }

    void SetUp() override
    {
        createTestDoc();
    }

    void TearDown() override
    {
        App::GetApplication().closeDocument(_docName.c_str());
    }
};

TEST_F(ConstructionPlaneShapes, ClassifyShapes)
{
    auto cylinder = _doc->addObject<Part::Cylinder>();
    cylinder->recomputeFeature();
    _boxes[0]->recomputeFeature();
    auto type = [](App::DocumentObject* obj, const char* sub) {
        return AttachEngine::getShapeType(obj, sub);
    };

    EXPECT_EQ(classify({type(_boxes[0], "Face1")}), Type::Offset);
    EXPECT_EQ(classify({type(_boxes[0], "Edge1")}), Type::Angle);
    EXPECT_EQ(classify({type(_boxes[0], "Face1"), type(_boxes[0], "Face2")}), Type::MidPlane);
    EXPECT_EQ(classify({type(cylinder, "Face1")}), Type::Tangent);
    for (const char* edge : {"Edge1", "Edge2", "Edge3"}) {
        auto ref = type(cylinder, edge);
        EXPECT_EQ(classify({ref}), ref == rtCircle ? Type::AlongPath : Type::Angle) << edge;
    }
    EXPECT_EQ(classify({type(_boxes[0], "")}), Type::Offset);
}
