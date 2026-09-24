// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include <QApplication>
#include <QTimer>

#include <App/Application.h>
#include <App/Document.h>
#include <Gui/ActiveObjectList.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/MDIView.h>
#include <Gui/MenuManager.h>
#include <Gui/ToolBarManager.h>
#include <Gui/WorkbenchManager.h>
#include <Mod/PartDesign/App/Body.h>
#include <Mod/Sketcher/Gui/ViewProviderSketch.h>
#include <Mod/Sketcher/Gui/Workbench.h>

#include "DesignWorkbench.h"

using namespace PartDesignGui;

#if 0  // needed for Qt's lupdate utility
    qApp->translate("Workbench", "Design");
    qApp->translate("Workbench", "&Assembly");
    qApp->translate("Workbench", "&Construct");
    qApp->translate("Workbench", "Assembly Joints");
#endif

namespace
{
// Toolbars only offered while a body is active
QStringList bodyToolbarNames()
{
    return {
        QStringLiteral("Part Design Helper Features"),
        QStringLiteral("Part Design Modeling Features"),
        QStringLiteral("Part Design Dress-Up Features"),
        QStringLiteral("Part Design Transformation Features"),
    };
}

// Toolbars only offered while an assembly is being edited
QStringList assemblyToolbarNames()
{
    return {QStringLiteral("Assembly Joints")};
}

// Toolbars hidden while a sketch is being edited
QStringList designToolbarNames()
{
    return {QStringLiteral("Design")};
}

std::vector<std::string> jointCommands()
{
    return {
        "Assembly_ToggleGrounded",
        "Assembly_CreateJointRigidGroup",
        "Separator",
        "Assembly_CreateJointFixed",
        "Assembly_CreateJointRevolute",
        "Assembly_CreateJointCylindrical",
        "Assembly_CreateJointSlider",
        "Assembly_CreateJointBall",
        "Separator",
        "Assembly_CreateJointDistance",
        "Assembly_CreateJointParallel",
        "Assembly_CreateJointPerpendicular",
        "Assembly_CreateJointAngle",
        "Separator",
        "Assembly_CreateJointRackPinion",
        "Assembly_CreateJointScrew",
        "Assembly_CreateJointGearBelt",
    };
}
}  // namespace

/// @namespace PartDesignGui @class DesignWorkbench
TYPESYSTEM_SOURCE(PartDesignGui::DesignWorkbench, PartDesignGui::Workbench)

DesignWorkbench::DesignWorkbench() = default;

DesignWorkbench::~DesignWorkbench() = default;

bool DesignWorkbench::isActive()
{
    auto workbench = Gui::WorkbenchManager::instance()->active();
    return workbench && workbench->name() == WorkbenchName;
}

void DesignWorkbench::activated()
{
    Workbench::activated();

    auto app = Gui::Application::Instance;
    // clang-format off
    connectNewDocument = app->signalNewDocument.connect(
        [this](const Gui::Document& doc, bool) { attachDocument(doc); });
    connectDeleteDocument = app->signalDeleteDocument.connect(
        [this](const Gui::Document& doc) {
            connectActivatedViewProvider.erase(&doc);
            scheduleUpdate();
        });
    connectActiveDocument = app->signalActiveDocument.connect(
        [this](const Gui::Document&) { scheduleUpdate(); });
    connectInEdit = app->signalInEdit.connect(
        [this](const Gui::ViewProviderDocumentObject&) { scheduleUpdate(); });
    connectResetEdit = app->signalResetEdit.connect(
        [this](const Gui::ViewProviderDocumentObject&) { scheduleUpdate(); });
    // clang-format on

    for (auto doc : App::GetApplication().getDocuments()) {
        if (auto guiDoc = app->getDocument(doc)) {
            attachDocument(*guiDoc);
        }
    }

    updateContextToolbars();
}

void DesignWorkbench::deactivated()
{
    connectNewDocument.disconnect();
    connectDeleteDocument.disconnect();
    connectActiveDocument.disconnect();
    connectInEdit.disconnect();
    connectResetEdit.disconnect();
    connectActivatedViewProvider.clear();

    Workbench::deactivated();
}

void DesignWorkbench::attachDocument(const Gui::Document& doc)
{
    connectActivatedViewProvider[&doc] = doc.signalActivatedViewProvider.connect(
        [this](const Gui::ViewProviderDocumentObject*, const char*) { scheduleUpdate(); }
    );
}

void DesignWorkbench::scheduleUpdate()
{
    // Active objects and edit mode are updated in several steps, therefore only
    // look at them once the current event has been processed completely
    if (updatePending) {
        return;
    }
    updatePending = true;
    QTimer::singleShot(0, qApp, [this]() {
        updatePending = false;
        if (isActive()) {
            updateContextToolbars();
        }
    });
}

void DesignWorkbench::updateContextToolbars()
{
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    Gui::MDIView* view = Gui::Application::Instance->activeView();

    bool sketchInEdit = doc && dynamic_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    bool bodyActive = view && view->getActiveObject<PartDesign::Body*>(PDBODYKEY);
    bool assemblyActive = view && view->getActiveObject<App::DocumentObject*>(ASSEMBLYKEY);

    using State = Gui::ToolBarManager::State;
    auto toolbars = Gui::ToolBarManager::getInstance();
    auto availableIf = [](bool condition) {
        return condition ? State::ForceAvailable : State::ForceHidden;
    };

    // While sketching, the sketcher manages its own toolbars
    toolbars->setState(
        designToolbarNames(),
        sketchInEdit ? State::ForceHidden : State::RestoreDefault
    );
    toolbars->setState(bodyToolbarNames(), availableIf(bodyActive && !sketchInEdit));
    toolbars->setState(assemblyToolbarNames(), availableIf(assemblyActive && !sketchInEdit));
}

Gui::MenuItem* DesignWorkbench::setupMenuBar() const
{
    Gui::MenuItem* root = Workbench::setupMenuBar();
    Gui::MenuItem* windows = root->findItem("&Windows");

    Gui::MenuItem* joints = new Gui::MenuItem;
    joints->setCommand("Assembly Joints");
    for (const auto& cmd : jointCommands()) {
        *joints << cmd;
    }

    Gui::MenuItem* assembly = new Gui::MenuItem;
    root->insertItem(windows, assembly);
    assembly->setCommand("&Assembly");
    *assembly << "Assembly_CreateAssembly"
              << "Assembly_Insert"
              << "Assembly_SolveAssembly"
              << "Separator"
              << "Assembly_CreateView"
              << "Assembly_CreateSnapshot"
              << "Assembly_CreateSimulation"
              << "Assembly_CreateBom"
              << "Separator" << joints << "Separator"
              << "Assembly_LinkSelectLinked"
              << "Assembly_SelectJointsOfComponent"
              << "Assembly_ExportASMT";

    Gui::MenuItem* construct = new Gui::MenuItem;
    root->insertItem(windows, construct);
    construct->setCommand("&Construct");
    *construct << "Part_OffsetPlane"
               << "Part_PlaneAtAngle"
               << "Part_TangentPlane"
               << "Part_MidPlane"
               << "Part_PlaneThroughTwoEdges"
               << "Part_PlaneThroughThreePoints"
               << "Part_PlaneTangentAtPoint"
               << "Part_PlaneAlongPath"
               << "Separator"
               << "Part_DatumLine"
               << "Part_DatumPoint"
               << "Part_CoordinateSystem";

    return root;
}

Gui::ToolBarItem* DesignWorkbench::setupToolBars() const
{
    Gui::ToolBarItem* root = Workbench::setupToolBars();

    // The part design toolbars only make sense while a body is active
    const auto bodyToolbars = bodyToolbarNames();
    for (auto item : root->getItems()) {
        if (bodyToolbars.contains(QString::fromStdString(item->command()))) {
            item->visibilityPolicy = Gui::ToolBarItem::DefaultVisibility::Unavailable;
        }
    }

    // Always available: create the building blocks of a design
    Gui::ToolBarItem* design = new Gui::ToolBarItem;
    design->setCommand("Design");
    *design << "Assembly_CreateAssembly"
            << "Assembly_Insert"
            << "Std_Part"
            << "PartDesign_Body"
            << "Separator"
            << "PartDesign_NewSketch"
            << "Part_ConstructionPlanes"
            << "Separator"
            << "Assembly_SolveAssembly"
            << "Assembly_CreateBom";
    // Place it right after the standard toolbars, before the part design ones
    if (!root->insertItem(root->findItem("Part Design Helper Features"), design)) {
        root->appendItem(design);
    }

    Gui::ToolBarItem* joints
        = new Gui::ToolBarItem(root, Gui::ToolBarItem::DefaultVisibility::Unavailable);
    joints->setCommand("Assembly Joints");
    for (const auto& cmd : jointCommands()) {
        *joints << cmd;
    }

    // Sketcher edit mode toolbars, shown and hidden by the sketcher itself
    using Visibility = Gui::ToolBarItem::DefaultVisibility;
    auto addSketcherToolbar = [root](const char* name, auto fill) {
        Gui::ToolBarItem* item = new Gui::ToolBarItem(root, Visibility::Unavailable);
        item->setCommand(name);
        fill(*item);
    };
    addSketcherToolbar("Edit Mode", [](Gui::ToolBarItem& item) {
        SketcherGui::addSketcherWorkbenchSketchEditModeActions(item);
    });
    addSketcherToolbar("Geometries", [](Gui::ToolBarItem& item) {
        SketcherGui::addSketcherWorkbenchGeometries(item);
    });
    addSketcherToolbar("Constraints", [](Gui::ToolBarItem& item) {
        SketcherGui::addSketcherWorkbenchConstraints(item);
    });
    addSketcherToolbar("Sketcher Tools", [](Gui::ToolBarItem& item) {
        SketcherGui::addSketcherWorkbenchTools(item);
    });
    addSketcherToolbar("B-Spline Tools", [](Gui::ToolBarItem& item) {
        SketcherGui::addSketcherWorkbenchBSplines(item);
    });
    addSketcherToolbar("Visual Helpers", [](Gui::ToolBarItem& item) {
        SketcherGui::addSketcherWorkbenchVisual(item);
    });

    return root;
}
