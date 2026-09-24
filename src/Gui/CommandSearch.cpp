// SPDX-License-Identifier: LGPL-2.1-or-later

/****************************************************************************
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include <algorithm>

#include <QGuiApplication>
#include <QLineEdit>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include "Application.h"
#include "Command.h"
#include "CommandCompleter.h"
#include "CommandSearch.h"

using namespace Gui;

CommandSearchPopup::CommandSearchPopup(QWidget* parent)
    : QFrame(parent, Qt::Popup)
    , edit(new QLineEdit(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    edit->setPlaceholderText(tr("Search commands…"));
    edit->setClearButtonEnabled(true);
    edit->setMinimumWidth(360);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(edit);

    auto completer = new CommandCompleter(edit, this);
    connect(
        completer,
        &CommandCompleter::commandActivated,
        this,
        &CommandSearchPopup::activateCommand
    );
}

void CommandSearchPopup::popup(const QPoint& pos)
{
    adjustSize();
    // Keep the popup on the screen
    QPoint topLeft = pos - QPoint(width() / 2, height() / 2);
    if (QScreen* screen = QGuiApplication::screenAt(pos)) {
        QRect area = screen->availableGeometry();
        topLeft.setX(std::clamp(topLeft.x(), area.left(), area.right() - width()));
        topLeft.setY(std::clamp(topLeft.y(), area.top(), area.bottom() - height()));
    }
    move(topLeft);
    show();
    edit->setFocus();
}

void CommandSearchPopup::activateCommand(const QByteArray& name)
{
    close();
    // Run the command once the popup is gone, it may open dialogs of its own
    QTimer::singleShot(0, qApp, [name]() {
        Application::Instance->commandManager().runCommandByName(name.constData());
    });
}

#include "moc_CommandSearch.cpp"
