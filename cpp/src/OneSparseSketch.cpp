#include "OneSparseSketch.hpp"
#include "HashUtils.hpp"

#include<limits>

OneSparseSketch::OneSparseSketch(std::uint64_t seed): phi_(0), iota_(0), fingerprint_(0), overflowed_(false),
fingerprint_base_(derive_fingerprint_base(seed)) {}

std::uint64_t OneSparseSketch::derive_fingerprint_base(std::uint64_t seed) noexcept{
    const std::uint64_t mixed = hash_utils::splitmix64(seed^FINGERPRINT_SEED_SALT);
    return 2ULL + mixed % (PRIME - 2ULL);
}

std::uint64_t OneSparseSketch::fingerprint_base()const noexcept{
    return fingerprint_base_;
}

void OneSparseSketch::update(std::int64_t item_id, std::int64_t delta){

    if(overflowed_ || delta == 0)
    {
        return;
    }

    const __int128_t delta_128 = static_cast<__int128_t>(delta);
    const __int128_t weighted_update = static_cast<__int128_t>(item_id) * delta_128;

    __int128_t updated_phi = 0;
    __int128_t updated_iota = 0;

    const bool phi_overflow = __builtin_add_overflow(phi_, delta_128, &updated_phi);

    const bool iota_overflow =__builtin_add_overflow(iota_,weighted_update,&updated_iota);

    if (phi_overflow || iota_overflow) {
        overflowed_ = true;
        return;
    }

    phi_ = updated_phi;
    iota_ = updated_iota;

    std::uint64_t item_power = mod_pow(fingerprint_base_, static_cast<std::uint64_t>(item_id));
    std::uint64_t delta_mod = mod_from_int(delta);

    fingerprint_ = (fingerprint_ + (delta_mod * item_power) %PRIME) %PRIME;
}

bool OneSparseSketch::fits_in_int64(__int128_t value) noexcept {
    return
        value >= static_cast<__int128_t>(std::numeric_limits<std::int64_t>::min()) &&
        value <= static_cast<__int128_t>(std::numeric_limits<std::int64_t>::max());
}

OneSparseRecoveryResult OneSparseSketch::recover() const{

    if (overflowed_) {
        return OneSparseRecoveryResult{
            RecoveryStatus::MomentOverflow,
            std::nullopt
        };
    }

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

    const __int128_t candidate_value = iota_/phi_;

    if (!fits_in_int64(candidate_value) ||!fits_in_int64(phi_))
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::InvalidCandidate,
            std::nullopt
        };
    }

    const std::int64_t candidate = static_cast<std::int64_t>(candidate_value);
    const std::int64_t frequency = static_cast<std::int64_t>(phi_);

    if (candidate < 0)
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::InvalidCandidate,
            std::nullopt
        };
    }


    std::uint64_t candidate_power = mod_pow(fingerprint_base_, static_cast<std::uint64_t>(candidate));

    const std::uint64_t phi_mod = mod_from_int(frequency);

    std::uint64_t expected_fingerprint = (phi_mod * candidate_power) % PRIME;

    if (fingerprint_ == expected_fingerprint)
    {
        return OneSparseRecoveryResult{
            RecoveryStatus::Success,
            RecoveredItem{candidate, frequency}
        };
    }

    return OneSparseRecoveryResult{
        RecoveryStatus::FingerprintMismatch,
        std::nullopt
    };
}


bool OneSparseSketch::empty()const{
    return !overflowed_ && phi_ == 0 && iota_ == 0 && fingerprint_ == 0;
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