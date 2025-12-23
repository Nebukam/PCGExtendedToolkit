// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_VECTORINPUTBOX(_HANDLE)\
SNew(SVectorInputBox)\
.X_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("X"))->GetValue(Val); return Val; })\
.Y_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Y"))->GetValue(Val); return Val; })\
.Z_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Z"))->GetValue(Val); return Val; })\
.OnXCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("X"))->SetValue(Val); })\
.OnYCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Y"))->SetValue(Val); })\
.OnZCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Z"))->SetValue(Val); })

#define PCGEX_ROTATORINPUTBOX(_HANDLE)\
SNew(SRotatorInputBox)\
.Roll_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Roll"))->GetValue(Val); return Val; })\
.Pitch_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Pitch"))->GetValue(Val); return Val; })\
.Yaw_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Yaw"))->GetValue(Val); return Val; })\
.OnRollCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Roll"))->SetValue(Val); })\
.OnPitchCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Pitch"))->SetValue(Val); })\
.OnYawCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Yaw"))->SetValue(Val); })
