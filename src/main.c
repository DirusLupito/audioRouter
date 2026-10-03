// Enable the C macros for COM interface calls.
#define COBJMACROS
#include <Windows.h>
// Define the GUID constants declared by the MMDevice and property-key headers.
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

int main(void) {
    printf("audioRouter starting.\n");

    // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex
    // HRESULT CoInitializeEx(
    //   LPVOID pvReserved,
    //   DWORD  dwCoInit
    // );
    //
    // pvReserved
    // Reserved; pass NULL.
    //
    // dwCoInit
    // COM initialization options for this thread. COINIT_MULTITHREADED selects
    // the multithreaded apartment.
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateinstance
    // HRESULT CoCreateInstance(
    //   REFCLSID  rclsid,
    //   LPUNKNOWN pUnkOuter,
    //   DWORD     dwClsContext,
    //   REFIID    riid,
    //   LPVOID    *ppv
    // );
    //
    // rclsid
    // Identifies the COM class to create: MMDeviceEnumerator here.
    //
    // pUnkOuter
    // The controlling object for COM aggregation. NULL for this standalone object.
    //
    // dwClsContext
    // Where the object runs. CLSCTX_INPROC_SERVER loads it into this process.
    //
    // riid
    // Identifies the interface we want from the object: IMMDeviceEnumerator.
    //
    // ppv
    // Receives the requested interface pointer. Release it when finished.
    IMMDeviceEnumerator *enumerator = NULL;
    CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                     &IID_IMMDeviceEnumerator, (void **)&enumerator);

    // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-enumaudioendpoints
    // HRESULT EnumAudioEndpoints(
    //   EDataFlow           dataFlow,
    //   DWORD               dwStateMask,
    //   IMMDeviceCollection **ppDevices
    // );
    //
    // dataFlow
    // eRender selects playback devices; eCapture selects recording devices;
    // eAll selects both.
    //
    // dwStateMask
    // Filters by device state. DEVICE_STATE_ACTIVE selects available devices.
    // See the documentation for the other state flags.
    //
    // ppDevices
    // Receives the device collection. Release it when finished.
    IMMDeviceCollection *devices = NULL;
    IMMDeviceEnumerator_EnumAudioEndpoints(
        enumerator, eRender, DEVICE_STATE_ACTIVE, &devices);

    // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdevicecollection-getcount
    // HRESULT GetCount(
    //   UINT *pcDevices
    // );
    //
    // pcDevices
    // Receives the number of entries in this collection.
    UINT output_count = 0;
    IMMDeviceCollection_GetCount(devices, &output_count);
    printf("\nPlayback devices (%u):\n", output_count);

    for (UINT index = 0; index < output_count; ++index) {
        // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdevicecollection-item
        // HRESULT Item(
        //   UINT      nDevice,
        //   IMMDevice **ppDevice
        // );
        //
        // nDevice
        // Zero-based index in this collection, from 0 through output_count - 1.
        //
        // ppDevice
        // Receives the device interface. Each successful Item call acquires
        // a reference that must be released when finished.
        IMMDevice *device = NULL;
        IMMDeviceCollection_Item(devices, index, &device);

        // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdevice-openpropertystore
        // HRESULT OpenPropertyStore(
        //   DWORD          stgmAccess,
        //   IPropertyStore **ppProperties
        // );
        //
        // stgmAccess
        // Access mode for the properties. STGM_READ requests read-only access.
        //
        // ppProperties
        // Receives the property-store interface. Release it when finished.
        IPropertyStore *properties = NULL;
        IMMDevice_OpenPropertyStore(device, STGM_READ, &properties);

        // https://learn.microsoft.com/en-us/windows/win32/api/propidlbase/ns-propidlbase-propvariant
        // Relevant member declarations from PROPVARIANT (other members omitted):
        // VARTYPE vt;
        // LPWSTR  pwszVal;
        //
        // vt
        // Identifies the value's type. Zero initialization sets VT_EMPTY.
        //
        // pwszVal
        // Holds the wide-string pointer when vt is VT_LPWSTR.
        // See the documentation for the full union and other value types.
        PROPVARIANT name = {0};

        // https://learn.microsoft.com/en-us/windows/win32/api/propsys/nf-propsys-ipropertystore-getvalue
        // HRESULT GetValue(
        //   REFPROPERTYKEY key,
        //   PROPVARIANT    *pv
        // );
        //
        // key
        // Identifies the property to read; in C, pass a pointer to the key.
        //
        // pv
        // Receives the property's type and value. Clear it when finished.
        //
        // https://learn.microsoft.com/en-us/windows/win32/coreaudio/pkey-device-friendlyname
        // PKEY_Device_FriendlyName returns a VT_LPWSTR containing the display name.
        IPropertyStore_GetValue(properties, &PKEY_Device_FriendlyName, &name);
        printf("  %u: %ls\n", index + 1, name.pwszVal);

        // https://learn.microsoft.com/en-us/windows/win32/api/propidl/nf-propidl-propvariantclear
        // HRESULT PropVariantClear(
        //   PROPVARIANT *pvar
        // );
        //
        // pvar
        // The value to clear. Frees its owned data, including the name string,
        // and resets the type to VT_EMPTY.
        PropVariantClear(&name);

        // https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-release
        // ULONG Release();
        //
        // Releases one reference to a COM interface. The object is destroyed
        // when its reference count reaches zero.
        IPropertyStore_Release(properties);
        IMMDevice_Release(device);
    }

    printf("Enter up to 64 binary digits to select playback devices.\n"
           "Leftmost digit = device 1, next digit = device 2, etc.\n"
           "Omitted digits are 0; all zeros exits.\n"
           "WARNING: Do not select the primary playback device used by Windows.\n");

    char input[65] = {0};
    uint64_t selected_devices = 0;
    int status = EXIT_SUCCESS;

    while (1) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Error reading input.\n");
            status = EXIT_FAILURE;
        } else {
            // for example, 000 means no devices selected, 001 means device 3 selected,
            // 100 means device 1 selected, 101 means devices 1 and 3 selected, etc.

            size_t digit_count = strcspn(input, "\r\n");
            for (size_t i = digit_count; i > 0; --i) {
                selected_devices = selected_devices * 2 + (input[i - 1] - '0');
            }

            uint64_t remaining_devices = selected_devices;
            for (UINT index = 0; index < output_count && remaining_devices != 0;
                ++index, remaining_devices /= 2) {
                if (remaining_devices % 2 == 0) {
                    continue;
                }

                IMMDevice *selected_device = NULL;
                IMMDeviceCollection_Item(devices, index, &selected_device);

                // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdevice-getid
                // HRESULT GetId(
                //   LPWSTR *ppstrId
                // );
                //
                // ppstrId
                // Receives an allocated endpoint ID string. Treat it as opaque;
                // it identifies this endpoint independently of the collection index.
                // Free the string with CoTaskMemFree when finished.
                LPWSTR endpoint_id = NULL;
                IMMDevice_GetId(selected_device, &endpoint_id);
                printf("Selected device: %u\n", index + 1);
                printf("Endpoint ID: %ls\n", endpoint_id);

                // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cotaskmemfree
                // void CoTaskMemFree(
                //   LPVOID pv
                // );
                //
                // pv
                // Address of the allocation to free; here, the string from GetId.
                CoTaskMemFree(endpoint_id);

                // selected_device is the IMMDevice to use for WASAPI playback.
                IMMDevice_Release(selected_device);
            }
        }

        printf("Are these selections correct? (y/n): ");
        char confirmation[8] = {0};
        if (fgets(confirmation, sizeof(confirmation), stdin) == NULL) {
            printf("Error reading input.\n");
            status = EXIT_FAILURE;
        } else if (confirmation[0] == 'y' || confirmation[0] == 'Y') {
            break;
        } else {
            printf("Enter up to 64 binary digits to select playback devices.\n"
                   "Leftmost digit = device 1, next digit = device 2, etc.\n"
                   "Omitted digits are 0; all zeros exits.\n"
                   "WARNING: Do not select the primary playback device used by Windows.\n");
            selected_devices = 0;
        }
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-getdefaultaudioendpoint
    // HRESULT GetDefaultAudioEndpoint(
    //   EDataFlow dataFlow,
    //   ERole     role,
    //   IMMDevice **ppEndpoint
    // );
    //
    // dataFlow
    // eRender requests the default playback device; eCapture requests the
    // default recording device. Loopback uses the playback device as its source.
    //
    // role
    // Selects which default to use. eConsole is the general system audio role;
    // eMultimedia is for media playback, and eCommunications is for voice calls.
    //
    // ppEndpoint
    // Receives the default device interface. Release it when finished.
    IMMDevice *source_device = NULL;
    IMMDeviceEnumerator_GetDefaultAudioEndpoint(
        enumerator, eRender, eConsole, &source_device);

    // Read the source's name and endpoint ID using the same calls documented above.
    IPropertyStore *source_properties = NULL;
    IMMDevice_OpenPropertyStore(source_device, STGM_READ, &source_properties);

    PROPVARIANT source_name = {0};
    IPropertyStore_GetValue(source_properties, &PKEY_Device_FriendlyName,
                           &source_name);

    LPWSTR source_endpoint_id = NULL;
    IMMDevice_GetId(source_device, &source_endpoint_id);

    printf("\nDefault playback device (loopback source):\n");
    printf("Name: %ls\n", source_name.pwszVal);
    printf("Endpoint ID: %ls\n", source_endpoint_id);
    printf("Direction: playback (eRender)\nRole: console (eConsole)\n");

    // check and see if the user selected the default playback device as one of the destinations.

    uint64_t remaining_devices = selected_devices;
    for (UINT index = 0; index < output_count && remaining_devices != 0;
         ++index, remaining_devices /= 2) {
        if (remaining_devices % 2 == 0) {
            continue;
        }

        IMMDevice *selected_device = NULL;
        IMMDeviceCollection_Item(devices, index, &selected_device);

        LPWSTR endpoint_id = NULL;
        IMMDevice_GetId(selected_device, &endpoint_id);
        int is_source = wcscmp(endpoint_id, source_endpoint_id) == 0;

        CoTaskMemFree(endpoint_id);
        IMMDevice_Release(selected_device);

        if (is_source) {
            printf("Error: device %u is the default playback device. "
                   "Choose a different destination to avoid audio feedback.\n",
                   index + 1);
            status = EXIT_FAILURE;
            break;
        }
    }

    if (status == EXIT_SUCCESS && selected_devices != 0) {
        // https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdevice-activate
        // HRESULT Activate(
        //   REFIID      iid,
        //   DWORD       dwClsCtx,
        //   PROPVARIANT *pActivationParams,
        //   void        **ppInterface
        // );
        //
        // iid
        // Identifies the interface to create: IAudioClient manages the stream.
        //
        // dwClsCtx
        // CLSCTX_ALL permits any supported COM execution context.
        //
        // pActivationParams
        // Optional activation settings. NULL uses ordinary endpoint activation.
        //
        // ppInterface
        // Receives the requested interface. Release it when finished.
        IAudioClient *audio_client = NULL;
        HRESULT result = IMMDevice_Activate(source_device, &IID_IAudioClient,
                                            CLSCTX_ALL, NULL,
                                            (void **)&audio_client);

        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getmixformat
        // HRESULT GetMixFormat(
        //   WAVEFORMATEX **ppDeviceFormat
        // );
        //
        // ppDeviceFormat
        // Receives an allocated description of the audio engine's mix format.
        // It can contain WAVEFORMATEXTENSIBLE data beyond the WAVEFORMATEX header.
        //
        // https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex
        // typedef struct tWAVEFORMATEX {
        //   WORD  wFormatTag;
        //   WORD  nChannels;
        //   DWORD nSamplesPerSec;
        //   DWORD nAvgBytesPerSec;
        //   WORD  nBlockAlign;
        //   WORD  wBitsPerSample;
        //   WORD  cbSize;
        // } WAVEFORMATEX;
        //
        // wFormatTag
        // Identifies the sample format; see the documentation for format tags.
        // nChannels
        // Channel count, such as 2 for stereo.
        // nSamplesPerSec
        // Sample rate in Hz.
        // nAvgBytesPerSec
        // Average bytes of audio data per second.
        // nBlockAlign
        // Bytes per complete audio frame for PCM or IEEE float audio.
        // wBitsPerSample
        // Bits used to store each channel's sample.
        // cbSize
        // Number of extra format bytes following this header.
        WAVEFORMATEX *mix_format = NULL;
        if (SUCCEEDED(result)) {
            result = IAudioClient_GetMixFormat(audio_client, &mix_format);
        }

        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
        // HRESULT Initialize(
        //   AUDCLNT_SHAREMODE  ShareMode,
        //   DWORD             StreamFlags,
        //   REFERENCE_TIME    hnsBufferDuration,
        //   REFERENCE_TIME    hnsPeriodicity,
        //   const WAVEFORMATEX *pFormat,
        //   LPCGUID           AudioSessionGuid
        // );
        //
        // ShareMode
        // AUDCLNT_SHAREMODE_SHARED shares the endpoint with other applications.
        // Loopback requires shared mode.
        //
        // StreamFlags
        // AUDCLNT_STREAMFLAGS_LOOPBACK captures the playback endpoint's mix.
        //
        // hnsBufferDuration
        // Requested buffer duration in 100-nanosecond units. Zero requests
        // the minimum buffer size required by the audio engine.
        //
        // hnsPeriodicity
        // Requested device period. Must be zero in shared mode.
        //
        // pFormat
        // The complete format description returned by GetMixFormat.
        //
        // AudioSessionGuid
        // Identifies an audio session. NULL uses the default session GUID.
        if (SUCCEEDED(result)) {
            result = IAudioClient_Initialize(audio_client, AUDCLNT_SHAREMODE_SHARED,
                                            AUDCLNT_STREAMFLAGS_LOOPBACK,
                                            0, 0, mix_format, NULL);
        }

        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getservice
        // HRESULT GetService(
        //   REFIID riid,
        //   void   **ppv
        // );
        //
        // riid
        // IID_IAudioCaptureClient requests access to captured audio packets.
        // Call GetService after the audio stream has been initialized.
        //
        // ppv
        // Receives the capture interface. Release it before the audio client.
        IAudioCaptureClient *capture_client = NULL;
        if (SUCCEEDED(result)) {
            result = IAudioClient_GetService(audio_client, &IID_IAudioCaptureClient,
                                            (void **)&capture_client);
        }

        if (SUCCEEDED(result)) {
            printf("\nLoopback capture stream initialized. Cleaning up.\n");
        } else {
            printf("Loopback setup failed (HRESULT 0x%08lX).\n",
                   (unsigned long)result);
            status = EXIT_FAILURE;
        }

        // The stream is initialized but has not been started, so no Stop is needed.
        // Release the capture service before its parent audio client.
        if (capture_client != NULL) {
            IAudioCaptureClient_Release(capture_client);
        }
        CoTaskMemFree(mix_format);
        if (audio_client != NULL) {
            IAudioClient_Release(audio_client);
        }
    }

    CoTaskMemFree(source_endpoint_id);
    PropVariantClear(&source_name);
    IPropertyStore_Release(source_properties);

    IMMDevice_Release(source_device);
    IMMDeviceCollection_Release(devices);
    IMMDeviceEnumerator_Release(enumerator);

    // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-couninitialize
    // void CoUninitialize();
    //
    // Balances this thread's successful CoInitializeEx call after its COM
    // interfaces have been released.
    CoUninitialize();

    return status;
}
