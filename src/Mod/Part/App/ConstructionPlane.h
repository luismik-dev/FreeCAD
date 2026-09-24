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

#include <optional>
#include <vector>

#include <Mod/Part/PartGlobal.h>

#include "Attacher.h"

/** Construction planes as known from other CAD programs, and how they map onto the
 * attachment modes of a datum plane.
 */
namespace Part::ConstructionPlane
{

enum class Type
{
    Offset,          ///< Offset from a planar face or plane
    Angle,           ///< Through a line, turned about it
    Tangent,         ///< Tangent to a cylindrical or conical face, turned about its axis
    MidPlane,        ///< Between two planar faces
    TwoEdges,        ///< Through two straight edges
    ThreePoints,     ///< Through three points
    TangentAtPoint,  ///< Tangent to a face at a point
    AlongPath,       ///< Normal to an edge at a position along it
};

/// All types, in the order in which they are offered
PartExport const std::vector<Type>& allTypes();

/// The type that the references make, if any
PartExport std::optional<Type> classify(const std::vector<Attacher::eRefType>& refs);

/// Whether the reference can be one of the references of the type
PartExport bool acceptsReference(Type type, Attacher::eRefType ref);

/// The largest number of references of the type
PartExport int maxReferences(Type type);

/// The references reordered as the attachment mode of the type expects them
PartExport std::vector<int> referenceOrder(Type type, const std::vector<Attacher::eRefType>& refs);

/// The attachment mode making the type with the references
PartExport Attacher::eMapMode mapMode(Type type, const std::vector<Attacher::eRefType>& refs);

/// The type made by an attachment mode and references, if any
PartExport std::optional<Type> typeOf(
    Attacher::eMapMode mode,
    const std::vector<Attacher::eRefType>& refs
);

enum class Value
{
    None,
    Distance,  ///< AttachmentOffset.Base.z
    Angle,     ///< Rotation about X of AttachmentOffset
    Position,  ///< MapPathParameter
};

/// The value that the type is defined by, besides its references
PartExport Value valueOf(Type type);

}  // namespace Part::ConstructionPlane
