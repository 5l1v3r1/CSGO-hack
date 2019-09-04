#include "HackFrame.h"

IDirect3D9* g_pOwnDirect3D = nullptr;							//�Լ���IDirect3D9
IDirect3DDevice9* g_pOwnDirect3DDevice = nullptr;				//�Լ���IDirect3DDevice9

IDirect3D9* g_pGameDirect3D = nullptr;							//��Ϸ�����IDirect3D9
IDirect3DDevice9* g_pGameDirect3DDevice = nullptr;				//��Ϸ�����IDirect3DDevice9

Type_Reset g_fReset = nullptr;									//Reset
Type_Present g_fPresent = nullptr;								//Present
Type_EndScene g_fEndScene = nullptr;							//EndScene
Type_DrawIndexedPrimitive g_fDrawIndexedPrimitive = nullptr;	//DrawIndexedPrimitive

HWND g_hGameWindow = NULL;										//��Ϸ���ھ��
WNDPROC g_fOriginProc = nullptr;								//ԭ���ں�����ַ

bool g_bCallOne = true;											//ֻ����һ��
bool g_bShowImgui = true;										//��ʾimgui�˵�
bool g_bShowDefenders = false;									//��ʾ������
bool g_bShowSleeper = false;									//��ʾǱ����
bool g_bDrawBox = false;										//��ʾ���﷽��
bool g_bTellFirearms = false;									//��ʾǹе
bool g_bAiMBotDefenders = false;								//��������
bool g_bAiMBotSleepers = false;									//Ǳ������

std::list<UINT> g_cDefenderStride;								//�����߱�ʶ
std::list<UINT> g_cSleeperStride;								//Ǳ���߱�ʶ
std::list<Firearm> g_cFirearm;									//ǹе��ʶ

char g_szFirearm[MAX_PATH] = { 0 };								//ǹе�ı�									

int g_nMetrixBaseAddress = 0;									//�����ַ
int g_nAngleBaseAddress = 0;									//ƫ���ǣ������ǻ�ַ
int g_nOwnBaseAddress = 0;										//�Լ���ַ
int g_nTargetBaseAddress = 0;									//���˻�ַ

blackbone::Process g_cGame;										//��Ϸ������Ϣ

void CreateDebugConsole()
{
#if _DEBUG
	AllocConsole();
	SetConsoleTitleA("HackFrame Log");
	freopen("CON", "w", stdout);
#endif
}

bool InitializeD3DHook(_In_ HWND hGameWindow)
{
	g_hGameWindow = hGameWindow;
	try
	{
		g_pOwnDirect3D = Direct3DCreate9(D3D_SDK_VERSION);
		if (g_pOwnDirect3D == nullptr)
			throw std::runtime_error("Direct3DCreate9 fail");

		D3DPRESENT_PARAMETERS stPresent;
		ZeroMemory(&stPresent, sizeof(D3DPRESENT_PARAMETERS));
		stPresent.Windowed = TRUE;
		stPresent.SwapEffect = D3DSWAPEFFECT_DISCARD;
		stPresent.BackBufferFormat = D3DFMT_UNKNOWN;
		stPresent.EnableAutoDepthStencil = TRUE;
		stPresent.AutoDepthStencilFormat = D3DFMT_D16;
		if (FAILED(g_pOwnDirect3D->CreateDevice(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			hGameWindow,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&stPresent,
			&g_pOwnDirect3DDevice)))
			throw std::runtime_error("CreateDevice fail");

		int* pDirect3DDeviceTable = (int*)*(int*)g_pOwnDirect3DDevice;

		g_fReset = reinterpret_cast<Type_Reset>(pDirect3DDeviceTable[F_Reset]);
		g_fPresent = reinterpret_cast<Type_Present>(pDirect3DDeviceTable[F_Present]);
		g_fEndScene = reinterpret_cast<Type_EndScene>(pDirect3DDeviceTable[F_EndScene]);
		g_fDrawIndexedPrimitive = reinterpret_cast<Type_DrawIndexedPrimitive>(pDirect3DDeviceTable[F_DrawIndexedPrimitive]);

		if (DetourTransactionBegin() != NO_ERROR)
			throw std::runtime_error("DetourTransactionBegin fail");
		if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR)
			throw std::runtime_error("DetourUpdateThread fail");
		if (DetourAttach(&(LPVOID&)g_fReset, MyReset) != NO_ERROR)
			throw std::runtime_error("Hook Reset fail");
		if (DetourAttach(&(LPVOID&)g_fPresent, MyPresent) != NO_ERROR)
			throw std::runtime_error("Hook Present fail");
		if (DetourAttach(&(LPVOID&)g_fEndScene, MyEndScene) != NO_ERROR)
			throw std::runtime_error("Hook EndScene fail");
		if (DetourAttach(&(LPVOID&)g_fDrawIndexedPrimitive, MyDrawIndexedPrimitive) != NO_ERROR)
			throw std::runtime_error("Hook DrawIndexedPrimitive fail");
		if (DetourTransactionCommit() != NO_ERROR)
			throw std::runtime_error("DetourTransactionCommit fail");

	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
		return false;
	}
	return true;
}

void ReleaseAll()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(LPVOID&)g_fReset, MyReset);
	DetourDetach(&(LPVOID&)g_fPresent, MyPresent);
	DetourDetach(&(LPVOID&)g_fEndScene, MyEndScene);
	DetourDetach(&(LPVOID&)g_fDrawIndexedPrimitive, MyDrawIndexedPrimitive);
	DetourTransactionCommit();

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

HRESULT _stdcall MyReset(IDirect3DDevice9* pDirect3DDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	HRESULT hRet = g_fReset(pDirect3DDevice, pPresentationParameters);
	ImGui_ImplDX9_CreateDeviceObjects();
	return hRet;
}

HRESULT _stdcall MyPresent(IDirect3DDevice9* pDirect3DDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
	return g_fPresent(pDirect3DDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT _stdcall MyEndScene(IDirect3DDevice9* pDirect3DDevice)
{
	if (g_bCallOne)
	{
		g_bCallOne = false;
		g_pGameDirect3DDevice = pDirect3DDevice;

		g_fOriginProc = (WNDPROC)SetWindowLongPtrA(g_hGameWindow, GWL_WNDPROC, (LONG)MyWindowProc);
		InitializeImgui();
		InitializeData();
		InitializeFirearm();
	}

	DrawPlayerBox(pDirect3DDevice);
	DrawImgui();

	return g_fEndScene(pDirect3DDevice);
}

HRESULT _stdcall MyDrawIndexedPrimitive(IDirect3DDevice9 * pDirect3DDevice, D3DPRIMITIVETYPE stType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
	//if (NumVertices <= g_nStride)
	//	g_fDrawIndexedPrimitive(pDirect3DDevice, stType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
	//return S_OK;
	bool bTargetModel = false;
	auto CheckDefender = [&]()->void
	{
		if (bTargetModel == false && g_bShowDefenders)
		{
			for (auto it : g_cDefenderStride)
			{
				if (it == NumVertices)
				{
					bTargetModel = true;
					break;
				}
			}
		}
	};
	auto CheckSleeper = [&]()->void
	{
		if (bTargetModel == false && g_bShowSleeper)
		{
			for (auto it : g_cSleeperStride)
			{
				if (it == NumVertices)
				{
					bTargetModel = true;
					break;
				}
			}
		}
	};
	auto CheckFirearm = [&]()->void
	{
		if (g_bTellFirearms)
		{
			for (auto it :g_cFirearm)
			{
				if (it.nCount == NumVertices)
				{
					strncpy(g_szFirearm, it.szName, sizeof(it.szName));
					break;
				}
			}
		}
	};
	
	CheckDefender();
	CheckSleeper();
	CheckFirearm();

	IDirect3DBaseTexture9* pOldTexture = nullptr;
	IDirect3DTexture9* pNewTexture = nullptr;
	if (bTargetModel)
	{
		pDirect3DDevice->SetRenderState(D3DRS_ZENABLE, false);
		pDirect3DDevice->GetTexture(0, &pOldTexture);
		pDirect3DDevice->SetTexture(0, pNewTexture);
		g_fDrawIndexedPrimitive(pDirect3DDevice, stType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		pDirect3DDevice->SetRenderState(D3DRS_ZENABLE, true);
		pDirect3DDevice->SetTexture(0, pOldTexture);
	}
	return g_fDrawIndexedPrimitive(pDirect3DDevice, stType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

bool InitializeImgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsLight();

	ImGui_ImplWin32_Init(g_hGameWindow);
	ImGui_ImplDX9_Init(g_pGameDirect3DDevice);
#ifdef Load_Fonts
	ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
	IM_ASSERT(font != NULL);
#endif // Load_Fonts
	return true;
}

void DrawImgui()
{
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (g_bShowImgui)
	{
		ImGui::Begin(u8"CSGO͸�����鸨��ģ��", &g_bShowImgui);
		ImGui::Text(u8"����˵��:F12		�˵���ʾ����");
		ImGui::Text(u8"����˵��:Shift	�����������");
		ImGui::Checkbox(u8"͸�ӱ�����", &g_bShowDefenders);
		ImGui::Checkbox(u8"͸��Ǳ����", &g_bShowSleeper);
		ImGui::Checkbox(u8"���鱣����", &g_bAiMBotDefenders);
		ImGui::Checkbox(u8"����Ǳ����", &g_bAiMBotSleepers);
		ImGui::Checkbox(u8"����ķ���", &g_bDrawBox);
		ImGui::Checkbox(u8"ǹе��Ԥ��", &g_bTellFirearms);

		static int nMapSelect = -1;
		const char* ComBoList[] = { 
			u8"����ɳ��2", u8"��Į�Գ�", u8"����С��", u8"��������", 
			u8"�ű���ս", u8"����֮��", u8"����԰", u8"�챦ʯ����", 
			u8"̩̹�ع�˾", u8"����Σ��", u8"�г�ͣ��վ", u8"��������԰",
			u8"����Σ��", u8"�˺�ˮ��" ,u8"�칫��¥",u8"Ӷ��ѵ��Ӫ",
			u8"�칫��",u8"�����С��",u8"�ֿ�ͻ��"};
		if (ImGui::Combo(u8"��ͼѡ��", &nMapSelect, ComBoList, IM_ARRAYSIZE(ComBoList)))
			InitializeStride(static_cast<GameMap>(nMapSelect));
		ImGui::End();
	}

	if (g_bTellFirearms)
	{
		ImGui::Begin(u8"ǹе��ʾ����", &g_bTellFirearms);
		ImGui::Text(g_szFirearm);
		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MyWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	if (uMsg == WM_KEYDOWN)
	{
		if (wParam == VK_F12)
			g_bShowImgui = !g_bShowImgui;		
	}

	if (g_fOriginProc)
		return CallWindowProcA(g_fOriginProc, hWnd, uMsg, wParam, lParam);

	return true;
}

void InitializeStride(GameMap eMap)
{
	g_cDefenderStride.clear();
	g_cSleeperStride.clear();

	switch (eMap)
	{
	case Map_HotCity:					//����ɳ��2
	{
		UINT nDefenderArray[] = { 140,1310,1383,1432,1761,1677,2052,2118,3763 };
		UINT nSleeperArray[] = { 622,903,984,1006,2369,2663,3225,3692,3742,3932,3845,5008,5781 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_DesertMaze:				//��Į�Գ�
	{	
		UINT nDefenderArray[] = { 136,140,1310,1383,1432,1967,2052,2482,5299 };
		UINT nSleeperArray[] = { 290,622,903,1006,2369,2663,3225,3692,3742,3845,5008,5781,3932,5572 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_PurgatoryTown:				//����С��
	{
		UINT nDefenderArray[] = { 136,140,1310,1383,1432,1967,2052,2482,5299 };
		UINT nSleeperArray[] = { 290,903,1106,1914,2369,2536 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_DeathBuilding:				//��������
	{
		UINT nDefenderArray[] = { 140,1222,1439,1735,1702,1310,1374,1432,2052,2254,2266,2489,2856,3093,7163 };
		UINT nSleeperArray[] = { 200,290,903,924,927,1053,1197,2056,2112,3368 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_GubaoFighting:				//�ű���ս
	{
		UINT nDefenderArray[] = { 140,256,1258,1432,1601,1611,1645,2052,3630 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_DeadCity:					//����֮��
	{
		UINT nDefenderArray[] = { 140,256,1258,1310,1383,1432,1601,1611,1645,2052,3630 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_Zoo:						//����԰
	{
		UINT nDefenderArray[] = { 140,1310,1374,1432,1222,1439,1735,1702,2052,3152,7163 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_RubyWineTown:				//�챦ʯ����
	{
		UINT nDefenderArray[] = { 136,140,1310,1383,1432,1967,2052,2482,5299 };
		UINT nSleeperArray[] = { 290,903,1462,1671,1925,2369,2447 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_TangentCompany:			//̹���ع�˾
	{
		UINT nDefenderArray[] = { 140,1222,1702,1735,1310,1374,1383,1432,2052,2254,2266,2856,7163 };
		UINT nSleeperArray[] = { 200,290,903,924,927,1053,1197,2056,2112,2369,3368 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_CoastalCrisis:				//����Σ��
	{
		UINT nDefenderArray[] = { 140,1222,1439,1702,1735,1310,1374,1383,1432,2052,2254,2266,2489,3093,7163 };
		UINT nSleeperArray[] = { 290,692,903,1003,1183,1314,1380,1996,2369,7236 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_TrainpParkingStation:		//�г�ͣ��վ
	{
		UINT nDefenderArray[] = { 252,290,903,1214,1215,1345,1624,1273,1525,2112,2369,3836,4159 };
		UINT nSleeperArray[] = { 64,140,1310,1383,1432,2639,3137,3245,4084,2924,4320,4510,4422,4533 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_DeathAmusementPark:		//��������԰
	{
		UINT nDefenderArray[] = { 116,140,1383,1410,1432,2052,2113,2151,2157,2239 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_NuclearCrisis:				//����Σ��
	{
		UINT nDefenderArray[] = { 140,1222,1439,1735,1702,1310,1374,1383,1432,2052,2266,2489,2856,7163 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_CanalWaterCity:			//�˺�ˮ��
	{
		UINT nDefenderArray[] = { 120,1310,1383,1432,2052,2539,2963,3137,3245,2924,3220,4510,4533,4666 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_OfficeBuilding:			//�칫��¥
	{
		UINT nDefenderArray[] = { 140,1222,1439,1735,1702,1310,1374,1383,2052,2489,3093,7163 };
		UINT nSleeperArray[] = { 200,290,646,927,1053,1197,903,2056,2112,2369,3368 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_MercenaryTrainingCamp:		//Ӷ��ѵ��Ӫ
	{
		UINT nDefenderArray[] = { 140,1222,1439,1702,1310,1374,1383,2052,2254,2489,2856,3093,7163 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_Office:					//�칫��
	{
		UINT nDefenderArray[] = { 140,1222,1439,1735,1702,1310,1383,1374,2052,2266,2856,7163 };
		UINT nSleeperArray[] = { 290,903,1462,1671,1925,2369,2447 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_ItalianTown:				//�����С��
	{
		UINT nDefenderArray[] = { 140,256,1310,1258,1645,1611,1601,1645,2052,3630 };
		UINT nSleeperArray[] = { 290,903,1106,1914,2112,2369,2536 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	case Map_WarehouseAssault:			//�ֿ�ͻ��
	{
		UINT nDefenderArray[] = { 140,1222,1439,1735,1702,1310,1383,1374,2052,2266,2856,3093,2489 };
		UINT nSleeperArray[] = { 290,622,903,1395,2112,2369,3164,4410 };
		for (int i = 0; i < sizeof(nDefenderArray) / sizeof(UINT); i++)
			g_cDefenderStride.emplace_back(nDefenderArray[i]);
		for (int i = 0; i < sizeof(nSleeperArray) / sizeof(UINT); i++)
			g_cSleeperStride.emplace_back(nSleeperArray[i]);
		break;
	}
	}
}

void InitializeData()
{
	g_cGame.Attach(GetCurrentProcessId());
	auto nClientBaseAddress = g_cGame.modules().GetModule(std::wstring(L"client_panorama.dll"))->baseAddress;
	auto nEngineBaseAddress = g_cGame.modules().GetModule(std::wstring(L"engine.dll"))->baseAddress;

	int nMatrixOffset = 0x4cf9804;
	int nAngleOffset = 0x590D94;
	int nOwnBaseOffset = 0xCF5A4C;
	int nTargetOffset = 0x4D07DE4;
	
	g_nMetrixBaseAddress = static_cast<int>(nClientBaseAddress) + nMatrixOffset;
	g_nAngleBaseAddress = static_cast<int>(nEngineBaseAddress) + nAngleOffset;
	g_nOwnBaseAddress = static_cast<int>(nClientBaseAddress) + nOwnBaseOffset;
	g_nTargetBaseAddress = static_cast<int>(nClientBaseAddress) + nTargetOffset;
}

void InitializeFirearm()
{
	Firearm stFireData[] =
	{
	{2258,u8"�������� ����"},
	{3796,u8"�������� MX1014"},
	{1941,u8"�������� MAG-7"},
	{2217,u8"�������� �ض�ɢ��ǹ"},
	{4280,u8"�������� M249"},
	{4122,u8"�������� �ڸ��"},

	{3851,u8"΢�ͳ��ǹ MP9"},
	{2729,u8"΢�ͳ��ǹ MAC-10"},
	{4485,u8"΢�ͳ��ǹ MP7"},
	{3152,u8"΢�ͳ��ǹ UMP5"},
	{4480,u8"΢�ͳ��ǹ P90"},
	{3398,u8"΢�ͳ��ǹ PP-Ұţ"},

	{3633,u8"��ǹ ����˹"},
	{15252,u8"��ǹ ������AR"},
	{3661,u8"��ǹ Ak-47"},
	{2770,u8"��ǹ M4A1"},
	{2899,u8"��ǹ SSG-08"},
	{4375,u8"��ǹ SG-553"},
	{1941,u8"��ǹ AUG "},
	{3685,u8"��ǹ AWP"},
	{3411,u8"��ǹ G3SG1"},
	{4557,u8"��ǹ SCAR-20"},

	{1871,u8"װ�� ��˹ X275"},

	{1310,u8"��ǹ P2000"},
	{2369,u8"��ǹ �����18"},
	{2322,u8"��ǹ ˫�ֱ�����"},
	{1766,u8"��ǹ P250"},
	{2847,u8"��ǹ Tec-9"},
	{1426,u8"��ǹ FN-57"},
	{2140,u8"��ǹ ɳĮ֮ӥ"},

	{4329,u8"���� C4ը��"}
	};

	for (int i = 0; i < sizeof(stFireData) / sizeof(Firearm); i++)
	{
		g_cFirearm.emplace_back(stFireData[i]);
	}
}

void DrawBox(IDirect3DDevice9* pDirect3DDevice, D3DCOLOR dwColor, int x, int y, int w, int h)
{
	auto FillRgbColor = [&](int x, int y, int w, int h) -> void
	{
		x = (x <= 0) ? 1 : x;
		y = (y <= 0) ? 1 : y;
		w = (w <= 0) ? 1 : w;
		h = (h <= 0) ? 1 : h;

		D3DRECT stRect = { x,y,x + w,y + h };
		pDirect3DDevice->Clear(1, &stRect, D3DCLEAR_TARGET, dwColor, 0.0f, 0);
	};
	auto DrawBorder = [&](int x, int y, int w, int h, int nPixel = 1) -> void
	{
		FillRgbColor(x, y + h, w, nPixel);			//�����±�
		FillRgbColor(x, y, nPixel, h);				//�������
		FillRgbColor(x, y, w, nPixel);				//�����ϱ�
		FillRgbColor(x + w, y, nPixel, h);			//�����ұ�
	};

	DrawBorder(x, y, w, h);
}

void DrawPlayerBox(IDirect3DDevice9* pDirect3DDevice)
{
	if (g_bDrawBox)
	{
		//��ȡ��Ϸ���ڵ�һ���Ⱥ͸߶�
		D3DVIEWPORT9 stView;
		if (FAILED(pDirect3DDevice->GetViewport(&stView)))return;
		DWORD dwHalfWidth = stView.Width / 2;
		DWORD dwHalfHeight = stView.Height / 2;

		int nOwnPosBaseAddress = 0;							//�Լ��������ַ
		int nOwnPosOffset = 0x35A4;							//�Լ�����ƫ��
		float pOwnPos[Coordinate_Number];					//�Լ�����

		int nBoxColor = D3DCOLOR_XRGB(0, 0, 255);			//���﷽����ɫ
		int nPlayerCamp;									//������Ӫ
		int nPlayerCmapOffset = 0xf4;						//������Ӫƫ��

		int nPlayerAddress = 0;								//�����ַ
		int nNextPlayerAddress = 0x10;						//��һ������ĵ�ַƫ��
		int nPlayerPosOffset = 0xa0;						//�����ַƫ��

		int nPlayerBlood = 0;								//����Ѫ��
		int nPlayerBloodOffset = 0x100;						//����Ѫ��ƫ��

		float fPlayerPos[Coordinate_Number];				//�����ַ
		AiMBotInfo stAiMBotInfo;							//�������������

		float fOwnMatrix[4][4];								//����

		//��ȡ������Ϣ
		g_cGame.memory().Read(g_nMetrixBaseAddress, sizeof(fOwnMatrix), fOwnMatrix);
		//��ȡ�ڴ��Լ���ַ
		g_cGame.memory().Read(g_nOwnBaseAddress, sizeof(int), &nOwnPosBaseAddress);
		//�����ڴ����ַ����ƫ�Ƶõ����ǵ�ǰ������
		g_cGame.memory().Read(nOwnPosBaseAddress + nOwnPosOffset, sizeof(pOwnPos), pOwnPos);

		for (int i = 0; i <= 20; i++)
		{
			//��ȡ�����ַ
			g_cGame.memory().Read(g_nTargetBaseAddress + i * nNextPlayerAddress, sizeof(int), &nPlayerAddress);
			if(nPlayerAddress == 0)
				continue;

			//��ȡ��������
			g_cGame.memory().Read(nPlayerAddress + nPlayerPosOffset, sizeof(fPlayerPos), fPlayerPos);
			//��ȡ������Ӫ
			g_cGame.memory().Read(nPlayerAddress + nPlayerCmapOffset, sizeof(nPlayerCamp), &nPlayerCamp);
			//��ȡ����Ѫ��
			g_cGame.memory().Read(nPlayerAddress + nPlayerBloodOffset, sizeof(nPlayerBlood), &nPlayerBlood);

			//��Ӫ
			//if ((nPlayerCamp == 2 && g_bAiMBotSleepers) || (nPlayerCamp == 3 && g_bAiMBotDefenders))
			//{
			//	float fX = (pOwnPos[Pos_X] - fPlayerPos[Pos_X])*(pOwnPos[Pos_X] - fPlayerPos[Pos_X]);
			//	float fY = (pOwnPos[Pos_Y] - fPlayerPos[Pos_Y])*(pOwnPos[Pos_Y] - fPlayerPos[Pos_Y]);
			//	float fZ = (pOwnPos[Pos_Z] - fPlayerPos[Pos_Z])*(pOwnPos[Pos_Z] - fPlayerPos[Pos_Z]);
			//	stAiMBotInfo.fVector = sqrt(fX + fY + fZ);
			//	stAiMBotInfo.fPlayerPos[Pos_X] = fPlayerPos[Pos_X];
			//	stAiMBotInfo.fPlayerPos[Pos_Y] = fPlayerPos[Pos_Y];
			//	stAiMBotInfo.fPlayerPos[Pos_Z] = fPlayerPos[Pos_Z];
			//}

			//����������
			if (nPlayerBlood <= 0)
				continue;

			//����
			float fTarget =
				fOwnMatrix[2][0] * fPlayerPos[Pos_X]
				+ fOwnMatrix[2][1] * fPlayerPos[Pos_Y]
				+ fOwnMatrix[2][2] * fPlayerPos[Pos_Z]
				+ fOwnMatrix[2][3];

			//���Ǻ���ĵ��˲���Ҫ����͸��
			if (fTarget < 0.01f)
				continue;

			fTarget = 1.0f / fTarget;

			float fBoxX = dwHalfWidth
				+ (fOwnMatrix[0][0] * fPlayerPos[Pos_X]
					+ fOwnMatrix[0][1] * fPlayerPos[Pos_Y]
					+ fOwnMatrix[0][2] * fPlayerPos[Pos_Z]
					+ fOwnMatrix[0][3])*fTarget*dwHalfWidth;

			float fBoxY_H = dwHalfHeight
				- (fOwnMatrix[1][0] * fPlayerPos[Pos_X]
					+ fOwnMatrix[1][1] * fPlayerPos[Pos_Y]
					+ fOwnMatrix[1][2] * (fPlayerPos[Pos_Z] + 75.0f)
					+ fOwnMatrix[1][3])*fTarget*dwHalfHeight;

			float fBoxY_W = dwHalfHeight
				- (fOwnMatrix[1][0] * fPlayerPos[Pos_X]
					+ fOwnMatrix[1][1] * fPlayerPos[Pos_Y]
					+ fOwnMatrix[1][2] * (fPlayerPos[Pos_Z] - 5.0f)
					+ fOwnMatrix[1][3])*fTarget*dwHalfHeight;

			//������Ӫ��Ҳ���ǲ��Զ��ѽ����������
			if ((nPlayerCamp == 2 && g_bAiMBotSleepers) || (nPlayerCamp == 3 && g_bAiMBotDefenders))
			{
				float fX = (pOwnPos[Pos_X] - fPlayerPos[Pos_X])*(pOwnPos[Pos_X] - fPlayerPos[Pos_X]);
				float fY = (pOwnPos[Pos_Y] - fPlayerPos[Pos_Y])*(pOwnPos[Pos_Y] - fPlayerPos[Pos_Y]);
				float fZ = (pOwnPos[Pos_Z] - fPlayerPos[Pos_Z])*(pOwnPos[Pos_Z] - fPlayerPos[Pos_Z]);
				float fVector = sqrt(fX + fY + fZ);
				if (fVector < stAiMBotInfo.fVector)
				{
					stAiMBotInfo.fVector = fVector;
					stAiMBotInfo.fPlayerPos[Pos_X] = fPlayerPos[Pos_X];
					stAiMBotInfo.fPlayerPos[Pos_Y] = fPlayerPos[Pos_Y];
					stAiMBotInfo.fPlayerPos[Pos_Z] = fPlayerPos[Pos_Z];
				}
			}

			if (nPlayerCamp == 2)
				nBoxColor = D3DCOLOR_XRGB(255, 0, 0);//Ǳ����
			if (nPlayerCamp == 3)
				nBoxColor = D3DCOLOR_XRGB(0, 255, 0);//������

			DrawBox(pDirect3DDevice, nBoxColor,
				static_cast<int>(fBoxX - ((fBoxY_W - fBoxY_H) / 4.0f)),static_cast<int>(fBoxY_H), 
				static_cast<int>((fBoxY_W - fBoxY_H) / 2.0f), static_cast<int>(fBoxY_W - fBoxY_H));
		}

		//������������������
		if (GetAsyncKeyState(VK_SHIFT))
			AiMBot(pOwnPos, stAiMBotInfo.fPlayerPos);
	}
}

void HideModule(void* pModule)
{
	void* pPEB = nullptr;

	//��ȡPEBָ��
	_asm
	{
		push eax
		mov eax, fs:[0x30]
		mov pPEB, eax
		pop eax
	}

	//�����õ�����ȫ��ģ���˫������ͷָ��
	void* pLDR = *((void**)((unsigned char*)pPEB + 0xc));
	void* pCurrent = *((void**)((unsigned char*)pLDR + 0x0c));
	void* pNext = pCurrent;

	//��������б�������ָ��ģ����ж�������
	do
	{
		void* pNextPoint = *((void**)((unsigned char*)pNext));
		void* pLastPoint = *((void**)((unsigned char*)pNext + 0x4));
		void* nBaseAddress = *((void**)((unsigned char*)pNext + 0x18));

		if (nBaseAddress == pModule)
		{
			*((void**)((unsigned char*)pLastPoint)) = pNextPoint;
			*((void**)((unsigned char*)pNextPoint + 0x4)) = pLastPoint;
			pCurrent = pNextPoint;
		}

		pNext = *((void**)pNext);
	} while (pCurrent != pNext);
}

void AiMBot(float* pOwnPos,float* pEnemyPos)
{
	int nAngleAddress = 0;					//ƫб�Ǻ͸����ǵ�ַ
	int nAngleOffsetX = 0x4d88;				//ƫб�Ǻ͸�����ƫ��

	float pAngle[Angle_Number];				//ƫб�Ǻ͸�����
	static float PI = 3.14f;				//

	if (g_bAiMBotDefenders == false && g_bAiMBotSleepers == false)
		return;

	//�õ���������͵�������Ĳ�ֵ
	float fDifferX = pOwnPos[Pos_X] - pEnemyPos[Pos_X];
	float fDifferY = pOwnPos[Pos_Y] - pEnemyPos[Pos_Y];
	float fDifferZ = pOwnPos[Pos_Z] - pEnemyPos[Pos_Z];

	//����ƫб�Ƕ�X
	pAngle[Angle_X] = atan(fDifferY / fDifferX);
	if (fDifferX >= 0.0f && fDifferY >= 0.0f)
		pAngle[Angle_X] = pAngle[Angle_X] / PI * 180 - 180;
	else if (fDifferX < 0.0f && fDifferY >= 0.0f)
		pAngle[Angle_X] = pAngle[Angle_X] / PI * 180;
	else if (fDifferX < 0.0f && fDifferY < 0.0)
		pAngle[Angle_X] = pAngle[Angle_X] / PI * 180;
	else if (fDifferX >= 0.0f && fDifferY < 0.0f)
		pAngle[Angle_X] = pAngle[Angle_X] / PI * 180 + 180;

	//����������Y
	pAngle[Angle_Y] = atan(fDifferZ / sqrt(fDifferX * fDifferX + fDifferY * fDifferY)) / PI * 180 + 0.5f;

	//���Ƕ�д��
	g_cGame.memory().Read(g_nAngleBaseAddress, sizeof(int), &nAngleAddress);
	g_cGame.memory().Write(nAngleAddress + nAngleOffsetX, sizeof(pAngle), pAngle);
}
