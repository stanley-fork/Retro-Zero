#ifndef CP0_ESCAPE_HOLD_H
#define CP0_ESCAPE_HOLD_H

#include <cstdint>
#include <map>
#include <linux/input.h>

// One physical ESC press, owned by the device on which it began. Callers drain
// pending events (especially releases) before checking the hold deadline.
class EscapeHold {
public:
    // Returns true only for a new press. Repeats cannot trigger Back or arm a
    // hold after a screen change. A lost event stream requires a fresh release.
    bool handle_event(int fd, const input_event &event, uint32_t now, bool allow_hold)
    {
        if (event.type == EV_SYN && event.code == SYN_DROPPED) {
            auto &device = devices_[fd];
            device.pressed = true;
            device.dropped = true;
            if (owner_ == fd) cancel();
            return false;
        }
        if (event.type == EV_SYN && event.code == SYN_REPORT) {
            auto device = devices_.find(fd);
            if (device != devices_.end()) device->second.dropped = false;
            return false;
        }
        if (event.type != EV_KEY || event.code != KEY_ESC) return false;

        auto &device = devices_[fd];
        if (device.dropped) return false;
        if (event.value == 0) {
            device.pressed = false;
            if (owner_ == fd) cancel();
        } else if (event.value == 1 && !device.pressed) {
            device.pressed = true;
            if (allow_hold && !active()) {
                owner_ = fd;
                started_ = now;
            }
            return true;
        }
        return false;
    }

    bool active() const { return owner_ >= 0; }
    uint32_t elapsed(uint32_t now) const { return active() ? now - started_ : 0; }
    void cancel() { owner_ = -1; }

    void disconnect(int fd)
    {
        if (owner_ == fd) cancel();
        devices_.erase(fd);
    }

    void reset()
    {
        cancel();
        devices_.clear();
    }

private:
    struct Device {
        bool pressed = false;
        bool dropped = false;
    };
    std::map<int, Device> devices_;
    int owner_ = -1;
    uint32_t started_ = 0;
};

#endif // CP0_ESCAPE_HOLD_H
