/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <windows.h>

namespace WorldBuilderMcpBridge
{

// TheSuperHackers @feature Xilonen 30/07/2026 Exposes safe WorldBuilder automation to local MCP clients.
const ULONG_PTR REQUEST_MAGIC = 0x57424D43; // "WBMC"
const wchar_t WINDOW_PROPERTY[] = L"GeneralsWorldBuilderMcp";
const UINT PROCESS_REQUEST_MESSAGE = WM_APP + 0x0143;
const ULONG_PTR BRIDGE_VERSION = 8;

void Attach(HWND window);
void Detach(HWND window);
BOOL IsEnabled(HWND window);
BOOL IsRequest(const COPYDATASTRUCT *copy_data);
BOOL HandleRequest(const COPYDATASTRUCT *copy_data);
BOOL QueueRequest(HWND window, const COPYDATASTRUCT *copy_data);
LRESULT ProcessQueuedRequest(LPARAM parameter);

} // namespace WorldBuilderMcpBridge
