// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Automatically include necessary headers
#include "Helpers/PCGAsync.h"
#include "PCGContext.h"

/*! 
\brief Simple copy-by-assignment. Expect Context to be set. 
\param _SOURCE The source TArray<FPCGPoint> to copy from.
\param _TARGET The target TArray<FPCGPoint> to copy to.
*/
#define PCGEX_SIMPLE_COPY_POINTS(_SOURCE, _TARGET) \
FPCGAsync::AsyncPointProcessing( Context, _SOURCE, _TARGET, \
[](const FPCGPoint& InPoint, FPCGPoint& OutPoint) \
{ OutPoint = InPoint; return true; });
/*! 
\brief Simple copy-by-assignment. Expect Context to be set.
\param _SOURCE The source TArray<FPCGPoint> to copy from.
\param _TARGET The target TArray<FPCGPoint> to copy to.
\param _BODY Code body appended after the copy.
\param ... List of captured members so they are made accessible inside the _BODY
*/
#define PCGEX_COPY_POINTS(_SOURCE, _TARGET, _BODY, ...) \
FPCGAsync::AsyncPointProcessing( Context, _SOURCE, _TARGET, \
[__VA_ARGS__](const FPCGPoint& InPoint, FPCGPoint& OutPoint) \
{ OutPoint = InPoint; \
_BODY \
return true; \
});
/*! 
\brief Simple async point loop that goes over each point in the provided TArray<FPCGPoint> _POINT. Exposes `Point` and `Index` members.
\param _POINTS TArray<FPCGPoint> to loop over
\param _BODY Loop body
\param ... List of captured members so they are made accessible inside the _BODY
*/
#define PCGEX_POINT_LOOP(_POINTS, _BODY, ...) \
FPCGAsync::AsyncPointProcessing(Context, static_cast<int32>(_POINTS.Num()), _POINTS, \
[__VA_ARGS__](int32 Index, FPCGPoint& Point) \
_BODY \
);
