#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif

// * * * For Math * * * //
#include <math.h>
#include <DirectXMath.h>

// * * * Win and DX Headers * * * //
#include <Windows.h>
#include <d3d11.h>          // d3d interface
#include <dxgi.h>           // dx driver interface
#include <d3dcompiler.h>    // shader compiler

// * * * Useful * * * //
#include <assert.h>
#include <WICTextureLoader.h>

// * * * Width / Height Window * * * //
const int width = 800;
const int height = 600;

// * * * Functions * * * //
void releasePtrs();
bool initWin( HINSTANCE hInstance, HWND& hWnd, int width, int height, const wchar_t CLASSNAME[] );
bool initD3D( HWND hWnd, RECT client );
bool initScenegraphics();
void updateCBuffs(float rot, float transform);

// * * * Global pointers * * * //
// Init Direct3D
ID3D11Device* pDevice = NULL;                   
ID3D11DeviceContext* pDeviceContext = NULL;     
IDXGISwapChain* pSwapchain = NULL;              
ID3D11RenderTargetView* pRenderTarget = NULL;   

// Depth stencil
ID3D11DepthStencilState* pDepthStencilState = NULL;
ID3D11DepthStencilView* pDepthStencilView = NULL;
ID3D11Texture2D* pDepthStencilBuffer = NULL;

// Blobs to get shader-info from shader-hlsl
ID3DBlob* pVertexShaderBlob = NULL, * pPixelShaderBlob = NULL, * pErrorBlob = NULL;

// Vertex/index
ID3D11Buffer* pVertexBuffer = NULL, * pIndexBuffer = NULL;

// Constant buffers
ID3D11Buffer* pCBuffer = NULL, * pCBufferLight = NULL; 

// Input layout ptr
ID3D11InputLayout* pInputLayout = NULL;

// Shader ptrs
ID3D11VertexShader* pVertexShader = NULL;
ID3D11PixelShader* pPixelShader = NULL;

// Texturing
ID3D11SamplerState* pSamplerState = NULL;
ID3D11ShaderResourceView* pChessTexture = NULL, * pGorillaTexture = NULL;

// Rasterrizer
ID3D11RasterizerState* pRasterizerState = NULL;

// * * * Vertex / cBuffer structure
struct cBuffer
{   
    DirectX::XMMATRIX WVP;      // WorldViewProjection Matrix (Combined)
    DirectX::XMMATRIX World;    // World view
};

// Light struct, and a cBufferLight that contains a Light struct
struct Light
{
    Light()
    {
        ZeroMemory(this, sizeof(Light));
    }
    DirectX::XMFLOAT3 ambientLightColor;
    float ambientLightStrength; // makes it 16 byte aligned

    DirectX::XMFLOAT3 dynamicLightColor;
    float dynamicLightStrength; // makes it 16 byte aligned
    
    DirectX::XMFLOAT3 dynamicLightPosition;
    float padding;              // makes it 16 byte aligned

    // How lightning decreases when moving away from object
    DirectX::XMFLOAT3 dynamicAttenuation;
    float padding2;             // makes it 16 byte aligned
    
};

struct cBufferLight
{
    Light light;
};

struct Vertex
{    
    Vertex( float x, float y, float z,
            float colRed, float colGreen, float colBlue, float colAlpha, 
            float u, float v,
            float nx, float ny, float nz ) 
            : pos( x, y, z ), col( colRed, colGreen, colBlue, colAlpha ), texcoord( u, v ), normal( nx, ny, nz ) { }

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 col;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texcoord;
};

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow ) {
    
    // * * * Variables and Structs * * * //    
    const wchar_t CLASSNAME[] = L"Pudzze class";  

    HWND hWnd = { 0 };
    ZeroMemory(&hWnd, sizeof(HWND));    

    // * * *  Create and show window  * * * //
    if ( !initWin( hInstance, hWnd, width, height, CLASSNAME ) ) {
        MessageBeep( 1 );
        MessageBoxA( 0, "[ERROR] Creating a window -> Closing program!", "Fatal Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    else {
        ShowWindow( hWnd, nCmdShow );
    }

    // * * *  USED TO GET CLIENT SIZE  * * * //
    RECT winRect;
    GetClientRect( hWnd, &winRect );
    // - - - - - - - - - - - - - - - - - - - //  

    // * * *  Init d3d  * * * //
    if (!initD3D( hWnd, winRect )) {

        MessageBeep(1);
        MessageBoxA(0, "[ERROR] Initialize D3D11 -> Closing program!", "Fatal Error", MB_OK | MB_ICONERROR);
        return GetLastError();
    }           
    
    // * * *  Init scenegraphics  * * * //
    if ( !initScenegraphics() ) {
        MessageBeep(1);
        MessageBoxA(0, "[ERROR] Initialize scene graphics -> Closing program!", "Fatal Error", MB_OK | MB_ICONERROR);
        return GetLastError();
    }    

    // - - - - - Settings buffers - - - - - //
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    //UINT vertexCount = 4;

    float rot = 0.0f;   // Rotation cBuffer
    float transform = -2.0f;    // translation cBuffer   
    // - - - - - - - - - - - - - - - - - - - - - - //            

    // * * * * * MAIN LOOP STARTS HERE * * * * * //
    MSG msg = { 0 };
    ZeroMemory( &msg, sizeof(MSG) );

    bool close_program = false;
    while ( !close_program ) {
        if ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) ) {

            if (msg.message == WM_QUIT) {
                UnregisterClass( CLASSNAME, hInstance );
                break;
            }
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else {          

            // Clear background and set color
            float backgroundColor[4] = { 0.0f, 0.2f, 0.25f, 1.0f };
            pDeviceContext->ClearRenderTargetView( pRenderTarget, backgroundColor );

            // Clear Depth/Stencil view
            pDeviceContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                   
            // Output Merger - Set render target and depth/stencil view
            pDeviceContext->OMSetRenderTargets(1, &pRenderTarget, pDepthStencilView);

            // Input Assembler 
            pDeviceContext->IASetInputLayout(pInputLayout);
            pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);   

            // Rasterizer state
            pDeviceContext->RSSetState(pRasterizerState);
            
            // Output merger - Depth stencil state
            pDeviceContext->OMSetDepthStencilState(pDepthStencilState, 0);            

            // Sets vertex- /pixelshader
            pDeviceContext->VSSetShader(pVertexShader, nullptr, 0);
            pDeviceContext->PSSetShader(pPixelShader, nullptr, 0);

            // Set sampler    
            pDeviceContext->PSSetSamplers(0, 1, &pSamplerState);     


            // - - - - - CONSTANT BUFFER EFFECT SETTINGS - - - - - //
            //Keep the quads rotating
            rot += .0002f;
            if (rot > 6.285f)
                rot = 0.0f;    

            transform += 0.0001f;
            if (transform >= 2.0f)
                transform = -2.0f; 
                   
            updateCBuffs(rot, transform); 

            // Set texture
            pDeviceContext->PSSetShaderResources(0, 1, &pGorillaTexture);   

            // Input assembler - Set vertex/Indexbuffers
            pDeviceContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);           
            pDeviceContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);            

            pDeviceContext->DrawIndexed( 6, 0, 0 );   

            // Present back and frontbuffer
            pSwapchain->Present( 0, 0 );
        }      
    }

    // * * * Release ptrs * * * //
    releasePtrs();

    return ( int )msg.wParam;
}

void releasePtrs()
{
    pCBufferLight->Release();

    pGorillaTexture->Release();
    pSamplerState->Release();

    pVertexShader->Release();
    pPixelShader->Release();
    pInputLayout->Release();

    pCBuffer->Release();
    pIndexBuffer->Release();
    pVertexBuffer->Release();

    pDepthStencilBuffer->Release();
    pDepthStencilView->Release();

    pRasterizerState->Release();

    pRenderTarget->Release();
    pDeviceContext->Release();
    pSwapchain->Release();
    pDevice->Release();
}

bool initWin(HINSTANCE hInstance, HWND& hWnd, int width, int height, const wchar_t CLASSNAME[])
{
    // * * * Create classex and register, then create window * * * //
    WNDCLASSEX wc = { 0 };
    ZeroMemory( &wc, sizeof(WNDCLASSEX) );

                wc.cbSize = sizeof( WNDCLASSEX );
                wc.style = CS_HREDRAW | CS_VREDRAW; 
                wc.lpfnWndProc = WndProc;
                wc.hInstance = hInstance;
                wc.hCursor = LoadCursor(NULL, IDC_ARROW);
                wc.hbrBackground = NULL;
                wc.lpszClassName = CLASSNAME;    

    if ( !RegisterClassEx(&wc) ) {
        MessageBeep (1 );
        MessageBoxA( 0, "[Error] Register class", "Fatal Error", MB_OK );
        return GetLastError();
    }

    // * * * size of the client area * * * //
    RECT client = { 0, 0, width, height };
    AdjustWindowRect( &client, WS_OVERLAPPEDWINDOW, FALSE );

    hWnd = CreateWindowEx(  NULL,
                            CLASSNAME,
                            L"D3D11 Assignment",
                            WS_OVERLAPPEDWINDOW,    // WS_OVERLAPPEDWINDOW : standard features
                            CW_USEDEFAULT,          // CW_USEDEFAULT:
                            CW_USEDEFAULT,          //          standard position of new window 
                            client.right - client.left,
                            client.bottom - client.top,
                            NULL,
                            NULL,
                            hInstance,
                            NULL);

    if (!hWnd) {
        MessageBeep( 1 );
        MessageBoxA( 0, "[Error] Creating a window failed!", "Fatal Error", MB_OK | MB_ICONERROR );
        return GetLastError();
    }
    else {
        return true;
    }    
}

bool initD3D(HWND hWnd, RECT client)
{
    // * * * * * Initialize D3D11 * * * * * //
    DXGI_SWAP_CHAIN_DESC swapchain_Desc = { 0 };
    ZeroMemory( &swapchain_Desc, sizeof( DXGI_SWAP_CHAIN_DESC ) );

                swapchain_Desc.BufferDesc.RefreshRate.Numerator = 0;
                swapchain_Desc.BufferDesc.RefreshRate.Denominator = 1;
                swapchain_Desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                swapchain_Desc.SampleDesc.Count = 1;
                swapchain_Desc.SampleDesc.Quality = 0;
                swapchain_Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;   // used when I wish to draw graphics into the backbuffer
                swapchain_Desc.BufferCount = 1;                                 // total buffers | 1 = one backbuffer
                swapchain_Desc.OutputWindow = hWnd;
                swapchain_Desc.Windowed = true;
                swapchain_Desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // create device, devicecontext and swapchain COMs 
    HRESULT hr = D3D11CreateDeviceAndSwapChain(  NULL,    // NULL tells DXGI to take the "best" graphic card to use
                                                 D3D_DRIVER_TYPE_HARDWARE,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 D3D11_SDK_VERSION,
                                                 &swapchain_Desc,
                                                 &pSwapchain,
                                                 &pDevice,
                                                 NULL,
                                                 &pDeviceContext);

    assert( S_OK == hr && pSwapchain && pDevice && pDeviceContext );
    if ( FAILED(hr) ) {
        MessageBeep(1);
        MessageBoxA( 0, "Error creating device and swapchain! [Failed]", "Fatal Error", MB_OK | MB_ICONERROR );
        return GetLastError();
    }

    // * * * * * RENDER TARGET SETUP * * * * * //
    ID3D11Texture2D* pBackBuffer;        
    hr = pSwapchain->GetBuffer( NULL, __uuidof( ID3D11Texture2D ), (void**)&pBackBuffer );    // get address of the backbuffer "texture"
    assert(SUCCEEDED(hr));

    // Use the backbuffer "texture" adress to create render target to ptr
    hr = pDevice->CreateRenderTargetView( pBackBuffer, NULL, &pRenderTarget );
    assert( SUCCEEDED(hr) );
    pBackBuffer->Release();

    // * * * * * Depth/Stencil View for render View * * * * * //
    D3D11_TEXTURE2D_DESC depthStencilDesc;
    ZeroMemory(&depthStencilDesc, sizeof(D3D11_TEXTURE2D_DESC));

                depthStencilDesc.Width = ( client.right - client.left );
                depthStencilDesc.Height = ( client.bottom - client.top );
                depthStencilDesc.MipLevels = 1;
                depthStencilDesc.ArraySize = 1;
                depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;    // depth 24byte, stencil 8byte
                depthStencilDesc.SampleDesc.Count = 1;
                depthStencilDesc.SampleDesc.Quality = 0;
                depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
                depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                depthStencilDesc.CPUAccessFlags = 0;
                depthStencilDesc.MiscFlags = 0;

    // Create depth/stencil view
    hr = pDevice->CreateTexture2D( &depthStencilDesc, NULL, &pDepthStencilBuffer );
    assert( SUCCEEDED(hr) );
    hr = pDevice->CreateDepthStencilView( pDepthStencilBuffer, NULL, &pDepthStencilView );
    assert( SUCCEEDED(hr) );

    // Create depth state - How depth works
    D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
    ZeroMemory( &depthStencilStateDesc, sizeof( D3D11_DEPTH_STENCIL_DESC) );

                depthStencilStateDesc.DepthEnable = true;
                depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
                depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;                

    hr = pDevice->CreateDepthStencilState( &depthStencilStateDesc, &pDepthStencilState );
    assert( SUCCEEDED(hr) );    

    // * * * * * Rasterizer State * * * * * //
    D3D11_RASTERIZER_DESC rasterizerStateDesc;
    ZeroMemory( &rasterizerStateDesc, sizeof( D3D11_RASTERIZER_DESC ) );

                rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
                rasterizerStateDesc.CullMode = D3D11_CULL_BACK;

    hr = pDevice->CreateRasterizerState( &rasterizerStateDesc, &pRasterizerState );
    assert( SUCCEEDED(hr) );


    // * * * * * CREATE VIEWPORT * * * * * //
    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)client.right - client.left;
    viewport.Height = (float)client.bottom - client.top;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    pDeviceContext->RSSetViewports( 1, &viewport );

    return true;
}

bool initScenegraphics()
{
    // * * * * * VERTEX- AND PIXEL-SHADER * * * * * //
    // Get vertex shader info
    HRESULT hr = D3DCompileFromFile( L"vertexShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", NULL, NULL, &pVertexShaderBlob, &pErrorBlob );
    if ( FAILED(hr) ) {
        if ( pErrorBlob ) {
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
            pErrorBlob->Release();
        }

        if ( pVertexShaderBlob ) {
            pVertexShaderBlob->Release();
        }
        assert( false );
    }

    // Get pixel shader info
    hr = D3DCompileFromFile( L"pixelShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", NULL, NULL, &pPixelShaderBlob, &pErrorBlob );
    if (FAILED(hr)) {
        if ( pErrorBlob ) {
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
            pErrorBlob->Release();
        }

        if (pPixelShaderBlob) {
            pPixelShaderBlob->Release();
        }
        assert( false );
    }

    // Create a vertex and a pixel shader from blob-info to vertex/pixel_ptrs    
    hr = pDevice->CreateVertexShader( pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(), NULL, &pVertexShader );
    assert( SUCCEEDED(hr) );

    hr = pDevice->CreatePixelShader( pPixelShaderBlob->GetBufferPointer(), pPixelShaderBlob->GetBufferSize(), NULL, &pPixelShader );
    assert( SUCCEEDED(hr) );

    // * * * * * INPUT LAYOUT * * * * * //
    // An input layout how to handle data from vertexbuffers
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
              { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
              { "COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
              { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
              { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = pDevice->CreateInputLayout( inputElementDesc, ARRAYSIZE(inputElementDesc), pVertexShaderBlob->GetBufferPointer(), pVertexShaderBlob->GetBufferSize(), &pInputLayout );
    assert( SUCCEEDED(hr) );

    // * * * * * VERTEX BUFFER / INDEX BUFFER * * * * * // 
    Vertex quad[] =
    {
                Vertex(-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, -1.0f),
                Vertex(-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 1.0f, -1.0f),
                Vertex(0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f),
                Vertex(0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f),
    };

    // Indices for vertex buffer
    DWORD indices[] = {
       0, 1, 2,
       0, 2, 3,
    };


    // Vertex buffer desciption
    D3D11_BUFFER_DESC vertexBufferDesc;
    ZeroMemory( &vertexBufferDesc, sizeof(D3D11_BUFFER_DESC) );

                vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
                vertexBufferDesc.ByteWidth = sizeof(Vertex) * 4;
                vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                vertexBufferDesc.CPUAccessFlags = 0;
                vertexBufferDesc.MiscFlags = 0;

    // Create vertex buffer data
    D3D11_SUBRESOURCE_DATA vertexBufferData;
    ZeroMemory( &vertexBufferData, sizeof(D3D11_SUBRESOURCE_DATA) );

                vertexBufferData.pSysMem = quad;

    hr = pDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &pVertexBuffer );
    assert( SUCCEEDED(hr) );   


    // Index buffer description
    D3D11_BUFFER_DESC indexBufferDesc;
    ZeroMemory( &indexBufferDesc, sizeof(D3D11_BUFFER_DESC) );

                indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
                indexBufferDesc.ByteWidth = sizeof(DWORD) * 6;
                indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                indexBufferDesc.CPUAccessFlags = 0;
                indexBufferDesc.MiscFlags = 0;

    // Create index buffer data
    D3D11_SUBRESOURCE_DATA indexBufferData;
    ZeroMemory( &indexBufferData, sizeof(D3D11_SUBRESOURCE_DATA) );

                indexBufferData.pSysMem = indices;

    hr = pDevice->CreateBuffer( &indexBufferDesc, &indexBufferData, &pIndexBuffer );
    assert( SUCCEEDED(hr) );
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

	
    // * * * * * Texturing * * * * * //
    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory( &samplerDesc, sizeof(D3D11_SAMPLER_DESC) );
				
                samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
                samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
                samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
                samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;    
                samplerDesc.MinLOD = 0;
                samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState( &samplerDesc, &pSamplerState );
    assert( SUCCEEDED(hr) );

    // - - - load texturefile from file - - - //
    hr = DirectX::CreateWICTextureFromFile(pDevice, L"Textures/gorilla.jpg", nullptr, &pGorillaTexture);
    if (!SUCCEEDED(hr)) {
        MessageBeep(1);
        MessageBoxA(0, "[Error] Load texturefile failed! -> Closing program!", "Fatal Error", MB_OK | MB_ICONERROR);
        return GetLastError();
    }
    // - - - - - - - - - - - - - - - - - - - - //


    // * * * * * CONSTANT BUFFER CREATION * * * * * //
    // Create buffer to send to cbuffer in vertexshader
    D3D11_BUFFER_DESC cBufferDesc;
    ZeroMemory( &cBufferDesc, sizeof(D3D11_BUFFER_DESC) );

                cBufferDesc.Usage = D3D11_USAGE_DEFAULT;
                cBufferDesc.ByteWidth = sizeof( cBuffer );
                cBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                cBufferDesc.CPUAccessFlags = 0;
                cBufferDesc.MiscFlags = 0;

    hr = pDevice->CreateBuffer( &cBufferDesc, NULL, &pCBuffer );
    assert( SUCCEEDED(hr) );

    // - - - - -  LIGHTNING BUFFER - - - - -  //
    ZeroMemory( &cBufferDesc, sizeof(D3D11_BUFFER_DESC) );

                cBufferDesc.Usage = D3D11_USAGE_DEFAULT;
                cBufferDesc.ByteWidth = sizeof( cBufferLight );
                cBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                cBufferDesc.CPUAccessFlags = 0;
                cBufferDesc.MiscFlags = 0;

    hr = pDevice->CreateBuffer( &cBufferDesc, NULL, &pCBufferLight );
    assert( SUCCEEDED(hr) );

    return true;
}

void updateCBuffs(float rot, float transform)
{
    // - - Constantbuffer objects, matrix to setup - - //
    cBuffer objectTransform;    
    Light light;    // light object to modify
    cBufferLight lightCBuffer;  // light-buffer to send into the shader       

    // - - - - - Spaces Settings - - - - - //
    DirectX::XMMATRIX worldViewProj;    // Used to send to cBuffer 
    DirectX::XMMATRIX worldSpace = DirectX::XMMatrixIdentity();  // World view matrix

    // - * * * - Viewspace settings - * * * - //
    DirectX::XMVECTOR eyePosition = DirectX::XMVectorSet(0.0f, 0.0f, -2.0f, 0.0f);
    DirectX::XMVECTOR targetPosition = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR camUpVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // Viewspace matrix
    DirectX::XMMATRIX viewSpace = DirectX::XMMatrixIdentity();
    viewSpace = DirectX::XMMatrixLookAtLH(eyePosition, targetPosition, camUpVector); // left handed coordinate system

    // - * * * - Projection settings - * * * - //
    float fovInDegrees = 90.0f;  // field of view
    float fovInRadians = (fovInDegrees / 360.0f) * 3.14f;
    float aspectRatio = (float)width / height;
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    // Projection Space matrix
    DirectX::XMMATRIX projectionSpace = DirectX::XMMatrixIdentity();
    projectionSpace = DirectX::XMMatrixPerspectiveFovLH(fovInRadians, aspectRatio, nearZ, farZ);  // left handed coordinate system
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //   
    

    // - - - - - CBUFFER Light Setup - - - - - //
    light.ambientLightColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);  // how much of objects rgb is used
    light.ambientLightStrength = 0.2f;  // how lit object is
    
    light.dynamicLightColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);  // light color
    light.dynamicLightStrength = 1.0f;  // light strength

    light.dynamicLightPosition = DirectX::XMFLOAT3(-0.90f, 0.0f, 0.0f);   // light position

    light.dynamicAttenuation = DirectX::XMFLOAT3(0.2f, 0.1f, 0.1f);     // light falloff 

    lightCBuffer.light = light;
    pDeviceContext->UpdateSubresource( pCBufferLight, 0, NULL, &lightCBuffer, 0, 0 );
    pDeviceContext->PSSetConstantBuffers( 0, 1, &pCBufferLight );
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

    
    // * * * * * CBUFFER MATRIX OBJECT TRANSFORM * * * * * //
    // Translation and rotations
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationZ(rot);
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(transform, 0.0f, 0.0f);

    // Set worldSpace's using the transformations
    worldSpace = rotation * translation;

    // constant buffer setting worldViewProj-matrix   
    worldViewProj = DirectX::XMMatrixIdentity();
    worldViewProj = worldSpace * viewSpace * projectionSpace;

    // Switch from raw to column-major format -> put matrix to constant buffer
    objectTransform.World = DirectX::XMMatrixTranspose(worldSpace); // For lightning
    objectTransform.WVP = DirectX::XMMatrixTranspose(worldViewProj);

    // Update resource and send to cbuffer
    pDeviceContext->UpdateSubresource( pCBuffer, 0, NULL, &objectTransform, 0, 0 );
    pDeviceContext->VSSetConstantBuffers( 0, 1, &pCBuffer );
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {

    switch (message) {
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }

    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}