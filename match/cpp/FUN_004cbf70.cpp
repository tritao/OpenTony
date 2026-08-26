struct QueueState {
    char padding[0x14];
    int* cursor;
    int value;
    int count;
};

void FUN_004cbf70(QueueState* state)
{
    if (state->count != 0x20) {
        *state->cursor = state->value;
        state->cursor += 1;
        state->value = 0;
        state->count = 0x20;
    }
}
