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

import FreeCAD
import FreeCADGui
import os
import sys
import unittest
import Sketcher
import Part
import PartDesign
import PartDesignGui
import tempfile

from PySide import QtGui, QtCore
from PySide.QtGui import QApplication

from PartDesignTests.TestMaterial import TestMaterial
from PartDesignTests.TestActiveObject import TestActiveObject
from PartDesignTests.TestSuppressed import TestSuppressedStrikethrough


# timer runs this class in order to access modal dialog
class CallableCheckWorkflow:
    def __init__(self, test):
        self.test = test

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        self.test.assertIsNotNone(dialog, "Dialog box could not be found")
        if dialog is not None:
            dialogcheck = CallableCheckDialogWasClosed(self.test)
            QtCore.QTimer.singleShot(500, dialogcheck)
            QtCore.QTimer.singleShot(0, dialog, QtCore.SLOT("accept()"))


class CallableCheckDialogWasClosed:
    def __init__(self, test):
        self.test = test

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        self.test.assertIsNone(dialog, "Dialog box was not closed by accept()")


class CallableCheckWarning:
    def __init__(self, test):
        self.test = test

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        self.test.assertIsNotNone(dialog, "Input dialog box could not be found")
        if dialog is not None:
            QtCore.QTimer.singleShot(0, dialog, QtCore.SLOT("accept()"))


class CallableComboBox:
    def __init__(self, test):
        self.test = test

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        self.test.assertIsNotNone(dialog, "Warning dialog box could not be found")
        if dialog is not None:
            cbox = dialog.findChild(QtGui.QComboBox)
            self.test.assertIsNotNone(cbox, "ComboBox widget could not be found")
            if cbox is not None:
                QtCore.QTimer.singleShot(0, dialog, QtCore.SLOT("accept()"))


class CallableCheckExemptionDialog:
    def __init__(self, test):
        self.test = test

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        if dialog is not None:
            dialogcheck = CallableCheckExemptionDialogWasClosed(self.test)
            QtCore.QTimer.singleShot(100, dialogcheck)
            QtCore.QTimer.singleShot(0, dialog, QtCore.SLOT("accept()"))


class CallableCheckExemptionDialogWasClosed:
    def __init__(self, test):
        self.test = test

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        self.test.assertIsNone(dialog, "Dialog box was not closed by accept()")


App = FreeCAD
Gui = FreeCADGui


# ---------------------------------------------------------------------------
# define the test cases to test the FreeCAD PartDesign module
# ---------------------------------------------------------------------------
class PartDesignGuiTestCases(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("SketchGuiTest")

    def testRefuseToMoveSingleFeature(self):
        FreeCAD.Console.PrintMessage(
            "Testing refuse to move the feature with dependencies from one body to another\n"
        )
        self.BodySource = self.Doc.addObject("PartDesign::Body", "Body")
        Gui.activateView("Gui::View3DInventor", True)
        Gui.activeView().setActiveObject("pdbody", self.BodySource)

        self.BoxObj = self.Doc.addObject("PartDesign::AdditiveBox", "Box")
        self.BoxObj.Length = 10.0
        self.BoxObj.Width = 10.0
        self.BoxObj.Height = 10.0
        self.BodySource.addObject(self.BoxObj)

        App.ActiveDocument.recompute()

        self.Sketch = self.Doc.addObject("Sketcher::SketchObject", "Sketch")
        self.Sketch.AttachmentSupport = (self.BoxObj, ("Face3",))
        self.Sketch.MapMode = "FlatFace"
        self.BodySource.addObject(self.Sketch)

        geoList = []
        geoList.append(Part.LineSegment(App.Vector(2.0, 8.0, 0), App.Vector(8.0, 8.0, 0)))
        geoList.append(Part.LineSegment(App.Vector(8.0, 8.0, 0), App.Vector(8.0, 2.0, 0)))
        geoList.append(Part.LineSegment(App.Vector(8.0, 2.0, 0), App.Vector(2.0, 2.0, 0)))
        geoList.append(Part.LineSegment(App.Vector(2.0, 2.0, 0), App.Vector(2.0, 8.0, 0)))
        self.Sketch.addGeometry(geoList, False)
        conList = []
        conList.append(Sketcher.Constraint("Coincident", 0, 2, 1, 1))
        conList.append(Sketcher.Constraint("Coincident", 1, 2, 2, 1))
        conList.append(Sketcher.Constraint("Coincident", 2, 2, 3, 1))
        conList.append(Sketcher.Constraint("Coincident", 3, 2, 0, 1))
        conList.append(Sketcher.Constraint("Horizontal", 0))
        conList.append(Sketcher.Constraint("Horizontal", 2))
        conList.append(Sketcher.Constraint("Vertical", 1))
        conList.append(Sketcher.Constraint("Vertical", 3))
        self.Sketch.addConstraint(conList)

        self.Pad = self.Doc.addObject("PartDesign::Pad", "Pad")
        self.Pad.Profile = self.Sketch
        self.Pad.Length = 10.000000
        self.Pad.Length2 = 100.000000
        self.Pad.Type = 0
        self.Pad.UpToFace = None
        self.Pad.Reversed = 0
        self.Pad.SideType = "One side"
        self.Pad.Offset = 0.000000

        self.BodySource.addObject(self.Pad)

        self.Doc.recompute()
        Gui.ActiveDocument.ActiveView.sendMessage("ViewFit")

        self.BodyTarget = self.Doc.addObject("PartDesign::Body", "Body")

        Gui.Selection.addSelection(App.ActiveDocument.Pad)
        cobj = CallableCheckWarning(self)
        QtCore.QTimer.singleShot(500, cobj)
        Gui.runCommand("PartDesign_MoveFeature")
        # assert dependencies of the Sketch
        self.assertEqual(len(self.BodySource.Group), 3, "Source body feature count is wrong")
        self.assertEqual(len(self.BodyTarget.Group), 0, "Target body feature count is wrong")

    def testMoveSingleFeature(self):
        FreeCAD.Console.PrintMessage("Testing moving one feature from one body to another\n")
        self.BodySource = self.Doc.addObject("PartDesign::Body", "Body")
        Gui.activateView("Gui::View3DInventor", True)
        Gui.activeView().setActiveObject("pdbody", self.BodySource)

        self.Sketch = self.Doc.addObject("Sketcher::SketchObject", "Sketch")
        self.BodySource.addObject(self.Sketch)
        self.Sketch.AttachmentSupport = (self.BodySource.Origin.OriginFeatures[3], [""])
        self.Sketch.MapMode = "FlatFace"

        geoList = []
        geoList.append(
            Part.LineSegment(
                App.Vector(-10.000000, 10.000000, 0), App.Vector(10.000000, 10.000000, 0)
            )
        )
        geoList.append(
            Part.LineSegment(
                App.Vector(10.000000, 10.000000, 0), App.Vector(10.000000, -10.000000, 0)
            )
        )
        geoList.append(
            Part.LineSegment(
                App.Vector(10.000000, -10.000000, 0), App.Vector(-10.000000, -10.000000, 0)
            )
        )
        geoList.append(
            Part.LineSegment(
                App.Vector(-10.000000, -10.000000, 0), App.Vector(-10.000000, 10.000000, 0)
            )
        )
        self.Sketch.addGeometry(geoList, False)
        conList = []
        conList.append(Sketcher.Constraint("Coincident", 0, 2, 1, 1))
        conList.append(Sketcher.Constraint("Coincident", 1, 2, 2, 1))
        conList.append(Sketcher.Constraint("Coincident", 2, 2, 3, 1))
        conList.append(Sketcher.Constraint("Coincident", 3, 2, 0, 1))
        conList.append(Sketcher.Constraint("Horizontal", 0))
        conList.append(Sketcher.Constraint("Horizontal", 2))
        conList.append(Sketcher.Constraint("Vertical", 1))
        conList.append(Sketcher.Constraint("Vertical", 3))
        self.Sketch.addConstraint(conList)

        self.Pad = self.Doc.addObject("PartDesign::Pad", "Pad")
        self.BodySource.addObject(self.Pad)
        self.Pad.Profile = self.Sketch
        self.Pad.Length = 10.000000
        self.Pad.Length2 = 100.000000
        self.Pad.Type = 0
        self.Pad.UpToFace = None
        self.Pad.Reversed = 0
        self.Pad.SideType = "One side"
        self.Pad.Offset = 0.000000

        self.Doc.recompute()
        Gui.ActiveDocument.ActiveView.sendMessage("ViewFit")

        self.BodyTarget = self.Doc.addObject("PartDesign::Body", "Body")

        Gui.Selection.addSelection(App.ActiveDocument.Pad)
        cobj = CallableComboBox(self)
        QtCore.QTimer.singleShot(500, cobj)
        Gui.runCommand("PartDesign_MoveFeature")
        # assert dependencies of the Sketch
        self.Doc.recompute()

        self.assertFalse(
            self.Sketch.AttachmentSupport[0][0] in self.BodySource.Origin.OriginFeatures
        )
        self.assertTrue(
            self.Sketch.AttachmentSupport[0][0] in self.BodyTarget.Origin.OriginFeatures
        )
        self.assertEqual(len(self.BodySource.Group), 0, "Source body feature count is wrong")
        self.assertEqual(len(self.BodyTarget.Group), 2, "Target body feature count is wrong")

    def tearDown(self):
        FreeCAD.closeDocument("SketchGuiTest")


class PartDesignTransformed(unittest.TestCase):
    def setUp(self):
        self.Doc = App.newDocument("PartDesignTransformed")
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.BoxObj = self.Doc.addObject("PartDesign::AdditiveBox", "Box")
        self.BoxObj.Length = 10.0
        self.BoxObj.Width = 10.0
        self.BoxObj.Height = 10.0
        App.ActiveDocument.recompute()
        # not adding box to the body to imitate undertermined workflow
        tempDir = tempfile.gettempdir()
        self.TempDoc = os.path.join(tempDir, "PartDesignTransformed.FCStd")
        if os.path.exists(self.TempDoc):
            os.remove(self.TempDoc)
        App.ActiveDocument.saveAs(self.TempDoc)
        App.closeDocument("PartDesignTransformed")

    def tearDown(self):
        # closing doc
        if App.ActiveDocument is not None and App.ActiveDocument.Name == PartDesignTransformed:
            App.closeDocument("PartDesignTransformed")
        # print ("omit closing document for debugging")

    def testMultiTransformCase(self):
        App.Console.PrintMessage("Testing applying MultiTransform to the Box outside the body\n")
        App.open(self.TempDoc)
        App.setActiveDocument("PartDesignTransformed")
        Gui.Selection.addSelection(App.ActiveDocument.Box)

        workflowcheck = CallableCheckWorkflow(self)
        QtCore.QTimer.singleShot(500, workflowcheck)
        Gui.runCommand("PartDesign_MultiTransform")

        App.closeDocument("PartDesignTransformed")


class CreateSketch(unittest.TestCase):

    def testPDCreateSketch(self):
        App.Console.PrintMessage("Testing the creation of a sketch\n")
        param = FreeCAD.ParamGet("User parameter:BaseApp/Preferences/Mod/PartDesign")
        useAttachmentSaved = param.GetBool("NewSketchUseAttachmentDialog", False)
        param.SetBool("NewSketchUseAttachmentDialog", False)
        App.newDocument()
        App.activeDocument().addObject("PartDesign::Body", "Body")
        App.ActiveDocument.getObject("Body").Label = "Body"
        App.ActiveDocument.getObject("Body").AllowCompound = True
        FreeCADGui.activateView("Gui::View3DInventor", True)
        FreeCADGui.activeView().setActiveObject("pdbody", App.activeDocument().Body)
        FreeCADGui.Selection.clearSelection()
        FreeCADGui.runCommand("Std_OrthographicCamera", 1)
        workflowcheck = CallableCheckExemptionDialog(self)
        QtCore.QTimer.singleShot(100, workflowcheck)
        FreeCADGui.runCommand("PartDesign_CompSketches", 0)
        activeDialog = FreeCADGui.Control.activeDialog()
        self.assertIsNotNone(activeDialog)
        if activeDialog is not None:
            FreeCADGui.Control.closeDialog()
        App.closeDocument(App.ActiveDocument.Name)
        param.SetBool("NewSketchUseAttachmentDialog", useAttachmentSaved)


class CallableRejectUnexpectedDialog:
    """Closes a modal dialog that should not have been shown and remembers it"""

    def __init__(self):
        self.shown = False

    def __call__(self):
        dialog = QApplication.activeModalWidget()
        if dialog is not None:
            self.shown = True
            QtCore.QTimer.singleShot(0, dialog, QtCore.SLOT("reject()"))


class QuickSketchStart(unittest.TestCase):
    """A sketch on a preselected plane starts without asking for a body"""

    def setUp(self):
        self.param = FreeCAD.ParamGet("User parameter:BaseApp/Preferences/Mod/PartDesign")
        self.useAttachmentSaved = self.param.GetBool("NewSketchUseAttachmentDialog", False)
        self.param.SetBool("NewSketchUseAttachmentDialog", False)
        self.Doc = App.newDocument("QuickSketchStart")
        FreeCADGui.activateView("Gui::View3DInventor", True)

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.ActiveDocument.resetEdit()
        App.closeDocument(self.Doc.Name)
        self.param.SetBool("NewSketchUseAttachmentDialog", self.useAttachmentSaved)

    @staticmethod
    def xyPlane(container):
        return [f for f in container.Origin.OriginFeatures if f.Role == "XY_Plane"][0]

    def runNewSketch(self):
        guard = CallableRejectUnexpectedDialog()
        QtCore.QTimer.singleShot(500, guard)
        FreeCADGui.runCommand("PartDesign_NewSketch", 0)
        QApplication.processEvents()
        self.assertFalse(guard.shown, "Unexpected modal dialog")
        sketches = self.Doc.findObjects("Sketcher::SketchObject")
        self.assertEqual(len(sketches), 1)
        return sketches[0]

    def testNoBodyCreatesBodyInActivePart(self):
        part = self.Doc.addObject("App::Part", "Part")
        self.Doc.recompute()
        FreeCADGui.activeView().setActiveObject("part", part)
        FreeCADGui.Selection.clearSelection()
        FreeCADGui.Selection.addSelection(self.xyPlane(part))

        sketch = self.runNewSketch()

        bodies = self.Doc.findObjects("PartDesign::Body")
        self.assertEqual(len(bodies), 1)
        self.assertIn(bodies[0], part.Group)
        self.assertIn(sketch, bodies[0].Group)

    def testSelectedPlaneChoosesBody(self):
        body1 = self.Doc.addObject("PartDesign::Body", "Body1")
        body2 = self.Doc.addObject("PartDesign::Body", "Body2")
        self.Doc.recompute()
        FreeCADGui.Selection.clearSelection()
        FreeCADGui.Selection.addSelection(self.xyPlane(body2))

        sketch = self.runNewSketch()

        self.assertIn(sketch, body2.Group)
        self.assertNotIn(sketch, body1.Group)


class PadProfileRegions(unittest.TestCase):
    """Regions of the sketch can be added to and removed from the profile in the task panel"""

    def setUp(self):
        import TestSketcherApp

        self.Doc = App.newDocument("PadProfileRegions")
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.Sketch = self.Body.newObject("Sketcher::SketchObject", "Sketch")
        self.Sketch.MakeInternals = True
        # A square inside a square gives two closed regions
        TestSketcherApp.CreateRectangleSketch(self.Sketch, (0, 0), (20, 20))
        TestSketcherApp.CreateRectangleSketch(self.Sketch, (5, 5), (10, 10))
        self.Pad = self.Body.newObject("PartDesign::Pad", "Pad")
        self.Pad.Profile = (self.Sketch, ["InternalFace1"])
        self.Pad.Length = 10
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.ActiveDocument.resetEdit()
        App.closeDocument(self.Doc.Name)

    def clickRegion(self, region):
        FreeCADGui.Selection.addSelection(
            self.Doc.Name, self.Body.Name, "{}.{}".format(self.Sketch.Name, region)
        )
        QApplication.processEvents()

    def testToggleRegions(self):
        FreeCADGui.ActiveDocument.setEdit(self.Pad)
        mainWindow = FreeCADGui.getMainWindow()
        button = mainWindow.findChild(QtGui.QToolButton, "buttonProfileRegions")
        regionList = mainWindow.findChild(QtGui.QListWidget, "listWidgetProfileRegions")
        group = mainWindow.findChild(QtGui.QGroupBox, "groupProfiles")
        self.assertIsNotNone(button)
        self.assertFalse(group.isHidden())
        self.assertEqual(regionList.count(), 1)

        padVisible = self.Pad.Visibility
        sketchVisible = self.Sketch.Visibility
        button.setChecked(True)
        self.assertTrue(self.Sketch.Visibility)

        self.clickRegion("InternalFace2")
        self.assertEqual(list(self.Pad.Profile[1]), ["InternalFace1", "InternalFace2"])
        self.assertEqual(regionList.count(), 2)
        self.assertAlmostEqual(self.Pad.Shape.Volume, 4000)

        # clicking a region again removes it
        self.clickRegion("InternalFace1")
        self.assertEqual(list(self.Pad.Profile[1]), ["InternalFace2"])
        self.assertEqual(regionList.count(), 1)

        # an edge selects the whole sketch
        self.clickRegion("Edge1")
        self.assertEqual(list(self.Pad.Profile[1]), [])

        # leaving the selection restores the visibility
        button.setChecked(False)
        self.assertEqual(self.Pad.Visibility, padVisible)
        self.assertEqual(self.Sketch.Visibility, sketchVisible)


class CommandSearch(unittest.TestCase):
    """Std_CommandSearch runs the command that is picked from the completion list"""

    def setUp(self):
        self.Doc = App.newDocument("CommandSearch")
        FreeCADGui.activateView("Gui::View3DInventor", True)

    def tearDown(self):
        popup = QApplication.activePopupWidget()
        if popup is not None:
            popup.close()
        App.closeDocument(self.Doc.Name)
        # The closed popups can leave no active window, which later keyboard tests need.
        # The window manager activates a new window, and the main window once it closes.
        window = FreeCADGui.getMainWindow()
        if QApplication.activeWindow() is None:
            dialog = QtGui.QDialog(window)
            dialog.show()
            self.waitForActiveWindow(dialog)
            dialog.close()
            dialog.deleteLater()
            self.waitForActiveWindow(window)

    @staticmethod
    def waitForActiveWindow(window, ms=2000):
        import time

        end = time.time() + ms / 1000.0
        while time.time() < end and QApplication.activeWindow() is not window:
            QApplication.processEvents()

    def testRunCommandFromSearch(self):
        try:
            from PySide6.QtTest import QTest
        except ImportError:
            self.skipTest("QtTest is not available")

        FreeCADGui.runCommand("Std_CommandSearch", 0)
        QApplication.processEvents()
        popup = QApplication.activePopupWidget()
        self.assertIsNotNone(popup, "Search popup not shown")
        edit = popup.findChild(QtGui.QLineEdit)
        self.assertIsNotNone(edit)

        QTest.keyClicks(edit, "PartDesign_Body")
        QApplication.processEvents()
        QTest.keyClick(edit, QtCore.Qt.Key_Down)
        QTest.keyClick(edit, QtCore.Qt.Key_Return)
        for _ in range(5):
            QApplication.processEvents()

        self.assertEqual(len(self.Doc.findObjects("PartDesign::Body")), 1)


class FusionShortcuts(unittest.TestCase):
    """The Fusion 360 preference pack and extruding while sketching"""

    def setUp(self):
        FreeCADGui.activateWorkbench("DesignWorkbench")
        self.Doc = App.newDocument("FusionShortcuts")

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.ActiveDocument.resetEdit()
        App.closeDocument(self.Doc.Name)

    @staticmethod
    def processEvents(ms=600):
        import time

        end = time.time() + ms / 1000.0
        while time.time() < end:
            QApplication.processEvents()

    def testPackCommandsExist(self):
        import xml.etree.ElementTree as ET

        path = App.getResourceDir() + "Gui/PreferencePacks/Fusion 360/Fusion 360.cfg"
        root = ET.parse(path).getroot()
        names = {
            node.get("Name")
            for group in root.iter("FCParamGroup")
            if group.get("Name") in ("Shortcut", "Priorities")
            for node in group
            if node.tag in ("FCText", "FCInt")
        }
        commands = set(FreeCADGui.listCommands())
        missing = sorted(name for name in names if name not in commands)
        self.assertEqual(missing, [], "Unknown commands in the Fusion 360 shortcuts")

    def testExtrudeKeyWhileSketching(self):
        try:
            from PySide6.QtTest import QTest
        except ImportError:
            self.skipTest("QtTest is not available")
        import TestSketcherApp

        body = self.Doc.addObject("PartDesign::Body", "Body")
        sketch = body.newObject("Sketcher::SketchObject", "Sketch")
        TestSketcherApp.CreateRectangleSketch(sketch, (0, 0), (10, 10))
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)
        FreeCADGui.activeView().setActiveObject("pdbody", body)

        pad = FreeCADGui.Command.get("PartDesign_Pad")
        oldShortcut = pad.getShortcut()
        pad.setShortcut("E")
        try:
            FreeCADGui.ActiveDocument.setEdit(sketch)
            self.processEvents()
            QTest.keyClick(QApplication.focusWidget(), QtCore.Qt.Key_E)
            self.processEvents()
        finally:
            pad.resetShortcut()
            if oldShortcut != pad.getShortcut():
                pad.setShortcut(oldShortcut)

        pads = self.Doc.findObjects("PartDesign::Pad")
        self.assertEqual(len(pads), 1)
        self.assertEqual(FreeCADGui.ActiveDocument.getInEdit().Object, pads[0])

    def testSketchKeysRepeat(self):
        """A group button takes over the shortcut of its last used tool, so the keys of
        the pack must be assigned to the tools and keep working on every press"""
        try:
            from PySide6.QtTest import QTest
        except ImportError:
            self.skipTest("QtTest is not available")
        import xml.etree.ElementTree as ET

        path = App.getResourceDir() + "Gui/PreferencePacks/Fusion 360/Fusion 360.cfg"
        root = ET.parse(path).getroot()
        shortcuts = {
            node.get("Name"): node.text
            for group in root.iter("FCParamGroup")
            if group.get("Name") == "Shortcut"
            for node in group
            if node.tag == "FCText"
        }
        commands = {name: FreeCADGui.Command.get(name) for name in shortcuts}
        tools = {key: name for name, key in shortcuts.items() if key in ("R", "C", "T", "P")}

        body = self.Doc.addObject("PartDesign::Body", "Body")
        sketch = body.newObject("Sketcher::SketchObject", "Sketch")
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)

        oldShortcuts = {name: command.getShortcut() for name, command in commands.items()}
        fired = []
        try:
            for name, command in commands.items():
                command.setShortcut(shortcuts[name])
            for action in FreeCADGui.getMainWindow().findChildren(QtGui.QAction):
                for key, name in tools.items():
                    if action.objectName() == name:
                        action.triggered.connect(lambda *args, key=key: fired.append(key))
            FreeCADGui.ActiveDocument.setEdit(sketch)
            self.processEvents()
            viewer = FreeCADGui.ActiveDocument.ActiveView.graphicsView()
            viewer.setFocus()
            self.processEvents(300)
            for key in "RRCCTTPP":
                QTest.keyClick(QApplication.focusWidget(), getattr(QtCore.Qt, "Key_" + key))
                # The group button shares the key once used, which delays the shortcut
                self.processEvents(800)
                QTest.keyClick(QApplication.focusWidget(), QtCore.Qt.Key_Escape)
                self.processEvents(300)
        finally:
            for name, command in commands.items():
                command.resetShortcut()
                if oldShortcuts[name] != command.getShortcut():
                    command.setShortcut(oldShortcuts[name])

        self.assertEqual("".join(fired), "RRCCTTPP")


class ExtrudeWithoutProfile(unittest.TestCase):
    """Pad without selection starts with no profile, which is picked in the task panel"""

    def setUp(self):
        import TestSketcherApp

        self.Doc = App.newDocument("ExtrudeWithoutProfile")
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.Sketch = self.Body.newObject("Sketcher::SketchObject", "Sketch")
        self.Sketch.MakeInternals = True
        # A square inside a square gives two closed regions
        TestSketcherApp.CreateRectangleSketch(self.Sketch, (0, 0), (20, 20))
        TestSketcherApp.CreateRectangleSketch(self.Sketch, (5, 5), (10, 10))
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)
        FreeCADGui.activeView().setActiveObject("pdbody", self.Body)
        FreeCADGui.Selection.clearSelection()
        # Let timers of earlier tests closing dialogs expire before looking for a warning
        import time

        end = time.time() + 0.3
        while time.time() < end:
            QApplication.processEvents()

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.ActiveDocument.resetEdit()
        App.closeDocument(self.Doc.Name)

    @staticmethod
    def processEvents():
        for _ in range(5):
            QApplication.processEvents()

    def startPad(self):
        FreeCADGui.runCommand("PartDesign_Pad", 0)
        self.processEvents()
        pads = self.Doc.findObjects("PartDesign::Pad")
        self.assertEqual(len(pads), 1)
        self.assertIsNotNone(FreeCADGui.Control.activeTaskDialog())
        return pads[0]

    def pick(self, element):
        FreeCADGui.Selection.addSelection(
            self.Doc.Name, self.Body.Name, "{}.{}".format(self.Sketch.Name, element)
        )
        self.processEvents()

    def testPickRegionAfterwards(self):
        pad = self.startPad()
        self.assertIsNone(pad.Profile)
        button = FreeCADGui.getMainWindow().findChild(QtGui.QToolButton, "buttonProfileRegions")
        self.assertTrue(button.isChecked())

        self.pick("InternalFace1")
        self.assertEqual(pad.Profile[0], self.Sketch)
        self.assertEqual(list(pad.Profile[1]), ["InternalFace1"])

        FreeCADGui.Control.activeTaskDialog().accept()
        self.processEvents()
        self.assertTrue(pad.Shape.isValid())
        self.assertIn(round(pad.Shape.Volume), (1000, 3000))

    def testPickWholeSketchByEdge(self):
        pad = self.startPad()
        self.pick("Edge1")
        self.assertEqual(pad.Profile[0], self.Sketch)
        self.assertEqual(list(pad.Profile[1]), [])

        FreeCADGui.Control.activeTaskDialog().accept()
        self.processEvents()
        # The inner square is a hole of the whole sketch
        self.assertAlmostEqual(pad.Shape.Volume, 3000)

    def testAcceptWithoutProfileKeepsDialog(self):
        self.startPad()
        warning = CallableRejectUnexpectedDialog()
        # Look for the warning until it shows, it may take a while to open
        timer = QtCore.QTimer()
        timer.timeout.connect(warning)
        timer.start(100)
        FreeCADGui.Control.activeTaskDialog().accept()
        self.processEvents()
        timer.stop()
        self.assertTrue(warning.shown, "No warning about the missing profile")
        self.assertIsNotNone(FreeCADGui.Control.activeTaskDialog())

    def testCancelRemovesPad(self):
        self.startPad()
        FreeCADGui.Control.activeTaskDialog().reject()
        self.processEvents()
        self.assertEqual(len(self.Doc.findObjects("PartDesign::Pad")), 0)
        self.assertTrue(self.Sketch.Visibility)


class Timeline(unittest.TestCase):
    """The timeline shows the features of the active body and moves its tip"""

    def setUp(self):
        import TestSketcherApp

        FreeCADGui.activateWorkbench("PartDesignWorkbench")
        self.Doc = App.newDocument("Timeline")
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.Sketch = self.Body.newObject("Sketcher::SketchObject", "Sketch")
        TestSketcherApp.CreateRectangleSketch(self.Sketch, (0, 0), (10, 10))
        self.Pad = self.Body.newObject("PartDesign::Pad", "Pad")
        self.Pad.Profile = self.Sketch
        self.Pad.Length = 5
        self.Pad2 = self.Body.newObject("PartDesign::Pad", "Pad2")
        self.Pad2.Profile = self.Sketch
        self.Pad2.Length = 10
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)
        FreeCADGui.activeView().setActiveObject("pdbody", self.Body)
        self.processEvents()
        dock = FreeCADGui.getMainWindow().findChild(QtGui.QDockWidget, "PartDesign_Timeline")
        self.assertIsNotNone(dock, "Timeline dock not found")
        self.timeline = dock.widget()
        self.list = self.timeline.findChild(QtGui.QListWidget)

    def tearDown(self):
        App.closeDocument(self.Doc.Name)

    @staticmethod
    def processEvents():
        for _ in range(5):
            QApplication.processEvents()

    def rollTo(self, slot):
        QtCore.QMetaObject.invokeMethod(self.timeline, "rollTo", QtCore.Q_ARG(int, slot))
        self.processEvents()

    def testShowsFeaturesAndMovesTip(self):
        self.assertEqual(self.list.count(), 3)
        self.assertEqual(self.Body.Tip, self.Pad2)

        self.rollTo(2)
        self.assertEqual(self.Body.Tip, self.Pad)
        self.assertEqual(self.list.count(), 3)

        self.rollTo(0)
        self.assertIsNone(self.Body.Tip)

        QtCore.QMetaObject.invokeMethod(self.timeline, "rollToEnd")
        self.processEvents()
        self.assertEqual(self.Body.Tip, self.Pad2)

    def testNoActiveBody(self):
        FreeCADGui.activeView().setActiveObject("pdbody", None)
        self.processEvents()
        self.assertTrue(self.list.isHidden())


class ConstructionPlanes(unittest.TestCase):
    """Construction planes in the Design workbench"""

    def setUp(self):
        import TestSketcherApp

        FreeCADGui.activateWorkbench("DesignWorkbench")
        self.Doc = App.newDocument("ConstructionPlanes")
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.Sketch = self.Body.newObject("Sketcher::SketchObject", "Sketch")
        TestSketcherApp.CreateRectangleSketch(self.Sketch, (0, 0), (10, 10))
        self.Pad = self.Body.newObject("PartDesign::Pad", "Pad")
        self.Pad.Profile = self.Sketch
        self.Pad.Length = 5
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)
        FreeCADGui.activeView().setActiveObject("pdbody", self.Body)
        FreeCADGui.Selection.clearSelection()
        self.processEvents()

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.getDocument(self.Doc.Name).resetEdit()
        App.closeDocument(self.Doc.Name)

    @staticmethod
    def processEvents():
        for _ in range(5):
            QApplication.processEvents()

    def makeOffsetPlane(self, face="Face6", distance=10.0):
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Body.Name, "Pad." + face)
        FreeCADGui.runCommand("Part_OffsetPlane")
        self.processEvents()
        edit = FreeCADGui.getMainWindow().findChild(QtGui.QAbstractSpinBox, "valueEdit")
        edit.setProperty("rawValue", distance)
        FreeCADGui.Control.activeTaskDialog().accept()
        self.processEvents()
        return self.Doc.findObjects("Part::DatumPlane")[0]

    def testMenuAndToolbar(self):
        menus = [
            menu
            for menu in FreeCADGui.getMainWindow().menuBar().findChildren(QtGui.QMenu)
            if menu.title() == "&Construct"
        ]
        self.assertEqual(len(menus), 1)
        texts = [action.text() for action in menus[0].actions()]
        self.assertIn("Offset Plane", texts)
        self.assertIn("Midplane", texts)
        toolbar = FreeCADGui.getMainWindow().findChild(QtGui.QToolBar, "Design")
        names = [action.objectName() for action in toolbar.actions()]
        self.assertIn("Part_ConstructionPlanes", names)

    def testPlaneInBodyAndTimeline(self):
        plane = self.makeOffsetPlane()
        self.assertIn(plane, self.Body.Group)
        self.assertAlmostEqual(plane.Placement.Base.z, 15.0)
        dock = FreeCADGui.getMainWindow().findChild(QtGui.QDockWidget, "PartDesign_Timeline")
        timeline = dock.widget().findChild(QtGui.QListWidget)
        names = [timeline.item(i).data(QtCore.Qt.UserRole) for i in range(timeline.count())]
        self.assertIn(plane.Name, names)

    def testSketchOnSelectedPlane(self):
        plane = self.makeOffsetPlane()
        FreeCADGui.runCommand("PartDesign_NewSketch")
        self.processEvents()
        sketches = self.Doc.findObjects("Sketcher::SketchObject")
        self.assertEqual(len(sketches), 2)
        self.assertEqual(sketches[-1].AttachmentSupport[0][0], plane)

    def testPickPlaneForNewSketch(self):
        plane = self.makeOffsetPlane()
        FreeCADGui.Selection.clearSelection()
        FreeCADGui.runCommand("PartDesign_NewSketch")
        self.processEvents()
        FreeCADGui.Selection.addSelection(self.Doc.Name, self.Body.Name, plane.Name + ".")
        self.processEvents()
        dialog = FreeCADGui.Control.activeTaskDialog()
        if dialog:
            dialog.accept()
            self.processEvents()
        sketches = self.Doc.findObjects("Sketcher::SketchObject")
        self.assertEqual(len(sketches), 2)
        self.assertEqual(sketches[-1].AttachmentSupport[0][0], plane)

    def testDoubleClickEditsPlane(self):
        plane = self.makeOffsetPlane()
        plane.ViewObject.doubleClicked()
        self.processEvents()
        combo = FreeCADGui.getMainWindow().findChild(QtGui.QComboBox, "typeCombo")
        self.assertIsNotNone(combo)
        edit = FreeCADGui.getMainWindow().findChild(QtGui.QAbstractSpinBox, "valueEdit")
        edit.setProperty("rawValue", 20.0)
        FreeCADGui.Control.activeTaskDialog().accept()
        self.assertAlmostEqual(plane.Placement.Base.z, 25.0)


class FusionOrbitCenter(unittest.TestCase):
    """A double click with the middle button pins the center of rotation"""

    def setUp(self):
        self.Doc = App.newDocument("FusionOrbitCenter")
        self.Box = self.Doc.addObject("Part::Box", "Box")
        self.Box.Length = 50
        self.Box.Width = 50
        self.Box.Height = 50
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)
        self.View = FreeCADGui.ActiveDocument.ActiveView
        self.View.setNavigationType("Gui::FusionNavigationStyle")
        self.View.viewIsometric()
        self.View.fitAll()
        self.processEvents()
        self.viewport = self.View.graphicsView().viewport()

    def tearDown(self):
        App.closeDocument(self.Doc.Name)

    @staticmethod
    def processEvents(ms=300):
        import time

        end = time.time() + ms / 1000.0
        while time.time() < end:
            QApplication.processEvents()

    def rotationCenter(self):
        """The position of the rotation center indicator, if shown"""
        root = self.View.getViewer().getSceneGraph()
        for i in range(root.getNumChildren()):
            child = root.getChild(i)
            if child.getTypeId().getName().getString() == "SoSkipBoundingGroup":
                translation = child.getByName("translation")
                if translation:
                    return App.Vector(translation.translation.getValue().getValue())
        return None

    def doubleMiddleClick(self, pos):
        from PySide6.QtTest import QTest

        QTest.mouseMove(self.viewport, pos)
        for _ in range(2):
            QTest.mouseClick(self.viewport, QtCore.Qt.MiddleButton, QtCore.Qt.NoModifier, pos, 20)
        self.processEvents()

    def testPinAndRelease(self):
        try:
            from PySide6.QtTest import QTest
            from pivy import coin
        except ImportError:
            self.skipTest("QtTest or pivy is not available")

        center = self.viewport.rect().center()
        self.doubleMiddleClick(center)
        pinned = self.rotationCenter()
        self.assertIsNotNone(pinned, "No rotation center after a double click on the box")
        self.assertTrue(self.Box.Shape.BoundBox.isInside(pinned) or
                        self.Box.Shape.distToShape(Part.Vertex(pinned))[0] < 1e-3)

        # Orbiting keeps the pinned center
        QTest.mousePress(self.viewport, QtCore.Qt.MiddleButton, QtCore.Qt.ShiftModifier, center)
        for dx in range(0, 60, 10):
            QTest.mouseMove(self.viewport, center + QtCore.QPoint(dx, dx // 2))
            self.processEvents(30)
        QTest.mouseRelease(
            self.viewport, QtCore.Qt.MiddleButton, QtCore.Qt.ShiftModifier,
            center + QtCore.QPoint(60, 30)
        )
        self.processEvents()
        self.assertIsNotNone(self.rotationCenter())
        self.assertTrue(self.rotationCenter().isEqual(pinned, 1e-4))

        # A double click in empty space releases it
        self.doubleMiddleClick(QtCore.QPoint(5, 5))
        self.assertIsNone(self.rotationCenter())

    def testSingleClickKeepsView(self):
        try:
            from PySide6.QtTest import QTest
        except ImportError:
            self.skipTest("QtTest is not available")

        camera = self.View.getCamera()
        pos = self.viewport.rect().center() + QtCore.QPoint(40, 20)
        QTest.mouseClick(self.viewport, QtCore.Qt.MiddleButton, QtCore.Qt.NoModifier, pos)
        self.processEvents(800)
        self.assertEqual(self.View.getCamera(), camera)
        self.assertIsNone(self.rotationCenter())


class PadOperation(unittest.TestCase):
    """The operation of the Pad/Pocket task panel changes the feature type or the body"""

    JOIN, CUT, INTERSECT, NEW_BODY = range(4)

    def setUp(self):
        self.Doc = App.newDocument("PadOperation")
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.Sketch = self.Body.newObject("Sketcher::SketchObject", "Sketch")
        import TestSketcherApp

        TestSketcherApp.CreateRectangleSketch(self.Sketch, (0, 0), (10, 10))
        self.Pad = self.Body.newObject("PartDesign::Pad", "Pad")
        self.Pad.Profile = self.Sketch
        self.Pad.Length = 7
        self.Doc.recompute()
        FreeCADGui.activateView("Gui::View3DInventor", True)

    def tearDown(self):
        FreeCADGui.Control.closeDialog()
        FreeCADGui.ActiveDocument.resetEdit()
        App.closeDocument(self.Doc.Name)

    @staticmethod
    def processEvents():
        for _ in range(5):
            QApplication.processEvents()

    def chooseOperation(self, index):
        combo = FreeCADGui.getMainWindow().findChild(QtGui.QComboBox, "comboOperation")
        self.assertIsNotNone(combo)
        self.assertEqual(combo.count(), 4)
        combo.setCurrentIndex(index)
        combo.activated.emit(index)
        self.processEvents()

    def closeTaskDialog(self, accept):
        dialog = FreeCADGui.Control.activeTaskDialog()
        self.assertIsNotNone(dialog)
        if accept:
            dialog.accept()
        else:
            dialog.reject()
        self.processEvents()

    def testSwitchToPocketAndCancel(self):
        self.Doc.openTransaction("Edit Pad")
        FreeCADGui.ActiveDocument.setEdit(self.Pad)
        self.chooseOperation(self.CUT)

        pockets = self.Doc.findObjects("PartDesign::Pocket")
        self.assertEqual(len(pockets), 1)
        self.assertIsNone(self.Doc.getObject("Pad"))
        self.assertEqual(pockets[0].Length.Value, 7)
        self.assertEqual(self.Body.Tip, pockets[0])
        self.assertIsNotNone(FreeCADGui.Control.activeDialog())

        # cancel restores the pad
        self.closeTaskDialog(accept=False)
        self.assertIsNotNone(self.Doc.getObject("Pad"))
        self.assertEqual(len(self.Doc.findObjects("PartDesign::Pocket")), 0)

    def testNewBody(self):
        self.Doc.openTransaction("Edit Pad")
        FreeCADGui.ActiveDocument.setEdit(self.Pad)
        self.chooseOperation(self.NEW_BODY)
        self.closeTaskDialog(accept=True)

        bodies = self.Doc.findObjects("PartDesign::Body")
        self.assertEqual(len(bodies), 2)
        newBody = [b for b in bodies if b != self.Body][0]
        self.assertIn(self.Pad, newBody.Group)
        self.assertNotIn(self.Pad, self.Body.Group)
        self.assertIn(self.Sketch, self.Body.Group)
        self.assertTrue(self.Pad.Profile[0].isDerivedFrom("PartDesign::SubShapeBinder"))
        self.assertAlmostEqual(self.Pad.Shape.Volume, 700)


# class PartDesignGuiTestCases(unittest.TestCase):
#   def setUp(self):
#       self.Doc = FreeCAD.newDocument("SketchGuiTest")
#
#   def testBoxCase(self):
#       self.Box = self.Doc.addObject('PartDesign::SketchObject','SketchBox')
#       self.Box.addGeometry(Part.LineSegment(App.Vector(-99.230339,36.960674,0),App.Vector(69.432587,36.960674,0)))
#       self.Box.addGeometry(Part.LineSegment(App.Vector(69.432587,36.960674,0),App.Vector(69.432587,-53.196629,0)))
#       self.Box.addGeometry(Part.LineSegment(App.Vector(69.432587,-53.196629,0),App.Vector(-99.230339,-53.196629,0)))
#       self.Box.addGeometry(Part.LineSegment(App.Vector(-99.230339,-53.196629,0),App.Vector(-99.230339,36.960674,0)))
#
#   def tearDown(self):
#       #closing doc
#       FreeCAD.closeDocument("SketchGuiTest")


class TestShapeBinder(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("PartDesignTestShapeBinder")

    def testDefaultColor(self):
        """
        A shape binder uses a different default color than a Part feature.
        This color must still be set after its creation.
        """
        self.Body = self.Doc.addObject("PartDesign::Body", "Body")
        self.Box = self.Doc.addObject("PartDesign::AdditiveBox", "Box")
        self.Body.addObject(self.Box)
        self.Doc.recompute()
        binder = self.Doc.addObject("PartDesign::ShapeBinder", "ShapeBinder")
        binder.Support = [(self.Box, "Face1")]

        grp = App.ParamGet("User parameter:BaseApp/Preferences/Mod/PartDesign")
        packed_color = grp.GetUnsigned("DefaultDatumColor", 0xFFD70099)
        r, g, b, a = binder.ViewObject.ShapeColor
        color = (
            int(r * 255.0 + 0.5) << 24
            | int(g * 255.0 + 0.5) << 16
            | int(b * 255.0 + 0.5) << 8
            | int(a * 255.0 + 0.5)
        )

        self.assertEqual(packed_color, color)

    def tearDown(self):
        FreeCAD.closeDocument(self.Doc.Name)


class TestSubShapeBinder(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("PartDesignTestSubShapeBinder")

    def tearDown(self):
        FreeCAD.closeDocument(self.Doc.Name)

    def testDefaultColor(self):
        """
        A sub-shape binder uses a different default color than a Part feature.
        This color must still be set after its creation.
        """
        body = self.Doc.addObject("PartDesign::Body", "Body")
        box = self.Doc.addObject("PartDesign::AdditiveBox", "Box")
        body.addObject(box)

        self.Doc.recompute()
        binder = body.newObject("PartDesign::SubShapeBinder", "Binder")
        binder.Support = [(box, ("Face1"))]

        grp = App.ParamGet("User parameter:BaseApp/Preferences/Mod/PartDesign")
        packed_color = grp.GetUnsigned("DefaultDatumColor", 0xFFD70099)
        r, g, b, a = binder.ViewObject.ShapeColor
        color = (
            int(r * 255.0 + 0.5) << 24
            | int(g * 255.0 + 0.5) << 16
            | int(b * 255.0 + 0.5) << 8
            | int(a * 255.0 + 0.5)
        )

        self.assertEqual(packed_color, color)


class TestDatumPlane(unittest.TestCase):
    def setUp(self):
        self.Doc = FreeCAD.newDocument("PartDesignTestDatumPlane")

    def tearDown(self):
        FreeCAD.closeDocument(self.Doc.Name)

    def testDefaultColor(self):
        """
        A datum object uses a different default color than a Part feature.
        This color must still be set after its creation.
        """
        body = self.Doc.addObject("PartDesign::Body", "Body")
        box = self.Doc.addObject("PartDesign::AdditiveBox", "Box")
        body.addObject(box)

        self.Doc.recompute()
        datum = body.newObject("PartDesign::Plane", "DatumPlane")
        datum.AttachmentSupport = [(box, "Face6")]
        datum.MapMode = "FlatFace"
        self.Doc.recompute()

        grp = App.ParamGet("User parameter:BaseApp/Preferences/Mod/PartDesign")
        packed_color = grp.GetUnsigned("DefaultDatumColor", 0xFFD70099)
        r, g, b, a = datum.ViewObject.ShapeColor
        color = (
            int(r * 255.0 + 0.5) << 24
            | int(g * 255.0 + 0.5) << 16
            | int(b * 255.0 + 0.5) << 8
            | int(a * 255.0 + 0.5)
        )

        self.assertEqual(packed_color, color)
