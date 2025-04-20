
#include <unordered_map>
#include <string>

typedef struct SpoutMeta
{
    unsigned int width;
    unsigned int height;
    DXGI_FORMAT format;
    HANDLE handle;
} SpoutMeta_t;

class GraphicsSystem {
public:
    GraphicsSystem(std::shared_ptr<spdlog::logger>& logger) {
        m_Logger = logger;
    }

    ~GraphicsSystem() {
        // Destructor implementation
    }

    void InitializeSystem(HWND &hwnd) {
        if (m_Device) {
            Shutdown();
        }

        HRESULT hr;
        if (m_CurrentAdapter > -1) {
            // Create the device and context with the specified adapter
            Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
            hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
            if (FAILED(hr)) {
                std::printf("Failed to create DXGIFactory\n");
                return;
            }

            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
            hr = factory->EnumAdapters1(m_CurrentAdapter, &adapter);
            if (FAILED(hr)) {
                std::printf("Failed to enumerate adapters\n");
                return;
            }

            DXGI_SWAP_CHAIN_DESC sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = 2;
            sd.BufferDesc.Width = 0;
            sd.BufferDesc.Height = 0;
            sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BufferDesc.RefreshRate.Numerator = 60;
            sd.BufferDesc.RefreshRate.Denominator = 1;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hwnd;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Windowed = TRUE;
            sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            UINT createDeviceFlags = 0;
            //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
            D3D_FEATURE_LEVEL featureLevel;
            const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
            HRESULT hr = D3D11CreateDeviceAndSwapChain(
                adapter.Get(),
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                createDeviceFlags,
                featureLevelArray,
                2,
                D3D11_SDK_VERSION,
                &sd,
                m_SwapChain.GetAddressOf(),
                m_Device.GetAddressOf(),
                &featureLevel,
                m_Context.GetAddressOf()
            );
            if (FAILED(hr)) {
                m_Logger->error("Failed to create D3D11 device\n");
                return;
            }


            adapter->GetDesc1(&m_AdapterDesc);
        } else {

            // Create the device and context
            DXGI_SWAP_CHAIN_DESC sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = 2;
            sd.BufferDesc.Width = 0;
            sd.BufferDesc.Height = 0;
            sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BufferDesc.RefreshRate.Numerator = 60;
            sd.BufferDesc.RefreshRate.Denominator = 1;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hwnd;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Windowed = TRUE;
            sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            UINT createDeviceFlags = 0;
            //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
            D3D_FEATURE_LEVEL featureLevel;
            const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
            HRESULT hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                createDeviceFlags,
                featureLevelArray,
                2,
                D3D11_SDK_VERSION,
                &sd,
                m_SwapChain.GetAddressOf(),
                m_Device.GetAddressOf(),
                &featureLevel,
                m_Context.GetAddressOf()
            );
            if (FAILED(hr)) {
                std::printf("Failed to create D3D11 device\n");
                return;
            }

        }

        hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(m_BackBuffer.GetAddressOf()));
        if (FAILED(hr)) {
            m_Logger->error("Failed to get back buffer");
            return;
        }

        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> BackBufferRTV;
        hr = m_Device->CreateRenderTargetView(m_BackBuffer.Get(), nullptr, BackBufferRTV.GetAddressOf());
        if (FAILED(hr)) {
            m_Logger->error("Failed to create render target view");
            return;
        }

        //Not sure why we do this
        m_BackBuffer->Release();

    };

    void Shutdown() {
        // Shutdown the graphics system
        if (m_Device) {
            m_Device->Release();
            m_Device = nullptr;
        }
        if (m_Context) {
            m_Context->Release();
            m_Context = nullptr;
        }
    };

    void SetGraphicsAdapter(int index) {
        auto adapters = GetGraphicsAdapters();
        // Set the graphics adapter based on the index
        if (index < 0 || index >= adapters.size()) {
            m_Logger->error("Invalid graphics adapter index: {}", index);
            return;
        }
        m_CurrentAdapter = index;
    };

    //Spout
    std::set<std::string> GetSpoutSenders() {
        // Get the list of senders
        std::set<std::string> senders;
        m_SpoutSender.GetSenderNames(&senders);
        return senders;
    };

    int GetSpoutSenderCount() {
        // Get the number of senders
        return m_SpoutSender.GetSenderCount();
    };

    bool AddSpoutSource(const std::string& senderName) {
        // Check the ActiveReceivers
        if (m_ActiveReceivers.find(senderName) != m_ActiveReceivers.end()) {
            m_Logger->error("Sender already exists");
            return false;
        }

        //List senders
        std::set<std::string> senders;
        m_SpoutSender.GetSenderNames(&senders);

        // Check if the sender name is valid
        if (senders.find(senderName) == senders.end()) {
			m_Logger->error("Sender not found: {}", senderName);
			return false;
		}
        
        
        SpoutMeta_t meta;
        DWORD format = 0;
        if (!m_SpoutSender.GetSenderInfo(senderName.c_str(), meta.width, meta.height, meta.handle, format)) {
            m_Logger->error("Failed to get sender info");
            return false;
        }

        meta.format = (DXGI_FORMAT)format;

        if (!meta.handle) {
            m_Logger->error("Sender handle is null");
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;



        spoutFrameCount frameCount;

        if(frameCount.CreateAccessMutex(senderName.c_str())) {
            m_Logger->error("Failed to create access mutex");
            return false;
        }

        if (!createStagingTexture(senderName, meta.width, meta.height, meta.format)) {
            m_Logger->error("Failed to create staging texture");
            return false;
        }

        if (!m_SpoutDirectX.OpenDX11shareHandle(m_Device.Get(), texture.GetAddressOf(), meta.handle)) {
            m_Logger->error("Failed to open DX11 share handle");
            return false;
        }

        m_SpoutTextures[senderName] = texture;
        m_SpoutFrameCounts[senderName] = frameCount;
        m_SpoutMeta[senderName] = meta;
        m_ActiveReceivers.insert(senderName);
        return true;
    };

    bool ReconfigureSpoutSource(const std::string& senderName, int width, int height, DXGI_FORMAT format) {
        // Reconfigure the spout source
        //Check active receivers
        if (m_ActiveReceivers.find(senderName) == m_ActiveReceivers.end()) {
            m_Logger->error("Sender {} not found in active receivers", senderName);
            return false;
        }
        // Check if the sender is still active
        if (!m_SpoutSender.FindSenderName(senderName.c_str())) {
            m_Logger->error("Sender not found");
            return false;
        }

        auto texture = m_SpoutTextures[senderName];
        auto stagingTexture = m_StagingTextures[senderName];
        auto srv = m_SRVs[senderName];

        if (!texture || !stagingTexture || !srv) {
            m_Logger->error("Failed to get spout texture or staging texture\n");
            return false;
        }
        // Recreate the staging texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        HRESULT hr = m_Device->CreateTexture2D(&desc, nullptr, &stagingTexture);
        if (FAILED(hr)) {
            m_Logger->error("Failed to create staging texture");
            return false;
        }
        // Create a shader resource view for the staging texture
        //Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        hr = m_Device->CreateShaderResourceView(stagingTexture.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr)) {
            m_Logger->error("Failed to create shader resource view");
            return false;
        }
        // Update the staging texture and shader resource view
        m_StagingTextures[senderName] = stagingTexture;
        m_SRVs[senderName] = srv;
        m_SpoutMeta[senderName].width = width;
        m_SpoutMeta[senderName].height = height;
        m_SpoutMeta[senderName].format = format;
        m_Logger->info("Reconfigured spout source: {} to {}x{} format: {}", senderName, width, height, format);
        return true;
    };

    void RemoveSpoutSource(const std::string& senderName) {
        // Remove the spout source
        if (m_ActiveReceivers.find(senderName) == m_ActiveReceivers.end()) {
            std::printf("Sender not found in active receivers");
            return;
        }
        auto &frame = m_SpoutFrameCounts.at(senderName);
        frame.CloseAccessMutex();
        frame.DisableFrameCount();
        frame.CleanupFrameCount();

        m_SpoutMeta.erase(senderName);
        m_SpoutTextures.erase(senderName);
        m_StagingTextures.erase(senderName);
        m_SRVs.erase(senderName);
        m_SpoutFrameCounts.erase(senderName);
        m_ActiveReceivers.erase(senderName);
    };

    void ReadFrame(const std::string& senderName) {
        // Read the frame from the spout source
        if (m_ActiveReceivers.find(senderName) == m_ActiveReceivers.end()) {
            std::printf("Sender not found in active receivers");
            return;
        }
        auto& meta = m_SpoutMeta[senderName];

        unsigned int width = 0;
        unsigned int height = 0;
        DWORD format;

        if (!m_SpoutSender.GetSenderInfo(senderName.c_str(), width, height, meta.handle, format)) {
            m_Logger->error("Failed to get sender info");
            return;
        }
        if (width != meta.width || height != meta.height || format != meta.format) {
            m_Logger->error("Sender info has changed");
            ReconfigureSpoutSource(senderName, width, height, (DXGI_FORMAT)format);
        }

        auto& frame = m_SpoutFrameCounts[senderName];
        auto texture = m_SpoutTextures[senderName];
        auto stagingTexture = m_StagingTextures[senderName];

        if (m_Device && frame.GetNewFrame()) {
            if (!texture || !stagingTexture || !m_Context) {
                m_Logger->error("Failed to get spout texture or staging texture");
                return;
            }
            m_Context->CopyResource(stagingTexture.Get(), texture.Get());
            
            return;
        }

    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture(const std::string& senderName) {
        // Get the spout texture
        if (m_ActiveReceivers.find(senderName) == m_ActiveReceivers.end()) {
            m_Logger->error("Sender not found in active receivers");
            return nullptr;
        }
        return m_StagingTextures.at(senderName);
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetShaderResourceView(const std::string& senderName) {
        // Get the shader resource view
        if (m_ActiveReceivers.find(senderName) == m_ActiveReceivers.end()) {
            m_Logger->error("Sender not found in active receivers");
            return nullptr;
        }
        return m_SRVs.at(senderName);
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> GetContext() {
        // Get the D3D11 context
        return m_Context;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> GetDevice() {
        // Get the D3D11 device
        return m_Device;
    }
    

    static std::vector<std::pair<std::string, int>> GetGraphicsAdapters() {

        Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            std::printf("Failed to create DXGIFactory");
            return {};
        }

        std::vector<std::pair<std::string, int>> adapters;

        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            // Convert the adapter name to a string
            std::wstring wstrName(desc.Description);
            std::string strName(wstrName.begin(), wstrName.end());

            // Store the adapter name and index
            adapters.push_back({ strName, i });
        }
        return adapters;
    }

private:

    bool createStagingTexture(const std::string& senderName, int width, int height, DXGI_FORMAT format) {
        // Check if the sender name already exists
        if (m_StagingTextures.find(senderName) != m_StagingTextures.end()) {
            std::printf("Staging texture already exists for sender: %s\n", senderName.c_str());
            return true;
        }
        // Create a staging texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
        HRESULT hr = m_Device->CreateTexture2D(&desc, nullptr, &stagingTexture);
        if (FAILED(hr)) {
            m_Logger->error("Failed to create staging texture\n");
            return false;
        }


        // Create a shader resource view for the staging texture

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

        hr = m_Device->CreateShaderResourceView(stagingTexture.Get(), &srvDesc, srv.GetAddressOf());
        if (FAILED(hr)) {
            m_Logger->error("Failed to create shader resource view");
            return false;
        }

        m_StagingTextures[senderName] = stagingTexture;
        m_SRVs[senderName] = srv;
        return true;
    }

    std::shared_ptr<spdlog::logger> m_Logger;

    int8_t m_CurrentAdapter = -1;
    std::string m_CurrentAdapterName;
    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    DXGI_ADAPTER_DESC1 m_AdapterDesc;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> m_KeyedMutex;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_BackBuffer;




    //Spout specific
    spoutDirectX m_SpoutDirectX;
    spoutSenderNames m_SpoutSender;
    std::unordered_map<std::string, spoutFrameCount> m_SpoutFrameCounts;
    std::unordered_map<std::string, SpoutMeta_t> m_SpoutMeta;
    std::set<std::string> m_ActiveReceivers;


    //Replace string with other key
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11Texture2D>> m_SpoutTextures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11Texture2D>> m_StagingTextures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_SRVs;

};