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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Gui/DocumentObserver.h>
#include <Gui/Selection/Selection.h>
#include <Gui/TaskView/TaskDialog.h>
#include <Gui/TaskView/TaskView.h>
#include <Mod/Part/App/ConstructionPlane.h>
#include <Mod/Part/PartGlobal.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace Part
{
class AttachExtension;
}

namespace Gui
{
class GizmoContainer;
class LinearGizmo;
class QuantitySpinBox;
class RadialGizmo;
class ViewProviderDocumentObject;
}  // namespace Gui

namespace PartGui
{

/// Task panel of a construction plane: its type, references and offset or angle
class PartGuiExport TaskConstructionPlane: public Gui::TaskView::TaskBox,
                                           public Gui::SelectionObserver
{
    Q_OBJECT

public:
    using Type = Part::ConstructionPlane::Type;

    TaskConstructionPlane(Gui::ViewProviderDocumentObject* vp, std::optional<Type> type);
    ~TaskConstructionPlane() override;

    Type getType() const
    {
        return type;
    }
    /// Whether the references make a plane
    bool isAttached() const;

Q_SIGNALS:
    void advancedRequested();

private:
    void onSelectionChanged(const Gui::SelectionChanges& msg) override;

    void onTypeChanged(int index);
    void onValueChanged(double value);
    void onReversedChanged(bool on);
    void onClear();

    void handleInitialSelection();
    void toggleReference(App::DocumentObject* obj, std::string sub);
    void setType(Type newType);
    /// Writes references and attachment mode to the plane and recomputes it
    void applyReferences();
    void updatePreview();
    void updateReferenceList();
    void updateValueField();
    std::vector<Attacher::eRefType> referenceTypes() const;

    void setupGizmos();
    void setGizmoPositions();

    Part::AttachExtension* getAttachExtension() const;

private:
    Gui::ViewProviderWeakPtrT vpWeak;
    App::DocumentObjectT planeT;
    Type type;

    QComboBox* typeCombo {nullptr};
    QLabel* hintLabel {nullptr};
    QListWidget* referenceList {nullptr};
    QPushButton* clearButton {nullptr};
    QLabel* valueLabel {nullptr};
    Gui::QuantitySpinBox* valueEdit {nullptr};
    QCheckBox* reversedCheck {nullptr};
    QLabel* statusLabel {nullptr};
    QPushButton* advancedButton {nullptr};

    Gui::LinearGizmo* distanceGizmo {nullptr};
    Gui::RadialGizmo* angleGizmo {nullptr};
    std::unique_ptr<Gui::GizmoContainer> gizmoContainer;
};

/// Task dialog editing a construction plane
class PartGuiExport TaskDlgConstructionPlane: public Gui::TaskView::TaskDialog
{
    Q_OBJECT

public:
    TaskDlgConstructionPlane(
        Gui::ViewProviderDocumentObject* vp,
        std::optional<Part::ConstructionPlane::Type> type
    );

    void open() override;
    bool accept() override;
    bool reject() override;

    QDialogButtonBox::StandardButtons getStandardButtons() const override
    {
        return QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
    }

private:
    void openAdvanced();

private:
    Gui::ViewProviderWeakPtrT vpWeak;
    App::DocumentObjectT planeT;
    TaskConstructionPlane* parameter;
};

/// Whether the task panel of construction planes can edit the datum plane
PartGuiExport bool canEditAsConstructionPlane(const App::DocumentObject* plane);

/// Puts a datum plane in edit mode with the construction plane task panel. A new plane
/// starts as the given type, unless the selection makes another one.
PartGuiExport void editConstructionPlane(
    App::DocumentObject* plane,
    std::optional<Part::ConstructionPlane::Type> type = std::nullopt
);

/// The type passed to editConstructionPlane(), taken by the view provider entering edit mode
PartGuiExport std::optional<Part::ConstructionPlane::Type> takePendingConstructionPlaneType();

/// The name of a construction plane type for the user
PartGuiExport QString constructionPlaneTypeName(Part::ConstructionPlane::Type type);

}  // namespace PartGui
