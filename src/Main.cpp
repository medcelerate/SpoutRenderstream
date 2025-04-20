
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
struct Vertex {
    float position[3];
    float uv[2];
};

// Full-screen quad vertices
const Vertex vertices[] = {
    { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
    { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
    { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
};

const UINT indices[6] = { 0,1,2,0,2,3 };

struct ConstantBuffer
{
    float opacity;
    float padding[3]; // Padding to align to 16 bytes
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
    ScopedSchema& schema,
    bool enableInput,
    bool enableOutput = true
) {
   
    //Change the below line to use a smart pointer

    schema.schema.engineName = "SpoutRS";
    schema.schema.engineVersion = "2.0.0";
    schema.schema.info = "";



    if (enableOutput) {
        schema.schema.scenes.nScenes = senders.size();
        schema.schema.scenes.scenes = static_cast<RemoteParameters*>(
            new RemoteParameters[schema.schema.scenes.nScenes]
        );

        int i = 0;
        for (auto &s: senders) {
            RemoteParameters scene;
            scene.name = s.c_str();
            schema.schema.scenes.scenes[i] = scene;
            if (enableInput)
            {
                schema.schema.scenes.scenes[i].nParameters = 1;
                schema.schema.scenes.scenes[i].parameters = static_cast<RemoteParameter*>(
                    new RemoteParameter[schema.schema.scenes.scenes[i].nParameters]
                    );

                RemoteParameterTypeDefaults defaults;
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
                schema.schema.scenes.scenes[i].parameters[0] = par;
            }
            else {
                schema.schema.scenes.scenes[i].nParameters = 0;
                schema.schema.scenes.scenes[i].parameters = nullptr;
            }
            i++;
		}
    } else {
        schema.schema.scenes.nScenes = 1;
        schema.schema.scenes.scenes = static_cast<RemoteParameters*>(
            new RemoteParameters[schema.schema.scenes.nScenes]
        );
        RemoteParameters scene;
        scene.name = "Default";
        scene.nParameters = 1;

        schema.schema.scenes.scenes[0] = scene;
        schema.schema.scenes.scenes[0].parameters = static_cast<RemoteParameter*>(
            new RemoteParameter[schema.schema.scenes.scenes[0].nParameters]
        );
        RemoteParameterTypeDefaults defaults;
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
        schema.schema.scenes.scenes[0].parameters[0] = par;
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


void LogToD3(RenderStream& rs, std::string msg, int level)
{
   
}

#undef main

int main(int argc, char* argv[])
{
   // while (!::IsDebuggerPresent())
    //    ::Sleep(100);

    //Configure spdlog file

    auto logger = spdlog::basic_logger_mt("Spout Logger", "logs/SPRS.log");

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

    // Create vertex buffer
    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(vertices);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {vertices};

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    result = D3DDevice->CreateBuffer(&vertexBufferDesc, &initData, vertexBuffer.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create vertex buffer");
        return 1;
    }

    // Create index buffer
    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(indices);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA indexInitData = {indices};

    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    result = D3DDevice->CreateBuffer(&indexBufferDesc, &indexInitData, indexBuffer.GetAddressOf());
    if (FAILED(result)) {
		logger->error("Failed to create index buffer");
		return 1;
	}


    // Create constant buffer
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;
    result = D3DDevice->CreateBuffer(&cbDesc, nullptr, constantBuffer.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create constant buffer");
        return 1;
    }

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // Create input layout
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    result = D3DDevice->CreateInputLayout(layout, ARRAYSIZE(layout), v_main, sizeof(v_main), inputLayout.GetAddressOf());
    if (FAILED(result)) {
        logger->error("Failed to create input layout");
        return 1;
    }

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    result = D3DDevice->CreateSamplerState(&sampDesc, samplerState.GetAddressOf());
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


    std::unordered_map<StreamHandle, RenderTarget> renderTargets;

    std::atomic_bool isRunning = true;

    while (true)
    {
        // Get the sender names.
        

        if (!DisableOutput) {

            int nSenders_u = Graphics.GetSpoutSenderCount();
            if (nSenders_u != nSenders) {
                    nSenders = nSenders_u;
                    logger->info("Found {} Spout Senders", nSenders);
                    std::set<std::string> senders = Graphics.GetSpoutSenders();
                    GenerateRenderStreamSchema(senders, schema, EnableInput);
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
                    Graphics.AddSpoutSource(description.channel);
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
            std::printf("Scene out of bounds\n");
            //   PNL("Scene out of bounds");
            continue;
        }

        std::string sceneName = schema.schema.scenes.scenes[frameData.scene].name;

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

            
            auto stagingTexture = Graphics.GetTexture(description.channel);
            auto stagingSRV = Graphics.GetShaderResourceView(description.channel);
            if (stagingTexture && stagingSRV) {
                Graphics.ReadFrame(description.channel);
            }

            if (!stagingTexture || !stagingSRV) {
                stagingTexture = Graphics.GetTexture(sceneName);
                stagingSRV = Graphics.GetShaderResourceView(sceneName);
            }


            if (!D3DContext) {
                logger->error("Failed to get context");
                continue;
            }
            //Using a pixel shader and vertex shader we will blit the output

            D3DContext->OMSetRenderTargets(1, target.view.GetAddressOf(), nullptr);

            const float clearColour[4] = { 1.f, 0.2f, 0.f, 0.f };
           // D3DContext->ClearRenderTargetView(target.view.Get(), clearColour);

            // Set what the inputs to the shader are
            D3DContext->IASetInputLayout(inputLayout.Get());
            D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            D3DContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

           // D3DContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
 
            // Set the shaders
            D3DContext->VSSetShader(vertexShader.Get(), nullptr, 0);
            D3DContext->PSSetShader(pixelShader.Get(), nullptr, 0);

            D3DContext->PSSetShaderResources(0, 1, stagingSRV.GetAddressOf());
            D3DContext->PSSetSamplers(0, 1, samplerState.GetAddressOf());
            D3DContext->Draw(6,0);

            SenderFrame data;
            data.type = RS_FRAMETYPE_DX11_TEXTURE;
            data.dx11.resource = target.texture.Get();

            rs.sendFrame(description.handle, data, response);
        }


    }
        
    return 0;
}