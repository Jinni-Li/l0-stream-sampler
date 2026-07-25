#include "OneSparseSketch.hpp"

OneSparseSketch::OneSparseSketch(): phi_(0), iota_(0), fingerprint_(0) {}

void OneSparseSketch::update(std::int64_t item_id, std::int64_t delta){
    phi_ += delta;
    iota_ += item_id * delta;

    std::uint64_t item_power = mod_pow(Z, static_cast<std::uint64_t>(item_id));
    std::uint64_t delta_mod = mod_from_int(delta);

    fingerprint_ = (fingerprint_ + (delta_mod * item_power) %PRIME) %PRIME;
}

OneSparseRecoveryResult OneSparseSketch::recover() const{
    if (empty())
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::Empty,
            std::nullopt
        };
    }

    if (phi_==0)
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::InvalidCandidate,
            std::nullopt
        };
    }
    
    if (iota_%phi_ != 0)
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::InvalidCandidate,
            std::nullopt
        };
    }

    const std::int64_t candidate = iota_/phi_;

    if (candidate < 0)
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::InvalidCandidate,
            std::nullopt
        };
    }


    std::uint64_t candidate_power = mod_pow(Z, static_cast<std::uint64_t>(candidate));

    const std::uint64_t phi_mod = mod_from_int(phi_);

    std::uint64_t expected_fingerprint = (phi_mod * candidate_power) % PRIME;

    if (fingerprint_ == expected_fingerprint)
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::Success,
            RecoveredItem{candidate, phi_}
        };
    }

    return OneSparseRecoveryResult{
        RecoveryStatus::FingerprintMismatch,
        std::nullopt
    };
}


bool OneSparseSketch::empty()const{
    return phi_ == 0 && iota_ == 0 && fingerprint_ == 0;
}

std::uint64_t OneSparseSketch::mod_pow(std::uint64_t base, std::uint64_t exponent){
    std::uint64_t result = 1;
    base %= PRIME;

    while (exponent > 0)
    {
        if (exponent%2 == 1)
        {
            result = (result * base) % PRIME;
        }
        
        base = (base*base) % PRIME;
        exponent /= 2;
    }

    return result;
}

std::uint64_t OneSparseSketch::mod_from_int(std::int64_t value){
    std::int64_t mod = static_cast<std::int64_t>(PRIME);
    std::int64_t result = value % mod;

    if (result < 0)
    {
        result += mod;
    }

    return static_cast<std::uint64_t>(result);
    
}