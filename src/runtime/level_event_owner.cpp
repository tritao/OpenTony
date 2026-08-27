#include "level_event_owner.hpp"

namespace opentony::runtime {
namespace {

[[nodiscard]] bool player_is_eligible(
    const PlayerState& player,
    const trg::TriggerLevelEventInputs& inputs) noexcept {
    // 0x00469a30 accepts mode 7 unconditionally; the ordinary path checks
    // the live skater +0x2dd4 word. Keep the field-offset name at PlayerState.
    return inputs.game_mode == 7 || player.level_event_field_2dd4() == 0;
}

} // namespace

trg::TriggerLevelEventFrameInput LevelEventGameplayOwner::frame_input(
    const trg::TriggerLevelEventInputs& inputs,
    bool mode7_input_active) const noexcept {
    trg::TriggerLevelEventFrameInput input{};
    input.players_eligible = player_is_eligible(primary_, inputs);
    input.secondary_present = secondary_ != nullptr;
    input.secondary_eligible = secondary_ == nullptr
        || player_is_eligible(*secondary_, inputs);
    input.primary_state7 = primary_.physics_state() == 7;
    input.primary_animation_state = primary_.animation_state();
    input.primary_animation_flag_107 = primary_.level_event_field_107() != 0;
    input.primary_pending_score = primary_.level_event_field_2a8();
    input.primary_score_input_active = primary_score_input_active_;
    input.mode7_input_active = mode7_input_active;
    if (secondary_ != nullptr) {
        input.secondary_state7 = secondary_->physics_state() == 7;
        input.secondary_animation_state = secondary_->animation_state();
        input.secondary_animation_flag_107 =
            secondary_->level_event_field_107() != 0;
        input.secondary_pending_score = secondary_->level_event_field_2a8();
        input.secondary_score_input_active = secondary_score_input_active_;
    }
    return input;
}

void LevelEventGameplayOwner::apply(
    const trg::TriggerLevelEventFrameResult& result) noexcept {
    if (result.primary_animation_started) {
        primary_.request_level_event_animation(result.primary_animation);
    }
    if (secondary_ != nullptr && result.secondary_animation_started) {
        secondary_->request_level_event_animation(result.secondary_animation);
    }

    // The retail call resets slot zero and slot one even when the secondary
    // skater pointer is null. PlayerReplayResetOwner preserves that boundary.
    for (std::size_t slot = 0; slot < result.replay_reset_requests; ++slot) {
        replay_owner_.reset_slot(slot);
    }

    if (result.primary_score_committed != 0) {
        primary_.apply_level_event_score(result.primary_score_committed);
    }
    if (secondary_ != nullptr && result.secondary_score_committed != 0) {
        secondary_->apply_level_event_score(result.secondary_score_committed);
    }

    if (result.primary_camera_delta != 0) {
        primary_camera_.apply_level_event_delta(result.primary_camera_delta);
    }
    if (secondary_camera_ != nullptr && result.secondary_camera_delta != 0) {
        secondary_camera_->apply_level_event_delta(result.secondary_camera_delta);
    }
}

} // namespace opentony::runtime
