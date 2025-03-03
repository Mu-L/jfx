/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "SVGTextPositioningElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGGlyphElement;

class SVGAltGlyphElement final : public SVGTextPositioningElement, public SVGURIReference {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGAltGlyphElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGAltGlyphElement);
public:
    static Ref<SVGAltGlyphElement> create(const QualifiedName&, Document&);

    const AtomString& glyphRef() const;
    ExceptionOr<void> setGlyphRef(const AtomString&);
    const AtomString& format() const;
    ExceptionOr<void> setFormat(const AtomString&);

    bool hasValidGlyphElements(Vector<String>& glyphNames) const;

private:
    SVGAltGlyphElement(const QualifiedName&, Document&);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGAltGlyphElement, SVGTextPositioningElement, SVGURIReference>;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
};

} // namespace WebCore
