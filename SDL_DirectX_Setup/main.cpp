#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

//IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_dx11.h"

#include "SDL.h"
#include "SDL_syswm.h"

// Shader Headers
#include "shaders/cube_vs.csh"
#include "shaders/cube_ps.csh"
// Shader Headers

#include "Camera.h"
#include "macros.h"
#include "WTime.h"
#include "Input.h"

// Namespaces
using namespace DirectX;
using Microsoft::WRL::ComPtr;

// Defines
#define SCREEN_WIDTH 1366
#define SCREEN_HEIGHT 800
#define NUM_SPHERES 256

// Thread stuff
#define THREAD_GROUP_COUNT        16
#define THREADS_PER_THREAD_GROUP  16

// D3D11 Stuff
ComPtr<ID3D11Device> g_Device;
ComPtr<IDXGISwapChain> g_Swapchain;
ComPtr<ID3D11DeviceContext> g_DeviceContext;
float g_aspectRatio = 1;
float g_rotation = 0;
bool g_fullscreen = false;

// States
ComPtr<ID3D11RasterizerState> rasterizerStateDefault;
ComPtr<ID3D11RasterizerState> rasterizerStateWireframe;

// Shader variables
ComPtr<ID3D11Buffer> constantBuffer;
ComPtr<ID3D11Buffer> instanceBuffer;
ComPtr<ID3D11Buffer> sphereInstanceBuffer;

// Z buffer
ComPtr<ID3D11Texture2D> zBuffer;
ComPtr<ID3D11DepthStencilView> depthStencil;

// For drawing  -> New stuff right here
ComPtr<ID3D11RenderTargetView> g_RenderTargetView;
D3D11_VIEWPORT g_viewport;
// D3D11 Stuff

// Custom struct WorldViewProjection
struct WorldViewProjection {
	XMFLOAT4X4 WorldMatrix;
	XMFLOAT4X4 ViewMatrix;
	XMFLOAT4X4 ProjectionMatrix;
	XMFLOAT4 CamPos;
} WORLD;

struct Instance
{
	XMFLOAT4 position;
	XMFLOAT4 color;
};

struct inst_packet
{
	XMFLOAT4X4 world;
	XMFLOAT4 color;
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 color;
};

struct Cube
{
	// Vertices
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	// d3d pointers
	ComPtr<ID3D11Buffer> vertex_buffer;
	ComPtr<ID3D11Buffer> index_buffer;

	ComPtr<ID3D11InputLayout> input_layout;
	ComPtr<ID3D11VertexShader> vertex_shader;
	ComPtr<ID3D11PixelShader> pixel_shader;
};

struct DebugSphere
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	// D3D Primitives
	ComPtr<ID3D11Buffer> vertex_buffer;
	ComPtr<ID3D11Buffer> index_buffer;

	ComPtr<ID3D11InputLayout> input_layout;
	ComPtr<ID3D11VertexShader> vertex_shader;
	ComPtr<ID3D11PixelShader> pixel_shader;
};

struct ComputePass
{
	ComPtr<ID3D11ComputeShader> computeShader;

	ComPtr<ID3D11Buffer> structured_uab;
	ComPtr<ID3D11Buffer> staging_buffer;
	ComPtr<ID3D11UnorderedAccessView> uav;
};

FPSCamera camera;
Cube cube;
DebugSphere sphere;
ComputePass compute_pass;
WTime timer;
Input input;

std::vector<Instance> g_instances;
std::vector<inst_packet> g_instanceMats;

std::vector<Instance> g_sphere_instances;
std::vector<inst_packet> g_sphere_instanceMats;

// States
long long ticks = 0;
bool show_ui = true;
bool update_cube_rotation = true;
bool show_cube_settings = false;
bool show_camera_settings = false;

float input_timer = 0;
float input_timer_thresh = .2;

ImVec4 clear_color = ImVec4(0, 0, 0, 1.00f);
// Custom stuff

// Forward declarations
void ConstructCube(Cube& cube, const BYTE* _vs, const BYTE* _ps, int vs_size, int ps_size);
void ConstructDebugSphere(ComPtr<ID3D11Device> p_device, DebugSphere& dsphere);
void InitComputePass(ComPtr<ID3D11Device> p_device, ComputePass& pass);

void LoadSettings();
void SaveSettings();
float GenRandomFlt(float LO, float HI);
XMFLOAT3 GenRandomPosition(float minx, float maxx, float miny, float maxy, float minz, float maxz);
XMFLOAT4 GenRandomPosition4(float minx, float maxx, float miny, float maxy, float minz, float maxz);

float GenRandomFlt(float LO, float HI)
{
	float r3 = LO + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (HI - LO)));
	return r3;
}

XMFLOAT3 GenRandomPosition(float minx, float maxx, float miny, float maxy, float minz, float maxz)
{
	return XMFLOAT3(GenRandomFlt(minx, maxx), GenRandomFlt(miny, maxy), GenRandomFlt(minz, maxz));
}

XMFLOAT4 GenRandomPosition4(float minx, float maxx, float miny, float maxy, float minz, float maxz)
{
	XMFLOAT3 a = GenRandomPosition(minx, maxx, miny, maxy, minz, maxz);
	return XMFLOAT4(a.x, a.y, a.z, 1);
}

void SaveSettings()
{
	std::ofstream settings;
	settings.open("files/settings.txt", std::ios::out);

	assert(settings.is_open());

	std::stringstream settingstring;

	settingstring << "#clear_color\n";
	settingstring << clear_color.x << " ";
	settingstring << clear_color.y << " ";
	settingstring << clear_color.z << " ";
	settingstring << "\n";

	settingstring << "#show_cube\n";
	settingstring << (int)show_cube_settings << "\n";
	settingstring << "#show_camera\n";
	settingstring << (int)show_camera_settings << "\n";

	settings.write(settingstring.str().c_str(), settingstring.str().length());

	settings.close();
}

void LoadSettings()
{
	std::ifstream settings;
	settings.open("files/settings.txt", std::ios::in | std::ios::binary);

	assert(settings.is_open());

	std::string line;
	std::string otherline;

	while (std::getline(settings, line))
	{
		if (line.find("clear_color") != std::string::npos)
		{
			std::getline(settings, otherline);

			std::stringstream color(otherline);

			color >> clear_color.x;
			color >> clear_color.y;
			color >> clear_color.z;
		}
		else if (line.find("#show_cube") != std::string::npos)
		{
			std::getline(settings, otherline);

			std::stringstream thing(otherline);
			thing >> show_cube_settings;
		}
		else if (line.find("#show_camera") != std::string::npos)
		{
			std::getline(settings, otherline);

			std::stringstream thing(otherline);
			thing >> show_camera_settings;
		}
	}
}

int main(int argc, char** argv)
{
	// Seed
	srand(time(NULL));

	// Members
	bool RUNNING = true;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		std::cout << "Initialization failed" << std::endl;
	}

	// Create window
	SDL_Window* m_window = SDL_CreateWindow("Compute Sandbox",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

	// Full screen
	if(g_fullscreen)
		SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN);

	if (!m_window)
	{
		std::cout << "Window initialization failed\n";
	}

	LoadSettings();

	// Instances
	// Make like 2 instances
	Instance i;
	ZeroMemory(&i, sizeof(Instance));

	i.position = XMFLOAT4(0, 0, 0, 1);
	g_instances.push_back(i);
	i.position = XMFLOAT4(0, 5, 0, 1);
	g_instances.push_back(i);

	XMMATRIX t;
	g_instanceMats.resize(g_instances.size());
	for (int i = 0; i < g_instances.size(); i++)
	{
		t = XMMatrixTranslation(g_instances[i].position.x, g_instances[i].position.y, g_instances[i].position.z);
		XMStoreFloat4x4(&g_instanceMats[i].world, t);
	}

	Instance si;
	ZeroMemory(&si, sizeof(Instance));

	//int num_instances = 256;
	for (int i = 0; i < NUM_SPHERES; i++)
	{
		si.position = GenRandomPosition4(-25, 25, 0, 50, -25, 25);
		si.color = GenRandomPosition4(0, 1, 0, 1, 0, 1);
		g_sphere_instances.push_back(si);
	}
	
	g_sphere_instanceMats.resize(g_sphere_instances.size());
	for (int i = 0; i < g_sphere_instances.size(); i++)
	{
		t = XMMatrixTranslation(g_sphere_instances[i].position.x, g_sphere_instances[i].position.y, g_sphere_instances[i].position.z);
		XMStoreFloat4x4(&g_sphere_instanceMats[i].world, t);
		g_sphere_instanceMats[i].color = g_sphere_instances[i].color;
	}

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(m_window, &wmInfo);
	HWND hWnd = wmInfo.info.win.window;

	// D3d11 code here
	RECT rect;
	GetClientRect(hWnd, &rect);

	// Attach d3d to the window
	D3D_FEATURE_LEVEL DX11 = D3D_FEATURE_LEVEL_11_0;
	DXGI_SWAP_CHAIN_DESC swap;
	ZeroMemory(&swap, sizeof(DXGI_SWAP_CHAIN_DESC));
	swap.BufferCount = 1;
	swap.OutputWindow = hWnd;
	swap.Windowed = true;
	swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap.BufferDesc.Width = rect.right - rect.left;
	swap.BufferDesc.Height = rect.bottom - rect.top;
	swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap.SampleDesc.Count = 1;

	g_aspectRatio = swap.BufferDesc.Width / (float)swap.BufferDesc.Height;

	HRESULT result;

	result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, &DX11, 1, D3D11_SDK_VERSION, &swap, &g_Swapchain, &g_Device, 0, &g_DeviceContext);
	assert(!FAILED(result));

	// Setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForD3D(m_window);
	ImGui_ImplDX11_Init(g_Device.Get(), g_DeviceContext.Get());

	ComPtr<ID3D11Resource> backbuffer;
	result = g_Swapchain->GetBuffer(0, __uuidof(backbuffer), (void**)backbuffer.GetAddressOf());
	result = g_Device->CreateRenderTargetView(backbuffer.Get(), NULL, &g_RenderTargetView);
	assert(!FAILED(result));

	// Setup viewport
	g_viewport.Width = swap.BufferDesc.Width;
	g_viewport.Height = swap.BufferDesc.Height;
	g_viewport.TopLeftY = g_viewport.TopLeftX = 0;
	g_viewport.MinDepth = 0;
	g_viewport.MaxDepth = 1;
	// D3d11 code here

	// Rasterizer states
	D3D11_RASTERIZER_DESC rdesc;
	ZeroMemory(&rdesc, sizeof(D3D11_RASTERIZER_DESC));
	rdesc.FrontCounterClockwise = false;
	rdesc.DepthBiasClamp = 1;
	rdesc.DepthBias = rdesc.SlopeScaledDepthBias = 0;
	rdesc.DepthClipEnable = true;
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.CullMode = D3D11_CULL_BACK;
	rdesc.AntialiasedLineEnable = false;
	rdesc.MultisampleEnable = false;

	result = g_Device->CreateRasterizerState(&rdesc, &rasterizerStateDefault);
	ASSERT_HRESULT_SUCCESS(result);

	// Wire frame Rasterizer State
	ZeroMemory(&rdesc, sizeof(D3D11_RASTERIZER_DESC));
	rdesc.FillMode = D3D11_FILL_WIREFRAME;
	rdesc.CullMode = D3D11_CULL_NONE;
	rdesc.DepthClipEnable = true;

	result = g_Device->CreateRasterizerState(&rdesc, rasterizerStateWireframe.GetAddressOf());
	ASSERT_HRESULT_SUCCESS(result);

	//g_DeviceContext->RSSetState(rasterizerStateDefault);
	g_DeviceContext->RSSetState(rasterizerStateWireframe.Get());

	// Initialize camera
	camera.SetPosition(XMFLOAT3(0, 1.5, -5));
	camera.Rotate(0, -20);
	camera.SetFOV(45);

	ConstructCube(cube, cube_vs, cube_ps, sizeof(cube_vs), sizeof(cube_ps));
	ConstructDebugSphere(g_Device, sphere);

	g_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//g_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	// Create constant buffer
	D3D11_BUFFER_DESC bDesc;
	D3D11_SUBRESOURCE_DATA subdata;
	ZeroMemory(&bDesc, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));

	bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bDesc.ByteWidth = sizeof(WorldViewProjection);
	bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bDesc.MiscFlags = 0;
	bDesc.StructureByteStride = 0;
	bDesc.Usage = D3D11_USAGE_DYNAMIC;

	result = g_Device->CreateBuffer(&bDesc, nullptr, constantBuffer.GetAddressOf());
	ASSERT_HRESULT_SUCCESS(result);

	// Instance buffer
	bDesc.Usage = D3D11_USAGE_DEFAULT;
	bDesc.CPUAccessFlags = 0;
	bDesc.ByteWidth = sizeof(inst_packet) * g_instanceMats.size();
	subdata.pSysMem = g_instanceMats.data();

	result = g_Device->CreateBuffer(&bDesc, nullptr, instanceBuffer.GetAddressOf());
	ASSERT_HRESULT_SUCCESS(result);
	
	// Sphere Instance buffer
	bDesc.Usage = D3D11_USAGE_DEFAULT;
	bDesc.CPUAccessFlags = 0;
	bDesc.ByteWidth = sizeof(inst_packet) * g_sphere_instanceMats.size();
	subdata.pSysMem = g_sphere_instanceMats.data();

	SucceedOrCrash(g_Device->CreateBuffer(&bDesc, nullptr, sphereInstanceBuffer.GetAddressOf()));

	// Z buffer 
	D3D11_TEXTURE2D_DESC zDesc;
	ZeroMemory(&zDesc, sizeof(zDesc));
	zDesc.ArraySize = 1;
	zDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	zDesc.Width = swap.BufferDesc.Width;
	zDesc.Height = swap.BufferDesc.Height;
	zDesc.Usage = D3D11_USAGE_DEFAULT;
	zDesc.Format = DXGI_FORMAT_D32_FLOAT;
	zDesc.MipLevels = 1;
	zDesc.SampleDesc.Count = 1;

	result = g_Device->CreateTexture2D(&zDesc, nullptr, &zBuffer);
	ASSERT_HRESULT_SUCCESS(result);
	result = g_Device->CreateDepthStencilView(zBuffer.Get(), nullptr, depthStencil.GetAddressOf());
	ASSERT_HRESULT_SUCCESS(result);

	// Compute pass initialization
	InitComputePass(g_Device, compute_pass);

	XMFLOAT3 cube_position = XMFLOAT3(0, 0, 0);
	float rotation_speed = 1;
	float fov = XMConvertToDegrees(camera.GetFOV());

	// State
	static float camera_speed = 20;
	static float camera_rotation_speed = 10;
	timer.ResetTime();
	float time_elapsed = 0;

	// Temp
	g_DeviceContext->UpdateSubresource(sphereInstanceBuffer.Get(), 0, nullptr, g_sphere_instanceMats.data(), 0, 0);
	// Temp

	// Main loop
	while (RUNNING)
	{
		// Timing
		timer.Update();
		double dt = timer.deltaTime;
		ticks += 1;
		time_elapsed += dt;

		input_timer += dt;

		RUNNING = input.ProcessInput(dt);

		if (input.IsKeyDown(SDL_SCANCODE_W).value)
		{
			XMFLOAT3 val = camera.GetLook();
			val.x *= camera_speed * dt;
			val.y *= camera_speed * dt;
			val.z *= camera_speed * dt;
			camera.Move(val);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_S).value)
		{
			XMFLOAT3 val = camera.GetLook();
			val.x *= -camera_speed * dt;
			val.y *= -camera_speed * dt;
			val.z *= -camera_speed * dt;
			camera.Move(val);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_A).value)
		{
			XMFLOAT3 val = camera.GetRight();
			val.x *= camera_speed * dt;
			val.y *= camera_speed * dt;
			val.z *= camera_speed * dt;
			camera.Move(val);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_D).value)
		{
			XMFLOAT3 val = camera.GetRight();
			val.x *= -camera_speed * dt;
			val.y *= -camera_speed * dt;
			val.z *= -camera_speed * dt;
			camera.Move(val);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_UP).value)
		{
			camera.Rotate(0, camera_rotation_speed * dt);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_DOWN).value)
		{
			camera.Rotate(0, -camera_rotation_speed * dt);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_LEFT).value)
		{
			camera.Rotate(camera_rotation_speed * dt, 0);
		}
		else if (input.IsKeyDown(SDL_SCANCODE_RIGHT).value)
		{
			camera.Rotate(-camera_rotation_speed * dt, 0);
		}
		// Camera

		if (input.IsKeyDown(SDL_SCANCODE_SPACE).value && input_timer > input_timer_thresh)
		{
			input_timer = 0;
			show_ui = !show_ui;
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_window);
		ImGui::NewFrame();

		{
			ImGui::Begin("Application Info");

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			ImGui::End();
		}

		// IMGUI Rendering
		ImGui::Render();

		// Output merger
		ID3D11RenderTargetView* tempRTV[] = { g_RenderTargetView.Get() };
		g_DeviceContext->OMSetRenderTargets(ARRAYSIZE(tempRTV), tempRTV, depthStencil.Get());

		float color[4];
		memcpy(color, &clear_color, sizeof(float) * 4);
		g_DeviceContext->ClearRenderTargetView(g_RenderTargetView.Get(), (float*)&clear_color);
		g_DeviceContext->ClearDepthStencilView(depthStencil.Get(), D3D11_CLEAR_DEPTH, 1, 0);

		g_DeviceContext->RSSetViewports(1, &g_viewport);

		// Cube
		if(false)
		{
			// Rotation
			if (update_cube_rotation)
			{
				g_rotation += 1 * dt;
				if (g_rotation > 360) g_rotation = g_rotation - 360;
			}

			// Draw the cube
			// World
			//XMMATRIX temp = XMMatrixIdentity();
			XMMATRIX temp = XMMatrixRotationY(g_rotation * rotation_speed);
			temp = XMMatrixMultiply(temp, XMMatrixTranslation(cube_position.x, cube_position.y, cube_position.z));
			XMStoreFloat4x4(&WORLD.WorldMatrix, temp);

			// View
			camera.GetViewMatrix(temp);
			XMStoreFloat4x4(&WORLD.ViewMatrix, temp);

			// Proj
			temp = XMMatrixPerspectiveFovLH(camera.GetFOV(), g_aspectRatio, 0.1f, 1000);
			XMStoreFloat4x4(&WORLD.ProjectionMatrix, temp);

			// Send the matrix to constant buffer
			D3D11_MAPPED_SUBRESOURCE gpuBuffer;
			HRESULT result = g_DeviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &gpuBuffer);
			memcpy(gpuBuffer.pData, &WORLD, sizeof(WORLD));
			g_DeviceContext->Unmap(constantBuffer.Get(), 0);

			g_DeviceContext->UpdateSubresource(instanceBuffer.Get(), 0, nullptr, g_instanceMats.data(), 0, 0);

			// Connect constant buffer to the pipeline
			ID3D11Buffer* cbuffers[] = {
				constantBuffer.Get(),
				instanceBuffer.Get()
			};
			g_DeviceContext->VSSetConstantBuffers(0, ARRAYSIZE(cbuffers), cbuffers);

			UINT teapotstrides[] = { sizeof(Vertex) };
			UINT teapotoffsets[] = { 0 };
			ID3D11Buffer* teapotVertexBuffers[] = { cube.vertex_buffer.Get() };
			g_DeviceContext->IASetVertexBuffers(0, ARRAYSIZE(teapotVertexBuffers), teapotVertexBuffers, teapotstrides, teapotoffsets);
			g_DeviceContext->IASetIndexBuffer(cube.index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			g_DeviceContext->VSSetShader(cube.vertex_shader.Get(), 0, 0);
			g_DeviceContext->PSSetShader(cube.pixel_shader.Get(), 0, 0);
			g_DeviceContext->IASetInputLayout(cube.input_layout.Get());

			g_DeviceContext->DrawIndexedInstanced(cube.indices.size(), g_instanceMats.size(), 0, 0, 0);
			// Draw the cube
		}

		// Compute pass
		{
			g_DeviceContext->CSSetShader(compute_pass.computeShader.Get(), nullptr, 0);
			g_DeviceContext->CSSetUnorderedAccessViews(0, 1, compute_pass.uav.GetAddressOf(), nullptr);

			// Update sub resource
			g_DeviceContext->UpdateSubresource(compute_pass.structured_uab.Get(), 0, NULL, g_sphere_instanceMats.data(), 0, 0);

			// Send time
			WORLD.CamPos.w = static_cast<float>(time_elapsed);
			// Send the matrix to constant buffer
			D3D11_MAPPED_SUBRESOURCE gpuBuffer;
			HRESULT result = g_DeviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &gpuBuffer);
			memcpy(gpuBuffer.pData, &WORLD, sizeof(WORLD));
			g_DeviceContext->Unmap(constantBuffer.Get(), 0);

			// Connect constant buffer to the pipeline
			ID3D11Buffer* cbuffers[] = {
				constantBuffer.Get(),
			};
			g_DeviceContext->CSSetConstantBuffers(0, ARRAYSIZE(cbuffers), cbuffers);

			g_DeviceContext->Dispatch(16, 1, 1);

			g_DeviceContext->CopyResource(compute_pass.staging_buffer.Get(), compute_pass.structured_uab.Get());
			g_DeviceContext->CopyResource(sphereInstanceBuffer.Get(), compute_pass.staging_buffer.Get());
		}
		
		// Sphere
		{
			// Reset primitive topology
			g_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

			// World
			XMMATRIX temp = XMMatrixIdentity();
			XMStoreFloat4x4(&WORLD.WorldMatrix, temp);
			// View
			camera.GetViewMatrix(temp);
			XMStoreFloat4x4(&WORLD.ViewMatrix, temp);
			// Proj
			temp = XMMatrixPerspectiveFovLH(camera.GetFOV(), g_aspectRatio, camera.GetNear(), camera.GetFar());
			XMStoreFloat4x4(&WORLD.ProjectionMatrix, temp);

			// Send the matrix to constant buffer
			D3D11_MAPPED_SUBRESOURCE gpuBuffer;
			HRESULT result = g_DeviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &gpuBuffer);
			memcpy(gpuBuffer.pData, &WORLD, sizeof(WORLD));
			g_DeviceContext->Unmap(constantBuffer.Get(), 0);

			//g_DeviceContext->UpdateSubresource(sphereInstanceBuffer.Get(), 0, nullptr, g_sphere_instanceMats.data(), 0, 0);

			// Connect constant buffer to the pipeline
			ID3D11Buffer* cbuffers[] = {
				constantBuffer.Get(),
				sphereInstanceBuffer.Get(),
			};
			g_DeviceContext->VSSetConstantBuffers(0, ARRAYSIZE(cbuffers), cbuffers);

			UINT strides[] = { sizeof(Vertex) };
			UINT offsets[] = { 0 };
			ID3D11Buffer* vbuffers[] = { sphere.vertex_buffer.Get() };
			g_DeviceContext->IASetVertexBuffers(0, ARRAYSIZE(vbuffers), vbuffers, strides, offsets);
			g_DeviceContext->IASetIndexBuffer(sphere.index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			g_DeviceContext->VSSetShader(sphere.vertex_shader.Get(), 0, 0);
			g_DeviceContext->PSSetShader(sphere.pixel_shader.Get(), 0, 0);
			g_DeviceContext->IASetInputLayout(sphere.input_layout.Get());

			g_DeviceContext->DrawIndexedInstanced(sphere.indices.size(), g_sphere_instances.size(), 0, 0, 0);
		}

		// IMGUI Rendering
		if(show_ui)
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		g_Swapchain->Present(0, 0);
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	// SDL shutdown
	if(m_window)
		SDL_DestroyWindow(m_window);
	SDL_Quit();

	SaveSettings();

	return EXIT_SUCCESS;
}

void ConstructCube(Cube& cube, const BYTE* _vs, const BYTE* _ps, int vs_size, int ps_size)
{
	// Construct vertices
	Vertex vert;
	float HUL = .5; //  Half unity length

	// Top forward left
	vert.position = XMFLOAT3(-HUL, HUL, -HUL);
	vert.color = XMFLOAT3(0, 0, 1);
	cube.vertices.push_back(vert);
	// Top forward right
	vert.position = XMFLOAT3(HUL, HUL, -HUL);
	vert.color = XMFLOAT3(0, 1, 0);
	cube.vertices.push_back(vert);
	// Top back left
	vert.position = XMFLOAT3(-HUL, HUL, HUL);
	vert.color = XMFLOAT3(1, 0, 0);
	cube.vertices.push_back(vert);
	// Top back right
	vert.position = XMFLOAT3(HUL, HUL, HUL);
	vert.color = XMFLOAT3(1, 0, 1);
	cube.vertices.push_back(vert);
	
	// Bottom forward left
	vert.position = XMFLOAT3(-HUL, -HUL, -HUL);
	vert.color = XMFLOAT3(1, 1, 0);
	cube.vertices.push_back(vert);
	// Bottom forward right
	vert.position = XMFLOAT3(HUL, -HUL, -HUL);
	vert.color = XMFLOAT3(0, 1, 1);
	cube.vertices.push_back(vert);
	// Bottom backward left
	vert.position = XMFLOAT3(-HUL, -HUL, HUL);
	vert.color = XMFLOAT3(1, 1, 1);
	cube.vertices.push_back(vert);
	// Bottom backward right
	vert.position = XMFLOAT3(HUL, -HUL, HUL);
	vert.color = XMFLOAT3(0, 0, 0);
	cube.vertices.push_back(vert);

	// Make the triangles with the indices
	// Front face
	cube.indices.push_back(0); cube.indices.push_back(1); cube.indices.push_back(5);
	cube.indices.push_back(0); cube.indices.push_back(5); cube.indices.push_back(4);
	// Back face
	cube.indices.push_back(2); cube.indices.push_back(7); cube.indices.push_back(3);
	cube.indices.push_back(2); cube.indices.push_back(6); cube.indices.push_back(7);
	// Top face
	cube.indices.push_back(2); cube.indices.push_back(3); cube.indices.push_back(1);
	cube.indices.push_back(2); cube.indices.push_back(1); cube.indices.push_back(0);
	// Bottom face
	cube.indices.push_back(6); cube.indices.push_back(5); cube.indices.push_back(7);
	cube.indices.push_back(6); cube.indices.push_back(4); cube.indices.push_back(5);
	// Left face
	cube.indices.push_back(2); cube.indices.push_back(0); cube.indices.push_back(4);
	cube.indices.push_back(2); cube.indices.push_back(4); cube.indices.push_back(6);
	// Right face
	cube.indices.push_back(1); cube.indices.push_back(3); cube.indices.push_back(7);
	cube.indices.push_back(1); cube.indices.push_back(7); cube.indices.push_back(5);

	// Make vertex buffer
	D3D11_BUFFER_DESC bufferDesc;
	D3D11_SUBRESOURCE_DATA subdata;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(Vertex) * cube.vertices.size();
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	subdata.pSysMem = cube.vertices.data();

	HRESULT result = g_Device->CreateBuffer(&bufferDesc, &subdata, &cube.vertex_buffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Index Buffer
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(int) * cube.indices.size();

	subdata.pSysMem = cube.indices.data();
	result = g_Device->CreateBuffer(&bufferDesc, &subdata, &cube.index_buffer);
	ASSERT_HRESULT_SUCCESS(result);

	// Load shaders
	result = g_Device->CreateVertexShader(_vs, vs_size, nullptr, &cube.vertex_shader);
	ASSERT_HRESULT_SUCCESS(result);
	result = g_Device->CreatePixelShader(_ps, ps_size, nullptr, &cube.pixel_shader);
	ASSERT_HRESULT_SUCCESS(result);

	// Make input layout for vertex buffer
	D3D11_INPUT_ELEMENT_DESC tempInputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	result = g_Device->CreateInputLayout(tempInputElementDesc, ARRAYSIZE(tempInputElementDesc), _vs, vs_size, &cube.input_layout);
	ASSERT_HRESULT_SUCCESS(result);
}

void ConstructDebugSphere(ComPtr<ID3D11Device> p_device, DebugSphere& dsphere)
{
	Vertex v;
	ZeroMemory(&v, sizeof(Vertex));

	// Globals
	XMVECTOR point, up, right, temp, rotation;
	XMFLOAT4 _up(0, 1, 0, 1);
	XMFLOAT4 _right(1, 0, 0, 1);
	XMFLOAT3 green(0, 1, 0);

	up = XMLoadFloat4(&_up);
	right = XMLoadFloat4(&_right);

	// Set color
	v.color = green;

	// Make a single circle
	v.position = XMFLOAT3(0, 1, 0);
	dsphere.vertices.push_back(v);
	// Load point
	point = XMLoadFloat3(&v.position);

	// 6 times - should form a circle
	int num_rotations = 8;
	float arc_length = 45;
	for (int i = 1; i < num_rotations; i++)
	{
		float angle = arc_length * i;
		float angle_radians = XMConvertToRadians(angle);
		rotation = XMQuaternionRotationAxis(right, angle_radians);
		temp = XMVector3Rotate(point, rotation);
		XMStoreFloat3(&v.position, temp);

		dsphere.vertices.push_back(v);
	}

	// Indices
	for (int i = 0; i < num_rotations; i++)
	{
		int current = i;
		int next = current + 1;

		next = next >= num_rotations ? 0 : next;

		dsphere.indices.push_back(current);
		dsphere.indices.push_back(next);
	}

	for (int a = 1; a < num_rotations; a++)
	{
		// Rotate all the values on the ring by 90 about up axis
		rotation = XMQuaternionRotationAxis(up, XMConvertToRadians(30 * a));
		for (int i = 0; i < num_rotations; i++)
		{
			point = XMLoadFloat3(&dsphere.vertices[i].position);
			temp = XMVector3Rotate(point, rotation);
			XMStoreFloat3(&v.position, temp);

			dsphere.vertices.push_back(v);
		}

		for (int i = num_rotations; i < (num_rotations * (a + 1)); i++)
		{
			int current = i;
			int next = current + 1;

			next = next >= (num_rotations * (a + 1)) ? 0 : next;

			dsphere.indices.push_back(current);
			dsphere.indices.push_back(next);
		}
	}

	// Add indices for the in-between rings
	for (int i = 1; i < num_rotations; i++)
	{
		int offset = i;
		int num_lines = 1;
		while (num_lines < num_rotations)
		{
			dsphere.indices.push_back(offset);
			dsphere.indices.push_back(offset + num_rotations);
			offset += num_rotations;
			num_lines++;
		}
	}

	// Make vertex buffer
	D3D11_BUFFER_DESC bufferDesc;
	D3D11_SUBRESOURCE_DATA subdata;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(Vertex) * static_cast<unsigned int>(dsphere.vertices.size());
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	subdata.pSysMem = dsphere.vertices.data();

	SucceedOrCrash(p_device->CreateBuffer(&bufferDesc, &subdata, dsphere.vertex_buffer.GetAddressOf()));

	// Index Buffer
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(int) * static_cast<UINT>(dsphere.indices.size());

	subdata.pSysMem = dsphere.indices.data();
	SucceedOrCrash(p_device->CreateBuffer(&bufferDesc, &subdata, dsphere.index_buffer.GetAddressOf()));


	// Shader names
	std::string sname = "sphere";
	std::string vshadername = sname + "_vs.hlsl";
	std::string pshadername = sname + "_ps.hlsl";

	std::wstring vshaderw(vshadername.begin(), vshadername.end());
	std::wstring pshaderw(pshadername.begin(), pshadername.end());

	LPCWSTR vshaderl = vshaderw.c_str();
	LPCWSTR pshaderl = pshaderw.c_str();

	// Shader shit
	ComPtr<ID3D10Blob> pshaderbuffer;
	ComPtr<ID3D10Blob> vshaderbuffer;

	ComPtr<ID3D10Blob> shader_compile_error;

	// Compile shaders
	SucceedOrCrash(D3DCompileFromFile(
		vshaderl,
		NULL,
		NULL,
		"main",
		"vs_5_0",
		D3DCOMPILE_DEBUG,
		0,
		vshaderbuffer.GetAddressOf(),
		shader_compile_error.GetAddressOf()));

	SucceedOrCrash(D3DCompileFromFile(
		pshaderl,
		NULL,
		NULL,
		"main",
		"ps_5_0",
		D3DCOMPILE_DEBUG,
		0,
		pshaderbuffer.GetAddressOf(),
		shader_compile_error.GetAddressOf()));

	// Create shader objects from buffers
	SucceedOrCrash(p_device->CreateVertexShader(
		vshaderbuffer->GetBufferPointer(),
		vshaderbuffer->GetBufferSize(),
		NULL,
		dsphere.vertex_shader.GetAddressOf()));

	SucceedOrCrash(p_device->CreatePixelShader(
		pshaderbuffer->GetBufferPointer(),
		pshaderbuffer->GetBufferSize(),
		NULL,
		dsphere.pixel_shader.GetAddressOf()));

	// Make input layout for vertex buffer
	D3D11_INPUT_ELEMENT_DESC tempInputElementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	SucceedOrCrash(p_device->CreateInputLayout(
		tempInputElementDesc,
		ARRAYSIZE(tempInputElementDesc),
		vshaderbuffer->GetBufferPointer(),
		vshaderbuffer->GetBufferSize(),
		dsphere.input_layout.GetAddressOf()));
}

void InitComputePass(ComPtr<ID3D11Device> p_device, ComputePass& pass)
{
	// Shader names
	std::string sname = "compute_basic";
	std::string cshadername = sname + "_cs.hlsl";

	std::wstring cshaderw(cshadername.begin(), cshadername.end());

	LPCWSTR cshaderl = cshaderw.c_str();

	// Shader shit
	ComPtr<ID3D10Blob> cshaderbuffer;

	ComPtr<ID3D10Blob> shader_compile_error;

	// Compile shaders
	SucceedOrCrash(D3DCompileFromFile(
		cshaderl,
		NULL,
		NULL,
		"main",
		"cs_5_0",
		D3DCOMPILE_DEBUG,
		0,
		cshaderbuffer.GetAddressOf(),
		shader_compile_error.GetAddressOf()));

	// Create shader objects from buffers
	SucceedOrCrash(p_device->CreateComputeShader(
		cshaderbuffer->GetBufferPointer(),
		cshaderbuffer->GetBufferSize(),
		NULL,
		pass.computeShader.GetAddressOf()));

	// Structured UAB
	D3D11_BUFFER_DESC bDesc;
	D3D11_SUBRESOURCE_DATA subdata;
	ZeroMemory(&bDesc, sizeof(D3D11_BUFFER_DESC));
	ZeroMemory(&subdata, sizeof(D3D11_SUBRESOURCE_DATA));

	bDesc.Usage = D3D11_USAGE_DEFAULT;
	bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	bDesc.ByteWidth = sizeof(inst_packet) * g_sphere_instanceMats.size();
	bDesc.StructureByteStride = sizeof(inst_packet);
	bDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	subdata.pSysMem = g_sphere_instanceMats.data();

	SucceedOrCrash(p_device->CreateBuffer(&bDesc, nullptr, pass.structured_uab.GetAddressOf()));

	bDesc.Usage = D3D11_USAGE_STAGING;
	bDesc.BindFlags = 0;
	bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	bDesc.MiscFlags = 0;

	SucceedOrCrash(p_device->CreateBuffer(&bDesc, nullptr, pass.staging_buffer.GetAddressOf()));

	// Create an unordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;
	// This is a Structured Buffer
	desc.Format = DXGI_FORMAT_R32_TYPELESS;       // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
	desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	desc.Buffer.NumElements = bDesc.ByteWidth / 4;

	SucceedOrCrash(p_device->CreateUnorderedAccessView(pass.structured_uab.Get(), &desc, pass.uav.GetAddressOf()));
}