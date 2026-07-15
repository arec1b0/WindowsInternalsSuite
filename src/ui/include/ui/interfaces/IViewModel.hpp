#pragma once

// Base contract for view models.
//
// A view model owns presentation state derived from the domain layer and exposes
// it as plain data. It never touches the OS UI: views observe it and render.
// Change notification is a single callback rather than a full observer registry,
// because each view model is bound to exactly one view in this application.

#include <functional>
#include <string>
#include <utility>

namespace wis::ui {

// Reported when a refresh fails, so the view can surface it in the status bar.
struct ViewModelError {
    bool present = false;
    std::string message;
};

class IViewModel {
public:
    virtual ~IViewModel() = default;

    // Re-reads the underlying domain state. Implementations must not throw:
    // failures are recorded via lastError() and reported through the callback.
    virtual void refresh() = 0;

    // Installs the change notification. Invoked after each refresh that alters
    // observable state. Passing an empty function detaches the view.
    void setOnChanged(std::function<void()> callback) {
        onChanged_ = std::move(callback);
    }

    [[nodiscard]] const ViewModelError& lastError() const noexcept { return lastError_; }

protected:
    // Invoked by implementations once observable state has settled.
    void notifyChanged() const {
        if (onChanged_) {
            onChanged_();
        }
    }

    void setError(std::string message) {
        lastError_.present = true;
        lastError_.message = std::move(message);
    }

    void clearError() {
        lastError_.present = false;
        lastError_.message.clear();
    }

private:
    std::function<void()> onChanged_;
    ViewModelError lastError_;
};

}  // namespace wis::ui
