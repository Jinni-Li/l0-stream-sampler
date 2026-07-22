#pragma once

#include <cstdint>
#include <optional>
#include <vector>

enum class RecoveryStatus{
    Success,
    Empty,
    TooDense,
    FingerprintMismatch,
    InvalidCandidate,
    IncompleteRecovery,
    RecoveryFailure
};

enum class SampleStatus{
    Success,
    EmptySupport,
    NoRecoverableLevel,
    RecoveryFailure,
    InvalidSample
};

inline const char* to_string(RecoveryStatus status){
    switch (status)
    {
    case RecoveryStatus::Success:
        return "success";
    case RecoveryStatus::Empty:
        return "empty";
    case RecoveryStatus::TooDense:
        return "too_dense";
    case RecoveryStatus::FingerprintMismatch:
        return "fingerprint_mismatch";
    case RecoveryStatus::InvalidCandidate:
        return "invalid_candidate";
    case RecoveryStatus::IncompleteRecovery:
        return "incomplete_recovery";
    case RecoveryStatus::RecoveryFailure:
        return "recovery_failure";
    
    default:
        return "unknown_recovery_status";
    }
}

inline const char* to_string(SampleStatus status){
    switch (status)
    {
    case SampleStatus::Success:
        return "success";
    case SampleStatus::EmptySupport:
        return "empty_support";
    case SampleStatus::NoRecoverableLevel:
        return "no_recoverable_level";
    case SampleStatus::InvalidSample:
        return "invalid_sample";
    case SampleStatus::RecoveryFailure:
        return "recovery_failure";
    
    default:
        return "unknown_sample_status";
    }
}

struct RecoveredItem
{
    std::int64_t item_id;
    std::int64_t frequency;
};


struct OneSparseRecoveryResult
{
    RecoveryStatus status;
    std::optional<RecoveredItem> item;
};

struct SSparseRecoveryResult
{
    RecoveryStatus status;
    std::vector<RecoveredItem> items;
};

struct SampleResult
{
    SampleStatus status;
    std::optional<std::int64_t> item;
    std::vector<std::int64_t> candidates;
};
