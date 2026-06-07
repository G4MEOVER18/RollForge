#pragma once
#include "rollforge_app.h"

#define RF_SAVE_DIR "/ext/subghz/rollforge"

/* Speichert ein RfSig als Flipper SubGHz RAW .sub Datei.
   filename_hint wird zusammen mit Zeitstempel zum Dateinamen.
   Gibt true bei Erfolg zurueck. */
bool rollforge_save_sig(const RfSig* sig, uint32_t frequency, const char* filename_hint);
