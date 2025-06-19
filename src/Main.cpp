
#define NOMINMAX

#include <cstdio>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>
#include <argparse/argparse.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <spdlog/sinks/basic_file_sink-inl.h>

#include "../SpoutGL/SpoutReceiver.h"
#include "../SpoutGL/SpoutSender.h"
#include "graphics.hpp"
#include "renderstream.hpp"
#include "PixelShader.h"
#include "VertexShader.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")




struct free_delete
{
    void operator()(void* x) { free(x); }
};

typedef struct WindowFormat {
    uint32_t width;
    uint32_t height;
} WindowFormat_t;

typedef struct RenderTarget {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
} RenderTarget_t;


// Vertex structure
struct Vertex { float x, y, z, u, v; };
Vertex quad[] =
{
    { -1,-1,0, 0,1 }, { -1, 1,0, 0,0 }, { 1, 1,0, 1,0 },
    { -1,-1,0, 0,1 }, { 1, 1,0, 1,0 }, { 1,-1,0, 1,1 },
};


float randomFloat()
{
    return (float)(rand()) / (float)(RAND_MAX);
}

DXGI_FORMAT toDxgiFormat(RSPixelFormat format)
{
    switch (format)
    {
    case RS_FMT_BGRA8:
    case RS_FMT_BGRX8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case RS_FMT_RGBA32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case RS_FMT_RGBA16:
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    default:
        throw std::runtime_error("Unhandled RS pixel format");
    }
}

void GenerateRenderStreamSchema(
    std::set<std::string> &senders,
    ScopedSchema& scoped,
    bool enableInput,
    bool storeChannels = false,
    bool enableOutput = true
) {
   
    //Change the below line to use a smart pointer

    scoped.schema.engineName = "SpoutRS";
    scoped.schema.engineVersion = _strdup(("RS" + std::to_string(RENDER_STREAM_VERSION_MAJOR) + "." + std::to_string(RENDER_STREAM_VERSION_MINOR)).c_str());
    scoped.schema.pluginVersion = _strdup("3.0");
    scoped.schema.info = "";



    if (enableOutput) {
        scoped.schema.scenes.nScenes = senders.size();
        scoped.schema.scenes.scenes = static_cast<RemoteParameters*>(malloc(sizeof(RemoteParameters) * scoped.schema.scenes.nScenes));

        int i = 0;
        for (const auto &s: senders) {
            RemoteParameters scene = {
				_strdup(s.c_str()),
				0,
				nullptr,
			};

            if (enableInput)
            {
                scene.nParameters = 1;
                scene.parameters = static_cast<RemoteParameter*>(malloc(sizeof(RemoteParameter) * 1));

                RemoteParameter par;
                
                par.group = "Input";
                par.displayName = "SpoutInput";
                par.key = "spout_input";
                par.type = RS_PARAMETER_IMAGE;
                par.nOptions = 0;
                par.options = nullptr;
                par.dmxOffset = -1;
                par.dmxType = RS_DMX_16_BE;
                par.flags = REMOTEPARAMETER_NO_FLAGS;
                scene.parameters[0] = par;
                scoped.schema.scenes.scenes[i] = scene;
            }
            else {
                scene.nParameters = 0;
                scene.parameters = nullptr;
                scoped.schema.scenes.scenes[i] = scene;
            }
            i++;
		}
        if (storeChannels) {
            scoped.schema.channels.nChannels = senders.size();
            std::vector<const char*> ptrs;
            ptrs.reserve(senders.size() + 1);
            scoped.schema.channels.channels = static_cast<const char**>(malloc(sizeof(const char*) * scoped.schema.channels.nChannels));
            int j = 0;
            for (const auto &s : senders) {
                ptrs.push_back(s.c_str());
			}
            scoped.schema.channels.channels = std::move(ptrs.data());
        }
    } else {
        scoped.schema.scenes.nScenes = 1;
        scoped.schema.scenes.scenes = static_cast<RemoteParameters*>(malloc(sizeof(RemoteParameters) * 1));
        RemoteParameters scene = {
               "Default",
               0,
               nullptr,
        };
        scene.nParameters = 1;
        scene.parameters = static_cast<RemoteParameter*>(malloc(sizeof(RemoteParameter) * 1));

        RemoteParameter par;
        par.group = "Input";
        par.displayName = "SpoutInput";
        par.key = "spout_input";
        par.type = RS_PARAMETER_IMAGE;
        par.nOptions = 0;
        par.options = nullptr;
        par.dmxOffset = -1;
        par.dmxType = RS_DMX_16_BE;
        par.flags = REMOTEPARAMETER_NO_FLAGS;
        scene.parameters[0] = par;
        scoped.schema.scenes.nScenes = 1;
        scoped.schema.scenes.scenes[0] = scene;
    }


    
}

bool GenerateDX11Texture(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    RenderTarget& target,
    int width,
    int height,
    RSPixelFormat format
)
{
    target.texture.Reset();
    target.view.Reset();

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = toDxgiFormat(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, target.texture.GetAddressOf());
    if (FAILED(hr))
    {
        std::printf("Failed to create texture\n");
        return false;

    }
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    ZeroMemory(&rtvDesc, sizeof(rtvDesc));
    rtvDesc.Format = desc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = device->CreateRenderTargetView(target.texture.Get(), &rtvDesc, target.view.GetAddressOf());
    if (FAILED(hr))
    {
        std::printf("Failed to create render target view\n");
        return false;
    }

}

struct Texture
{
    uint32_t width = 0;
    uint32_t height = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
};

std::optional<Texture> createTexture(Microsoft::WRL::ComPtr<ID3D11Device> device, ImageFrameData image)
{
    Texture texture;
    texture.width = image.width;
    texture.height = image.height;

    D3D11_TEXTURE2D_DESC rtDesc;
    ZeroMemory(&rtDesc, sizeof(D3D11_TEXTURE2D_DESC));
    rtDesc.Width = texture.width;
    rtDesc.Height = texture.height;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = toDxgiFormat(image.format);
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    rtDesc.CPUAccessFlags = 0;
    rtDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    if (FAILED(device->CreateTexture2D(&rtDesc, nullptr, texture.resource.GetAddressOf())))
        return std::nullopt;

    return texture;
}


void LogToD3(RenderStream& rs, std::string msg, int level)
{
    auto s = std::to_string(level) + ": " + msg;
    RS_LOG(s.c_str());
}

std::string GetLaunchDirectory() {
    char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);

}

#undef main

int main(int argc, char* argv[])
{
    //while (!::IsDebuggerPresent())
   //     ::Sleep(100);


    //Configure spdlog file

    std::string logFile = GetLaunchDirectory() + "\\SPRS.log";

    auto logger = spdlog::basic_logger_mt("Spout Logger", logFile);

  // Setup argpraeser
    argparse::ArgumentParser program("SpoutRS");

    program.add_argument("--windowed", "-w").help("Render Visible Window")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--clearsenders", "-r").help("Sets the default behavior to remove sender names from the list of available senders.")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--input", "-i").help("Presents a texture input from disguise as a RenderStream Spout source.")
		.default_value(false)
		.implicit_value(true);

    program.add_argument("--no-output").help("Disables outputs, for use when wanting to just send an output.")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--graphics-adapter", "-g").help("Selects the graphics adapter to use.")
        .default_value(-1)
        .action([](const std::string& value) { return std::stoi(value); });

    program.add_argument("--store-channels").help("Save the spout senders as channels")
        .default_value(false)
		.implicit_value(true);

    program.add_argument("--timeout-limit").help("Sets the timeout limit for the receiver.")
        .default_value(5000)
        .action([](const std::string& value) { return std::stoi(value); });


    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
       logger->info("Error: {}", err.what());
        return 1;
    }

    bool Windowed = program.get<bool>("--windowed");
    bool RemoveSenders = program.get<bool>("--clearsenders");
	bool EnableInput = program.get<bool>("--input");
    bool DisableOutput = program.get<bool>("--no-output");
    int graphicsAdapter = program.get<int>("--graphics-adapter");
    bool StoreChannels = program.get<bool>("--store-channels");
    int timeoutLimit = program.get<int>("--timeout-limit");



    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        logger->error("SDL_Init Error: {}", SDL_GetError());
        return 1;
    }

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    std::unique_ptr<SDL_Window, void (*)(SDL_Window*)> window(SDL_CreateWindow("SpoutIP", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags), SDL_DestroyWindow);

    if (!Windowed) {
        SDL_HideWindow(window.get());
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window.get(), &wmInfo)) {
        logger->error("Failed to get window information: {}", SDL_GetError());
        return 1;
    }

    HWND hwnd = wmInfo.info.win.window;

    WindowFormat windowFormat;


    GraphicsSystem Graphics(logger);

    //Get the graphics adapters
    std::vector<std::pair<std::string, int>> adapters = Graphics.GetGraphicsAdapters();
    if (adapters.empty()) {
        std::printf("No graphics adapters found\n");
        return 1;
    }

    Graphics.SetGraphicsAdapter(graphicsAdapter);
    Graphics.InitializeSystem(hwnd);

    auto D3DDevice = Graphics.GetDevice();
    auto D3DContext = Graphics.GetContext();

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    HRESULT result = D3DDevice->CreateVertexShader(v_main, sizeof(v_main), nullptr, vertexShader.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create vertex shader");
        return 1;
    }

    // Create pixel shader
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    result = D3DDevice->CreatePixelShader(p_main, sizeof(p_main), nullptr, pixelShader.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create pixel shader");
        return 1;
    }



    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // Create input layout
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;
    result = D3DDevice->CreateInputLayout(layout, ARRAYSIZE(layout), v_main, sizeof(v_main), inputLayout.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create input layout");
        return 1;
    }

    // Create vertex buffer
    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(quad);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = quad;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    result = D3DDevice->CreateBuffer(&vertexBufferDesc, &initData, vertexBuffer.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create vertex buffer");
        return 1;
    }

    //Create sampler
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    result = D3DDevice->CreateSamplerState(&samplerDesc, samplerState.GetAddressOf());
    if (FAILED(result)) {
		logger->error("Failed to create sampler state");
		return 1;
	}
	



    if (Windowed) {
        //Create a directx swap chain for the window.
        //Should maybe use SDL
    }

    // Create renderstream object

    RenderStream rs;

    // Initialize the renderstream system.
    // This may produce an exception on designer machines.
    // I also found out it will do this if it is not compiled for x64 :(
    rs.initialise();


    // Initialize renderstream with opengl support.
    rs.initialiseGpGpuWithDX11Device(D3DDevice.Get());

    // Setup a stream descriptions pointer.
    std::unique_ptr<const StreamDescriptions> Descriptions(nullptr);


    ScopedSchema schema;

    size_t nSenders = 0;

    try {
        const Schema* importedSchema = rs.loadSchema(argv[0]);
        schema.schema = *importedSchema;
    }
    catch (const RenderStreamError& e)
    {
        if (e.error == RS_ERROR_NOTFOUND)
        {
            logger->info("No schema found on disk");
        }
        else
        {
            logger->info("Failed to load schema: {}", e.what());
        }
    }

    std::string pr = "Graphics Adapters:";
    logger->info(pr.c_str());
    RS_LOG(pr.c_str());
    for (auto& g : adapters) {
        std::string p = g.first + " : " + std::to_string(g.second);
        logger->info(p);
		RS_LOG(p.c_str());
	}
    
    //Move this into graphics
    Texture InputTexture;
    spoutDirectX SpoutDX;
    spoutSenderNames SpoutSender;
    spoutFrameCount SpoutFrame;
    bool SpoutInit = false;
    HANDLE SpoutSharedHandle = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> SpoutTexture;



    std::unordered_map<StreamHandle, RenderTarget> renderTargets;

    std::atomic_bool isRunning = true;

    while (true)
    {
        // Get the sender names.
        

        if (!DisableOutput) {

            int nSenders_u = Graphics.GetSpoutSenderCount();
            if (nSenders_u != nSenders) {
                    nSenders = nSenders_u;
                    LogToD3(rs, "Found " + std::to_string(nSenders), 0);
                    logger->info("Found {} Spout Senders", nSenders);
                    std::set<std::string> senders = Graphics.GetSpoutSenders();
                    GenerateRenderStreamSchema(senders, schema, EnableInput, StoreChannels);
                    rs.setSchema(&schema.schema);
                    rs.saveSchema(argv[0], &schema.schema);
            }

        }

        auto awaitResult = rs.awaitFrameData(timeoutLimit);
        if (std::holds_alternative<RS_ERROR>(awaitResult))
        {
            RS_ERROR err = std::get<RS_ERROR>(awaitResult);
            // Update the streams pointer when a change error occurs.
            if (err == RS_ERROR_STREAMS_CHANGED)
            {
                Descriptions.reset(rs.getStreams());
                const size_t numStreams = Descriptions ? Descriptions->nStreams : 0;
                for (size_t i = 0; i < numStreams; ++i)
                {
                    const StreamDescription& description = Descriptions->streams[i];
                    RenderTarget& target = renderTargets[description.handle];
                    GenerateDX11Texture(Graphics.GetDevice(), target, description.width, description.height, description.format);
                   // Graphics.AddSpoutSource(description.channel);
                }

                logger->info("Found {} Streams", Descriptions->nStreams);
                // PNL(fmt::sprintf("Found %d Streams\n", header->nStreams))
                continue;
            }
            else if (err == RS_ERROR_TIMEOUT)
            {
                continue;
            }
            else if (err != RS_ERROR_SUCCESS)
            {
                logger->info("rs_awaitFrameData returned success");
                break;
            }
        }

        //Get the

        const FrameData& frameData = std::get<FrameData>(awaitResult);
        const size_t numStreams = Descriptions ? Descriptions->nStreams : 0;
        if (frameData.scene >= schema.schema.scenes.nScenes)
        {
            logger->error("Scene out of bounds: {}", frameData.scene);
            //   PNL("Scene out of bounds");
            continue;
        }

       // LogToD3(rs, "Scene: " + std::to_string(frameData.scene), 0);
      //  LogToD3(rs, "Frame: " + std::string(schema.schema.scenes.scenes[frameData.scene].name), 0);

        std::string sceneName = schema.schema.scenes.scenes[frameData.scene].name;

      // logger->info("Scene: {}", sceneName);

        if (EnableInput) {
            const auto& scene = schema.schema.scenes.scenes[frameData.scene];
            ParameterValues values = rs.getFrameParameters(scene);
            ImageFrameData image = values.get<ImageFrameData>("spout_input");
            if (image.height != InputTexture.height || image.width != InputTexture.width) {
				auto tex = createTexture(D3DDevice, image);
                if (tex) {
					InputTexture = tex.value();
				}
				else {
					logger->error("Failed to create texture");
					continue;
				}
                if (SpoutInit) {
                    SpoutTexture.Reset();
                    SpoutDX.CreateSharedDX11Texture(Graphics.GetDevice().Get(), image.width, image.height, toDxgiFormat(image.format), SpoutTexture.GetAddressOf(), SpoutSharedHandle);
                    SpoutSender.UpdateSender("Disguise", image.width, image.height, SpoutSharedHandle);
                }
			}
            SenderFrame data;
            data.type = RS_FRAMETYPE_DX11_TEXTURE;
            data.dx11.resource = InputTexture.resource.Get();
            rs.getFrameImage(image.imageId, data);
            if (!SpoutInit) {
                SpoutDX.CreateSharedDX11Texture(Graphics.GetDevice().Get(), image.width, image.height, toDxgiFormat(image.format), SpoutTexture.GetAddressOf(), SpoutSharedHandle);
                SpoutInit = SpoutSender.CreateSender("Disguise", image.width, image.height, SpoutSharedHandle, (DWORD)toDxgiFormat(image.format));
                
               // SpoutFrame.CreateAccessMutex("Disguise");
                SpoutFrame.EnableFrameCount("Disguise");
            }
            if (SpoutFrame.CheckAccess()) {
                D3DContext->CopyResource(SpoutTexture.Get(), InputTexture.resource.Get());
                D3DContext->Flush();
                SpoutFrame.SetNewFrame();
                SpoutFrame.AllowAccess();
            }
        }


        if (!DisableOutput) {
            Graphics.AddSpoutSource(sceneName);
        }
        Graphics.ReadFrame(sceneName);

        for (size_t i = 0; i < numStreams; ++i)
        {
            const StreamDescription& description = Descriptions->streams[i];

            CameraResponseData cameraData;
            cameraData.tTracked = frameData.tTracked;
            try
            {
                cameraData.camera = rs.getFrameCamera(description.handle);
            }
            catch (const RenderStreamError& e)
            {
                // It's possible to race here and be processing a request
                // which uses data from before streams changed.
                // TODO: Fix this in the API dll
                if (e.error == RS_ERROR_NOTFOUND)
                    continue;

                throw;
            }

            const RenderTarget& target = renderTargets.at(description.handle);
            FrameResponseData response = {};
            response.cameraData = &cameraData;

            //Check if the description channel matches a spout channel

            Graphics.AddSpoutSource(description.channel);

            auto stagingTexture = Graphics.GetTexture(description.channel);
            auto stagingSRV = Graphics.GetShaderResourceView(description.channel);
            if (stagingTexture && stagingSRV) {
                Graphics.ReadFrame(description.channel);
            }

            if (!stagingTexture || !stagingSRV) {
                stagingTexture = Graphics.GetTexture(sceneName);
                stagingSRV = Graphics.GetShaderResourceView(sceneName);
            }

            
            //auto stagingTexture = Graphics.GetTexture(sceneName);
           //auto stagingSRV = Graphics.GetShaderResourceView(sceneName);

            if (!D3DContext) {
                logger->error("Failed to get context");
                continue;
            }
            //Using a pixel shader and vertex shader we will blit the output

            D3DContext->OMSetRenderTargets(1, target.view.GetAddressOf(), nullptr);

            const float clearColour[4] = { 0.f, 0.8f, 0.f, 0.f };
            D3DContext->ClearRenderTargetView(target.view.Get(), clearColour);

            D3D11_VIEWPORT viewport;
            ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
            viewport.Width = static_cast<float>(description.width);
            viewport.Height = static_cast<float>(description.height);
            viewport.MinDepth = 0;
            viewport.MaxDepth = 1;
            D3DContext->RSSetViewports(1, &viewport);


            // Set what the inputs to the shader are

            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            D3DContext->IASetInputLayout(inputLayout.Get());
            D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            D3DContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);


           // D3DContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
 
            // Set the shaders
            D3DContext->VSSetShader(vertexShader.Get(), nullptr, 0);

            D3DContext->PSSetSamplers(0, 1, samplerState.GetAddressOf());
            D3DContext->PSSetShaderResources(0, 1, stagingSRV.GetAddressOf());

            D3DContext->PSSetShader(pixelShader.Get(), nullptr, 0);



           // D3DContext->PSSetShaderResources(0, 1, stagingSRV.GetAddressOf());
            D3DContext->Draw(6,0);

            //Check for errors


            SenderFrame data;
            data.type = RS_FRAMETYPE_DX11_TEXTURE;
            data.dx11.resource = target.texture.Get();

            rs.sendFrame(description.handle, data, response);
        }


    }
        
    return 0;
}