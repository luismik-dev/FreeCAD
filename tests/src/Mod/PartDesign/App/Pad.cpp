// SPDX-License-Identifier: LGPL-2.1-or-later

#include <gtest/gtest.h>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include "src/App/InitApplication.h"

#include <App/Application.h>
#include <App/Document.h>
#include <Mod/Part/App/Geometry.h>
#include <Mod/PartDesign/App/Body.h>
#include <Mod/PartDesign/App/FeaturePad.h>
#include <Mod/Sketcher/App/SketchObject.h>

// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)

class PadTest: public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        tests::initApplication();
    }

    void SetUp() override
    {
        _doc = App::GetApplication().newDocument("Pad_test", "testUser");
        _body = _doc->addObject<PartDesign::Body>();
        _sketch = _doc->addObject<Sketcher::SketchObject>("Sketch");
        _body->addObject(_sketch);

        _sketch->AttachmentSupport.setValue(_doc->getObject("XY_Plane"), "");
        _sketch->MapMode.setValue("FlatFace");
        Part::GeomCircle circle;
        circle.setRadius(10.0);
        _sketch->addGeometry(&circle, false);
    }

    void TearDown() override
    {
        App::GetApplication().closeDocument(_doc->getName());
    }

    App::Document* getDocument() const
    {
        return _doc;
    }

    PartDesign::Body* getBody() const
    {
        return _body;
    }

    Sketcher::SketchObject* getSketch() const
    {
        return _sketch;
    }

private:
    App::Document* _doc = nullptr;
    PartDesign::Body* _body = nullptr;
    Sketcher::SketchObject* _sketch = nullptr;
};

TEST_F(PadTest, TestMidPlaneTwoLength)
{
    auto doc = getDocument();
    auto body = getBody();
    auto sketch = getSketch();

    doc->recompute();

    auto pad = doc->addObject<PartDesign::Pad>("Pad");
    body->addObject(pad);
    pad->Profile.setValue(sketch, {""});
    pad->Direction.setValue(0.0, 0.0, 1.0);
    pad->Midplane.setValue(true);
    pad->Length.setValue(10.0);
    pad->Length2.setValue(20.0);

    pad->SideType.setValue("Two sides");

    doc->recompute();

    auto bbox = pad->Shape.getBoundingBox();

    EXPECT_DOUBLE_EQ(bbox.MaxX, 10.0);
    EXPECT_DOUBLE_EQ(bbox.MinX, -10.0);
    EXPECT_DOUBLE_EQ(bbox.MaxY, 10.0);
    EXPECT_DOUBLE_EQ(bbox.MinY, -10.0);
    EXPECT_DOUBLE_EQ(bbox.MaxZ, 10.0);
    EXPECT_DOUBLE_EQ(bbox.MinZ, -20.0);
}

class PadRegionsTest: public PadTest
{
protected:
    void SetUp() override
    {
        PadTest::SetUp();
        // Replace the circle by a square inside a square, giving two closed regions
        auto sketch = getSketch();
        sketch->deleteAllGeometry();
        sketch->MakeInternals.setValue(true);
        addSquare(0.0, 20.0);
        addSquare(5.0, 10.0);
        getDocument()->recompute();
    }

    void addSquare(double corner, double size)
    {
        const Base::Vector3d p1(corner, corner, 0.0);
        const Base::Vector3d p2(corner + size, corner, 0.0);
        const Base::Vector3d p3(corner + size, corner + size, 0.0);
        const Base::Vector3d p4(corner, corner + size, 0.0);
        for (const auto& [start, end] : {std::pair {p1, p2}, {p2, p3}, {p3, p4}, {p4, p1}}) {
            Part::GeomLineSegment line;
            line.setPoints(start, end);
            getSketch()->addGeometry(&line, false);
        }
    }

    PartDesign::Pad* addPad(const char* name, const std::vector<std::string>& subs)
    {
        auto pad = getDocument()->addObject<PartDesign::Pad>(name);
        getBody()->addObject(pad);
        pad->Profile.setValue(getSketch(), subs);
        pad->Length.setValue(10.0);
        return pad;
    }
};

TEST_F(PadRegionsTest, TestSketchWithoutUsersIsNotConsumed)
{
    EXPECT_FALSE(PartDesign::ProfileBased::isProfileFullyConsumed(getSketch()));
}

TEST_F(PadRegionsTest, TestWholeSketchIsConsumed)
{
    addPad("Pad", {});
    EXPECT_TRUE(PartDesign::ProfileBased::isProfileFullyConsumed(getSketch()));
}

TEST_F(PadRegionsTest, TestRegionsConsumedOneByOne)
{
    auto doc = getDocument();
    auto sketch = getSketch();

    auto pad = addPad("Pad", {"InternalFace1"});
    doc->recompute();
    EXPECT_FALSE(pad->isError());
    EXPECT_FALSE(PartDesign::ProfileBased::isProfileFullyConsumed(sketch));

    auto pad2 = addPad("Pad2", {"InternalFace2"});
    doc->recompute();
    EXPECT_FALSE(pad2->isError());
    EXPECT_TRUE(PartDesign::ProfileBased::isProfileFullyConsumed(sketch));
    // A feature about to be removed does not count
    EXPECT_FALSE(PartDesign::ProfileBased::isProfileFullyConsumed(sketch, pad2));

    // Both regions padded to the same length give the full block
    GProp_GProps properties;
    BRepGProp::VolumeProperties(pad2->Shape.getValue(), properties);
    EXPECT_NEAR(properties.Mass(), 4000.0, 1e-6);
}

// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)
