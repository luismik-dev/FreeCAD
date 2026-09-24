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

#include "ConstructionPlane.h"

using namespace Attacher;

namespace Part::ConstructionPlane
{

namespace
{

// Whether the reference is of the shape type or a more specific one
bool is(eRefType ref, eRefType type)
{
    eRefType shape = eRefType(ref & (rtFlagHasPlacement - 1));
    while (shape != rtAnything) {
        if (shape == type) {
            return true;
        }
        shape = AttachEngine::downgradeType(shape);
    }
    return type == rtAnything;
}

bool hasPlacement(eRefType ref)
{
    return (ref & rtFlagHasPlacement) != 0;
}

bool isLine(eRefType ref)
{
    return is(ref, rtLine);
}

bool isPlanar(eRefType ref)
{
    return is(ref, rtFlatFace);
}

bool isRound(eRefType ref)
{
    return is(ref, rtCylindricalFace) || is(ref, rtConicalFace);
}

bool isVertex(eRefType ref)
{
    return is(ref, rtVertex);
}

// A whole object with a placement, like a sketch or a body, but no line
bool isPlacement(eRefType ref)
{
    return hasPlacement(ref) && !isLine(ref);
}

}  // namespace

const std::vector<Type>& allTypes()
{
    static const std::vector<Type> types {
        Type::Offset,
        Type::Angle,
        Type::Tangent,
        Type::MidPlane,
        Type::TwoEdges,
        Type::ThreePoints,
        Type::TangentAtPoint,
        Type::AlongPath,
    };
    return types;
}

std::optional<Type> classify(const std::vector<eRefType>& refs)
{
    auto count = [&refs](bool (*pred)(eRefType)) {
        return std::count_if(refs.begin(), refs.end(), pred);
    };
    switch (refs.size()) {
        case 1: {
            eRefType ref = refs[0];
            if (isLine(ref)) {
                return Type::Angle;
            }
            if (isPlanar(ref) || isPlacement(ref)) {
                return Type::Offset;
            }
            if (isRound(ref)) {
                return Type::Tangent;
            }
            if (is(ref, rtEdge)) {
                return Type::AlongPath;
            }
            return std::nullopt;
        }
        case 2:
            if (count(isLine) == 2) {
                return Type::TwoEdges;
            }
            if (count(isLine) == 1 && count(isPlanar) == 1) {
                return Type::Angle;
            }
            if (count(isRound) == 1 && count(isPlanar) == 1) {
                return Type::Tangent;
            }
            if (count(isPlanar) == 2) {
                return Type::MidPlane;
            }
            if (count(isVertex) == 1 && (is(refs[0], rtFace) || is(refs[1], rtFace))) {
                return Type::TangentAtPoint;
            }
            if (count(isVertex) == 1 && (is(refs[0], rtEdge) || is(refs[1], rtEdge))) {
                return Type::AlongPath;
            }
            return std::nullopt;
        case 3:
            if (count(isVertex) == 3) {
                return Type::ThreePoints;
            }
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

bool acceptsReference(Type type, eRefType ref)
{
    switch (type) {
        case Type::Offset:
            return isPlanar(ref) || isPlacement(ref);
        case Type::Angle:
            return isLine(ref) || isPlanar(ref);
        case Type::Tangent:
            return isRound(ref) || isPlanar(ref);
        case Type::MidPlane:
            return isPlanar(ref);
        case Type::TwoEdges:
            return isLine(ref);
        case Type::ThreePoints:
            return isVertex(ref);
        case Type::TangentAtPoint:
            return is(ref, rtFace) || isVertex(ref);
        case Type::AlongPath:
            return is(ref, rtEdge) || isVertex(ref);
    }
    return false;
}

int maxReferences(Type type)
{
    switch (type) {
        case Type::Offset:
            return 1;
        case Type::ThreePoints:
            return 3;
        default:
            return 2;
    }
}

std::vector<int> referenceOrder(Type type, const std::vector<eRefType>& refs)
{
    std::vector<int> order(refs.size());
    for (std::size_t i = 0; i < refs.size(); ++i) {
        order[i] = static_cast<int>(i);
    }
    // The line or round face goes first, the planar face defining the zero angle second
    bool (*first)(eRefType) = nullptr;
    if (type == Type::Angle) {
        first = isLine;
    }
    else if (type == Type::Tangent) {
        first = isRound;
    }
    else if (type == Type::AlongPath) {
        first = [](eRefType ref) {
            return !isVertex(ref);
        };
    }
    if (first) {
        std::stable_partition(order.begin(), order.end(), [&refs, first](int i) {
            return first(refs[i]);
        });
    }
    return order;
}

eMapMode mapMode(Type type, const std::vector<eRefType>& refs)
{
    switch (type) {
        case Type::Offset:
            return !refs.empty() && isPlacement(refs[0]) ? mmObjectXY : mmFlatFace;
        case Type::Angle:
            return mmPlaneThroughLine;
        case Type::Tangent:
            return mmTangentPlaneAtAngle;
        case Type::MidPlane:
            return mmMidPlane;
        case Type::TwoEdges:
        case Type::ThreePoints:
            return mmThreePointsPlane;
        case Type::TangentAtPoint:
            return mmTangentPlane;
        case Type::AlongPath:
            return mmNormalToPath;
    }
    return mmDeactivated;
}

std::optional<Type> typeOf(eMapMode mode, const std::vector<eRefType>& refs)
{
    switch (mode) {
        case mmFlatFace:
        case mmObjectXY:
            return Type::Offset;
        case mmPlaneThroughLine:
            return Type::Angle;
        case mmTangentPlaneAtAngle:
            return Type::Tangent;
        case mmMidPlane:
            return Type::MidPlane;
        case mmThreePointsPlane:
            if (refs.size() == 2 && isLine(refs[0]) && isLine(refs[1])) {
                return Type::TwoEdges;
            }
            if (refs.size() == 3 && std::all_of(refs.begin(), refs.end(), isVertex)) {
                return Type::ThreePoints;
            }
            return std::nullopt;
        case mmTangentPlane:
            return Type::TangentAtPoint;
        case mmNormalToPath:
            return Type::AlongPath;
        default:
            return std::nullopt;
    }
}

Value valueOf(Type type)
{
    switch (type) {
        case Type::Offset:
            return Value::Distance;
        case Type::Angle:
        case Type::Tangent:
            return Value::Angle;
        case Type::AlongPath:
            return Value::Position;
        default:
            return Value::None;
    }
}

}  // namespace Part::ConstructionPlane
