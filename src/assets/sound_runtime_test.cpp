#include "sound_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"

int main() {
    const std::array<opentony::assets::SoundDescription, 3> descriptions{{
        {0xa1, "rollconcrete2", 1},
        {0xa2, "drip3", 0},
        {-1, "", 0},
    }};
    opentony::assets::SoundBankRuntime bank;
    bank.build(descriptions);
    CHECK(bank.descriptions().size() == 2);
    CHECK(bank.resource_path(0xa1) == "audio/rollconcrete2.wav");
    CHECK(bank.description_for_sound(0xa2).has_value());
    CHECK(!bank.description_for_sound(0xff).has_value());

    const std::size_t first = bank.publish_loaded_sound(0);
    CHECK(first == 0);
    CHECK(bank.publish_loaded_sound(0) == first);
    CHECK(bank.slot(first).allocation_size() == 0x28);
    CHECK(bank.slot(first).state_flags() == 0);
    bank.mark_started(first);
    CHECK((bank.slot(first).state_flags() & 0x2U) != 0U);
    bank.mark_stopped(first);
    CHECK(bank.slot(first).state_flags() == 0);

    const std::size_t second = bank.publish_loaded_sound(1);
    bank.mark_started(second);
    CHECK(bank.slot(second).state_flags() == 0);
    return 0;
}
