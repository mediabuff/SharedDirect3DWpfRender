#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <strsafe.h>

#define WIDTH 300
#define HEIGHT 300

typedef HRESULT (WINAPI *DIRECT3DCREATE9EX)(UINT SDKVersion, IDirect3D9Ex**);

DIRECT3DCREATE9EX       g_pfnCreate9Ex  = NULL;
BOOL                    g_is9Ex         = FALSE;
LPDIRECT3D9             g_pD3D          = NULL;
LPDIRECT3D9EX           g_pD3DEx        = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice    = NULL;
LPDIRECT3DDEVICE9EX     g_pd3dDeviceEx  = NULL;
LPDIRECT3DSURFACE9      g_pd3dSurface   = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB           = NULL;


LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


WNDCLASSEX g_wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
      GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"Foo", NULL };


struct CUSTOMVERTEX
{
    FLOAT x, y, z;
    DWORD color;
};


#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)


VOID SetupMatrices()
{
    // Set up rotation matrix
    D3DXMATRIXA16 matWorld;
    UINT  iTime  = timeGetTime() % 1000;
    FLOAT fAngle = iTime * (2.0f * D3DX_PI) / 1000.0f;
    D3DXMatrixRotationY(&matWorld, fAngle);
    g_pd3dDevice->SetTransform(D3DTS_WORLD, &matWorld);

    // Set up view matrix
    D3DXVECTOR3 vEyePt(0.0f, 3.0f,-4.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);
    g_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);

    // Set up projection matrix
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI/4, 1.0f, 1.0f, 100.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}


DWORD GetVertexProcessingCaps()
{
    D3DCAPS9 caps;
    DWORD dwVertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    if (SUCCEEDED(g_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)))
    {
        if ((caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == D3DDEVCAPS_HWTRANSFORMANDLIGHT)
        {
            dwVertexProcessing = D3DCREATE_HARDWARE_VERTEXPROCESSING;
        }
    }
    return dwVertexProcessing;
}


void InitializeD3D(HWND hWnd, D3DPRESENT_PARAMETERS d3dpp)
{
    // initialize Direct3D
    if (NULL == (g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
    {
        return;
    }

    // determine what type of vertex processing to use based on the device capabilities
    DWORD dwVertexProcessing = GetVertexProcessingCaps();

    // create the D3D device
    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        dwVertexProcessing | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &d3dpp, &g_pd3dDevice)))
    {
        return;
    }
}


void InitializeD3DEx(HWND hWnd, D3DPRESENT_PARAMETERS d3dpp)
{
    // initialize Direct3D using the Ex function
    if (FAILED((*g_pfnCreate9Ex)(D3D_SDK_VERSION, &g_pD3DEx)))
    {
        return;
    }

    // obtain the standard D3D interface
    if (FAILED(g_pD3DEx->QueryInterface(__uuidof(IDirect3D9), reinterpret_cast<void **>(&g_pD3D))))
    {
        return;
    }

    // determine what type of vertex processing to use based on the device capabilities
    DWORD dwVertexProcessing = GetVertexProcessingCaps();

    // create the D3D device using the Ex function
    if (FAILED(g_pD3DEx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        dwVertexProcessing | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &d3dpp, NULL, &g_pd3dDeviceEx)))
    {
        return;
    }

    // obtain the standard D3D device interface
    if (FAILED(g_pd3dDeviceEx->QueryInterface(__uuidof(IDirect3DDevice9), reinterpret_cast<void **>(&g_pd3dDevice))))
    {
        return;
    }
}


extern "C" __declspec(dllexport) LPVOID WINAPI InitializeScene()
{
    // vertices for geometry
    CUSTOMVERTEX g_Vertices[] =
    {
        { -1.0f,-1.0f, 0.0f, 0xffff0000, },
        {  1.0f,-1.0f, 0.0f, 0xff0000ff, },
        {  0.0f, 1.0f, 0.0f, 0xC0ffffff, },
    };

    // set up the structure used to create the D3DDevice
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferHeight = 1;
    d3dpp.BackBufferWidth = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;

    // create the device's window
    RegisterClassEx(&g_wc);
    HWND hWnd = CreateWindow(L"Foo", L"Foo", WS_OVERLAPPEDWINDOW, 
        0, 0, 0, 0, NULL, NULL, g_wc.hInstance, NULL);

    // Vista requires the D3D "Ex" functions for optimal performance.  
    // The Ex functions are only supported with WDDM drivers, so they 
    // will not be available on XP.  As such, we must use the D3D9Ex 
    // functions on Vista and the D3D9 functions on XP.

    // Rather than query the OS version, we can simply check for the
    // 9Ex functions, since this is ultimately what we care about.
    HMODULE hD3D9 = LoadLibrary(TEXT("d3d9.dll"));
    g_pfnCreate9Ex = (DIRECT3DCREATE9EX)GetProcAddress(hD3D9, "Direct3DCreate9Ex");
    g_is9Ex = (g_pfnCreate9Ex != NULL);
    FreeLibrary(hD3D9);

    if (g_is9Ex)
    {
        InitializeD3DEx(hWnd, d3dpp);
    }
    else
    {
        InitializeD3D(hWnd, d3dpp);
    }

    // create and set the render target surface
    // it should be lockable on XP and nonlockable on Vista
    if (FAILED(g_pd3dDevice->CreateRenderTarget(WIDTH, HEIGHT, 
        D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, 
        !g_is9Ex, // lockable
        &g_pd3dSurface, NULL)))
    {
        return NULL;
    }
    g_pd3dDevice->SetRenderTarget(0, g_pd3dSurface);

    // turn off culling to view both sides of the triangle
    g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    
    // turn off D3D lighting to use vertex colors
    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // create vertex buffer
    if (FAILED(g_pd3dDevice->CreateVertexBuffer(3*sizeof(CUSTOMVERTEX),
        0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL)))
    {
        return NULL;
    }

    // fill vertex buffer
    VOID* pVertices;
    if (FAILED(g_pVB->Lock(0, sizeof(g_Vertices), (void**)&pVertices, 0)))
    {    
        return NULL;
    }
    memcpy(pVertices, g_Vertices, sizeof(g_Vertices));
    g_pVB->Unlock();

    return g_pd3dSurface;
}


extern "C" __declspec(dllexport) void WINAPI RenderScene(LPSIZE pSize)
{
    // clear the surface to transparent
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

    // render the scene
    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        // setup the world, view, and projection matrices
        SetupMatrices();

        // render the vertex buffer contents
        g_pd3dDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
        g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
        g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 1);

        // end the scene
        g_pd3dDevice->EndScene();
    }

    // return the full size of the surface
    pSize->cx = WIDTH;
    pSize->cy = HEIGHT;
}


extern "C" __declspec(dllexport) VOID WINAPI ReleaseScene()
{
    UnregisterClass(NULL, g_wc.hInstance);

    if (g_pVB != NULL)
        g_pVB->Release();

    if (g_pd3dDevice != NULL)
        g_pd3dDevice->Release();

    if (g_pd3dDeviceEx != NULL)
        g_pd3dDeviceEx->Release();

    if (g_pD3D != NULL)
        g_pD3D->Release();

    if (g_pD3DEx != NULL)
        g_pD3DEx->Release();

    if (g_pd3dSurface != NULL)
        g_pd3dSurface->Release();
}
