#include <Windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void) {
    int status = EXIT_SUCCESS;

    printf("audioRouter starting.\n");


    // https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutgetnumdevs
    // UINT waveOutGetNumDevs();

    uint32_t output_count = (uint32_t) waveOutGetNumDevs();
    printf("\nPlayback devices (%u):\n", output_count);
    for (uint32_t id = 0; id < output_count; ++id) {

        // https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveoutcapsa
        // typedef struct tagWAVEOUTCAPSA {
        //   WORD      wMid;
        //   WORD      wPid;
        //   MMVERSION vDriverVersion;
        //   CHAR      szPname[MAXPNAMELEN];
        //   DWORD     dwFormats;
        //   WORD      wChannels;
        //   WORD      wReserved1;
        //   DWORD     dwSupport;
        // } WAVEOUTCAPSA, *PWAVEOUTCAPSA, *NPWAVEOUTCAPSA, *LPWAVEOUTCAPSA;
        // wMid
        // 
        // Manufacturer identifier for the device driver for the device. Manufacturer identifiers are defined in Manufacturer and Product Identifiers.
        // 
        // wPid
        // 
        // Product identifier for the device. Product identifiers are defined in Manufacturer and Product Identifiers.
        // 
        // vDriverVersion
        // 
        // Version number of the device driver for the device. The high-order byte is the major version number, and the low-order byte is the minor version number.
        // 
        // szPname[MAXPNAMELEN]
        // 
        // Product name in a null-terminated string.
        // 
        // dwFormats
        // 
        // Standard formats that are supported.


        WAVEOUTCAPSA device;

        // https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutgetdevcapsa
        // MMRESULT waveOutGetDevCapsA(
        //   UINT_PTR       uDeviceID,
        //   LPWAVEOUTCAPSA pwoc,
        //   UINT           cbwoc
        // );
        // 
        // uDeviceID
        //
        // Identifier of the waveform-audio output device. It can be either a device identifier or a handle of an open waveform-audio output device.
        //
        // pwoc
        //
        // Pointer to a WAVEOUTCAPSA structure to be filled with information about the capabilities of the device.
        //
        // cbwoc
        //
        // Size, in bytes, of the WAVEOUTCAPSA structure.

        MMRESULT result = waveOutGetDevCapsA(id, &device, sizeof(device));
        if (result != MMSYSERR_NOERROR) {
            printf("Could not read playback device %u (error %u).\n",
                    id, result);
            status = EXIT_FAILURE;
            continue;
        }
        printf("  %u: %s\n", id, device.szPname);
    }

    printf("Identify by number which playback device(s) you wish to split audio to. WARNING: Do not select the primary playback device used by Windows.\n");

    char input[256] = {0};

    uint32_t selected_device_id = 0;

    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("Error reading input.\n");
        status = EXIT_FAILURE;
    } else {
        // Remove newline character if present
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        // Process the input as needed

        selected_device_id = (uint32_t) strtoul(input, NULL, 10);
        if (selected_device_id >= output_count) {
            printf("Invalid device ID selected.\n");
            status = EXIT_FAILURE;
        }
    }
    printf("Selected device ID: %u\n", selected_device_id);



    return status;
}
