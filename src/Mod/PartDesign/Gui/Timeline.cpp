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
#include <cstring>

#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <App/Application.h>
#include <App/Document.h>
#include <Gui/ActiveObjectList.h>
#include <Gui/Application.h>
#include <Gui/Command.h>
#include <Gui/Document.h>
#include <Gui/MDIView.h>
#include <Gui/Selection/Selection.h>
#include <Gui/ViewProvider.h>
#include <Mod/PartDesign/App/Body.h>

#include "Timeline.h"

using namespace PartDesignGui;

namespace
{
constexpr int timelineIconSize = 24;
constexpr int markerGrip = 6;

bool isTimelineWorkbench(const char* name)
{
    return name
        && (std::strcmp(name, "PartDesignWorkbench") == 0
            || std::strcmp(name, "DesignWorkbench") == 0);
}
}  // namespace

// ----------------------------------------------------------------------------

TimelineList::TimelineList(QWidget* parent)
    : QListWidget(parent)
{
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(false);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setIconSize(QSize(timelineIconSize, timelineIconSize));
    setSpacing(4);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMouseTracking(true);
    setFixedHeight(timelineIconSize + 36);
}

void TimelineList::setMarkerSlot(int slot)
{
    marker = std::clamp(slot, 0, count());
    viewport()->update();
}

int TimelineList::markerPosition(int slot) const
{
    if (count() == 0) {
        return spacing();
    }
    if (slot <= 0) {
        return visualItemRect(item(0)).left() - spacing() / 2 - 1;
    }
    QRect rect = visualItemRect(item(std::min(slot, count()) - 1));
    return rect.right() + spacing() / 2 + 1;
}

int TimelineList::slotAt(int x) const
{
    for (int i = 0; i < count(); ++i) {
        if (x < visualItemRect(item(i)).center().x()) {
            return i;
        }
    }
    return count();
}

void TimelineList::paintEvent(QPaintEvent* event)
{
    QListWidget::paintEvent(event);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    const int x = markerPosition(dragSlot >= 0 ? dragSlot : marker);
    const QColor color = palette().color(QPalette::Highlight);
    painter.setPen(QPen(color, 3));
    painter.drawLine(x, markerGrip, x, viewport()->height() - 2);
    painter.setBrush(color);
    painter.drawPolygon(
        QPolygon({QPoint(x - markerGrip, 0), QPoint(x + markerGrip, 0), QPoint(x, markerGrip)})
    );
}

void TimelineList::mousePressEvent(QMouseEvent* event)
{
    const int x = event->position().toPoint().x();
    if (event->button() == Qt::LeftButton && std::abs(x - markerPosition(marker)) <= markerGrip) {
        dragSlot = marker;
        viewport()->update();
        return;
    }
    QListWidget::mousePressEvent(event);
}

void TimelineList::mouseMoveEvent(QMouseEvent* event)
{
    const int x = event->position().toPoint().x();
    if (dragSlot >= 0) {
        dragSlot = slotAt(x);
        viewport()->update();
        return;
    }
    const bool nearMarker = std::abs(x - markerPosition(marker)) <= markerGrip;
    viewport()->setCursor(nearMarker ? Qt::SizeHorCursor : Qt::ArrowCursor);
    QListWidget::mouseMoveEvent(event);
}

void TimelineList::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragSlot >= 0) {
        const int slot = dragSlot;
        dragSlot = -1;
        viewport()->update();
        if (slot != marker) {
            Q_EMIT markerMoved(slot);
        }
        return;
    }
    QListWidget::mouseReleaseEvent(event);
}

// ----------------------------------------------------------------------------

TimelineView::TimelineView(QWidget* parent)
    : QWidget(parent)
    , list(new TimelineList(this))
    , placeholder(new QLabel(this))
{
    setWindowTitle(tr("Timeline"));

    placeholder->setText(tr("Activate a body to see its timeline"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setEnabled(false);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(list);
    layout->addWidget(placeholder);

    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setToolTip(tr("Drag the marker to roll the body back or forward, double-click a "
                        "feature to edit it"));
    connect(list, &TimelineList::markerMoved, this, &TimelineView::rollTo);
    connect(list, &QListWidget::itemClicked, this, &TimelineView::onItemClicked);
    connect(list, &QListWidget::itemDoubleClicked, this, &TimelineView::onItemDoubleClicked);
    connect(list, &QWidget::customContextMenuRequested, this, &TimelineView::showContextMenu);

    rebuildTimer.setSingleShot(true);
    connect(&rebuildTimer, &QTimer::timeout, this, &TimelineView::rebuild);

    // clang-format off
    auto app = Gui::Application::Instance;
    connectNewDocument = app->signalNewDocument.connect(
        [this](const Gui::Document& doc, bool) { attachDocument(doc); });
    connectDeleteDocument = app->signalDeleteDocument.connect(
        [this](const Gui::Document& doc) {
            connectActivatedViewProvider.erase(&doc);
            scheduleRebuild();
        });
    connectActiveDocument = app->signalActiveDocument.connect(
        [this](const Gui::Document&) { scheduleRebuild(); });
    connectWorkbench = app->signalActivateWorkbench.connect(
        [this](const char* name) { onWorkbenchActivated(name); });

    auto& appSignals = App::GetApplication();
    connectNewObject = appSignals.signalNewObject.connect(
        [this](const App::DocumentObject&) { scheduleRebuild(); });
    connectDeletedObject = appSignals.signalDeletedObject.connect(
        [this](const App::DocumentObject&) { scheduleRebuild(); });
    connectChangedObject = appSignals.signalChangedObject.connect(
        [this](const App::DocumentObject&, const App::Property& prop) {
            static const char* relevant[] = {"Group", "Tip", "BaseFeature", "Label", "Suppressed"};
            const char* name = prop.getName();
            if (name && std::ranges::any_of(relevant, [name](const char* r) {
                    return std::strcmp(name, r) == 0;
                })) {
                scheduleRebuild();
            }
        });
    connectRecomputed = appSignals.signalRecomputed.connect(
        [this](const App::Document&) { scheduleRebuild(); });
    connectUndo = appSignals.signalUndoDocument.connect(
        [this](const App::Document&) { scheduleRebuild(); });
    connectRedo = appSignals.signalRedoDocument.connect(
        [this](const App::Document&) { scheduleRebuild(); });
    // clang-format on

    for (auto doc : App::GetApplication().getDocuments()) {
        if (auto guiDoc = app->getDocument(doc)) {
            attachDocument(*guiDoc);
        }
    }

    rebuild();
}

TimelineView::~TimelineView() = default;

void TimelineView::attachDocument(const Gui::Document& doc)
{
    connectActivatedViewProvider[&doc] = doc.signalActivatedViewProvider.connect(
        [this](const Gui::ViewProviderDocumentObject*, const char*) { scheduleRebuild(); }
    );
}

void TimelineView::scheduleRebuild()
{
    // Several changes usually come together, e.g. on recompute or undo
    rebuildTimer.start(0);
}

void TimelineView::onWorkbenchActivated(const char* name)
{
    // The timeline belongs to the part design workbenches, don't clutter the others.
    // The dock window manager restores the visibility when coming back.
    if (!isTimelineWorkbench(name)) {
        if (auto dock = qobject_cast<QDockWidget*>(parentWidget())) {
            dock->hide();
        }
    }
}

PartDesign::Body* TimelineView::shownBody() const
{
    App::Document* doc = App::GetApplication().getDocument(bodyDocument.c_str());
    return doc ? freecad_cast<PartDesign::Body*>(doc->getObject(bodyName.c_str())) : nullptr;
}

App::DocumentObject* TimelineView::objectOf(const QListWidgetItem* item) const
{
    PartDesign::Body* body = shownBody();
    if (!body || !item) {
        return nullptr;
    }
    std::string name = item->data(Qt::UserRole).toString().toStdString();
    return body->getDocument()->getObject(name.c_str());
}

void TimelineView::rebuild()
{
    PartDesign::Body* body = nullptr;
    if (Gui::MDIView* view = Gui::Application::Instance->activeView()) {
        body = view->getActiveObject<PartDesign::Body*>(PDBODYKEY);
    }

    list->clear();
    list->setVisible(body != nullptr);
    placeholder->setVisible(body == nullptr);
    if (!body) {
        bodyDocument.clear();
        bodyName.clear();
        return;
    }
    bodyDocument = body->getDocument()->getName();
    bodyName = body->getNameInDocument();

    const std::vector<App::DocumentObject*> model = body->getFullModel();
    App::DocumentObject* tip = body->Tip.getValue();
    auto tipIt = std::ranges::find(model, tip);
    int tipIndex = tipIt != model.end() ? static_cast<int>(tipIt - model.begin()) : -1;
    // Without any solid there is nothing to roll back
    if (!tip && std::ranges::none_of(model, &PartDesign::Body::isSolidFeature)) {
        tipIndex = static_cast<int>(model.size()) - 1;
    }

    for (int i = 0; i < static_cast<int>(model.size()); ++i) {
        App::DocumentObject* obj = model[i];
        auto item = new QListWidgetItem(list);
        QIcon icon;
        if (auto vp = Gui::Application::Instance->getViewProvider(obj)) {
            icon = vp->getIcon();
        }
        // Features after the tip are not part of the current model
        if (i > tipIndex) {
            icon = QIcon(icon.pixmap(list->iconSize(), QIcon::Disabled));
        }
        item->setIcon(icon);

        QString label = QString::fromUtf8(obj->Label.getValue());
        if (obj->isError()) {
            label += QLatin1String(" - ") + tr("error");
            item->setBackground(QColor(255, 0, 0, 60));
        }
        item->setToolTip(label);
        item->setData(Qt::UserRole, QString::fromLatin1(obj->getNameInDocument()));
    }
    list->setMarkerSlot(tipIndex + 1);
}

void TimelineView::moveTip(App::DocumentObject* target)
{
    PartDesign::Body* body = shownBody();
    if (!body) {
        return;
    }
    App::DocumentObject* oldTip = body->Tip.getValue();
    if (oldTip == target) {
        return;
    }

    Gui::Document* doc = Gui::Application::Instance->getDocument(body->getDocument());
    doc->openCommand(QT_TRANSLATE_NOOP("Command", "Move tip to selected feature"));
    if (target) {
        FCMD_OBJ_CMD(body, "Tip = " << Gui::Command::getObjectCmd(target));
        FCMD_OBJ_SHOW(target);
    }
    else {
        FCMD_OBJ_CMD(body, "Tip = None");
    }
    // Only show the new state of the body
    if (oldTip && oldTip != body->BaseFeature.getValue()) {
        FCMD_OBJ_HIDE(oldTip);
    }
    Gui::Command::updateActive();
    doc->commitCommand();
}

void TimelineView::rollTo(int slot)
{
    PartDesign::Body* body = shownBody();
    if (!body) {
        return;
    }
    // The new tip is the last solid feature before the marker
    const std::vector<App::DocumentObject*> model = body->getFullModel();
    slot = std::clamp(slot, 0, static_cast<int>(model.size()));
    App::DocumentObject* target = nullptr;
    for (int i = 0; i < slot; ++i) {
        if (PartDesign::Body::isSolidFeature(model[i]) || model[i] == body->BaseFeature.getValue()) {
            target = model[i];
        }
    }
    moveTip(target);
    rebuild();
}

void TimelineView::rollToEnd()
{
    if (PartDesign::Body* body = shownBody()) {
        rollTo(static_cast<int>(body->getFullModel().size()));
    }
}

void TimelineView::onItemClicked(QListWidgetItem* item)
{
    if (App::DocumentObject* obj = objectOf(item)) {
        Gui::Selection().clearSelection();
        Gui::Selection().addSelection(bodyDocument.c_str(), obj->getNameInDocument());
    }
}

void TimelineView::onItemDoubleClicked(QListWidgetItem* item)
{
    if (App::DocumentObject* obj = objectOf(item)) {
        if (auto vp = Gui::Application::Instance->getViewProvider(obj)) {
            vp->doubleClicked();
        }
    }
}

void TimelineView::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = list->itemAt(pos);
    QMenu menu(this);
    if (item) {
        const int row = list->row(item);
        menu.addAction(tr("Edit"), this, [this, item]() { onItemDoubleClicked(item); });
        menu.addAction(tr("Roll Back to Here"), this, [this, row]() { rollTo(row + 1); });
    }
    menu.addAction(tr("Roll Forward to End"), this, &TimelineView::rollToEnd);
    menu.exec(list->viewport()->mapToGlobal(pos));
}

#include "moc_Timeline.cpp"
