// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   Copyright (c) 2024 Ondsel (PL Boyer) <development@ondsel.com>         *
 *                                                                         *
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
#include <App/DocumentObjectGroup.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/Control.h>
#include <Gui/View3DInventor.h>
#include <Gui/View3DInventorViewer.h>
#include <Gui/Inventor/SoAxisCrossKit.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Mod/Part/App/AttachExtension.h>
#include <Mod/Part/App/PartFeature.h>

#include "TaskConstructionPlane.h"
#include "ViewProviderDatum.h"


using namespace PartGui;

PROPERTY_SOURCE_WITH_EXTENSIONS(PartGui::ViewProviderLine, Gui::ViewProviderLine)

ViewProviderLine::ViewProviderLine()
{
    PartGui::ViewProviderAttachExtension::initExtension(this);
}

bool ViewProviderLine::doubleClicked()
{
    showAttachmentEditor();
    return true;
}

PROPERTY_SOURCE_WITH_EXTENSIONS(PartGui::ViewProviderPlane, Gui::ViewProviderPlane)

const char* ViewProviderPlane::SizeModeEnums[] = {"Screen", "Model", nullptr};

ViewProviderPlane::ViewProviderPlane()
{
    PartGui::ViewProviderAttachExtension::initExtension(this);

    ADD_PROPERTY_TYPE(
        SizeMode,
        (0L),
        "Display Options",
        App::Prop_None,
        "Screen: the plane has a fixed size on the screen.\n"
        "Model: the plane covers its references in the model."
    );
    SizeMode.setEnums(SizeModeEnums);
}

void ViewProviderPlane::onChanged(const App::Property* prop)
{
    if (prop == &SizeMode && pCoords) {
        updatePlaneSize();
    }
    Gui::ViewProviderPlane::onChanged(prop);
}

void ViewProviderPlane::updateData(const App::Property* prop)
{
    auto geo = freecad_cast<App::GeoFeature*>(getObject());
    if (geo && prop == &geo->Placement && pCoords) {
        updatePlaneSize();
    }
    Gui::ViewProviderPlane::updateData(prop);
}

void ViewProviderPlane::updatePlaneSize()
{
    auto plane = freecad_cast<App::GeoFeature*>(getObject());
    auto attach = plane ? plane->getExtensionByType<Part::AttachExtension>(true) : nullptr;
    displayCenter = Base::Vector3d();
    if (SizeMode.getValue() != 1 || !attach || attach->AttachmentSupport.getValues().empty()) {
        Gui::ViewProviderPlane::updatePlaneSize();
        return;
    }

    // The bounding box of the references in the coordinates of the plane
    Base::Placement toPlane = plane->Placement.getValue().inverse();
    Base::BoundBox3d box;
    const auto& objs = attach->AttachmentSupport.getValues();
    const auto& subs = attach->AttachmentSupport.getSubValues();
    for (std::size_t i = 0; i < objs.size(); ++i) {
        Part::TopoShape shape;
        try {
            shape = Part::Feature::getTopoShape(
                objs[i],
                Part::ShapeOption::NeedSubElement | Part::ShapeOption::ResolveLink
                    | Part::ShapeOption::Transform,
                subs[i].c_str()
            );
        }
        catch (...) {
            continue;
        }
        if (shape.isNull()) {
            continue;
        }
        Base::BoundBox3d refBox = shape.getBoundBox();
        for (unsigned short corner = 0; corner < 8; ++corner) {
            Base::Vector3d point = refBox.CalcPoint(corner);
            toPlane.multVec(point, point);
            box.Add(point);
        }
    }
    if (!box.IsValid()) {
        Gui::ViewProviderPlane::updatePlaneSize();
        return;
    }

    // References seen edge-on, like a path the plane is normal to, get a square plane
    double diagonal = box.CalcDiagonalLength();
    double minSize = std::max(diagonal * 0.5, 1.0);
    double margin = std::max(box.LengthX(), box.LengthY()) * 0.1 + 1.0;
    Base::Vector3d center = box.GetCenter();
    displayCenter = Base::Vector3d(center.x, center.y, 0.0);
    double halfX = std::max(box.LengthX() / 2.0 + margin, minSize / 2.0);
    double halfY = std::max(box.LengthY() / 2.0 + margin, minSize / 2.0);
    auto x0 = static_cast<float>(center.x - halfX);
    auto x1 = static_cast<float>(center.x + halfX);
    auto y0 = static_cast<float>(center.y - halfY);
    auto y1 = static_cast<float>(center.y + halfY);

    soScale->active = false;
    SbVec3f verts[4] = {
        SbVec3f(x1, y1, 0),
        SbVec3f(x1, y0, 0),
        SbVec3f(x0, y0, 0),
        SbVec3f(x0, y1, 0),
    };
    pTextTranslation->translation.setValue(verts[0]);
    pCoords->point.setNum(4);
    pCoords->point.setValues(0, 4, verts);
}

bool ViewProviderPlane::doubleClicked()
{
    if (canEditAsConstructionPlane(getObject())) {
        editConstructionPlane(getObject());
    }
    else {
        showAttachmentEditor();
    }
    return true;
}

bool ViewProviderPlane::setEdit(int ModNum)
{
    // New construction planes and planes made by one are edited in the construction plane panel
    auto type = takePendingConstructionPlaneType();
    if (ModNum == ViewProvider::Default && (type || canEditAsConstructionPlane(getObject()))) {
        Gui::Control().showDialog(new TaskDlgConstructionPlane(this, type));
        return true;
    }
    return Gui::ViewProviderPlane::setEdit(ModNum);
}

void ViewProviderPlane::unsetEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default) {
        Gui::Control().closeDialog();
        return;
    }
    Gui::ViewProviderPlane::unsetEdit(ModNum);
}


PROPERTY_SOURCE_WITH_EXTENSIONS(PartGui::ViewProviderPoint, Gui::ViewProviderPoint)

ViewProviderPoint::ViewProviderPoint()
{
    PartGui::ViewProviderAttachExtension::initExtension(this);
}

bool ViewProviderPoint::doubleClicked()
{
    showAttachmentEditor();
    return true;
}


PROPERTY_SOURCE_WITH_EXTENSIONS(PartGui::ViewProviderLCS, Gui::ViewProviderCoordinateSystem)

ViewProviderLCS::ViewProviderLCS()
{
    PartGui::ViewProviderAttachExtension::initExtension(this);
}

bool ViewProviderLCS::doubleClicked()
{
    showAttachmentEditor();
    return true;
}
