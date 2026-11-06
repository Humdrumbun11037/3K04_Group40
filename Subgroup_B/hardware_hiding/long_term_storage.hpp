#ifndef LONG_TERM_STORAGE_HPP
#define LONG_TERM_STORAGE_HPP

#include "external_interfaces.hpp"
#include "events_logs/event_formatting.h"
#include "message_processing/encryption.hpp"

#include <cstdint>
#include <string>
#include <vector>

class FileAuditTrail : public AuditTrail {
public:
    FileAuditTrail(
        const std::string& activeFilename,
        const std::string& longTermFilename,
        EncryptionManager& encryption
    );

    void append(const Event& event) override;

    std::vector<Event> search(
        const std::string& from,
        const std::string& to
    ) const override;

    void archiveOlderThan13Months(
        const std::string& currentTime
    ) override;

private:
    std::string activeFilename_;
    std::string longTermFilename_;

    EncryptionManager& encryption_;
    std::uint64_t nextSequenceNumber_;

    std::uint64_t findNextSequenceNumber() const;
};

#endif