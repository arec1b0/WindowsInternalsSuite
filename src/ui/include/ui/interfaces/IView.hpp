#pragma once

// Base contract for views. A view renders one view model and translates user
// input into view model calls. It owns no domain state.

namespace wis::ui {

class IView {
public:
    virtual ~IView() = default;

    // Pulls current state from the bound view model and repaints. Called on the
    // UI thread, typically from the view model's change notification.
    virtual void render() = 0;

    // Called when the view becomes visible; a good point to start refreshing.
    virtual void onActivated() {}

    // Called when the view is hidden; a good point to stop periodic refreshes.
    virtual void onDeactivated() {}
};

}  // namespace wis::ui
