#include "long_term_storage.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

namespace {

// One decrypted record, together with its original encrypted file line.
struct StoredRecord {
    std::uint64_t sequenceNumber;
    Event event;
    std::string encryptedLine;
};

// Convert encrypted bytes into hexadecimal text for file storage.
std::string bytesToHex(const std::vector<uint8_t>& bytes)
{
    static const char hexDigits[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(bytes.size() * 2);

    for (uint8_t byte : bytes) {
        result.push_back(hexDigits[byte >> 4]);
        result.push_back(hexDigits[byte & 0x0F]);
    }

    return result;
}

// Convert hexadecimal file text back into encrypted bytes.
std::vector<uint8_t> hexToBytes(const std::string& hex)
{
    if (hex.empty() || hex.size() % 2 != 0) {
        throw std::runtime_error("Invalid encrypted storage record.");
    }

    auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };

    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int high = hexValue(hex[i]);
        int low = hexValue(hex[i + 1]);

        if (high < 0 || low < 0) {
            throw std::runtime_error("Invalid hexadecimal storage record.");
        }

        result.push_back(
            static_cast<uint8_t>((high << 4) | low)
        );
    }

    return result;
}

// Serialize an event, including its unique sequence number.
std::string serializeRecord(
    std::uint64_t sequenceNumber,
    const Event& event
)
{
    std::ostringstream output;

    output << sequenceNumber << ' '
           << std::quoted(eventTypeName(event.type)) << ' '
           << event.adcId << ' '
           << std::quoted(event.timestamp) << ' '
           << event.fields.size();

    for (const auto& field : event.fields) {
        output << ' '
               << std::quoted(field.first) << ' '
               << std::quoted(field.second);
    }

    return output.str();
}

// Deserialize a decrypted event record.
StoredRecord deserializeRecord(const std::string& plaintext)
{
    std::istringstream input(plaintext);

    StoredRecord record{};
    std::string typeName;
    std::size_t fieldCount = 0;

    if (!(input >> record.sequenceNumber
                >> std::quoted(typeName)
                >> record.event.adcId
                >> std::quoted(record.event.timestamp)
                >> fieldCount)) {
        throw std::runtime_error("Invalid decrypted event record.");
    }

    if (record.sequenceNumber == 0 ||
        !timeutil::isValid(record.event.timestamp)) {
        throw std::runtime_error("Invalid event sequence number or timestamp.");
    }

    if (!eventTypeFromName(typeName, record.event.type)) {
        throw std::runtime_error("Unknown event type in storage record.");
    }

    for (std::size_t i = 0; i < fieldCount; ++i) {
        std::string name;
        std::string value;

        if (!(input >> std::quoted(name) >> std::quoted(value))) {
            throw std::runtime_error("Invalid event fields in storage record.");
        }

        record.event.fields[name] = value;
    }

    // Reject unexpected extra data.
    input >> std::ws;

    if (!input.eof()) {
        throw std::runtime_error("Unexpected data in storage record.");
    }

    return record;
}

// Read, decrypt, and validate every record in a storage file.
std::vector<StoredRecord> readRecords(
    const std::string& filename,
    EncryptionManager& encryption
)
{
    std::vector<StoredRecord> records;

    std::ifstream file(filename);

    // A file that does not exist is treated as an empty storage file.
    if (!file.is_open()) {
        if (fs::exists(filename)) {
            throw std::runtime_error("Could not open storage file: " + filename);
        }

        return records;
    }

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) {
            throw std::runtime_error("Empty record found in storage file.");
        }

        std::vector<uint8_t> encryptedBytes = hexToBytes(line);

        std::vector<uint8_t> plaintextBytes =
            encryption.decryptData(encryptedBytes);

        std::string plaintext(
            plaintextBytes.begin(),
            plaintextBytes.end()
        );

        StoredRecord record = deserializeRecord(plaintext);
        record.encryptedLine = line;

        records.push_back(std::move(record));
    }

    if (file.bad()) {
        throw std::runtime_error("Error reading storage file: " + filename);
    }

    return records;
}

// Calculate the date/time exactly 13 calendar months before currentTime.
// If the target month has fewer days, the day is clamped to its last day.
std::string thirteenMonthsAgo(const std::string& currentTime)
{
    if (!timeutil::isValid(currentTime)) {
        throw std::invalid_argument("Invalid archival timestamp.");
    }

    int year, month, day, hour, minute, second;

    if (std::sscanf(
            currentTime.c_str(),
            "%4d-%2d-%2d %2d:%2d:%2d",
            &year, &month, &day, &hour, &minute, &second
        ) != 6) {
        throw std::invalid_argument("Invalid archival timestamp.");
    }

    // Move back 13 calendar months.
    int totalMonths = year * 12 + (month - 1) - 13;

    year = totalMonths / 12;
    month = totalMonths % 12 + 1;

    if (month <= 0) {
        month += 12;
        --year;
    }

    const bool leapYear =
        (year % 400 == 0) ||
        (year % 4 == 0 && year % 100 != 0);

    const int daysInMonth[] = {
        31, leapYear ? 29 : 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (day > daysInMonth[month - 1]) {
        day = daysInMonth[month - 1];
    }

    std::ostringstream result;

    result << std::setfill('0')
           << std::setw(4) << year << '-'
           << std::setw(2) << month << '-'
           << std::setw(2) << day << ' '
           << std::setw(2) << hour << ':'
           << std::setw(2) << minute << ':'
           << std::setw(2) << second;

    return result.str();
}

// Replace the active file using a temporary file and backup.
// The backup allows recovery if the final rename fails.
void replaceFile(
    const std::string& filename,
    const std::vector<std::string>& retainedLines
)
{
    const std::string temporaryFilename = filename + ".tmp";
    const std::string backupFilename = filename + ".bak";

    {
        std::ofstream temporary(
            temporaryFilename,
            std::ios::trunc
        );

        if (!temporary.is_open()) {
            throw std::runtime_error("Could not create temporary storage file.");
        }

        for (const std::string& line : retainedLines) {
            temporary << line << '\n';
        }

        temporary.flush();

        if (!temporary.good()) {
            throw std::runtime_error("Could not write temporary storage file.");
        }
    }

    std::error_code error;

    // Remove an old backup, if one exists.
    fs::remove(backupFilename, error);
    error.clear();

    // Move the original active file to the backup.
    if (fs::exists(filename)) {
        fs::rename(filename, backupFilename, error);

        if (error) {
            throw std::runtime_error("Could not back up active storage file.");
        }
    }

    // Install the new active file.
    fs::rename(temporaryFilename, filename, error);

    if (error) {
        // Attempt to restore the original active file.
        if (fs::exists(backupFilename)) {
            std::error_code restoreError;
            fs::rename(backupFilename, filename, restoreError);
        }

        throw std::runtime_error("Could not replace active storage file.");
    }

    // The new active file is in place, so the backup is no longer needed.
    fs::remove(backupFilename, error);
}

} // End of helper namespace


// Constructor.
FileAuditTrail::FileAuditTrail(
    const std::string& activeFilename,
    const std::string& longTermFilename,
    EncryptionManager& encryption
)
    : activeFilename_(activeFilename),
      longTermFilename_(longTermFilename),
      encryption_(encryption),
      nextSequenceNumber_(1)
{
    fs::path activeParent = fs::path(activeFilename_).parent_path();
fs::path archiveParent = fs::path(longTermFilename_).parent_path();

if (!activeParent.empty()) {
    fs::create_directories(activeParent);
}

if (!archiveParent.empty()) {
    fs::create_directories(archiveParent);
}

    nextSequenceNumber_ = findNextSequenceNumber();
}


// Find the next unused sequence number across both files.
std::uint64_t FileAuditTrail::findNextSequenceNumber() const
{
    auto activeRecords = readRecords(activeFilename_, encryption_);
    auto archivedRecords = readRecords(longTermFilename_, encryption_);

    std::uint64_t maximum = 0;

    for (const auto& record : activeRecords) {
        maximum = std::max(maximum, record.sequenceNumber);
    }

    for (const auto& record : archivedRecords) {
        maximum = std::max(maximum, record.sequenceNumber);
    }

    if (maximum == std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("Storage sequence number limit reached.");
    }

    return maximum + 1;
}


// Append one encrypted event to active storage.
void FileAuditTrail::append(const Event& event)
{
    if (!timeutil::isValid(event.timestamp)) {
        throw std::invalid_argument("Cannot store event with invalid timestamp.");
    }

    const std::string plaintext =
        serializeRecord(nextSequenceNumber_, event);

    std::vector<uint8_t> plaintextBytes(
        plaintext.begin(),
        plaintext.end()
    );

    // Encrypt before writing anything to disk.
    std::vector<uint8_t> encryptedBytes =
        encryption_.encryptData(plaintextBytes);

    const std::string encryptedLine = bytesToHex(encryptedBytes);

    fs::path activeParent = fs::path(activeFilename_).parent_path();

    if (!activeParent.empty()) {
        fs::create_directories(activeParent);
    }

    std::ofstream file(activeFilename_, std::ios::app);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open active storage file.");
    }

    file << encryptedLine << '\n';
    file.flush();

    if (!file.good()) {
        throw std::runtime_error("Could not write active storage record.");
    }

    if (nextSequenceNumber_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("Storage sequence number limit reached.");
    }

    ++nextSequenceNumber_;
}


// Search both active and long-term storage.
std::vector<Event> FileAuditTrail::search(
    const std::string& from,
    const std::string& to
) const
{
    if (!timeutil::isValid(from) ||
        !timeutil::isValid(to) ||
        from > to) {
        throw std::invalid_argument("Invalid timestamp search range.");
    }

    auto activeRecords = readRecords(activeFilename_, encryption_);
    auto archivedRecords = readRecords(longTermFilename_, encryption_);

    std::map<std::uint64_t, Event> uniqueRecords;

    auto collect = [&](const std::vector<StoredRecord>& records) {
        for (const auto& record : records) {
            if (record.event.timestamp >= from &&
                record.event.timestamp <= to) {

                // Sequence numbers prevent duplicate results if a crash
                // occurred during archival.
                uniqueRecords.emplace(
                    record.sequenceNumber,
                    record.event
                );
            }
        }
    };

    collect(activeRecords);
    collect(archivedRecords);

    std::vector<std::pair<std::uint64_t, Event>> ordered(
        uniqueRecords.begin(),
        uniqueRecords.end()
    );

    std::stable_sort(
        ordered.begin(),
        ordered.end(),
        [](const auto& a, const auto& b) {
            if (a.second.timestamp != b.second.timestamp) {
                return a.second.timestamp < b.second.timestamp;
            }

            return a.first < b.first;
        }
    );

    std::vector<Event> results;

    for (const auto& record : ordered) {
        results.push_back(record.second);
    }

    return results;
}


// Move records older than 13 calendar months into long-term storage.
void FileAuditTrail::archiveOlderThan13Months(
    const std::string& currentTime
)
{
    const std::string cutoff = thirteenMonthsAgo(currentTime);

    auto activeRecords = readRecords(activeFilename_, encryption_);
    auto archivedRecords = readRecords(longTermFilename_, encryption_);

    std::set<std::uint64_t> archivedSequenceNumbers;

    for (const auto& record : archivedRecords) {
        archivedSequenceNumbers.insert(record.sequenceNumber);
    }

    std::vector<StoredRecord> recordsToArchive;
    std::vector<std::string> retainedActiveLines;

    for (const auto& record : activeRecords) {
        if (record.event.timestamp < cutoff) {
            recordsToArchive.push_back(record);
        } else {
            retainedActiveLines.push_back(record.encryptedLine);
        }
    }

    if (recordsToArchive.empty()) {
        return;
    }

    // Copy old records to long-term storage first.
    std::ofstream archiveFile(
        longTermFilename_,
        std::ios::app
    );

    if (!archiveFile.is_open()) {
        throw std::runtime_error("Could not open long-term storage file.");
    }

    for (const auto& record : recordsToArchive) {
        // Avoid writing a duplicate if a previous archival attempt
        // copied the record but failed before removing it from active storage.
        if (archivedSequenceNumbers.count(record.sequenceNumber) == 0) {
            archiveFile << record.encryptedLine << '\n';
        }
    }

    archiveFile.flush();

    if (!archiveFile.good()) {
        throw std::runtime_error(
            "Could not copy records to long-term storage. "
            "Active records have been preserved."
        );
    }

    archiveFile.close();

    // Only after the archive copy succeeds do we replace active storage.
    replaceFile(activeFilename_, retainedActiveLines);
}