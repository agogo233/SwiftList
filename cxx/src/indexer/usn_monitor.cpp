#include "indexer/usn_monitor.h"

#include <chrono>
#include <thread>

namespace swiftlist::indexer {

UsnMonitor::UsnMonitor(index_v2::LiveIndex& live) : live_(live) {}

UsnMonitor::~UsnMonitor() {
    Stop();
}

bool UsnMonitor::Start(wchar_t driveLetter) {
    if (running_) return true;

    if (!volume_.Open(driveLetter)) return false;
    driveLetter_ = driveLetter;

    if (!EnsureJournal()) {
        volume_.Close();
        return false;
    }

    lastReadUsn_ = journal_.NextUsn;
    running_ = true;
    thread_ = std::thread([this] { PollLoop(); });
    return true;
}

void UsnMonitor::Stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    volume_.Close();
}

void UsnMonitor::PollLoop() {
    while (running_) {
        if (!volume_.IsOpen()) {
            if (!volume_.Open(driveLetter_)) {
                if (!running_) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs_));
                continue;
            }
            if (!EnsureJournal()) break;
            lastReadUsn_ = journal_.NextUsn;
        }

        // Note: kReasonClose is intentionally excluded. Newly created files may
        // only emit a Close record after write completion; they will be indexed
        // on the next BasicInfoChange or when the MFT scanner runs again.
        uint32_t reasonMask = kReasonFileCreate | kReasonFileDelete |
                               kReasonRenameNewName | kReasonRenameOldName |
                               kReasonBasicInfoChange;

        bool ok = ReadUsnJournal(volume_.Get(), journal_, lastReadUsn_,
                                  reasonMask, [this](const UsnRecord& rec) {
            lastReadUsn_ = rec.Usn + 1;
            live_.Mutate([&](index_v2::Snapshot& /*snap*/, index_v2::DeltaOverlay& delta) {
                if (rec.Reason & (kReasonFileCreate | kReasonRenameNewName)) {
                    index_v2::FileRecordInput input;
                    input.Id = rec.FileReferenceNumber;
                    input.ParentId = rec.ParentFileReferenceNumber;
                    input.Name = rec.FileName;
                    if (rec.FileAttributes & kFileAttrDirectory) {
                        input.Flags = index_v2::FileRecordFlags::Directory;
                    }
                    delta.Upsert(std::move(input));
                }
                if (rec.Reason & (kReasonFileDelete | kReasonRenameOldName)) {
                    delta.Remove(rec.FileReferenceNumber);
                }
            });
        });

        if (!ok) {
            journal_ = {};
            lastReadUsn_ = 0;
            volume_.Close();
            continue;
        }

        int64_t targetUsn = journal_.NextUsn;
        QueryUsnJournal(volume_.Get(), journal_);

        if (journal_.NextUsn == targetUsn) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs_));
        }
    }
}

bool UsnMonitor::EnsureJournal() {
    if (!QueryUsnJournal(volume_.Get(), journal_)) {
        if (!CreateUsnJournal(volume_.Get())) return false;
        if (!QueryUsnJournal(volume_.Get(), journal_)) return false;
    }
    return true;
}

} // namespace swiftlist::indexer
