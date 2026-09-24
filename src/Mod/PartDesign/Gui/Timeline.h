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
#include <string>

#include <QListWidget>
#include <QTimer>
#include <QWidget>
#include <fastsignals/signal.h>

#include <Mod/PartDesign/PartDesignGlobal.h>

class QLabel;

namespace App
{
class DocumentObject;
}

namespace Gui
{
class Document;
}

namespace PartDesign
{
class Body;
}

namespace PartDesignGui
{

/**
 * Horizontal list of the features of a body with a rollback marker placed after the tip.
 * The marker can be dragged between the features.
 */
class TimelineList: public QListWidget
{
    Q_OBJECT

public:
    explicit TimelineList(QWidget* parent = nullptr);

    /// The marker is placed after the first @a slot items
    void setMarkerSlot(int slot);
    int markerSlot() const
    {
        return marker;
    }

Q_SIGNALS:
    void markerMoved(int slot);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int slotAt(int x) const;
    int markerPosition(int slot) const;

    int marker = 0;
    int dragSlot = -1;
};

/**
 * The timeline of the active body, like in other CAD programs: shows the features in the
 * order of creation, allows to roll back the model by moving the marker, and to edit a
 * feature by double-clicking it.
 */
class PartDesignGuiExport TimelineView: public QWidget
{
    Q_OBJECT

public:
    explicit TimelineView(QWidget* parent = nullptr);
    ~TimelineView() override;

    /// The name the dock window is registered with
    static constexpr const char* DockName = "PartDesign_Timeline";

public Q_SLOTS:
    /// Moves the tip of the shown body to the last solid feature of the first @a slot items
    void rollTo(int slot);
    /// Moves the tip to the last solid feature
    void rollToEnd();

private:
    void attachDocument(const Gui::Document& doc);
    void scheduleRebuild();
    void rebuild();
    void onWorkbenchActivated(const char* name);

    PartDesign::Body* shownBody() const;
    App::DocumentObject* objectOf(const QListWidgetItem* item) const;
    void moveTip(App::DocumentObject* target);

    void onItemClicked(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);
    void showContextMenu(const QPoint& pos);

    TimelineList* list;
    QLabel* placeholder;
    QTimer rebuildTimer;
    std::string bodyDocument;
    std::string bodyName;

    fastsignals::scoped_connection connectNewDocument;
    fastsignals::scoped_connection connectDeleteDocument;
    fastsignals::scoped_connection connectActiveDocument;
    fastsignals::scoped_connection connectWorkbench;
    fastsignals::scoped_connection connectNewObject;
    fastsignals::scoped_connection connectDeletedObject;
    fastsignals::scoped_connection connectChangedObject;
    fastsignals::scoped_connection connectRecomputed;
    fastsignals::scoped_connection connectUndo;
    fastsignals::scoped_connection connectRedo;
    std::map<const Gui::Document*, fastsignals::scoped_connection> connectActivatedViewProvider;
};

}  // namespace PartDesignGui
