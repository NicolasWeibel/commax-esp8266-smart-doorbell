#include "PersistentStore.h"

#include <cstddef>
#include <cstring>

#include "AppConfig.h"

namespace {
constexpr uint16_t kRecordVersion = 4;
constexpr uint16_t kLegacyRecordVersion = 3;
}

bool PersistentStore::begin() {
  generation_ = 0;
  migratedLegacy_ = false;
  LittleFSConfig config;
  config.setAutoFormat(false);
  LittleFS.setConfig(config);
  ready_ = LittleFS.begin();
  return ready_;
}

bool PersistentStore::ready() const {
  return ready_;
}

bool PersistentStore::load(PersistentStateRecord& record) {
  record = PersistentStateRecord{};
  if (!ready_) {
    return false;
  }

  PersistentStateRecord candidate{};
  if (readRecord(AppConfig::kStatePath, candidate) && validate(candidate)) {
    record = candidate;
    generation_ = candidate.generation;
    return true;
  }
  if (readRecord(AppConfig::kStateBackupPath, candidate) &&
      validate(candidate)) {
    record = candidate;
    generation_ = candidate.generation;
    return true;
  }

  PersistentStateRecordV3 legacy{};
  const bool legacyLoaded =
      (readLegacyRecord(AppConfig::kLegacyStatePath, legacy) &&
       validateLegacy(legacy)) ||
      (readLegacyRecord(AppConfig::kLegacyStateBackupPath, legacy) &&
       validateLegacy(legacy));
  if (!legacyLoaded) {
    return false;
  }

  record = PersistentStateRecord{};
  record.generation = legacy.generation;
  record.session = legacy.session;
  std::memcpy(record.jobs, legacy.jobs, sizeof(record.jobs));
  record.offline = PersistentOfflineSummary{};
  generation_ = legacy.generation;
  migratedLegacy_ = true;
  return true;
}

bool PersistentStore::save(PersistentStateRecord& record) {
  if (!ready_) {
    return false;
  }

  record.generation = generation_ + 1U;
  record.version = kRecordVersion;
  record.recordSize = sizeof(PersistentStateRecord);
  record.crc32 = calculateCrc(record);

  if (!writeRecord(record)) {
    return false;
  }
  generation_ = record.generation;
  if (migratedLegacy_) {
    removeLegacyFiles();
    migratedLegacy_ = false;
  }
  return true;
}

bool PersistentStore::clear() {
  if (!ready_) {
    return false;
  }
  bool ok = true;
  const char* paths[] = {
      AppConfig::kStatePath,
      AppConfig::kStateBackupPath,
      AppConfig::kStateTemporaryPath,
      AppConfig::kLegacyStatePath,
      AppConfig::kLegacyStateBackupPath,
      AppConfig::kLegacyStateTemporaryPath,
  };
  for (const char* path : paths) {
    if (LittleFS.exists(path)) {
      ok = LittleFS.remove(path) && ok;
    }
  }
  return ok;
}

bool PersistentStore::migratedLegacy() const {
  return migratedLegacy_;
}

bool PersistentStore::readRecord(const char* path,
                                 PersistentStateRecord& record) const {
  if (!LittleFS.exists(path)) {
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file || file.size() != sizeof(PersistentStateRecord)) {
    if (file) {
      file.close();
    }
    return false;
  }
  const size_t read = file.read(
      reinterpret_cast<uint8_t*>(&record), sizeof(record));
  file.close();
  return read == sizeof(record);
}

bool PersistentStore::readLegacyRecord(
    const char* path,
    PersistentStateRecordV3& record) const {
  if (!LittleFS.exists(path)) {
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file || file.size() != sizeof(PersistentStateRecordV3)) {
    if (file) {
      file.close();
    }
    return false;
  }
  const size_t read = file.read(
      reinterpret_cast<uint8_t*>(&record), sizeof(record));
  file.close();
  return read == sizeof(record);
}

bool PersistentStore::writeRecord(PersistentStateRecord& record) {
  File temporary = LittleFS.open(AppConfig::kStateTemporaryPath, "w");
  if (!temporary) {
    return false;
  }
  const size_t written = temporary.write(
      reinterpret_cast<const uint8_t*>(&record), sizeof(record));
  temporary.flush();
  temporary.close();
  if (written != sizeof(record)) {
    LittleFS.remove(AppConfig::kStateTemporaryPath);
    return false;
  }

  if (LittleFS.exists(AppConfig::kStateBackupPath)) {
    LittleFS.remove(AppConfig::kStateBackupPath);
  }
  if (LittleFS.exists(AppConfig::kStatePath)) {
    if (!LittleFS.rename(AppConfig::kStatePath,
                         AppConfig::kStateBackupPath)) {
      LittleFS.remove(AppConfig::kStateTemporaryPath);
      return false;
    }
  }
  if (!LittleFS.rename(AppConfig::kStateTemporaryPath,
                       AppConfig::kStatePath)) {
    return false;
  }
  return true;
}

bool PersistentStore::validate(const PersistentStateRecord& record) const {
  return record.version == kRecordVersion &&
         record.recordSize == sizeof(PersistentStateRecord) &&
         record.crc32 == calculateCrc(record);
}

bool PersistentStore::validateLegacy(
    const PersistentStateRecordV3& record) const {
  return record.version == kLegacyRecordVersion &&
         record.recordSize == sizeof(PersistentStateRecordV3) &&
         record.crc32 == calculateLegacyCrc(record);
}

uint32_t PersistentStore::calculateCrc(
    const PersistentStateRecord& record) const {
  return calculateCrcBytes(
      reinterpret_cast<const uint8_t*>(&record),
      sizeof(record),
      offsetof(PersistentStateRecord, crc32),
      sizeof(record.crc32));
}

uint32_t PersistentStore::calculateLegacyCrc(
    const PersistentStateRecordV3& record) const {
  return calculateCrcBytes(
      reinterpret_cast<const uint8_t*>(&record),
      sizeof(record),
      offsetof(PersistentStateRecordV3, crc32),
      sizeof(record.crc32));
}

uint32_t PersistentStore::calculateCrcBytes(const uint8_t* data,
                                            size_t length,
                                            size_t crcOffset,
                                            size_t crcSize) const {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    const bool insideCrc = i >= crcOffset && i < crcOffset + crcSize;
    const uint8_t value = insideCrc ? 0U : data[i];
    crc ^= value;
    for (uint8_t bit = 0; bit < 8U; ++bit) {
      const uint32_t mask = -(crc & 1UL);
      crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

void PersistentStore::removeLegacyFiles() {
  const char* paths[] = {
      AppConfig::kLegacyStatePath,
      AppConfig::kLegacyStateBackupPath,
      AppConfig::kLegacyStateTemporaryPath,
  };
  for (const char* path : paths) {
    if (LittleFS.exists(path)) {
      LittleFS.remove(path);
    }
  }
}
