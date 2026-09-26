#pragma once
#include <string>
#include <vector>

namespace psm::Log
{
    // printf-style. Lines are buffered until Claim(); after that they go to
    // <base>.log next to the plugin. A copy of recent lines is always kept in
    // memory either way.
    void Write(const char* level, const char* fmt, ...);

    // `base` names the file and its archives: "PrivateStorageMaster" gives
    // PrivateStorageMaster.log and PrivateStorageMaster.01.log upwards.
    //
    // It is a parameter because two processes load this plugin. Session one
    // put both of them in one file: lines from each landed at the other's file
    // offset, one was cut in half and three vanished, and the result read as
    // three hooks failing when all five had installed. Whoever is not the game
    // gets its own name and never touches the real log.
    void Claim(const wchar_t* base);
    // Deletes archived sessions past the newest `keep`. Claim keeps them all
    // because it runs before the ini is read; the caller prunes once it knows
    // DebugLog is off.
    void Prune(const wchar_t* base, int keep);
    bool Claimed();
    void Shutdown();
    void Snapshot(std::vector<std::string>& out, int maxLines);

    // DebugLog=0 keeps only notes and errors. Everything logged with LOG or
    // LOG_OK is dropped before it is formatted.
    void SetDebug(bool on);
    // The text of the last error logged, "" when none.
    void LastError(char* out, size_t cap);
    bool Debug();
}

#define LOG(...)     do { if (::psm::Log::Debug()) ::psm::Log::Write("info ", __VA_ARGS__); } while (0)
#define LOG_OK(...)  do { if (::psm::Log::Debug()) ::psm::Log::Write("ok   ", __VA_ARGS__); } while (0)
#define LOG_NOTE(...) ::psm::Log::Write("note ", __VA_ARGS__)
#define LOG_ERR(...) ::psm::Log::Write("error", __VA_ARGS__)
