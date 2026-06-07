#include "rollforge_storage.h"
#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <stdio.h>

bool rollforge_save_sig(const RfSig* sig, uint32_t frequency, const char* filename_hint) {
    if(!sig || sig->count == 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/subghz");
    storage_simply_mkdir(storage, RF_SAVE_DIR);

    // Dateiname: rollforge/<hint>_HHMMSS.sub
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char path[128];
    snprintf(path, sizeof(path), "%s/%s_%02u%02u%02u.sub",
        RF_SAVE_DIR,
        filename_hint ? filename_hint : "capture",
        dt.hour, dt.minute, dt.second);

    File* f = storage_file_alloc(storage);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        // Flipper SubGhz RAW Format
        char hdr[256];
        int n = snprintf(hdr, sizeof(hdr),
            "Filetype: Flipper SubGhz RAW File\n"
            "Version: 1\n"
            "Frequency: %lu\n"
            "Preset: FuriHalSubGhzPresetOok650Async\n"
            "Protocol: RAW\n"
            "RAW_Data: ",
            (unsigned long)frequency);
        storage_file_write(f, hdr, (size_t)n);

        // RAW Data: positive=HIGH(level=1), negative=LOW. Werte als ASCII.
        // Format: "350 -300 700 -650 ..."
        char buf[24];
        size_t count = sig->count;
        for(size_t i = 0; i < count; i++) {
            uint32_t packed = sig->buf[i];
            bool     level  = (packed & 0x80000000UL) != 0;
            int32_t  dur    = (int32_t)(packed & 0x7FFFFFFFUL);
            if(!level) dur = -dur;
            n = snprintf(buf, sizeof(buf), (i == 0) ? "%ld" : " %ld", (long)dur);
            storage_file_write(f, buf, (size_t)n);
        }
        storage_file_write(f, "\n", 1);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
