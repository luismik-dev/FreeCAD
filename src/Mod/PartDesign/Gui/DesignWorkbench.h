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

#pragma once

#include <map>
#include <fastsignals/signal.h>

#include "Workbench.h"

namespace Gui
{
class Document;
}

namespace PartDesignGui
{

/**
 * A workbench combining part modeling, sketching and assembling, so that parts can be designed
 * in the context of an assembly without switching workbenches. The toolbars follow the context:
 * the modeling tools are offered while a body is active, the joint tools while an assembly is
 * being edited and the sketcher tools while a sketch is being edited.
 */
class PartDesignGuiExport DesignWorkbench: public Workbench
{
    TYPESYSTEM_HEADER_WITH_OVERRIDE();

public:
    DesignWorkbench();
    ~DesignWorkbench() override;

    /// The name the workbench is registered with
    static constexpr const char* WorkbenchName = "DesignWorkbench";
    /// Returns true if this workbench is the active one
    static bool isActive();

    void activated() override;
    void deactivated() override;

protected:
    Gui::MenuItem* setupMenuBar() const override;
    Gui::ToolBarItem* setupToolBars() const override;

private:
    void attachDocument(const Gui::Document& doc);
    void scheduleUpdate();
    void updateContextToolbars();

    fastsignals::scoped_connection connectNewDocument;
    fastsignals::scoped_connection connectDeleteDocument;
    fastsignals::scoped_connection connectActiveDocument;
    fastsignals::scoped_connection connectInEdit;
    fastsignals::scoped_connection connectResetEdit;
    // A body edited in the context of an assembly may live in another document, therefore
    // active object changes are tracked in all open documents
    std::map<const Gui::Document*, fastsignals::scoped_connection> connectActivatedViewProvider;
    bool updatePending = false;
};

}  // namespace PartDesignGui
