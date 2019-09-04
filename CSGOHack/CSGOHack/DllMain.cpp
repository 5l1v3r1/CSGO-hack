
/*
  CSGO��Ϸ����͸�Ӹ���ģ��
  �����ô������κ�Υ��������

  Copyright (C) 2019 FYH

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful

*/

#include "HackFrame.h"

//��ʼ��
void Initialize(void* pBuf);

//�ж���Ϸ����
bool IsGameProcess(HWND hGameWindow);

BOOL WINAPI DllMain(
	_In_ void* _DllHandle,
	_In_ unsigned long _Reason,
	_In_opt_ void* _Reserved)
{
	switch (_Reason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(static_cast<HMODULE>(_DllHandle));
		HideModule(_DllHandle);
		CreateDebugConsole();
		_beginthread(Initialize, 0, NULL);
		break;
	case DLL_PROCESS_DETACH:
		ReleaseAll();
		break;
	}
	return TRUE;
}

void Initialize(void* pBuf)
{
	const char* szGameTitle = "Counter-Strike: Global Offensive";
	HWND hGameWindow = FindWindowA(NULL, szGameTitle);
	if (hGameWindow == NULL)
	{
		std::cout << "FindWindowA fail" << std::endl;
		return;
	}

	if (IsGameProcess(hGameWindow) == false)
	{
		std::cout << "Game Process fail" << std::endl;
		return;
	}

	InitializeD3DHook(hGameWindow);
}

bool IsGameProcess(HWND hGameWindow)
{
	DWORD dwCurrentID = 0, dwTargetID = 0;
	GetWindowThreadProcessId(hGameWindow, &dwTargetID);
	dwCurrentID = GetCurrentProcessId();
	return (dwCurrentID == dwTargetID);
}
