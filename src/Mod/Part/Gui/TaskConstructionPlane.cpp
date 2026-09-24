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

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <Standard_Failure.hxx>

#include <App/Datums.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/ElementNamingUtils.h>
#include <App/ObjectIdentifier.h>
#include <Base/Converter.h>
#include <Base/Tools.h>
#include <Gui/Application.h>
#include <Gui/CommandT.h>
#include <Gui/Control.h>
#include <Gui/Document.h>
#include <Gui/Inventor/Draggers/Gizmo.h>
#include <Gui/Inventor/Draggers/SoRotationDragger.h>
#include <Gui/QuantitySpinBox.h>
#include <Gui/Utilities.h>
#include <Gui/ViewProviderDragger.h>
#include <Mod/Part/App/AttachExtension.h>
#include <Mod/Part/App/DatumFeature.h>

#include "TaskAttacher.h"
#include "TaskConstructionPlane.h"
#include "ViewProviderAttachExtension.h"

using namespace PartGui;
using namespace Attacher;
namespace CP = Part::ConstructionPlane;

namespace
{
std::optional<CP::Type> pendingType;

int minReferences(CP::Type type)
{
    switch (type) {
        case CP::Type::MidPlane:
        case CP::Type::TwoEdges:
        case CP::Type::TangentAtPoint:
            return 2;
        case CP::Type::ThreePoints:
            return 3;
        default:
            return 1;
    }
}

QString selectionHint(CP::Type type)
{
    switch (type) {
        case CP::Type::Offset:
            return TaskConstructionPlane::tr("Select a planar face, a plane or a sketch.");
        case CP::Type::Angle:
            return TaskConstructionPlane::tr(
                "Select a straight edge or a line, and optionally a planar face that the plane "
                "is parallel to at zero angle."
            );
        case CP::Type::Tangent:
            return TaskConstructionPlane::tr(
                "Select a cylindrical or conical face, and optionally a planar face that the "
                "plane is parallel to at zero angle."
            );
        case CP::Type::MidPlane:
            return TaskConstructionPlane::tr("Select two planar faces or planes.");
        case CP::Type::TwoEdges:
            return TaskConstructionPlane::tr("Select two straight edges or lines.");
        case CP::Type::ThreePoints:
            return TaskConstructionPlane::tr("Select three points.");
        case CP::Type::TangentAtPoint:
            return TaskConstructionPlane::tr("Select a face and a point on it.");
        case CP::Type::AlongPath:
            return TaskConstructionPlane::tr("Select an edge, and optionally a point on it.");
    }
    return {};
}

eRefType shapeType(App::DocumentObject* obj, const std::string& sub)
{
    try {
        return AttachEngine::getShapeType(obj, sub);
    }
    catch (...) {
        return rtAnything;
    }
}

// Datum elements are referenced as a whole
bool isDatum(const App::DocumentObject* obj)
{
    return obj->isDerivedFrom<App::DatumElement>() || obj->isDerivedFrom<Part::Datum>();
}

/// Lets only references of the type through
class ReferenceGate: public Gui::SelectionGate
{
public:
    ReferenceGate(const App::DocumentObject* plane, CP::Type type)
        : plane(plane)
        , type(type)
    {}

    bool allow(App::Document* /*doc*/, App::DocumentObject* obj, const char* sub) override
    {
        if (!obj) {
            return false;
        }
        std::string subName = sub ? sub : "";
        App::DocumentObject* subObj = obj->getSubObject(subName.c_str());
        if (!subObj || subObj == plane) {
            return false;
        }
        if (isDatum(subObj)) {
            return CP::acceptsReference(type, shapeType(subObj, ""));
        }
        return CP::acceptsReference(type, shapeType(obj, subName));
    }

private:
    const App::DocumentObject* plane;
    CP::Type type;
};

}  // namespace

QString PartGui::constructionPlaneTypeName(CP::Type type)
{
    switch (type) {
        case CP::Type::Offset:
            return TaskConstructionPlane::tr("Offset plane");
        case CP::Type::Angle:
            return TaskConstructionPlane::tr("Plane at angle");
        case CP::Type::Tangent:
            return TaskConstructionPlane::tr("Tangent plane");
        case CP::Type::MidPlane:
            return TaskConstructionPlane::tr("Midplane");
        case CP::Type::TwoEdges:
            return TaskConstructionPlane::tr("Plane through two edges");
        case CP::Type::ThreePoints:
            return TaskConstructionPlane::tr("Plane through three points");
        case CP::Type::TangentAtPoint:
            return TaskConstructionPlane::tr("Plane tangent to face at point");
        case CP::Type::AlongPath:
            return TaskConstructionPlane::tr("Plane along path");
    }
    return {};
}

bool PartGui::canEditAsConstructionPlane(const App::DocumentObject* plane)
{
    auto attach = plane ? plane->getExtensionByType<Part::AttachExtension>(true) : nullptr;
    if (!attach || attach->AttachmentSupport.getValues().empty()) {
        return false;
    }
    std::vector<eRefType> types;
    const auto& objs = attach->AttachmentSupport.getValues();
    const auto& subs = attach->AttachmentSupport.getSubValues();
    for (std::size_t i = 0; i < objs.size(); ++i) {
        types.push_back(shapeType(objs[i], subs[i]));
    }
    return CP::typeOf(eMapMode(attach->MapMode.getValue()), types).has_value();
}

void PartGui::editConstructionPlane(App::DocumentObject* plane, std::optional<CP::Type> type)
{
    pendingType = type;
    Gui::cmdSetEdit(plane);
    pendingType.reset();
}

std::optional<CP::Type> PartGui::takePendingConstructionPlaneType()
{
    auto type = pendingType;
    pendingType.reset();
    return type;
}

// ---------------------------------------------------------------------------------------------

TaskConstructionPlane::TaskConstructionPlane(
    Gui::ViewProviderDocumentObject* vp,
    std::optional<Type> initialType
)
    : TaskBox(QPixmap(), tr("Construction Plane"), true, nullptr)
    , SelectionObserver(true, Gui::ResolveMode::NoResolve)
    , vpWeak(vp)
    , planeT(vp->getObject())
    , type(initialType.value_or(Type::Offset))
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);

    typeCombo = new QComboBox(widget);
    typeCombo->setObjectName(QStringLiteral("typeCombo"));
    for (Type t : CP::allTypes()) {
        typeCombo->addItem(constructionPlaneTypeName(t), static_cast<int>(t));
    }
    layout->addWidget(typeCombo);

    hintLabel = new QLabel(widget);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    referenceList = new QListWidget(widget);
    referenceList->setObjectName(QStringLiteral("referenceList"));
    referenceList->setMaximumHeight(90);
    layout->addWidget(referenceList);

    clearButton = new QPushButton(tr("Clear references"), widget);
    clearButton->setObjectName(QStringLiteral("clearButton"));
    layout->addWidget(clearButton);

    auto* form = new QFormLayout();
    valueLabel = new QLabel(widget);
    valueEdit = new Gui::QuantitySpinBox(widget);
    valueEdit->setObjectName(QStringLiteral("valueEdit"));
    valueEdit->setKeyboardTracking(false);
    form->addRow(valueLabel, valueEdit);
    layout->addLayout(form);

    reversedCheck = new QCheckBox(tr("Reverse the normal"), widget);
    reversedCheck->setObjectName(QStringLiteral("reversedCheck"));
    layout->addWidget(reversedCheck);

    statusLabel = new QLabel(widget);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    advancedButton = new QPushButton(tr("Advanced…"), widget);
    advancedButton->setObjectName(QStringLiteral("advancedButton"));
    advancedButton->setToolTip(tr("Edit the attachment of the plane with all attachment modes"));
    layout->addWidget(advancedButton);

    groupLayout()->addWidget(widget);

    // An existing plane keeps its type and value
    Part::AttachExtension* attach = getAttachExtension();
    auto existing = CP::typeOf(eMapMode(attach->MapMode.getValue()), referenceTypes());
    if (existing && !attach->AttachmentSupport.getValues().empty()) {
        type = *existing;
    }
    else {
        handleInitialSelection();
    }

    typeCombo->setCurrentIndex(typeCombo->findData(static_cast<int>(type)));
    reversedCheck->setChecked(attach->MapReversed.getValue());

    connect(typeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        onTypeChanged(index);
    });
    connect(valueEdit, qOverload<double>(&Gui::QuantitySpinBox::valueChanged), this, [this](double v) {
        onValueChanged(v);
    });
    connect(reversedCheck, &QCheckBox::toggled, this, [this](bool on) { onReversedChanged(on); });
    connect(clearButton, &QPushButton::clicked, this, [this] { onClear(); });
    connect(advancedButton, &QPushButton::clicked, this, &TaskConstructionPlane::advancedRequested);

    Gui::Selection().addSelectionGate(new ReferenceGate(planeT.getObject(), type));
    gateInstalled = true;
    setupGizmos();
    updateValueField();
    updateReferenceList();
    applyReferences();
}

TaskConstructionPlane::~TaskConstructionPlane()
{
    finishSelection();
}

void TaskConstructionPlane::finishSelection()
{
    if (gateInstalled) {
        Gui::Selection().rmvSelectionGate();
        gateInstalled = false;
    }
    detachSelection();
}

void TaskConstructionPlane::showIncomplete()
{
    statusLabel->setText(tr("Select references that make a plane."));
    statusLabel->setStyleSheet(QStringLiteral("QLabel{color: red;}"));
}

Part::AttachExtension* TaskConstructionPlane::getAttachExtension() const
{
    App::DocumentObject* plane = planeT.getObject();
    return plane ? plane->getExtensionByType<Part::AttachExtension>() : nullptr;
}

std::vector<eRefType> TaskConstructionPlane::referenceTypes() const
{
    std::vector<eRefType> types;
    Part::AttachExtension* attach = getAttachExtension();
    if (!attach) {
        return types;
    }
    const auto& objs = attach->AttachmentSupport.getValues();
    const auto& subs = attach->AttachmentSupport.getSubValues();
    for (std::size_t i = 0; i < objs.size(); ++i) {
        types.push_back(shapeType(objs[i], subs[i]));
    }
    return types;
}

bool TaskConstructionPlane::isAttached() const
{
    Part::AttachExtension* attach = getAttachExtension();
    App::DocumentObject* plane = planeT.getObject();
    return attach && plane && attach->MapMode.getValue() != mmDeactivated && !plane->isError()
        && attach->isAttacherActive();
}

void TaskConstructionPlane::handleInitialSelection()
{
    App::DocumentObject* plane = planeT.getObject();
    std::vector<App::DocumentObject*> objs;
    std::vector<std::string> subs;
    for (const auto& sel : Gui::Selection().getSelectionEx(
             plane->getDocument()->getName(),
             App::DocumentObject::getClassTypeId(),
             Gui::ResolveMode::NoResolve
         )) {
        std::vector<std::string> names = sel.getSubNames();
        if (names.empty()) {
            names.emplace_back();
        }
        for (const auto& name : names) {
            App::DocumentObject* obj = plane->getDocument()->getObject(sel.getFeatName());
            std::string sub = name;
            TaskAttacher::findCorrectObjAndSubInThisContext(plane, obj, sub);
            if (!obj || obj == plane) {
                continue;
            }
            if (isDatum(obj)) {
                sub.clear();
            }
            objs.push_back(obj);
            subs.push_back(sub);
        }
    }
    if (objs.empty()) {
        return;
    }

    std::vector<eRefType> types;
    for (std::size_t i = 0; i < objs.size(); ++i) {
        types.push_back(shapeType(objs[i], subs[i]));
    }
    // Take the selection as it is if it makes a plane, else the references fitting the type
    if (auto selectedType = CP::classify(types)) {
        type = *selectedType;
    }
    std::vector<App::DocumentObject*> fitObjs;
    std::vector<std::string> fitSubs;
    for (std::size_t i = 0; i < objs.size(); ++i) {
        if (CP::acceptsReference(type, types[i])
            && static_cast<int>(fitObjs.size()) < CP::maxReferences(type)) {
            fitObjs.push_back(objs[i]);
            fitSubs.push_back(subs[i]);
        }
    }
    getAttachExtension()->AttachmentSupport.setValues(fitObjs, fitSubs);
    Gui::Selection().clearSelection();
}

void TaskConstructionPlane::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (msg.Type != Gui::SelectionChanges::AddSelection) {
        return;
    }
    App::DocumentObject* plane = planeT.getObject();
    if (!plane || !msg.pObjectName) {
        return;
    }
    App::DocumentObject* obj = plane->getDocument()->getObject(msg.pObjectName);
    std::string sub = msg.pSubName ? msg.pSubName : "";
    TaskAttacher::findCorrectObjAndSubInThisContext(plane, obj, sub);
    if (!obj || obj == plane) {
        return;
    }
    if (isDatum(obj)) {
        sub.clear();
    }
    toggleReference(obj, sub);

    // Picking the same element again removes it, so do not keep it selected
    QTimer::singleShot(0, this, [] { Gui::Selection().clearSelection(); });
}

void TaskConstructionPlane::toggleReference(App::DocumentObject* obj, std::string sub)
{
    Part::AttachExtension* attach = getAttachExtension();
    std::vector<App::DocumentObject*> objs = attach->AttachmentSupport.getValues();
    std::vector<std::string> subs = attach->AttachmentSupport.getSubValues();

    std::size_t index = 0;
    while (index < objs.size() && (objs[index] != obj || subs[index] != sub)) {
        ++index;
    }
    if (index < objs.size()) {
        objs.erase(objs.begin() + static_cast<long>(index));
        subs.erase(subs.begin() + static_cast<long>(index));
    }
    else if (CP::acceptsReference(type, shapeType(obj, sub))) {
        // A full set of references gets its last one replaced
        if (static_cast<int>(objs.size()) >= CP::maxReferences(type)) {
            objs.pop_back();
            subs.pop_back();
        }
        objs.push_back(obj);
        subs.push_back(sub);
    }
    else {
        return;
    }
    attach->AttachmentSupport.setValues(objs, subs);
    updateReferenceList();
    applyReferences();
}

void TaskConstructionPlane::applyReferences()
{
    Part::AttachExtension* attach = getAttachExtension();
    if (!attach) {
        return;
    }
    std::vector<App::DocumentObject*> objs = attach->AttachmentSupport.getValues();
    std::vector<std::string> subs = attach->AttachmentSupport.getSubValues();
    std::vector<eRefType> types = referenceTypes();

    std::vector<int> order = CP::referenceOrder(type, types);
    std::vector<App::DocumentObject*> orderedObjs;
    std::vector<std::string> orderedSubs;
    std::vector<eRefType> orderedTypes;
    for (int i : order) {
        orderedObjs.push_back(objs[i]);
        orderedSubs.push_back(subs[i]);
        orderedTypes.push_back(types[i]);
    }
    if (orderedObjs != objs || orderedSubs != subs) {
        attach->AttachmentSupport.setValues(orderedObjs, orderedSubs);
        updateReferenceList();
    }

    eMapMode mode = mmDeactivated;
    if (static_cast<int>(orderedObjs.size()) >= minReferences(type)) {
        mode = CP::mapMode(type, orderedTypes);
    }
    if (attach->MapMode.getValue() != mode) {
        attach->MapMode.setValue(mode);
    }
    updatePreview();
}

void TaskConstructionPlane::updatePreview()
{
    Part::AttachExtension* attach = getAttachExtension();
    App::DocumentObject* plane = planeT.getObject();
    if (!attach || !plane) {
        return;
    }

    QString error;
    bool attached = false;
    try {
        attached = attach->positionBySupport();
        plane->recomputeFeature();
    }
    catch (Base::Exception& e) {
        error = QCoreApplication::translate("Exception", e.what());
    }
    catch (Standard_Failure& e) {
        error = tr("OCC error: %1").arg(QString::fromLatin1(e.GetMessageString()));
    }
    catch (...) {
        error = tr("unknown error");
    }

    int count = static_cast<int>(attach->AttachmentSupport.getValues().size());
    if (!error.isEmpty()) {
        statusLabel->setText(tr("The references do not make a plane: %1").arg(error));
        statusLabel->setStyleSheet(QStringLiteral("QLabel{color: red;}"));
    }
    else if (!attached || count < minReferences(type)) {
        statusLabel->setText(tr("Not enough references"));
        statusLabel->setStyleSheet(QString());
    }
    else {
        statusLabel->setText(tr("The plane is defined"));
        statusLabel->setStyleSheet(QStringLiteral("QLabel{color: green;}"));
    }
    setGizmoPositions();
}

void TaskConstructionPlane::updateReferenceList()
{
    Part::AttachExtension* attach = getAttachExtension();
    referenceList->clear();
    const auto& objs = attach->AttachmentSupport.getValues();
    const auto& subs = attach->AttachmentSupport.getSubValues();
    for (std::size_t i = 0; i < objs.size(); ++i) {
        QString text = QString::fromUtf8(objs[i]->Label.getValue());
        std::string element = Data::oldElementName(subs[i].c_str());
        if (!element.empty()) {
            text += QStringLiteral(": ") + QString::fromStdString(element);
        }
        referenceList->addItem(text);
    }
    hintLabel->setText(selectionHint(type));
}

void TaskConstructionPlane::updateValueField()
{
    App::DocumentObject* plane = planeT.getObject();
    Part::AttachExtension* attach = getAttachExtension();
    CP::Value value = CP::valueOf(type);

    valueEdit->blockSignals(true);
    valueEdit->unbind();
    Base::Placement offset = attach->AttachmentOffset.getValue();
    switch (value) {
        case CP::Value::Distance:
            valueLabel->setText(tr("Distance"));
            valueEdit->setUnit(Base::Unit::Length);
            valueEdit->setRange(-1e7, 1e7);
            valueEdit->setSingleStep(1.0);
            valueEdit->setValue(offset.getPosition().z);
            valueEdit->bind(App::ObjectIdentifier::parse(plane, "AttachmentOffset.Base.z"));
            break;
        case CP::Value::Angle: {
            double yaw, pitch, roll;
            offset.getRotation().getYawPitchRoll(yaw, pitch, roll);
            valueLabel->setText(tr("Angle"));
            valueEdit->setUnit(Base::Unit::Angle);
            valueEdit->setRange(-360.0, 360.0);
            valueEdit->setSingleStep(5.0);
            valueEdit->setValue(roll);
        } break;
        case CP::Value::Position:
            valueLabel->setText(tr("Position along edge"));
            valueEdit->setUnit(Base::Unit());
            valueEdit->setRange(0.0, 1.0);
            valueEdit->setSingleStep(0.05);
            valueEdit->setValue(attach->MapPathParameter.getValue());
            valueEdit->bind(App::ObjectIdentifier::parse(plane, "MapPathParameter"));
            break;
        case CP::Value::None:
            break;
    }
    valueEdit->blockSignals(false);

    bool visible = value != CP::Value::None;
    valueLabel->setVisible(visible);
    valueEdit->setVisible(visible);
}

void TaskConstructionPlane::onTypeChanged(int index)
{
    setType(static_cast<Type>(typeCombo->itemData(index).toInt()));
}

void TaskConstructionPlane::setType(Type newType)
{
    if (newType == type) {
        return;
    }
    type = newType;

    // Start the new type without value, keeping the references that fit
    Part::AttachExtension* attach = getAttachExtension();
    attach->AttachmentOffset.setValue(Base::Placement());
    attach->MapPathParameter.setValue(0.0);
    std::vector<App::DocumentObject*> objs;
    std::vector<std::string> subs;
    std::vector<eRefType> types = referenceTypes();
    for (std::size_t i = 0; i < types.size(); ++i) {
        if (CP::acceptsReference(type, types[i])
            && static_cast<int>(objs.size()) < CP::maxReferences(type)) {
            objs.push_back(attach->AttachmentSupport.getValues()[i]);
            subs.push_back(attach->AttachmentSupport.getSubValues()[i]);
        }
    }
    attach->AttachmentSupport.setValues(objs, subs);

    if (gateInstalled) {
        Gui::Selection().rmvSelectionGate();
    }
    Gui::Selection().addSelectionGate(new ReferenceGate(planeT.getObject(), type));
    gateInstalled = true;

    QSignalBlocker blocker(typeCombo);
    typeCombo->setCurrentIndex(typeCombo->findData(static_cast<int>(type)));
    updateValueField();
    updateReferenceList();
    applyReferences();
}

void TaskConstructionPlane::onValueChanged(double value)
{
    Part::AttachExtension* attach = getAttachExtension();
    switch (CP::valueOf(type)) {
        case CP::Value::Distance: {
            Base::Placement offset = attach->AttachmentOffset.getValue();
            Base::Vector3d pos = offset.getPosition();
            pos.z = value;
            offset.setPosition(pos);
            attach->AttachmentOffset.setValue(offset);
        } break;
        case CP::Value::Angle: {
            Base::Placement offset = attach->AttachmentOffset.getValue();
            offset.setRotation(Base::Rotation(Base::Vector3d(1, 0, 0), Base::toRadians(value)));
            attach->AttachmentOffset.setValue(offset);
        } break;
        case CP::Value::Position:
            attach->MapPathParameter.setValue(value);
            break;
        case CP::Value::None:
            return;
    }
    updatePreview();
}

void TaskConstructionPlane::onReversedChanged(bool on)
{
    getAttachExtension()->MapReversed.setValue(on);
    updatePreview();
}

void TaskConstructionPlane::onClear()
{
    getAttachExtension()->AttachmentSupport.setValues({}, {});
    updateReferenceList();
    applyReferences();
}

void TaskConstructionPlane::setupGizmos()
{
    auto dragger = vpWeak.get<Gui::ViewProviderDragger>();
    if (!Gui::GizmoContainer::isEnabled() || !dragger) {
        return;
    }

    distanceGizmo = new Gui::LinearGizmo(valueEdit);
    distanceGizmo->setClickCallback([this] { reversedCheck->toggle(); });
    angleGizmo = new Gui::RadialGizmo(valueEdit);
    gizmoContainer = Gui::GizmoContainer::create({distanceGizmo, angleGizmo}, dragger);
    setGizmoPositions();
}

void TaskConstructionPlane::setGizmoPositions()
{
    if (!gizmoContainer) {
        return;
    }

    auto plane = freecad_cast<App::GeoFeature*>(planeT.getObject());
    CP::Value value = CP::valueOf(type);
    bool shown = plane && isAttached()
        && (value == CP::Value::Distance || (value == CP::Value::Angle && type == Type::Angle));
    gizmoContainer->visible = shown;
    if (!shown) {
        return;
    }

    // The plane without its offset, in the coordinates of its container
    Base::Placement base = plane->Placement.getValue()
        * getAttachExtension()->AttachmentOffset.getValue().inverse();
    Base::Rotation rot = base.getRotation();
    Base::Vector3d origin = base.getPosition();

    distanceGizmo->setVisibility(value == CP::Value::Distance);
    distanceGizmo->Gizmo::setDraggerPlacement(origin, rot.multVec(Base::Vector3d(0, 0, 1)));

    angleGizmo->setVisibility(value == CP::Value::Angle);
    angleGizmo->Gizmo::setDraggerPlacement(origin, rot.multVec(Base::Vector3d(0, 1, 0)));
    angleGizmo->getDraggerContainer()->setArcNormalDirection(
        Base::convertTo<SbVec3f>(rot.multVec(Base::Vector3d(1, 0, 0)))
    );

    gizmoContainer->calculateScaleAndOrientation();
}

// ---------------------------------------------------------------------------------------------

TaskDlgConstructionPlane::TaskDlgConstructionPlane(
    Gui::ViewProviderDocumentObject* vp,
    std::optional<CP::Type> type
)
    : vpWeak(vp)
    , planeT(vp->getObject())
    , parameter(new TaskConstructionPlane(vp, type))
{
    setDocumentName(vp->getDocument()->getDocument()->getName());
    Content.push_back(parameter);
    connect(parameter, &TaskConstructionPlane::advancedRequested, this, [this] {
        openAdvanced();
    });
}

void TaskDlgConstructionPlane::open()
{
    Gui::DocumentT doc(getDocumentName());
    if (Gui::Document* document = doc.getDocument()) {
        if (!document->hasPendingCommand()) {
            document->openCommand(QT_TRANSLATE_NOOP("Command", "Edit construction plane"));
        }
    }
}

bool TaskDlgConstructionPlane::accept()
{
    App::DocumentObject* obj = planeT.getObject();
    Gui::DocumentT doc(getDocumentName());
    Gui::Document* document = doc.getDocument();
    if (!obj || !document) {
        return true;
    }
    if (!parameter->isAttached()) {
        parameter->showIncomplete();
        return false;
    }

    try {
        auto attach = obj->getExtensionByType<Part::AttachExtension>();
        Base::Placement plm = attach->AttachmentOffset.getValue();
        double yaw, pitch, roll;
        plm.getRotation().getYawPitchRoll(yaw, pitch, roll);
        Gui::cmdAppObjectArgs(
            obj,
            "AttachmentOffset = App.Placement(App.Vector(%.10f, %.10f, %.10f), "
            "App.Rotation(%.10f, %.10f, %.10f))",
            plm.getPosition().x,
            plm.getPosition().y,
            plm.getPosition().z,
            yaw,
            pitch,
            roll
        );
        Gui::cmdAppObjectArgs(obj, "MapReversed = %s", attach->MapReversed.getValue() ? "True" : "False");
        Gui::cmdAppObjectArgs(
            obj,
            "AttachmentSupport = %s",
            attach->AttachmentSupport.getPyReprString().c_str()
        );
        Gui::cmdAppObjectArgs(obj, "MapPathParameter = %f", attach->MapPathParameter.getValue());
        Gui::cmdAppObjectArgs(
            obj,
            "MapMode = '%s'",
            AttachEngine::getModeName(eMapMode(attach->MapMode.getValue())).c_str()
        );
        Gui::cmdAppObject(obj, "recompute()");
        if (!obj->isValid()) {
            throw Base::RuntimeError(obj->getStatusString());
        }
        Gui::cmdGuiDocument(obj, "resetEdit()");
        document->commitCommand();
    }
    catch (const Base::Exception& e) {
        QMessageBox::warning(
            parameter,
            tr("Construction plane"),
            QCoreApplication::translate("Exception", e.what())
        );
        return false;
    }

    // Keep the plane selected, so that a sketch can be started on it right away
    parameter->finishSelection();
    Gui::Selection().clearSelection();
    Gui::Selection().addSelection(obj->getDocument()->getName(), obj->getNameInDocument());
    return true;
}

bool TaskDlgConstructionPlane::reject()
{
    Gui::DocumentT doc(getDocumentName());
    Gui::Document* document = doc.getDocument();
    if (document) {
        // Abort before leaving edit mode, which would commit the changes
        document->abortCommand();
        document->resetEdit();
        Gui::Command::doCommand(Gui::Command::Doc, "%s.recompute()", doc.getAppDocumentPython().c_str());
    }
    return true;
}

void TaskDlgConstructionPlane::openAdvanced()
{
    // Continue in the attachment editor within the same transaction
    Gui::DocumentT doc(getDocumentName());
    App::DocumentObjectT plane = planeT;
    if (Gui::Document* document = doc.getDocument()) {
        document->resetEdit();
    }
    QTimer::singleShot(0, [plane] {
        App::DocumentObject* obj = plane.getObject();
        auto provider = freecad_cast<Gui::ViewProviderDocumentObject*>(
            Gui::Application::Instance->getViewProvider(obj)
        );
        if (auto ext = provider ? provider->getExtensionByType<ViewProviderAttachExtension>(true)
                                : nullptr) {
            ext->showAttachmentEditor();
        }
    });
}

#include "moc_TaskConstructionPlane.cpp"
