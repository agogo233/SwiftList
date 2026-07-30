#pragma once

#include "indexer/journal_reader.h"
#include "indexer/usn_record_parser.h"
#include "indexer/volume_helper.h"
#include "index_v2/live_index.h"

#include <atomic>
#include <thread>

namespace swiftlist::indexer {

class UsnMonitor {
public:
    explicit UsnMonitor(index_v2::LiveIndex& live);
    ~UsnMonitor();

    UsnMonitor(const UsnMonitor&) = delete;
    UsnMonitor& operator=(const UsnMonitor&) = delete;

    bool Start(wchar_t driveLetter);
    void Stop();
    bool IsRunning() const { return running_; }

    void SetPollInterval(int ms) { pollIntervalMs_ = ms; }

private:
    void PollLoop();
    bool EnsureJournal();

    index_v2::LiveIndex& live_;
    VolumeHandle volume_;
    wchar_t driveLetter_ = 0;
    UsnJournalData journal_ = {};
    int64_t lastReadUsn_ = 0;
    int pollIntervalMs_ = 500;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace swiftlist::indexer
