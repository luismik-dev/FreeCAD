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

#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/GroupExtension.h>
#include <Gui/ActiveObjectList.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/MDIView.h>

#include "TaskConstructionPlane.h"

using namespace PartGui;
namespace CP = Part::ConstructionPlane;

namespace
{

// The container a new construction plane goes into: the active body, else the active
// assembly or part, whichever is nested deeper
App::DocumentObject* activeContainer()
{
    Gui::MDIView* view = Gui::Application::Instance->activeView();
    if (!view) {
        return nullptr;
    }
    if (auto body = view->getActiveObject<App::DocumentObject*>(PDBODYKEY)) {
        return body;
    }
    auto part = view->getActiveObject<App::DocumentObject*>(PARTKEY);
    auto assembly = view->getActiveObject<App::DocumentObject*>(ASSEMBLYKEY);
    if (part && assembly) {
        auto group = part->getExtensionByType<App::GroupExtension>(true);
        return group && group->hasObject(assembly, true) ? assembly : part;
    }
    return part ? part : assembly;
}

}  // namespace

/// Creates a datum plane and edits it in the construction plane task panel
class CmdPartConstructionPlane: public Gui::Command
{
public:
    CmdPartConstructionPlane(
        const char* name,
        CP::Type type,
        const char* menuText,
        const char* toolTip,
        const char* objectName
    )
        : Command(name)
        , type(type)
        , objectName(objectName)
    {
        sGroup = "Part";
        sMenuText = menuText;
        sToolTipText = toolTip;
        sWhatsThis = name;
        sStatusTip = toolTip;
        sPixmap = name;
    }

    const char* className() const override
    {
        return "CmdPartConstructionPlane";
    }

protected:
    void activated(int /*iMsg*/) override
    {
        openCommand(QT_TRANSLATE_NOOP("Command", "Create construction plane"));
        std::string name = getUniqueObjectName(objectName);
        doCommand(Doc, "obj = App.ActiveDocument.addObject('Part::DatumPlane', '%s')", name.c_str());
        doCommand(Gui, "Gui.ActiveDocument.getObject('%s').SizeMode = 'Model'", name.c_str());
        if (App::DocumentObject* container = activeContainer()) {
            doCommand(
                Doc,
                "App.ActiveDocument.getObject('%s').addObject(obj)",
                container->getNameInDocument()
            );
        }
        App::DocumentObject* plane = getDocument()->getObject(name.c_str());
        if (!plane) {
            abortCommand();
            return;
        }
        editConstructionPlane(plane, type);
    }

    bool isActive() override
    {
        return hasActiveDocument();
    }

private:
    CP::Type type;
    const char* objectName;
};

/// All construction planes in a drop-down button
class CmdPartConstructionPlanes: public Gui::GroupCommand
{
public:
    CmdPartConstructionPlanes()
        : GroupCommand("Part_ConstructionPlanes")
    {
        sGroup = "Part";
        sMenuText = QT_TR_NOOP("Construction Plane");
        sToolTipText = QT_TR_NOOP("Creates a construction plane");
        sWhatsThis = "Part_ConstructionPlanes";
        sStatusTip = sToolTipText;

        setCheckable(false);
        setRememberLast(true);

        addCommand("Part_OffsetPlane");
        addCommand("Part_PlaneAtAngle");
        addCommand("Part_TangentPlane");
        addCommand("Part_MidPlane");
        addCommand("Part_PlaneThroughTwoEdges");
        addCommand("Part_PlaneThroughThreePoints");
        addCommand("Part_PlaneTangentAtPoint");
        addCommand("Part_PlaneAlongPath");
    }

    const char* className() const override
    {
        return "CmdPartConstructionPlanes";
    }

    bool isActive() override
    {
        return hasActiveDocument();
    }
};

void CreateConstructionPlaneCommands()
{
    Gui::CommandManager& rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_OffsetPlane",
        CP::Type::Offset,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Offset Plane"),
        QT_TRANSLATE_NOOP(
            "CmdPartConstructionPlane",
            "Creates a plane at a distance from a planar face, a plane or a sketch"
        ),
        "OffsetPlane"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_PlaneAtAngle",
        CP::Type::Angle,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Plane at Angle"),
        QT_TRANSLATE_NOOP(
            "CmdPartConstructionPlane",
            "Creates a plane through a straight edge or line, turned by an angle"
        ),
        "PlaneAtAngle"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_TangentPlane",
        CP::Type::Tangent,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Tangent Plane"),
        QT_TRANSLATE_NOOP(
            "CmdPartConstructionPlane",
            "Creates a plane tangent to a cylindrical or conical face, turned around its axis"
        ),
        "TangentPlane"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_MidPlane",
        CP::Type::MidPlane,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Midplane"),
        QT_TRANSLATE_NOOP(
            "CmdPartConstructionPlane",
            "Creates a plane midway between two planar faces or planes"
        ),
        "MidPlane"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_PlaneThroughTwoEdges",
        CP::Type::TwoEdges,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Plane Through Two Edges"),
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Creates a plane through two straight edges"),
        "PlaneThroughTwoEdges"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_PlaneThroughThreePoints",
        CP::Type::ThreePoints,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Plane Through Three Points"),
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Creates a plane through three points"),
        "PlaneThroughThreePoints"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_PlaneTangentAtPoint",
        CP::Type::TangentAtPoint,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Plane Tangent to Face at Point"),
        QT_TRANSLATE_NOOP(
            "CmdPartConstructionPlane",
            "Creates a plane tangent to a face at a point"
        ),
        "TangentPlane"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlane(
        "Part_PlaneAlongPath",
        CP::Type::AlongPath,
        QT_TRANSLATE_NOOP("CmdPartConstructionPlane", "Plane Along Path"),
        QT_TRANSLATE_NOOP(
            "CmdPartConstructionPlane",
            "Creates a plane normal to an edge, at a position along it"
        ),
        "PlaneAlongPath"
    ));
    rcCmdMgr.addCommand(new CmdPartConstructionPlanes());
}
