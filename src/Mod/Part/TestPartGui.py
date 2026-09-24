# SPDX-License-Identifier: LGPL-2.1-or-later

# **************************************************************************
#   Copyright (c) 2011 Juergen Riegel <FreeCAD@juergen-riegel.net>        *
#                                                                         *
#   This file is part of the FreeCAD CAx development system.              *
#                                                                         *
#   This program is free software; you can redistribute it and/or modify  *
#   it under the terms of the GNU Lesser General Public License (LGPL)    *
#   as published by the Free Software Foundation; either version 2 of     *
#   the License, or (at your option) any later version.                   *
#   for detail see the LICENCE text file.                                 *
#                                                                         *
#   FreeCAD is distributed in the hope that it will be useful,            *
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#   GNU Library General Public License for more details.                  *
#                                                                         *
#   You should have received a copy of the GNU Library General Public     *
#   License along with FreeCAD; if not, write to the Free Software        *
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
#   USA                                                                   *
# **************************************************************************

import math
import os
import sys
import unittest
import FreeCAD
import FreeCADGui
import Part
import PartGui
import Sketcher
from PySide import QtWidgets


def findDockWidget(name):
    """Get a dock widget by name"""
    mw = FreeCADGui.getMainWindow()
    dws = mw.findChildren(QtWidgets.QDockWidget)
    for dw in dws:
        if dw.objectName() == name:
            return dw
    return None


"""
#---------------------------------------------------------------------------
# define the test cases to test the FreeCAD Part module
#---------------------------------------------------------------------------
"""
from parttests.ColorPerFaceTest import ColorPerFaceTest
from parttests.ColorTransparencyTest import ColorTransparencyTest
from parttests.TaskFaceAppearancesTest import TaskFaceAppearancesGuiTest


# class PartGuiTestCases(unittest.TestCase):
#    def setUp(self):
#        self.Doc = FreeCAD.newDocument("PartGuiTest")
#
#    def testBoxCase(self):
#        self.Box = self.Doc.addObject('Part::SketchObject','SketchBox')
#        self.Box.addGeometry(Part.LineSegment(App.Vector(-99.230339,36.960674,0),App.Vector(69.432587,36.960674,0)))
#        self.Box.addGeometry(Part.LineSegment(App.Vector(69.432587,36.960674,0),App.Vector(69.432587,-53.196629,0)))
#        self.Box.addGeometry(Part.LineSegment(App.Vector(69.432587,-53.196629,0),App.Vector(-99.230339,-53.196629,0)))
#        self.Box.addGeometry(Part.LineSegment(App.Vector(-99.230339,-53.196629,0),App.Vector(-99.230339,36.960674,0)))
#
#    def tearDown(self):
#        #closing doc
#        FreeCAD.closeDocument("PartGuiTest")
class PartGuiViewProviderTestCases(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("PartGuiTest")

    def testCanDropObject(self):
        # https://github.com/FreeCAD/FreeCAD/pull/6850
        box = self.Doc.addObject("Part::Box", "Box")
        with self.assertRaises(TypeError):
            box.ViewObject.canDragObject(0)
        with self.assertRaises(TypeError):
            box.ViewObject.canDropObject(0)
        box.ViewObject.canDropObject()
        with self.assertRaises(TypeError):
            box.ViewObject.dropObject(box, 0)

    def tearDown(self):
        # closing doc
        FreeCAD.closeDocument("PartGuiTest")


class ProjectionOnSurfaceTestCases(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("ProjectionOnSurface")

    def testSketchInternalFaceAsSupportFace(self):
        sketch = self.Doc.addObject("Sketcher::SketchObject", "Sketch")
        sketch.MakeInternals = True
        sketch.addGeometry(
            [
                Part.LineSegment(FreeCAD.Vector(0, 0), FreeCAD.Vector(10, 0)),
                Part.LineSegment(FreeCAD.Vector(10, 0), FreeCAD.Vector(10, 10)),
                Part.LineSegment(FreeCAD.Vector(10, 10), FreeCAD.Vector(0, 10)),
                Part.LineSegment(FreeCAD.Vector(0, 10), FreeCAD.Vector(0, 0)),
            ],
            False,
        )
        self.Doc.recompute()

        FreeCADGui.activateWorkbench("PartWorkbench")
        FreeCADGui.updateGui()
        FreeCADGui.runCommand("Part_ProjectionOnSurface")
        FreeCADGui.updateGui()

        taskDialog = FreeCADGui.Control.activeTaskDialog()
        self.assertIsNotNone(taskDialog)
        supportButton = None
        for widget in taskDialog.getDialogContent():
            supportButton = widget.findChild(QtWidgets.QPushButton, "pushButtonAddProjFace")
            if supportButton:
                break
        self.assertIsNotNone(supportButton)
        supportButton.click()
        FreeCADGui.Selection.addSelection(sketch, "InternalFace1")

        projection = self.Doc.getObject("Projection")
        self.assertIsNotNone(projection)
        self.assertEqual(projection.SupportFace[0], sketch)
        self.assertEqual(projection.SupportFace[1], ["InternalFace1"])

    def tearDown(self):
        FreeCADGui.Selection.clearSelection()
        guiDocument = FreeCADGui.getDocument("ProjectionOnSurface")
        if FreeCADGui.Control.activeDialog(guiDocument):
            FreeCADGui.Control.closeDialog(guiDocument)
        FreeCAD.closeDocument("ProjectionOnSurface")


class PartMirrorGuiTestCases(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("PartMirrorGuiTest")

    def tearDown(self):
        if FreeCADGui.Control.activeDialog():
            FreeCADGui.Control.closeDialog()
        FreeCADGui.Selection.clearSelection()
        FreeCAD.closeDocument(self.Doc.Name)

    def mirrorBoxWithLabel(self, label):
        if not FreeCAD.GuiUp:
            self.skipTest("This test requires a graphical user interface (GUI).")

        box = self.Doc.addObject("Part::Box", "Box")
        box.Label = label
        self.Doc.recompute()

        FreeCADGui.Selection.clearSelection()
        FreeCADGui.Selection.addSelection(self.Doc.Name, box.Name)
        FreeCADGui.runCommand("Part_Mirror")
        self.assertTrue(FreeCADGui.Control.activeDialog(), "Part Mirror task dialog did not open.")

        FreeCADGui.Control.activeTaskDialog().accept()
        QtWidgets.QApplication.processEvents()

        mirrors = [obj for obj in self.Doc.Objects if obj.isDerivedFrom("Part::Mirroring")]
        self.assertEqual(1, len(mirrors))
        return mirrors[0].Label

    def testMirrorLabelWithUnicodeIsNotDoubleEscaped(self):
        self.assertEqual("caf\u00e9 (Mirror #1)", self.mirrorBoxWithLabel("caf\u00e9"))

    def testMirrorLabelEscapesQuotesBeforePythonCommand(self):
        label = 'a");print("Erasing your hard drive, please stand by....")'
        self.assertEqual(f"{label} (Mirror #1)", self.mirrorBoxWithLabel(label))

    def testMirrorLabelWithNewlinesIsNotMangled(self):
        label = "a\nb\nc"
        self.assertEqual(f"{label} (Mirror #1)", self.mirrorBoxWithLabel(label))


class SectionCutTestCases(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("SectionCut")

    def testOpenDialog(self):
        box = self.Doc.addObject("Part::Box", "SectionCutBoxX")
        comp = self.Doc.addObject("Part::Compound", "SectionCutCompound")
        comp.Links = box
        grp = self.Doc.addObject("App::DocumentObjectGroup", "SectionCutX")
        grp.addObject(comp)
        self.Doc.recompute()

        FreeCADGui.runCommand("Part_SectionCut")
        dw = findDockWidget("Section Cutting")
        if dw:
            box = dw.findChild(QtWidgets.QDialogButtonBox)
            button = box.button(QtWidgets.QDialogButtonBox.Close)
            button.click()
        else:
            print("No section cutting panel found")

    def tearDown(self):
        FreeCAD.closeDocument("SectionCut")


class ConstructionPlaneTestCases(unittest.TestCase):
    """The construction plane task panel of datum planes"""

    def setUp(self):
        self.Doc = FreeCAD.newDocument("ConstructionPlane")
        self.Box = self.Doc.addObject("Part::Box", "Box")
        self.Doc.recompute()
        self.Plane = self.Doc.addObject("Part::DatumPlane", "Plane")
        self.Plane.AttachmentSupport = [(self.Box, "Face6")]
        self.Plane.MapMode = "FlatFace"
        self.Doc.recompute()

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.getDocument(self.Doc.Name).resetEdit()
        FreeCAD.closeDocument(self.Doc.Name)

    @staticmethod
    def findWidget(cls, name):
        return FreeCADGui.getMainWindow().findChild(cls, name)

    def edit(self):
        FreeCADGui.getDocument(self.Doc.Name).setEdit(self.Plane.Name)
        FreeCADGui.updateGui()

    def testOffsetValue(self):
        self.edit()
        combo = self.findWidget(QtWidgets.QComboBox, "typeCombo")
        self.assertIsNotNone(combo)
        self.assertEqual(combo.currentText(), "Offset plane")
        self.findWidget(QtWidgets.QAbstractSpinBox, "valueEdit").setProperty("rawValue", 5.0)
        FreeCADGui.Control.activeTaskDialog().accept()
        self.assertAlmostEqual(self.Plane.AttachmentOffset.Base.z, 5.0)
        self.assertAlmostEqual(self.Plane.Placement.Base.z, 15.0)
        self.assertIsNone(FreeCADGui.getDocument(self.Doc.Name).getInEdit())

    def testRejectRestores(self):
        self.edit()
        self.findWidget(QtWidgets.QAbstractSpinBox, "valueEdit").setProperty("rawValue", 5.0)
        FreeCADGui.Control.activeTaskDialog().reject()
        self.assertAlmostEqual(self.Plane.AttachmentOffset.Base.z, 0.0)
        self.assertAlmostEqual(self.Plane.Placement.Base.z, 10.0)

    def testPickReferences(self):
        self.edit()
        # A picked face replaces the face of an offset plane
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face1")
        FreeCADGui.updateGui()
        self.assertEqual(self.Plane.AttachmentSupport, [(self.Box, ("Face1",))])
        # A midplane takes a second face
        combo = self.findWidget(QtWidgets.QComboBox, "typeCombo")
        combo.setCurrentIndex(combo.findText("Midplane"))
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face2")
        FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().accept()
        self.assertEqual(self.Plane.MapMode, "MidPlane")
        self.assertAlmostEqual(self.Plane.Placement.Base.x, 5.0)

    def testPickedReferenceToggles(self):
        self.edit()
        combo = self.findWidget(QtWidgets.QComboBox, "typeCombo")
        combo.setCurrentIndex(combo.findText("Midplane"))
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face6")
        FreeCADGui.updateGui()
        self.assertEqual(len(self.Plane.AttachmentSupport), 0)
        self.assertEqual(self.Plane.MapMode, "Deactivated")

    def testPlaneAtAngle(self):
        self.Plane.AttachmentSupport = [(self.Box, "Edge1")]
        self.Plane.MapMode = "PlaneThroughLine"
        self.Doc.recompute()
        self.edit()
        combo = self.findWidget(QtWidgets.QComboBox, "typeCombo")
        self.assertEqual(combo.currentText(), "Plane at angle")
        self.findWidget(QtWidgets.QAbstractSpinBox, "valueEdit").setProperty("rawValue", 30.0)
        FreeCADGui.Control.activeTaskDialog().accept()
        rotation = self.Plane.AttachmentOffset.Rotation
        self.assertAlmostEqual(math.degrees(rotation.Angle), 30.0)
        self.assertTrue(rotation.Axis.isEqual(FreeCAD.Vector(1, 0, 0), 1e-7))

    def testOtherModesUseAttachmentEditor(self):
        self.Plane.MapMode = "ObjectXZ"
        self.Plane.AttachmentSupport = [(self.Box, "")]
        self.Doc.recompute()
        self.Plane.ViewObject.doubleClicked()
        FreeCADGui.updateGui()
        self.assertIsNotNone(FreeCADGui.Control.activeTaskDialog())
        self.assertIsNone(self.findWidget(QtWidgets.QComboBox, "typeCombo"))


class ConstructionPlaneCommandTestCases(unittest.TestCase):
    """The commands creating construction planes"""

    def setUp(self):
        self.Doc = FreeCAD.newDocument("ConstructionPlaneCommand")
        self.Box = self.Doc.addObject("Part::Box", "Box")
        self.Doc.recompute()
        FreeCADGui.Selection.clearSelection()

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.getDocument(self.Doc.Name).resetEdit()
        FreeCAD.closeDocument(self.Doc.Name)

    def planes(self):
        return self.Doc.findObjects("Part::DatumPlane")

    def testOffsetPlaneFromSelection(self):
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face6")
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().accept()
        self.assertEqual(len(self.planes()), 1)
        self.assertEqual(self.planes()[0].MapMode, "FlatFace")
        self.assertEqual(FreeCADGui.Selection.getSelection()[0], self.planes()[0])

    def testSelectionChoosesType(self):
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face1")
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face2")
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().accept()
        plane = self.planes()[0]
        self.assertEqual(plane.MapMode, "MidPlane")
        self.assertAlmostEqual(plane.Placement.Base.x, 5.0)

    def testPickReferencesAfterCommand(self):
        FreeCADGui.runCommand("Part_PlaneThroughThreePoints")
        FreeCADGui.updateGui()
        for vertex in ("Vertex1", "Vertex3", "Vertex5"):
            FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, vertex)
            FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().accept()
        plane = self.planes()[0]
        self.assertEqual(plane.MapMode, "ThreePointsPlane")
        self.assertEqual(len(plane.AttachmentSupport[0][1]), 3)

    def testGateRejectsWrongReferences(self):
        FreeCADGui.runCommand("Part_MidPlane")
        FreeCADGui.updateGui()
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Edge1")
        FreeCADGui.updateGui()
        self.assertEqual(len(self.planes()[0].AttachmentSupport), 0)

    def testCancelRemovesPlane(self):
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face6")
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().reject()
        FreeCADGui.updateGui()
        self.assertEqual(self.planes(), [])

    def testAcceptNeedsReferences(self):
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        self.assertFalse(FreeCADGui.Control.activeTaskDialog().accept())

    def testPlaneGoesIntoActivePart(self):
        part = self.Doc.addObject("App::Part", "Part")
        FreeCADGui.ActiveDocument.ActiveView.setActiveObject("part", part)
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face6")
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().accept()
        self.assertIn(self.planes()[0], part.Group)

    def testModelSize(self):
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Box.Name, "Face6")
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        FreeCADGui.Control.activeTaskDialog().accept()
        self.assertEqual(self.planes()[0].ViewObject.SizeMode, "Model")
        datum = self.Doc.addObject("Part::DatumPlane", "Datum")
        self.assertEqual(datum.ViewObject.SizeMode, "Screen")
        # Switching back and forth does not break the plane
        self.planes()[0].ViewObject.SizeMode = "Screen"
        self.planes()[0].ViewObject.SizeMode = "Model"

    def testModelSizeOfInfinitePlane(self):
        # An offset from an origin plane falls back to the size on the screen
        part = self.Doc.addObject("App::Part", "Part")
        self.Doc.recompute()
        yz = [o for o in part.Origin.OriginFeatures if o.Role == "YZ_Plane"][0]
        sub = "{}.{}.".format(part.Origin.Name, yz.Name)
        FreeCADGui.Selection.addSelection(self.Doc.Name, part.Name, sub)
        FreeCADGui.runCommand("Part_OffsetPlane")
        FreeCADGui.updateGui()
        edit = FreeCADGui.getMainWindow().findChild(QtWidgets.QAbstractSpinBox, "valueEdit")
        edit.setProperty("rawValue", 50.0)
        FreeCADGui.Control.activeTaskDialog().accept()
        FreeCADGui.updateGui()
        plane = self.planes()[0]
        self.assertEqual(plane.MapMode, "ObjectXY")
        box = plane.ViewObject.getBoundingBox()
        self.assertLess(box.DiagonalLength, 1e6)

    def testGroupCommand(self):
        self.assertIn("Part_ConstructionPlanes", FreeCADGui.listCommands())
        for name in (
            "Part_OffsetPlane",
            "Part_PlaneAtAngle",
            "Part_TangentPlane",
            "Part_MidPlane",
            "Part_PlaneThroughTwoEdges",
            "Part_PlaneThroughThreePoints",
            "Part_PlaneTangentAtPoint",
            "Part_PlaneAlongPath",
        ):
            self.assertFalse(FreeCADGui.Command.get(name).getInfo()["pixmap"] == "")
