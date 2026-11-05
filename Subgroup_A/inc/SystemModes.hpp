#ifndef SYSTEM_MODES_HPP
#define SYSTEM_MODES_HPP

enum class SystemState {
    Idle,
    Dispensation_Mode,
    Fault_Mode,
    Security_Lockout,
    Offline_Mode
};

extern SystemState currentState;

#endif