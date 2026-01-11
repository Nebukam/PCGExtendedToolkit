/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Date      :  6 December 2025                                                 *
* Release   :  BETA RELEASE                                                    *
* Website   :  https://www.angusj.com                                          *
* Copyright :  Angus Johnson 2010-2025                                         *
* Purpose   :  Delaunay Triangulation                                          *
* License   :  https://www.boost.org/LICENSE_1_0.txt                           *
*******************************************************************************/

#pragma once

#ifndef PCGEX_CLIPPER_H_TRIANGULATION
#define PCGEX_CLIPPER_H_TRIANGULATION

#include <stack>
#include "Clipper2Lib/clipper.core.h"

namespace PCGExClipper2Lib
{
    enum class PCGEXELEMENTSCLIPPER2_API TriangulateResult
    {
        success,
        fail,
        no_polygons,
        paths_intersect
    };

    // Triangulate - this function will not accept intesecting paths
    PCGEXELEMENTSCLIPPER2_API
    TriangulateResult Triangulate(const Paths64& pp, Paths64& solution, bool useDelaunay = true);
    
    PCGEXELEMENTSCLIPPER2_API
    TriangulateResult Triangulate(const PathsD& pp, int decPlaces, PathsD& solution, bool useDelaunay = true);
} // Clipper2Lib namespace

#endif
