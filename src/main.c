// Enable the C macros for COM interface calls.
#define COBJMACROS
#include <Windows.h>
// Define the GUID constants declared by the MMDevice and property-key headers.
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
