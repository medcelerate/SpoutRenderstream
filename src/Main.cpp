#include <gl/glew.h>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL

#define NOMINMAX

#include <GLFW/glfw3native.h>
#include <cstdio>
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "../SpoutGL/SpoutReceiver.h"
#include "../SpoutGL/SpoutSender.h"
#include "../includes/renderstream.hpp"
#include "argparse.hpp"
#include "graphics.hpp"

// TODO
// Add cli args for stream name and window size
// Add a way to get the stream name from the sender
// Add a scoped schema for renderstream



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

        for (int i = 0; i < schema.schema.scenes.nScenes; i++) {
            RemoteParameters scene;
            scene.name = senders[i].c_str();
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
            } else {
                schema.schema.scenes.scenes[i].nParameters = 0;
                schema.schema.scenes.scenes[i].parameters = nullptr;
            }
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

void GenerateDX11Texture(
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
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &target.texture.GetAddressOf());
    if (FAILED(hr))
    {
        std::printf("Failed to create texture\n");
        throw std::runtime_error("Failed to create texture");
    }
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    ZeroMemory(&rtvDesc, sizeof(rtvDesc));
    rtvDesc.Format = desc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = device->CreateRenderTargetView(target.texture.Get(), &rtvDesc, &target.view.GetAddressOf());
    if (FAILED(hr))
    {
        std::printf("Failed to create render target view\n");
        throw std::runtime_error("Failed to create render target view");
    }

}


void LogToD3(RenderStream& rs, std::string msg, int level)
{
    switch (level)
    {
    case 0:
        RS_LOG(msg.c_str());
        break;
    case 1:

    }
}

int main(int argc, char* argv[])
{
   //  while (!::IsDebuggerPresent())
    //    ::Sleep(100);

    //Configure spdlog file

    auto logger = spdlog::basic_logger_mt("basic_logger", "SPRS.log");

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

    bool Windowed = program["--windowed"];
    bool RemoveSenders = program["--remove_sender_names"];
	bool EnableInput = program["--inputs"];
    bool DisableOutput = program["--disable_outputs"];
    int graphicsAdapter = program["--graphics-adapter"];
    int timeoutLimit = program["--timeout-limit"];



    // Setup receiver
    


    // Initialize glfw window system
    if (!glfwInit())
    {

        std::printf("GLFW failed to init\n");
        glfwTerminate();
        return 1;
    }

    // Set Opengl Versions (P.S. If you accidentally put two of these like I did you will get a very strange read access error or something IDK)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Makes the window all floaty and see-through.
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);

    //Control window visibility


    if (isWindowed) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    }
    else {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    // A modern (and possibly messy) window pointer setup.
    // void(*)(GLFWwindow*) is a placeholder (any) type for the last arguments which is what will be called when the pointer needs to be released.
    std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> window(glfwCreateWindow(1280, 720, "VX SpoutRS (Non-Commercial)", nullptr, nullptr), glfwDestroyWindow);

    // Check that the pointer is not a nullptr
    if (!window)
    {
        std::printf("Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    WindowFormat windowFormat;

    // Get the final draw buffer size.
    glfwGetFramebufferSize(window.get(), &windowFormat.width, &windowFormat.height);

    // I kind of don't know exactly what this does yet.
    glfwMakeContextCurrent(window.get());


        // Get the window contexts from the native platform.
    // Since RS is windows only we can just get the native windows types directly.
    // Alternative is to wrap it in platform agnostic class.
    HGLRC hc = glfwGetWGLContext(window.get());
    HDC dc = GetDC(glfwGetWin32Window(window.get()));

        // Check that theese are not null.
    if (!hc)
    {
        std::printf("Unable to get WGL Context\n");
        //d3 logging rs_logToD3();
        return 1;
    }

    if (!dc)
    {
        std::printf("Unable to get native window\n");
        return 1;
    }

    Graphics GraphicsSystem;

    //Get the graphics adapters
    std::vector<std::pair<std::string, int>> adapters = GraphicsSystem.GetGraphicsAdapters();
    if (adapters.empty()) {
        std::printf("No graphics adapters found\n");
        return 1;
    }

    GraphicsSystem.SetGraphicsAdapter(graphicsAdapter);
    GraphicsSystem.InitializeSystem();

    auto D3DDevice = GraphicsSystem.GetDevice();
    auto D3DContext = GraphicsSystem.GetContext();

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    result = D3DDevice->CreateVertexShader(v_main, sizeof(v_main), nullptr, vertexShader.GetAddressOf());
    if (FAILED(result)) {
        spdlog::error("Failed to create vertex shader: {}", result);
        return;
    }

    // Create pixel shader
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    result = D3DDevice->CreatePixelShader(p_main, sizeof(p_main), nullptr, pixelShader.GetAddressOf());
    if (FAILED(result)) {
        spdlog::error("Failed to create pixel shader: {}", result);
        return;
    }

    // Create vertex buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    result = D3DDevice->CreateBuffer(&bufferDesc, &initData, vertexBuffer.GetAddressOf());
    if (FAILED(result)) {
        spdlog::error("Failed to create vertex buffer: {}", result);
        return;
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
        spdlog::error("Failed to create constant buffer: {}", result);
        return;
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
        spdlog::error("Failed to create input layout: {}", result);
        return;
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
        spdlog::error("Failed to create sampler state: {}", result);
        return;
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
            std::printf("No schema found on disk\n");
        }
        else
        {
            std::printf("Failed to load schema: %s\n", e.what());
        }
    }


    std::unordered_map<StreamHandle, RenderTarget> renderTargets;


    while (!glfwWindowShouldClose(window.get()))
    {
        // Get the sender names.

        glfwPollEvents();

        

        if (!OutputDisabled) {

            int nSenders_u = GraphicsSystem.GetSpoutSenderCount();
            if (nSenders_u != nSenders) {
                    nSenders = nSenders_u;
                    std::printf("Found %d Spout Senders\n", nSenders);
                    std::set<std::string> senders = GraphicsSystem.GetSpoutSenders();
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
                const size_t numStreams = header ? header->nStreams : 0;
                for (size_t i = 0; i < numStreams; ++i)
                {
                    const StreamDescription& description = header->streams[i];
                    RenderTarget& target = renderTargets[description.handle];
                    GenerateDX11Texture(GraphicsSystem.GetDevice(), target, description.width, description.height, description.format);
                    GraphicsSystem.AddSpoutSource(description.channel);
                }

                std::printf("Found %d Streams\n", header->nStreams);
                // PNL(fmt::sprintf("Found %d Streams\n", header->nStreams))
                continue;
            }
            else if (err == RS_ERROR_TIMEOUT)
            {
                continue;
            }
            else if (err != RS_ERROR_SUCCESS)
            {
                std::printf("rs_awaitFrameData returned %d", err);
                break;
            }
        }

        //Get the

        const FrameData& frameData = std::get<FrameData>(awaitResult);
        const size_t numStreams = header ? header->nStreams : 0;
        if (frameData.scene >= schema.schema.scenes.nScenes)
        {
            std::printf("Scene out of bounds\n");
            //   PNL("Scene out of bounds");
            continue;
        }

        std::string sceneName = schema.schema.scenes.scenes[frameData.scene].name;

        if (!OutputDisabled) {
            GraphicsSystem.AddSpoutSource(sceneName);
        }
        GraphicsSystem.ReadFrame(sceneName);

        for (size_t i = 0; i < numStreams; ++i)
        {
            const StreamDescription& description = header->streams[i];

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

            
            auto stagingTexture = GraphicsSystem.GetTexture(description.channel);
            auto stagingSRV = GraphicsSystem.GetShaderResourceView(description.channel);
            if (stagingTexture && stagingSRV) {
                GraphicsSystem.ReadFrame(description.channel)
            }

            if (!stagingTexture || !stagingSRV) {
                stagingTexture = GraphicsSystem.GetStagingTexture(sceneName);
                stagingSRV = GraphicsSystem.GetShaderResourceView(sceneName);
            }


            if (!D3DContext) {
                std::printf("Failed to get context\n");
                continue;
            }
            //Using a pixel shader and vertex shader we will blit the output

            D3DContext->OMSetRenderTargets(1, target.view, nullptr);

            // Set what the inputs to the shader are
            D3DContext->IASetInputLayout(inputLayout.Get());
            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            D3DContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
            D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
 
            // Set the shaders
            D3DContext->VSSetShader(vertexShader.Get(), nullptr, 0);
            D3DContext->PSSetShader(pixelShader.Get(), nullptr, 0);

            D3DContext->PSSetShaderResources(0, 1, stagingSRV.GetAddressOf());
            D3DContext->PSSetSamplers(0, 1, samplerState.GetAddressOf());
            D3DContext->Draw(4, 0);

            SenderFrame data;
            data.type = RS_FRAMETYPE_DX11_TEXTURE;
            data.dx11.resource = target.texture.Get();

            rs.sendFrame(description.handle, data, response);
        }


        glfwSwapBuffers(window.get());
    }
        
    return 0;
}