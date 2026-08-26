#include "sound_runtime.hpp"

#include <array>
#include <cassert>

int main() {
    const std::array<opentony::assets::SoundDescription, 3> descriptions{{
        {0xa1, "rollconcrete2", 1},
        {0xa2, "drip3", 0},
        {-1, "", 0},
    }};
    opentony::assets::SoundBankRuntime bank;
    bank.build(descriptions);
    assert(bank.descriptions().size() == 2);
    assert(bank.resource_path(0xa1) == "audio/rollconcrete2.wav");
    assert(bank.description_for_sound(0xa2).has_value());
    assert(!bank.description_for_sound(0xff).has_value());

    const std::size_t first = bank.publish_loaded_sound(0);
    assert(first == 0);
    assert(bank.publish_loaded_sound(0) == first);
    assert(bank.slot(first).allocation_size() == 0x28);
    assert(bank.slot(first).state_flags() == 0);
    bank.mark_started(first);
    assert((bank.slot(first).state_flags() & 0x2U) != 0U);
    bank.mark_stopped(first);
    assert(bank.slot(first).state_flags() == 0);

    const std::size_t second = bank.publish_loaded_sound(1);
    bank.mark_started(second);
    assert(bank.slot(second).state_flags() == 0);
    return 0;
}
