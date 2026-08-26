#include "level_trigger_state.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace opentony::trg {
namespace {

[[nodiscard]] bool accepts_suspend_or_signal(std::uint16_t node_type) noexcept {
    // FUN_004c5c70/FUN_004c5b60 dispatch only to the type-1/type-7
    // gameplay-object lists. Linked crates and command points are not
    // touched by these two commands.
    return node_type == 1 || node_type == 7;
}

[[nodiscard]] bool accepts_visible(std::uint16_t node_type) noexcept {
    // FUN_004c77f0 resolves the object/link-key path for these node types.
    // Type-1/type-7 gameplay objects take the separate kill/signal path and
    // are diagnosed as non-crates by SendVisible.
    return node_type == 2 || node_type == 5 || node_type == 9
        || node_type == 12 || node_type == 14;
}

[[nodiscard]] bool accepts_kill(std::uint16_t node_type) noexcept {
    // FUN_004c7a00 has explicit branches for baddies, crates, rails and
    // special live objects. Other TRG records are skipped by the retail
    // loop, so do not invent a kill mutation for them here.
    return node_type == 1 || node_type == 2 || node_type == 7
        || node_type == 9 || node_type == 10 || node_type == 11
        || node_type >= 500;
}

} // namespace

void LevelTriggerState::reset() {
    const std::uint16_t configured_visible_mask = visible_mask_;
    const GapTable* configured_gap_table = gap_table_;
    *this = LevelTriggerState{};
    visible_mask_ = configured_visible_mask;
    gap_table_ = configured_gap_table;
}

void LevelTriggerState::set_object_identifier(std::size_t node, std::uint16_t identifier) {
    TriggerObjectState* current = find_object(node);
    if (current == nullptr) {
        throw FormatError("cannot assign an identifier to an unknown trigger object");
    }
    current->identifier = identifier;
}

void LevelTriggerState::bind_psx_models(const assets::PsxArchive& archive) {
    bound_model_count_ = 0;
    bound_scene_instance_count_ = 0;
    bound_scene_position_count_ = 0;
    for (TriggerObjectState& current : objects_) {
        current.asset_model_index = CommandPointRuntime::npos;
        current.asset_model_name = 0;
        current.asset_scene_instance_count = 0;
        current.asset_scene_first_instance = CommandPointRuntime::npos;
        current.asset_scene_position = {};
        current.has_asset_scene_position = false;
        if (current.link_key == 0) {
            continue;
        }
        const auto name = std::find(
            archive.model_names().begin(),
            archive.model_names().end(),
            current.link_key);
        if (name == archive.model_names().end()) {
            diagnostics_.push_back("TRG link key has no PSX model-name hash");
            continue;
        }
        current.asset_model_index = static_cast<std::size_t>(
            std::distance(archive.model_names().begin(), name));
        current.asset_model_name = current.link_key;
        for (std::size_t scene_index = 0; scene_index < archive.objects().size(); ++scene_index) {
            if (archive.objects()[scene_index].model_index
                != current.asset_model_index) {
                continue;
            }
            if (current.asset_scene_instance_count == 0) {
                current.asset_scene_first_instance = scene_index;
            }
            ++current.asset_scene_instance_count;
        }
        if (current.asset_scene_instance_count == 1) {
            current.asset_scene_position = archive.objects()[current.asset_scene_first_instance].position;
            current.has_asset_scene_position = true;
            ++bound_scene_position_count_;
        }
        bound_scene_instance_count_ += current.asset_scene_instance_count;
        ++bound_model_count_;
    }
}

void LevelTriggerState::mark_gap_complete(std::uint32_t checksum) {
    const auto found = std::find_if(
        gaps_.begin(),
        gaps_.end(),
        [checksum](const TriggerGapState& gap) { return gap.checksum == checksum; });
    if (found == gaps_.end()) {
        TriggerGapState state{};
        state.checksum = checksum;
        state.completed = true;
        gaps_.push_back(std::move(state));
    } else {
        found->completed = true;
    }
}

void LevelTriggerState::mark_goal_complete(std::uint16_t goal, bool complete) {
    if (goal >= goals_.size()) {
        throw FormatError("goal index is outside the retail range");
    }
    goals_[goal] = complete;
}

void LevelTriggerState::advance_time(std::uint32_t milliseconds) {
    time_ms_ += milliseconds;
    for (TriggerTimerState& timer : timers_) {
        if (!timer.fired && timer.due_ms <= time_ms_) {
            timer.fired = true;
            events_.push_back(TriggerEvent{
                TriggerEvent::Kind::TimerFired,
                CommandPointRuntime::npos,
                CommandPointRuntime::npos,
                0x0097,
                0,
                timer.duration_ms,
            });
        }
    }
}

const TriggerObjectState* LevelTriggerState::object(std::size_t node) const {
    return find_object(node);
}

std::uint16_t LevelTriggerState::global_word(std::uint16_t opcode) const noexcept {
    if (opcode >= has_global_word_.size() || !has_global_word_[opcode]) {
        return 0;
    }
    return global_words_[opcode];
}

bool LevelTriggerState::career_flag(std::uint16_t flag) const {
    return flag < career_flags_.size() && career_flags_[flag];
}

bool LevelTriggerState::goal_complete(std::uint16_t goal) const {
    return goal < goals_.size() && goals_[goal];
}

void LevelTriggerState::on_object_node(std::size_t node) {
    (void)ensure_object(node, 1, TriggerObjectKind::Object);
}

void LevelTriggerState::on_object_node_data(std::size_t node, std::span<const std::byte>) {
    (void)ensure_object(node, 1, TriggerObjectKind::Object);
}

void LevelTriggerState::on_script_object(
    std::size_t source,
    std::uint32_t key,
    std::array<std::uint16_t, 3> parameters) {
    TriggerScriptObjectState object{};
    object.source_node = source;
    object.script_key = key;
    object.parameters = parameters;
    object.identifier = static_cast<std::uint16_t>(script_objects_.size());
    script_objects_.push_back(object);
}

void LevelTriggerState::on_pickup_node(std::size_t node) {
    (void)ensure_object(node, 5, TriggerObjectKind::Pickup);
}

void LevelTriggerState::on_pickup_node_data(std::size_t node, std::span<const std::byte>) {
    (void)ensure_object(node, 5, TriggerObjectKind::Pickup);
}

void LevelTriggerState::on_spawn_node(
    std::size_t node,
    std::uint16_t type,
    std::uint16_t subtype,
    std::array<std::int32_t, 3> position,
    std::span<const std::byte>) {
    TriggerObjectState& current = ensure_object(
        node,
        type,
        type == 5 ? TriggerObjectKind::Pickup : TriggerObjectKind::Object);
    current.subtype = subtype;
    current.position = position;
    current.has_position = true;
    if (type == 5) {
        current.spawn_family = TriggerSpawnFamily::Pickup;
    } else if (subtype == 0x00cb) {
        current.spawn_family = TriggerSpawnFamily::ObjectCb;
    } else if (subtype == 0x0192) {
        current.spawn_family = TriggerSpawnFamily::Object192;
    } else if (subtype >= 0x00d5 && subtype <= 0x00dc) {
        current.spawn_family = TriggerSpawnFamily::SpecialVehicle;
        switch (subtype) {
        case 0x00d5:
            current.factory_resource = "c_taxi";
            current.factory_model_selector = 0x71;
            current.has_factory_model_selector = true;
            break;
        case 0x00d6:
            current.factory_resource = "c_police";
            break;
        case 0x00d7:
            current.factory_resource = "c_bus";
            current.factory_model_selector = 0x121;
            current.has_factory_model_selector = true;
            break;
        case 0x00d8:
            current.factory_resource = "c_cable";
            current.factory_model_selector = 0x86;
            current.has_factory_model_selector = true;
            break;
        case 0x00d9:
            current.factory_resource = "c_kart";
            current.factory_model_selector = 0x111;
            current.has_factory_model_selector = true;
            break;
        case 0x00da:
            current.factory_resource = "c_mar";
            current.factory_model_selector = 0xdd;
            current.has_factory_model_selector = true;
            break;
        case 0x00db:
            current.factory_resource = "c_bull";
            current.factory_model_selector = 0x145;
            current.has_factory_model_selector = true;
            break;
        case 0x00dc:
            current.factory_resource = "c_gull";
            current.factory_model_selector = 0xffffffffU;
            current.has_factory_model_selector = true;
            break;
        default:
            break;
        }
    }
}

void LevelTriggerState::on_spawn_orientation(
    std::size_t node,
    std::array<std::uint16_t, 3> orientation) {
    if (TriggerObjectState* current = find_object(node); current != nullptr) {
        current->orientation = orientation;
        current->has_orientation = true;
    }
}

void LevelTriggerState::on_special_node(
    std::size_t node,
    std::uint16_t type,
    std::span<const std::byte>) {
    (void)ensure_object(node, type, TriggerObjectKind::Special);
}

void LevelTriggerState::on_linked_node(
    std::size_t node,
    std::uint16_t type,
    std::uint32_t key,
    std::span<const std::byte>) {
    if (type == 6) {
        return;
    }
    const TriggerObjectKind kind = type == 12 || type == 14
        ? TriggerObjectKind::Special
        : TriggerObjectKind::LinkedObject;
    (void)ensure_object(node, type, kind, key);
}

void LevelTriggerState::on_restart_node(
    std::size_t node,
    std::string_view name,
    std::array<std::int32_t, 3> position) {
    const auto found = std::find_if(
        restarts_.begin(),
        restarts_.end(),
        [node](const TriggerRestartState& restart) { return restart.node == node; });
    if (found == restarts_.end()) {
        restarts_.push_back(TriggerRestartState{node, std::string(name), position, 0, 0});
    } else {
        found->name = name;
        found->position = position;
    }
}

void LevelTriggerState::on_restart_node_data(
    std::size_t node,
    std::uint32_t auxiliary,
    std::uint16_t auxiliary_word) {
    const auto found = std::find_if(
        restarts_.begin(),
        restarts_.end(),
        [node](const TriggerRestartState& restart) { return restart.node == node; });
    if (found != restarts_.end()) {
        found->auxiliary = auxiliary;
        found->auxiliary_word = auxiliary_word;
    }
}

void LevelTriggerState::on_node_pulse(std::size_t node) {
    if (TriggerObjectState* current = find_object(node); current != nullptr) {
        ++current->pulses;
    }
    record_target_event(TriggerEvent::Kind::Pulse, CommandPointRuntime::npos, node);
}

void LevelTriggerState::on_suspend_activate(
    std::size_t source,
    std::uint16_t opcode,
    std::span<const std::uint16_t> links) {
    const TriggerEvent::Kind kind = opcode == 4
        ? TriggerEvent::Kind::Suspend
        : TriggerEvent::Kind::Activate;
    for (const std::uint16_t target : links) {
        TriggerObjectState* current = find_object(target);
        if (current != nullptr && accepts_suspend_or_signal(current->node_type)) {
            current->suspended = opcode == 4;
            current->active = opcode == 5;
            ++current->suspend_activate_calls;
        }
        record_target_event(kind, source, target, opcode);
    }
}

void LevelTriggerState::on_signal(
    std::size_t source,
    std::span<const std::uint16_t> links) {
    for (const std::uint16_t target : links) {
        if (TriggerObjectState* current = find_object(target);
            current != nullptr && accepts_suspend_or_signal(current->node_type)) {
            ++current->signals;
        }
        record_target_event(TriggerEvent::Kind::Signal, source, target, 0x000a);
    }
}

void LevelTriggerState::on_kill(
    std::size_t source,
    std::uint16_t opcode,
    std::span<const std::uint16_t> links) {
    for (const std::uint16_t target : links) {
        if (TriggerObjectState* current = find_object(target);
            current != nullptr && accepts_kill(current->node_type)) {
            ++current->kills;
            if (opcode == 0x000c) {
                current->alive = false;
                current->killed = true;
                current->active = false;
            } else {
                current->flags = static_cast<std::uint16_t>(current->flags | visible_mask_);
            }
        }
        record_target_event(TriggerEvent::Kind::Kill, source, target, opcode);
    }
}

void LevelTriggerState::on_visible(
    std::size_t source,
    std::uint16_t value,
    std::span<const std::uint16_t> links) {
    for (const std::uint16_t target : links) {
        if (TriggerObjectState* current = find_object(target);
            current != nullptr && accepts_visible(current->node_type)) {
            current->visible_commanded = value != 0;
            // In the one-player retail path SendVisible(1) clears 0x41. A
            // zero command leaves the target word unchanged.
            if (value != 0) {
                current->flags = static_cast<std::uint16_t>(current->flags & ~visible_mask_);
            }
        }
        record_target_event(TriggerEvent::Kind::Visible, source, target, 0x000d, value);
    }
}

void LevelTriggerState::on_object_flag_by_id(std::uint16_t identifier, bool set) {
    TriggerObjectState* current = find_identifier(identifier);
    if (current != nullptr) {
        if (set) {
            current->flags = static_cast<std::uint16_t>(current->flags | 1U);
        } else {
            current->flags = static_cast<std::uint16_t>(current->flags & ~1U);
        }
        record_target_event(
            TriggerEvent::Kind::ObjectFlag,
            CommandPointRuntime::npos,
            current->node,
            set ? 0x0084 : 0x0083,
            identifier);
        return;
    }
    if (TriggerScriptObjectState* script = find_script_identifier(identifier);
        script != nullptr) {
        if (set) {
            script->flags = static_cast<std::uint16_t>(script->flags | 1U);
        } else {
            script->flags = static_cast<std::uint16_t>(script->flags & ~1U);
        }
        record_target_event(
            TriggerEvent::Kind::ObjectFlag,
            CommandPointRuntime::npos,
            script->source_node,
            set ? 0x0084 : 0x0083,
            identifier);
        return;
    }
    diagnostics_.push_back("object flag command named an unregistered object id");
    record_target_event(
        TriggerEvent::Kind::ObjectFlag,
        CommandPointRuntime::npos,
        CommandPointRuntime::npos,
        set ? 0x0084 : 0x0083,
        identifier);
}

void LevelTriggerState::on_global_word(std::uint16_t opcode, std::uint16_t value) {
    if (opcode < global_words_.size()) {
        global_words_[opcode] = value;
        has_global_word_[opcode] = true;
    }
}

void LevelTriggerState::on_fog_range(
    std::uint16_t near_range,
    std::uint16_t far_range,
    std::uint16_t mode) {
    fog_ = TriggerFogState{near_range, far_range, mode, true};
}

void LevelTriggerState::on_music(std::int16_t track) {
    music_track_ = static_cast<std::uint16_t>(track);
}

void LevelTriggerState::on_sound(std::int16_t sound) {
    sound_id_ = sound;
}

void LevelTriggerState::on_resource(std::uint16_t mode, std::string_view name) {
    resources_.push_back(TriggerResourceRequest{mode, std::string(name)});
}

void LevelTriggerState::on_flush_resources() {
    ++resource_flushes_;
}

void LevelTriggerState::on_fixed_path(
    std::uint16_t first,
    std::uint16_t second,
    std::span<const FixedPathRecord> records) {
    TriggerPathState state{first, second, {}};
    copy_path_records(state.records, records);
    paths_.push_back(std::move(state));
}

void LevelTriggerState::on_fixed_path_records(std::span<const FixedPathRecord> records) {
    copy_path_records(legacy_path_records_, records);
}

void LevelTriggerState::on_competition_name(std::string_view name) {
    competition_name_ = name;
}

void LevelTriggerState::on_restart_selected(
    std::uint16_t opcode,
    std::size_t node,
    std::string_view) {
    selected_restart_ = node;
    events_.push_back(TriggerEvent{
        TriggerEvent::Kind::RestartSelected,
        CommandPointRuntime::npos,
        node,
        opcode,
        0,
        0,
    });
}

void LevelTriggerState::on_apply_restart(
    std::size_t node,
    std::array<std::int32_t, 3> position) {
    last_restart_ = TriggerRestartApplication{node, position, 0, 0, true};
    events_.push_back(TriggerEvent{
        TriggerEvent::Kind::RestartApplied,
        CommandPointRuntime::npos,
        node,
        0,
        0,
        0,
    });
}

void LevelTriggerState::on_apply_restart_data(
    std::size_t node,
    std::uint32_t auxiliary,
    std::uint16_t auxiliary_word) {
    if (last_restart_.set && last_restart_.node == node) {
        last_restart_.auxiliary = auxiliary;
        last_restart_.auxiliary_word = auxiliary_word;
    }
}

void LevelTriggerState::on_initial_state(std::uint16_t state) {
    initial_state_ = state;
}

void LevelTriggerState::on_timer(std::uint32_t milliseconds) {
    timers_.push_back(TriggerTimerState{time_ms_ + milliseconds, milliseconds, false});
    events_.push_back(TriggerEvent{
        TriggerEvent::Kind::TimerScheduled,
        CommandPointRuntime::npos,
        CommandPointRuntime::npos,
        0x0097,
        0,
        milliseconds,
    });
}

void LevelTriggerState::on_reverb_type(std::uint8_t type) {
    reverb_type_ = type;
}

void LevelTriggerState::on_level_event_state() {
    ++level_event_updates_;
}

void LevelTriggerState::on_script_value(std::uint32_t value) {
    script_value_ = value;
}

void LevelTriggerState::on_level_value(std::uint32_t value) {
    level_value_ = value;
}

void LevelTriggerState::on_gap(
    std::size_t source,
    std::uint32_t checksum,
    std::uint16_t argument) {
    const auto found = std::find_if(
        gaps_.begin(),
        gaps_.end(),
        [checksum](const TriggerGapState& gap) { return gap.checksum == checksum; });
    TriggerGapState* state = nullptr;
    if (found == gaps_.end()) {
        TriggerGapState new_state{};
        new_state.checksum = checksum;
        new_state.last_argument = argument;
        new_state.last_source = source;
        new_state.seen = 1;
        gaps_.push_back(std::move(new_state));
        state = &gaps_.back();
    } else {
        found->last_argument = argument;
        found->last_source = source;
        ++found->seen;
        state = &*found;
    }
    if (gap_table_ != nullptr) {
        if (const TriggerGapDefinition* definition = gap_table_->find(argument);
            definition != nullptr) {
            state->definition_found = true;
            state->flags = definition->flags;
            state->score = definition->score;
            state->name = definition->name;
            state->deferred = (definition->flags & 0x40U) != 0;
            if ((definition->flags & 0x08U) == 0 && !state->deferred && !state->awarded) {
                state->completed = true;
                state->awarded = true;
                state->pulse_pending = true;
            }
        }
    }
    events_.push_back(TriggerEvent{
        TriggerEvent::Kind::GapSeen,
        source,
        CommandPointRuntime::npos,
        0x00c9,
        argument,
        checksum,
    });
}

bool LevelTriggerState::take_gap_pulse(
    std::uint32_t checksum,
    std::uint16_t argument) {
    const auto found = std::find_if(
        gaps_.begin(),
        gaps_.end(),
        [checksum, argument](const TriggerGapState& gap) {
            return gap.checksum == checksum && gap.last_argument == argument;
        });
    if (found == gaps_.end() || !found->pulse_pending) {
        return false;
    }
    found->pulse_pending = false;
    return true;
}

void LevelTriggerState::set_career_flag(std::uint16_t flag) {
    if (flag < career_flags_.size()) {
        career_flags_[flag] = true;
    }
}

void LevelTriggerState::on_legacy_command(
    std::uint16_t opcode,
    std::span<const std::byte> bytes,
    std::size_t source) {
    TriggerLegacyCommand command{opcode, source, {}};
    command.bytes.assign(bytes.begin(), bytes.end());
    legacy_commands_.push_back(std::move(command));
}

void LevelTriggerState::on_unknown_command(
    std::uint16_t opcode,
    std::uint32_t offset,
    std::size_t source,
    std::span<const std::byte> remaining) {
    TriggerUnknownCommand command{opcode, offset, source, {}};
    command.remaining.assign(remaining.begin(), remaining.end());
    unknown_commands_.push_back(std::move(command));
}

void LevelTriggerState::on_diagnostic(std::string_view message) {
    diagnostics_.emplace_back(message);
}

TriggerObjectState* LevelTriggerState::find_object(std::size_t node) {
    const auto found = std::find_if(
        objects_.begin(),
        objects_.end(),
        [node](const TriggerObjectState& object) { return object.node == node; });
    return found == objects_.end() ? nullptr : &*found;
}

const TriggerObjectState* LevelTriggerState::find_object(std::size_t node) const {
    const auto found = std::find_if(
        objects_.begin(),
        objects_.end(),
        [node](const TriggerObjectState& object) { return object.node == node; });
    return found == objects_.end() ? nullptr : &*found;
}

TriggerObjectState& LevelTriggerState::ensure_object(
    std::size_t node,
    std::uint16_t type,
    TriggerObjectKind kind,
    std::uint32_t key) {
    if (TriggerObjectState* current = find_object(node); current != nullptr) {
        current->node_type = type;
        current->kind = kind;
        if (key != 0) {
            current->link_key = key;
        }
        return *current;
    }
    TriggerObjectState object{};
    object.node = node;
    object.node_type = type;
    object.kind = kind;
    object.link_key = key;
    if ((kind == TriggerObjectKind::Object || kind == TriggerObjectKind::Pickup)
        && node <= std::numeric_limits<std::uint16_t>::max()) {
        object.identifier = static_cast<std::uint16_t>(node);
    }
    objects_.push_back(object);
    return objects_.back();
}

TriggerObjectState* LevelTriggerState::find_identifier(std::uint16_t identifier) {
    const auto found = std::find_if(
        objects_.begin(),
        objects_.end(),
        [identifier](const TriggerObjectState& object) {
            return object.identifier == identifier;
        });
    return found == objects_.end() ? nullptr : &*found;
}

TriggerScriptObjectState* LevelTriggerState::find_script_identifier(std::uint16_t identifier) {
    const auto found = std::find_if(
        script_objects_.begin(),
        script_objects_.end(),
        [identifier](const TriggerScriptObjectState& object) {
            return object.identifier == identifier;
        });
    return found == script_objects_.end() ? nullptr : &*found;
}

void LevelTriggerState::record_target_event(
    TriggerEvent::Kind kind,
    std::size_t source,
    std::size_t target,
    std::uint16_t opcode,
    std::uint16_t value) {
    events_.push_back(TriggerEvent{kind, source, target, opcode, value, 0});
}

void LevelTriggerState::copy_path_records(
    std::vector<FixedPathRecord>& destination,
    std::span<const FixedPathRecord> records) {
    destination.insert(destination.end(), records.begin(), records.end());
}

} // namespace opentony::trg
