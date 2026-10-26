#pragma once

#include <Arduino.h>
#include <LittleFS.h>

#include "AppTypes.h"

class PersistentStore {
 public:
  bool begin();
  bool ready() const;
  bool load(PersistentStateRecord& record);
  bool save(PersistentStateRecord& record);
  bool clear();
  bool migratedLegacy() const;

 private:
  bool readRecord(const char* path, PersistentStateRecord& record) const;
  bool readLegacyRecord(const char* path,
                        PersistentStateRecordV3& record) const;
  bool writeRecord(PersistentStateRecord& record);
  bool validate(const PersistentStateRecord& record) const;
  bool validateLegacy(const PersistentStateRecordV3& record) const;
  uint32_t calculateCrc(const PersistentStateRecord& record) const;
  uint32_t calculateLegacyCrc(const PersistentStateRecordV3& record) const;
  uint32_t calculateCrcBytes(const uint8_t* data,
                             size_t length,
                             size_t crcOffset,
                             size_t crcSize) const;
  void removeLegacyFiles();

  bool ready_ = false;
  bool migratedLegacy_ = false;
  uint32_t generation_ = 0;
};
