//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection
//
// MODIFIED for OpenCNC3D. This is not EA's original file.
// Adds OPENCNC3D_SAVE_WIDTH_MASK and OPENCNC3D_SAVE_WIDTH_WIN64, the mark a 64-bit
// Windows build puts into the version word at the start of a saved game, so that it can
// refuse a save written by a 32-bit build. It does not change the game simulation.
// The complete diff against upstream is brain/patches/vanilla-opencnc3d.patch.

#pragma once

#ifndef DLL_INTERFACE_VERSION_H
#define DLL_INTERFACE_VERSION_H

/*
** DLL Interface version
**
**
**
*/
#define CNC_DLL_API_VERSION 0x102

/*
** OpenCNC3D: the pointer width a saved game was written with.
**
** A saved game is raw engine memory, pointers included, so a save written by a 32-bit
** build cannot be read by a 64-bit one, and cannot be converted. DLLExportClass::Save
** writes the version word first. A _WIN64 build ORs OPENCNC3D_SAVE_WIDTH_WIN64 into it,
** and a _WIN64 DLLExportClass::Load refuses a save whose word does not carry it, before
** it reads anything into the engine. Every other build writes and reads the word exactly
** as before. Bits 16-23 are free because CNC_DLL_API_VERSION is 0x102, and the check
** below keeps them free. A host that inspects a save's first word takes both values
** from this header, or asserts at compile time that its own copies equal them.
*/
#define OPENCNC3D_SAVE_WIDTH_MASK  0x00FF0000u
#define OPENCNC3D_SAVE_WIDTH_WIN64 0x00640000u

#if (CNC_DLL_API_VERSION & OPENCNC3D_SAVE_WIDTH_MASK) != 0
#error "CNC_DLL_API_VERSION has grown into the save width byte"
#endif

#endif // DLL_INTERFACE_VERSION_H
