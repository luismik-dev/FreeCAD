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


#include <Inventor/SoPickedPoint.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <QApplication>

#include "Navigation/NavigationStyle.h"
#include "View3DInventorViewer.h"


using namespace Gui;

// ----------------------------------------------------------------------------------

/* TRANSLATOR Gui::FusionNavigationStyle */

TYPESYSTEM_SOURCE(Gui::FusionNavigationStyle, Gui::RevitNavigationStyle)

FusionNavigationStyle::FusionNavigationStyle() = default;

FusionNavigationStyle::~FusionNavigationStyle() = default;

std::string FusionNavigationStyle::userFriendlyName() const
{
    // do not mark this for translation
    return "Fusion/Inventor";
}

void FusionNavigationStyle::pinRotationCenter(const SbVec2s& pos)
{
    // Do not pick the indicator of the pinned center itself
    viewer->showRotationCenter(false);

    SoRayPickAction action(viewer->getSoRenderManager()->getViewportRegion());
    action.setPoint(pos);
    action.setRadius(viewer->getPickRadius());
    action.apply(viewer->getSoRenderManager()->getSceneGraph());

    if (SoPickedPoint* picked = action.getPickedPoint()) {
        pinnedCenter = picked->getPoint();
        showPinnedRotationCenter();
    }
    else {
        pinnedCenter.reset();
    }
}

void FusionNavigationStyle::showPinnedRotationCenter()
{
    if (!pinnedCenter) {
        return;
    }
    setRotationCenter(*pinnedCenter);
    viewer->showRotationCenter(true);
    viewer->changeRotationCenterPosition(*pinnedCenter);
}

void FusionNavigationStyle::saveCursorPosition(const SoEvent* const ev)
{
    inherited::saveCursorPosition(ev);
    if (pinnedCenter) {
        setRotationCenter(*pinnedCenter);
    }
}

SbBool FusionNavigationStyle::processSoEvent(const SoEvent* const ev)
{
    const ViewerMode oldMode = currentmode;
    const float doubleClickInterval = float(QApplication::doubleClickInterval()) / 1000.0F;

    // A click with the middle button: released soon after being pressed, without moving
    bool middleClick = false;
    if (ev->isOfType(SoMouseButtonEvent::getClassTypeId()) && !isSeekMode()) {
        const auto event = static_cast<const SoMouseButtonEvent*>(ev);
        if (event->getButton() == SoMouseButtonEvent::BUTTON3
            && event->getState() == SoButtonEvent::UP) {
            middleClick = !lockrecenter && !shiftdown && !ctrldown
                && (ev->getTime() - centerTime).getValue() < doubleClickInterval;
            // Unlike in the Revit style, a click does not center the view at the point
            lockrecenter = true;
        }
    }

    SbBool processed = inherited::processSoEvent(ev);

    if (middleClick) {
        const SbVec2s pos = ev->getPosition();
        const SbVec2s moved = pos - lastMiddleClickPos;
        constexpr int maxMove = 5;
        const bool doubleClick = lastMiddleClickTime != SbTime::zero()
            && (ev->getTime() - lastMiddleClickTime).getValue() < doubleClickInterval
            && std::abs(moved[0]) <= maxMove && std::abs(moved[1]) <= maxMove;
        if (doubleClick) {
            pinRotationCenter(pos);
            lastMiddleClickTime = SbTime::zero();
        }
        else {
            lastMiddleClickTime = ev->getTime();
            lastMiddleClickPos = pos;
        }
        processed = true;
    }

    // Orbiting hides the indicator when it ends, keep showing the pinned center
    if (pinnedCenter && oldMode != currentmode
        && (oldMode == DRAGGING || oldMode == SPINNING)) {
        showPinnedRotationCenter();
    }

    return processed;
}
