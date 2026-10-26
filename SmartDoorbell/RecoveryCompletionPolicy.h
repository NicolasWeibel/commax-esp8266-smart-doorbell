#pragma once

enum class RecoveryCompletionAction : unsigned char {
  Finalize,
  WaitForInput,
  ScheduleUpdate,
};

struct RecoveryCompletionPolicy {
  static RecoveryCompletionAction afterSuccessfulPublish(
      bool inputActivityInFlight,
      bool summaryChanged) {
    if (inputActivityInFlight) {
      return RecoveryCompletionAction::WaitForInput;
    }
    return summaryChanged ? RecoveryCompletionAction::ScheduleUpdate
                          : RecoveryCompletionAction::Finalize;
  }

  static RecoveryCompletionAction afterInputSettles(bool summaryChanged) {
    return summaryChanged ? RecoveryCompletionAction::ScheduleUpdate
                          : RecoveryCompletionAction::Finalize;
  }
};
