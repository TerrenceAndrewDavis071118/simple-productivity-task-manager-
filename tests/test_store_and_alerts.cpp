#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../src/audio.hpp"
#include "../src/store.hpp"

// Lightweight test assertion helpers
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            std::exit(1); \
        } \
    } while (0)

static std::filesystem::path create_test_db(const std::string& name) {
    auto p = std::filesystem::temp_directory_path() / ("tk_test_" + name + ".db");
    std::filesystem::remove(p);
    return p;
}

// Test 1: Short task (duration <= 5 minutes) must NEVER trigger 5m warning
void test_short_task_lifecycle() {
    auto db = create_test_db("short");
    setenv("XDG_DATA_HOME", db.parent_path().c_str(), 1);

    tk::Store store;
    time_t t0 = 10000;
    time_t t_start = t0 + 100;
    time_t t_deadline = t_start + 120; // 2 minutes duration
    time_t t_expires = t_deadline + 60;

    const auto& task = store.add("Quick task", t_start, t_deadline, t_expires);
    long id = task.id;
    TEST_ASSERT(task.warned_5m == true, "Short task should have warned_5m initialized to true");

    bool changed = false;

    // Before start: nothing
    auto events = store.tick(t_start - 10, changed);
    TEST_ASSERT(events.empty(), "No events before start");
    TEST_ASSERT(!changed, "No changed before start");

    // At start: Ready only
    events = store.tick(t_start, changed);
    TEST_ASSERT(events.size() == 1, "Expected exactly 1 event at start");
    TEST_ASSERT(events[0].kind == tk::NotifKind::Ready, "Expected Ready event at start");
    TEST_ASSERT(events[0].task_id == id, "Task ID mismatch");
    TEST_ASSERT(changed, "Store should mark changed");

    // Between start and deadline: no 5m warning!
    events = store.tick(t_start + 60, changed);
    TEST_ASSERT(events.empty(), "No 5m warning for short task");

    // At deadline: no 5m warning
    events = store.tick(t_deadline, changed);
    TEST_ASSERT(events.empty(), "No warning at deadline");

    // At expiration: moved to trash
    events = store.tick(t_expires, changed);
    TEST_ASSERT(events.empty(), "No notif on expiration");
    const auto* t = store.find(id);
    TEST_ASSERT(t != nullptr && t->status == tk::Status::Trash, "Task should be in Trash");

    std::filesystem::remove(db);
    std::cout << "[PASS] test_short_task_lifecycle\n";
}

// Test 2: Standard task (> 5 minutes duration) triggers Ready and 5m Warning at correct times
void test_standard_task_lifecycle() {
    auto db = create_test_db("standard");
    setenv("XDG_DATA_HOME", db.parent_path().c_str(), 1);

    tk::Store store;
    time_t t_start = 20000;
    time_t t_deadline = t_start + 600; // 10 minutes duration
    time_t t_warn = t_deadline - 300;  // 5 minutes before deadline
    time_t t_expires = t_deadline + 300;

    const auto& task = store.add("Standard task", t_start, t_deadline, t_expires);
    long id = task.id;
    TEST_ASSERT(!task.warned_5m, "10-minute task should not have warned_5m initially");
    TEST_ASSERT(!task.notified, "Task should not be notified initially");

    bool changed = false;

    // Tick before start
    auto events = store.tick(t_start - 5, changed);
    TEST_ASSERT(events.empty(), "No events before start");

    // Tick at start
    events = store.tick(t_start, changed);
    TEST_ASSERT(events.size() == 1 && events[0].kind == tk::NotifKind::Ready, "Ready fired at start");

    // Tick between start and warn time
    events = store.tick(t_warn - 10, changed);
    TEST_ASSERT(events.empty(), "No events before 5m mark");

    // Tick at 5m warning mark
    events = store.tick(t_warn, changed);
    TEST_ASSERT(events.size() == 1, "Expected 1 event at 5m mark");
    TEST_ASSERT(events[0].kind == tk::NotifKind::Deadline5m, "Expected Deadline5m event");
    TEST_ASSERT(events[0].task_id == id, "Task ID mismatch");

    // Subsequent tick in warning window: idempotent (does not fire again)
    events = store.tick(t_warn + 10, changed);
    TEST_ASSERT(events.empty(), "5m warning must not repeat");

    // Tick at deadline
    events = store.tick(t_deadline, changed);
    TEST_ASSERT(events.empty(), "No event at deadline");

    // Tick at expiration
    events = store.tick(t_expires, changed);
    const auto* t = store.find(id);
    TEST_ASSERT(t != nullptr && t->status == tk::Status::Trash, "Task expired to trash");

    std::filesystem::remove(db);
    std::cout << "[PASS] test_standard_task_lifecycle\n";
}

// Test 3: Prior attempt bug check: future short task MUST NOT fire 5m warning before start
void test_future_short_task_no_premature_warning() {
    auto db = create_test_db("future_short");
    setenv("XDG_DATA_HOME", db.parent_path().c_str(), 1);

    tk::Store store;
    time_t t_now = 50000;
    time_t t_start = t_now + 3600;      // Starts in 1 hour
    time_t t_deadline = t_start + 180;  // 3 minutes long (ends in 1h3m)
    time_t t_expires = t_deadline + 60;

    store.add("Tomorrow short task", t_start, t_deadline, t_expires);

    // If unguarded, deadline - 300 = t_start - 120 (which is in the future relative to t_now, but
    // what if we tick at t_start - 100, which is >= deadline - 300?)
    bool changed = false;
    time_t t_premature = t_start - 100; // 100 seconds before start, but after deadline - 300!
    auto events = store.tick(t_premature, changed);
    TEST_ASSERT(events.empty(), "FATAL BUG REGRESSION: 5m warning fired BEFORE task even started!");

    // Tick at start: should only fire Ready, NOT 5m warning
    events = store.tick(t_start, changed);
    TEST_ASSERT(events.size() == 1, "Expected only Ready event at start");
    TEST_ASSERT(events[0].kind == tk::NotifKind::Ready, "Expected Ready kind");

    std::filesystem::remove(db);
    std::cout << "[PASS] test_future_short_task_no_premature_warning\n";
}

// Test 4: Early completion suppresses 5m warning
void test_early_completion_suppression() {
    auto db = create_test_db("early_comp");
    setenv("XDG_DATA_HOME", db.parent_path().c_str(), 1);

    tk::Store store;
    time_t t_start = 60000;
    time_t t_deadline = t_start + 1000;
    time_t t_expires = t_deadline + 200;

    const auto& task = store.add("To complete early", t_start, t_deadline, t_expires);
    long id = task.id;

    bool changed = false;
    store.tick(t_start, changed); // Fires ready

    // Complete task early
    store.set_status(id, tk::Status::Achieved);

    // Tick at 5m mark
    auto events = store.tick(t_deadline - 300, changed);
    TEST_ASSERT(events.empty(), "Completed task should not receive 5m deadline warning");

    std::filesystem::remove(db);
    std::cout << "[PASS] test_early_completion_suppression\n";
}

// Test 5: Post-deadline app launch suppresses stale notifications
void test_post_deadline_launch_suppression() {
    auto db = create_test_db("post_deadline");
    setenv("XDG_DATA_HOME", db.parent_path().c_str(), 1);

    tk::Store store;
    time_t t_start = 70000;
    time_t t_deadline = t_start + 1000;
    time_t t_expires = t_deadline + 500;

    store.add("Missed task", t_start, t_deadline, t_expires);

    // App launched post-deadline, but before expiration
    bool changed = false;
    auto events = store.tick(t_deadline + 100, changed);
    TEST_ASSERT(events.empty(), "Stale notifications must be suppressed after deadline");

    const auto* t = store.find(1);
    TEST_ASSERT(t != nullptr && t->notified && t->warned_5m, "Flags should be marked to suppress future ticks");

    std::filesystem::remove(db);
    std::cout << "[PASS] test_post_deadline_launch_suppression\n";
}

// Test 6: Database backward compatibility (7-col, 8-col, 9-col)
void test_database_compatibility() {
    auto db_dir = std::filesystem::temp_directory_path() / "tk_compat_test";
    std::filesystem::create_directories(db_dir / "task-killer");
    auto db_file = db_dir / "task-killer" / "tasks.db";

    setenv("XDG_DATA_HOME", db_dir.c_str(), 1);

    // Write a mixed DB: 7-col legacy, 8-col legacy, 9-col new
    time_t now = std::time(nullptr);
    {
        std::ofstream out(db_file);
        // 7 col: id, status, notified, start_at, deadline_at, expires_at, title
        out << "1\t0\t1\t" << (now - 1000) << "\t" << (now + 1000) << "\t" << (now + 2000) << "\tLegacy 7 col\n";
        // 8 col: id, status, notified, start_at, deadline_at, expires_at, completed_at, title
        out << "2\t0\t0\t" << (now + 500) << "\t" << (now + 1500) << "\t" << (now + 2500) << "\t0\tLegacy 8 col\n";
        // 9 col: id, status, notified, start_at, deadline_at, expires_at, completed_at, title, warned_5m
        out << "3\t0\t1\t" << (now - 800) << "\t" << (now + 600) << "\t" << (now + 1600) << "\t0\tNew 9 col\t1\n";
    }

    tk::Store store;
    store.load();

    TEST_ASSERT(store.tasks.size() == 3, "All 3 tasks loaded");

    const auto* t1 = store.find(1);
    TEST_ASSERT(t1 != nullptr, "Task 1 found");
    TEST_ASSERT(t1->title == "Legacy 7 col", "Task 1 title mismatch");
    TEST_ASSERT(t1->notified == true, "Task 1 notified");
    TEST_ASSERT(t1->warned_5m == false, "Task 1 warned_5m should be false (active, duration > 5m)");

    const auto* t2 = store.find(2);
    TEST_ASSERT(t2 != nullptr, "Task 2 found");
    TEST_ASSERT(t2->title == "Legacy 8 col", "Task 2 title mismatch");
    TEST_ASSERT(t2->notified == false, "Task 2 not notified");

    const auto* t3 = store.find(3);
    TEST_ASSERT(t3 != nullptr, "Task 3 found");
    TEST_ASSERT(t3->title == "New 9 col", "Task 3 title mismatch");
    TEST_ASSERT(t3->warned_5m == true, "Task 3 warned_5m preserved from col 9");

    // Test saving and reloading
    store.save();
    tk::Store store2;
    store2.load();
    TEST_ASSERT(store2.tasks.size() == 3, "Reloaded all 3 tasks");
    const auto* r3 = store2.find(3);
    TEST_ASSERT(r3 != nullptr && r3->warned_5m == true, "Reloaded warned_5m preserved");

    std::filesystem::remove_all(db_dir);
    std::cout << "[PASS] test_database_compatibility\n";
}

// Test 7: Audio file exists and is valid WAV
void test_audio_file_exists() {
    std::string path = tk::locate_sound_file();
    TEST_ASSERT(!path.empty(), "Sound file must be located");
    TEST_ASSERT(std::filesystem::exists(path), "Located sound file must exist on disk");

    std::ifstream in(path, std::ios::binary);
    TEST_ASSERT(in.is_open(), "Sound file must be readable");
    char header[12];
    in.read(header, 12);
    TEST_ASSERT(in.gcount() == 12, "Failed to read 12-byte WAV header");
    TEST_ASSERT(std::string(header, 4) == "RIFF", "RIFF header missing");
    TEST_ASSERT(std::string(header + 8, 4) == "WAVE", "WAVE signature missing");

    std::cout << "[PASS] test_audio_file_exists (" << path << ")\n";
}

// Test 8: Player command resolution
void test_audio_player_resolution() {
    std::string player = tk::get_player_command();
    TEST_ASSERT(!player.empty(), "At least one player command must be found on system");
    TEST_ASSERT(player == "canberra-gtk-play" || player == "pw-play" ||
                player == "paplay" || player == "aplay", "Unexpected player: " + player);
    std::cout << "[PASS] test_audio_player_resolution (detected: " << player << ")\n";
}

// Test 9: Non-blocking audio playback and 500ms debounce
void test_audio_playback_and_debounce() {
    // 1st call: should spawn successfully
    auto start = std::chrono::steady_clock::now();
    bool played1 = tk::play_alert_sound(false);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    TEST_ASSERT(elapsed < 50, "play_alert_sound must be non-blocking (< 50ms), took " + std::to_string(elapsed) + "ms");
    TEST_ASSERT(played1, "First play should succeed");

    // Immediate 2nd call: should be debounced
    bool played2 = tk::play_alert_sound(false);
    TEST_ASSERT(!played2, "Immediate 2nd play must be debounced (< 500ms)");

    // After waiting > 500ms: should play again
    g_usleep(550 * 1000); // 550ms
    bool played3 = tk::play_alert_sound(false);
    TEST_ASSERT(played3, "Play after 500ms debounce window should succeed");

    std::cout << "[PASS] test_audio_playback_and_debounce\n";
}

int main() {
    std::cout << "Running Task Killer store & alert tests...\n";
    test_short_task_lifecycle();
    test_standard_task_lifecycle();
    test_future_short_task_no_premature_warning();
    test_early_completion_suppression();
    test_post_deadline_launch_suppression();
    test_database_compatibility();
    test_audio_file_exists();
    test_audio_player_resolution();
    test_audio_playback_and_debounce();
    std::cout << "ALL TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
