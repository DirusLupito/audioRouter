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

typedef struct {
    IAudioClient *client;
    IAudioCaptureClient *capture;
    WAVEFORMATEX *format;
    BOOL started;
} CaptureStream;

typedef struct {
    IAudioClient *client;
    IAudioRenderClient *render;
    UINT32 buffer_frames;
    BYTE *queue;
    UINT32 capacity;
    UINT32 queued;
    BOOL started;
} PlaybackStream;

static volatile LONG stop_requested = 0;

static BOOL WINAPI handle_console_signal(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        // Windows invokes this handler on another thread; only signal main.
        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedexchange
        // LONG InterlockedExchange(
        //   LONG volatile *Target,
        //   LONG          Value
        // );
        //
        // Target
        // Points to the shared 32-bit value to update atomically.
        //
        // Value
        // The replacement value. Here, 1 requests that forwarding stop.
        //
        // Returns the previous value. We do not need it when setting the flag.
        InterlockedExchange(&stop_requested, 1);
        return TRUE;
    }
    return FALSE;
}

static IMMDeviceEnumerator *initialize_audio(void) {
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

    return enumerator;
}

static IMMDeviceCollection *list_playback_devices(IMMDeviceEnumerator *enumerator,
                                                 UINT *device_count) {
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

    *device_count = output_count;
    return devices;
}

static int select_playback_devices(IMMDeviceCollection *devices, UINT output_count,
                                   uint64_t *selection) {
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

    *selection = selected_devices;
    return status;
}

static IMMDevice *get_default_playback_device(IMMDeviceEnumerator *enumerator) {
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

    // Read the source's name and endpoint ID using the calls documented in
    // the listing and selection functions.
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

    CoTaskMemFree(source_endpoint_id);
    PropVariantClear(&source_name);
    IPropertyStore_Release(source_properties);

    return source_device;
}

static int validate_destinations(IMMDeviceCollection *devices, UINT output_count,
                                 uint64_t selected_devices, IMMDevice *source_device) {
    int status = EXIT_SUCCESS;
    LPWSTR source_endpoint_id = NULL;
    IMMDevice_GetId(source_device, &source_endpoint_id);

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

    CoTaskMemFree(source_endpoint_id);
    return status;
}

static HRESULT initialize_loopback(IMMDevice *source_device, CaptureStream *stream) {

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
    HRESULT result = IMMDevice_Activate(source_device, &IID_IAudioClient,
                                        CLSCTX_ALL, NULL,
                                        (void **)&stream->client);

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
    if (SUCCEEDED(result)) {
        result = IAudioClient_GetMixFormat(stream->client, &stream->format);
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
    // Requested buffer duration in 100-nanosecond units. 1000000 is 100 ms,
    // giving the polling loop some room for scheduling delays.
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
        result = IAudioClient_Initialize(stream->client, AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_LOOPBACK,
                                        1000000, 0, stream->format, NULL);
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
    if (SUCCEEDED(result)) {
        result = IAudioClient_GetService(stream->client, &IID_IAudioCaptureClient,
                                        (void **)&stream->capture);
    }

    if (SUCCEEDED(result)) {
        printf("\nLoopback capture ready: %lu Hz, %u channels.\n",
               stream->format->nSamplesPerSec, stream->format->nChannels);
    }
    return result;
}

static HRESULT initialize_playback(IMMDeviceCollection *devices, UINT output_count,
                                   uint64_t selected_devices, const WAVEFORMATEX *format,
                                   PlaybackStream *streams, UINT *stream_count) {
    uint64_t remaining_devices = selected_devices;
    for (UINT index = 0; index < output_count && remaining_devices != 0;
         ++index, remaining_devices /= 2) {
        if (remaining_devices % 2 == 0) {
            continue;
        }

        // Count this slot before setup so cleanup also handles partial failure.
        PlaybackStream *stream = &streams[(*stream_count)++];
        IMMDevice *destination = NULL;
        HRESULT result = IMMDeviceCollection_Item(devices, index, &destination);
        if (SUCCEEDED(result)) {
            result = IMMDevice_Activate(destination, &IID_IAudioClient,
                                       CLSCTX_ALL, NULL, (void **)&stream->client);
        }

        // https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants
        // AUTOCONVERTPCM converts sample rate and channel layout as needed.
        // SRC_DEFAULT_QUALITY selects the higher quality sample-rate converter.
        // All our buffers use the source format; Windows converts at each output.
        if (SUCCEEDED(result)) {
            result = IAudioClient_Initialize(stream->client, AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                1000000, 0, format, NULL);
        }
        if (SUCCEEDED(result)) {
            result = IAudioClient_GetService(stream->client, &IID_IAudioRenderClient,
                                            (void **)&stream->render);
        }

        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getbuffersize
        // HRESULT GetBufferSize(UINT32 *pNumBufferFrames);
        // pNumBufferFrames
        // Receives the actual playback buffer capacity in audio frames.
        if (SUCCEEDED(result)) {
            result = IAudioClient_GetBufferSize(stream->client, &stream->buffer_frames);
        }
        if (SUCCEEDED(result)) {
            // Each destination holds at most another 100 ms of pending audio.
            stream->capacity = format->nSamplesPerSec / 10;
            stream->queue = malloc((size_t)stream->capacity * format->nBlockAlign);
            if (stream->queue == NULL) {
                result = E_OUTOFMEMORY;
            }
        }
        if (destination != NULL) {
            IMMDevice_Release(destination);
        }
        if (FAILED(result)) {
            printf("Playback setup failed for device %u.\n", index + 1);
            return result;
        }
        printf("Playback stream ready for device %u.\n", index + 1);
    }
    return S_OK;
}

// A small linear queue keeps the copying simple. Keep the newest audio if full.
static void queue_audio(PlaybackStream *stream, const BYTE *data, UINT32 frames,
                        const WAVEFORMATEX *format, BOOL silent) {
    size_t frame_bytes = format->nBlockAlign;
    if (frames >= stream->capacity) {
        if (!silent) {
            data += (size_t)(frames - stream->capacity) * frame_bytes;
        }
        frames = stream->capacity;
        stream->queued = 0;
    } else if (frames > stream->capacity - stream->queued) {
        UINT32 discard = frames - (stream->capacity - stream->queued);
        stream->queued -= discard;
        memmove(stream->queue, stream->queue + (size_t)discard * frame_bytes,
                (size_t)stream->queued * frame_bytes);
    }

    BYTE *tail = stream->queue + (size_t)stream->queued * frame_bytes;
    if (silent) {
        // Uncompressed 8-bit PCM is unsigned; other PCM/float silence is zero.
        memset(tail, format->wBitsPerSample == 8 ? 128 : 0,
               (size_t)frames * frame_bytes);
    } else {
        memcpy(tail, data, (size_t)frames * frame_bytes);
    }
    stream->queued += frames;
}

static HRESULT capture_audio(CaptureStream *source, PlaybackStream *outputs,
                             UINT output_count) {
    // https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedcompareexchange
    // LONG InterlockedCompareExchange(
    //   LONG volatile *Destination,
    //   LONG          ExChange,
    //   LONG          Comperand
    // );
    //
    // Destination
    // Points to the shared 32-bit value to access atomically.
    //
    // ExChange
    // The value to store if the current value matches Comperand.
    //
    // Comperand
    // The value to compare with the current value of Destination.
    //
    // Returns the value observed before the operation. Passing 0 for both
    // ExChange and Comperand leaves the flag's value unchanged: 0 stays 0,
    // and a nonzero value is not replaced. This acts as an atomic read paired
    // with the console handler's InterlockedExchange write.
    while (!InterlockedCompareExchange(&stop_requested, 0, 0)) {
        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer
        // HRESULT GetBuffer(
        //   BYTE **ppData, UINT32 *pNumFramesToRead, DWORD *pdwFlags,
        //   UINT64 *pu64DevicePosition, UINT64 *pu64QPCPosition
        // );
        // ppData
        // Receives a packet pointer, valid until ReleaseBuffer.
        // pNumFramesToRead
        // Receives its frame count; an empty capture buffer has no frames.
        // pdwFlags
        // SILENT means the packet represents silence; its pointer need not be read.
        // pu64DevicePosition, pu64QPCPosition
        // Optional timestamps. NULL skips them in this basic forwarding loop.
        BYTE *data = NULL;
        UINT32 frames = 0;
        DWORD flags = 0;
        HRESULT result = IAudioCaptureClient_GetBuffer(source->capture, &data,
                                                       &frames, &flags, NULL, NULL);
        if (FAILED(result) || frames == 0) {
            return result;
        }

        for (UINT i = 0; i < output_count; ++i) {
            queue_audio(&outputs[i], data, frames, source->format,
                        (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
        }

        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-releasebuffer
        // HRESULT ReleaseBuffer(UINT32 NumFramesRead);
        // NumFramesRead
        // Release the entire packet after making our own copies for every output.
        result = IAudioCaptureClient_ReleaseBuffer(source->capture, frames);
        if (FAILED(result)) {
            return result;
        }
    }
    return S_OK;
}

static HRESULT play_queued_audio(PlaybackStream *stream, WORD frame_bytes) {
    if (stream->queued == 0) {
        return S_OK;
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-getcurrentpadding
    // HRESULT GetCurrentPadding(UINT32 *pNumPaddingFrames);
    // pNumPaddingFrames
    // Receives the number of frames already queued in the WASAPI playback buffer.
    UINT32 padding = 0;
    HRESULT result = IAudioClient_GetCurrentPadding(stream->client, &padding);
    if (FAILED(result)) {
        return result;
    }

    UINT32 frames = stream->buffer_frames - padding;
    if (frames > stream->queued) {
        frames = stream->queued;
    }
    if (frames == 0) {
        return S_OK;
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiorenderclient-getbuffer
    // HRESULT GetBuffer(UINT32 NumFramesRequested, BYTE **ppData);
    // NumFramesRequested
    // Number of frames to write, no larger than the available playback space.
    // ppData
    // Receives a writable buffer owned by WASAPI.
    BYTE *data = NULL;
    result = IAudioRenderClient_GetBuffer(stream->render, frames, &data);
    if (FAILED(result)) {
        return result;
    }
    memcpy(data, stream->queue, (size_t)frames * frame_bytes);

    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiorenderclient-releasebuffer
    // HRESULT ReleaseBuffer(UINT32 NumFramesWritten, DWORD dwFlags);
    // NumFramesWritten
    // Number of frames to submit for playback.
    // dwFlags
    // Zero submits the copied samples; SILENT submits silence instead.
    result = IAudioRenderClient_ReleaseBuffer(stream->render, frames, 0);
    if (SUCCEEDED(result)) {
        stream->queued -= frames;
        memmove(stream->queue, stream->queue + (size_t)frames * frame_bytes,
                (size_t)stream->queued * frame_bytes);
    }
    return result;
}

static HRESULT forward_audio(CaptureStream *source, PlaybackStream *outputs,
                             UINT output_count) {
    InterlockedExchange(&stop_requested, 0);
    // https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler
    // BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add);
    // HandlerRoutine
    // Called by Windows on Ctrl+C or Ctrl+Break; ours signals the polling loop.
    // Add
    // TRUE registers the handler; FALSE removes it after stream cleanup.
    if (!SetConsoleCtrlHandler(handle_console_signal, TRUE)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT result = S_OK;
    for (UINT i = 0; i < output_count; ++i) {
        // Prime each output with silence to give the polling loop some headroom.
        BYTE *data = NULL;
        result = IAudioRenderClient_GetBuffer(outputs[i].render,
                                             outputs[i].buffer_frames, &data);
        if (SUCCEEDED(result)) {
            result = IAudioRenderClient_ReleaseBuffer(outputs[i].render,
                outputs[i].buffer_frames, AUDCLNT_BUFFERFLAGS_SILENT);
        }
        // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-start
        // HRESULT Start();
        // Starts processing audio for an initialized stream.
        if (SUCCEEDED(result)) {
            result = IAudioClient_Start(outputs[i].client);
        }
        if (FAILED(result)) {
            return result;
        }
        outputs[i].started = TRUE;
    }

    result = IAudioClient_Start(source->client);
    if (FAILED(result)) {
        return result;
    }
    source->started = TRUE;
    printf("Forwarding audio. Press Ctrl+C to stop.\n");
    fflush(stdout);

    while (!InterlockedCompareExchange(&stop_requested, 0, 0)) {
        result = capture_audio(source, outputs, output_count);
        if (FAILED(result)) {
            return result;
        }
        for (UINT i = 0; i < output_count; ++i) {
            result = play_queued_audio(&outputs[i], source->format->nBlockAlign);
            if (FAILED(result)) {
                return result;
            }
        }

        // Sleep for a short time to avoid busy-waiting. 
        Sleep(1);
    }
    printf("\nStopping audio forwarding.\n");
    return S_OK;
}

static void cleanup_streams(CaptureStream *source, PlaybackStream *outputs,
                            UINT output_count) {
    // https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclient-stop
    // HRESULT Stop();
    // Stops processing a started stream. Release its service before its client.
    if (source->started) {
        IAudioClient_Stop(source->client);
    }
    for (UINT i = 0; i < output_count; ++i) {
        if (outputs[i].started) {
            IAudioClient_Stop(outputs[i].client);
        }
        if (outputs[i].render != NULL) {
            IAudioRenderClient_Release(outputs[i].render);
        }
        if (outputs[i].client != NULL) {
            IAudioClient_Release(outputs[i].client);
        }
        free(outputs[i].queue);
    }
    if (source->capture != NULL) {
        IAudioCaptureClient_Release(source->capture);
    }
    if (source->client != NULL) {
        IAudioClient_Release(source->client);
    }
    CoTaskMemFree(source->format);
    SetConsoleCtrlHandler(handle_console_signal, FALSE);
}

static void cleanup_audio(IMMDeviceEnumerator *enumerator,
                          IMMDeviceCollection *devices, IMMDevice *source_device) {
    IMMDevice_Release(source_device);
    IMMDeviceCollection_Release(devices);
    IMMDeviceEnumerator_Release(enumerator);

    // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-couninitialize
    // void CoUninitialize();
    //
    // Balances this thread's successful CoInitializeEx call after its COM
    // interfaces have been released.
    CoUninitialize();
}

int main(void) {
    printf("audioRouter starting.\n");

    IMMDeviceEnumerator *enumerator = initialize_audio();
    UINT output_count = 0;
    IMMDeviceCollection *devices = list_playback_devices(enumerator, &output_count);

    uint64_t selected_devices = 0;
    int status = select_playback_devices(devices, output_count, &selected_devices);

    IMMDevice *source_device = get_default_playback_device(enumerator);
    if (status == EXIT_SUCCESS) {
        status = validate_destinations(devices, output_count, selected_devices, source_device);
    }

    CaptureStream source = {0};
    PlaybackStream outputs[64] = {0};
    UINT stream_count = 0;
    HRESULT result = S_OK;
    if (status == EXIT_SUCCESS && selected_devices != 0) {
        result = initialize_loopback(source_device, &source);
        if (SUCCEEDED(result)) {
            result = initialize_playback(devices, output_count, selected_devices,
                                        source.format, outputs, &stream_count);
        }
        if (SUCCEEDED(result)) {
            result = forward_audio(&source, outputs, stream_count);
        }
        if (FAILED(result)) {
            printf("Audio forwarding failed (HRESULT 0x%08lX).\n", (unsigned long)result);
            status = EXIT_FAILURE;
        }
    }

    cleanup_streams(&source, outputs, stream_count);
    cleanup_audio(enumerator, devices, source_device);
    return status;
}
