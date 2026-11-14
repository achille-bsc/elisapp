#pragma once

namespace mars::platform {

	/// Drives LVGL: provides its millisecond tick and runs its timers.
	class EventLoop {
	public:
		/// Installs the monotonic tick source. Must be called after lv_init().
		static void installTick();

		/// Runs forever, sleeping until the next LVGL timer is due. How often an idle
		/// screen wakes up is therefore set by the display backend's input timer
		/// (5 ms polling with SDL; a device backend should block on its input fd).
		[[noreturn]] static void run();
	};

}
