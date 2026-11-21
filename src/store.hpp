#pragma once

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "platform.hpp"

namespace tk {

enum class Status { Active = 0, Achieved = 1, Trash = 2 };
enum class StatScope { Range = 0, Day = 1, Month = 2, Year = 3 };
enum class Outcome { OnTime = 0, Late = 1, Failed = 2 };
enum class NotifKind { Ready = 0, Deadline5m = 1 };

struct NotifItem {
    long task_id = 0;
    NotifKind kind = NotifKind::Ready;
};

struct SubTask {
    std::string title;
    bool done = false;
    std::string image_path;
};

struct Task {
    long id = 0;
    std::string title;
    std::string detail;
    std::vector<SubTask> subtasks;
    time_t start_at = 0;
    time_t deadline_at = 0;
    time_t expires_at = 0;
    time_t completed_at = 0;
    Status status = Status::Active;
    bool notified = false;
    bool warned_5m = false;
};

struct VictimItem {
    std::string title;
    Outcome outcome = Outcome::OnTime;
    std::string detail;
};

struct StatSummary {
    int on_time = 0;
    int late = 0;
    int failed = 0;
    std::vector<VictimItem> victims;

    int total_completed() const { return on_time + late; }
    int total() const { return on_time + late + failed; }
};

inline std::string unescape_newlines(const std::string& s) {
    std::string res;
    res.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == 'n') {
                res += '\n';
                ++i;
                continue;
            } else if (s[i + 1] == 'r') {
                res += '\r';
                ++i;
                continue;
            }
        }
        res += s[i];
    }
    return res;
}

inline std::vector<std::string> extract_image_paths(const std::string& raw_detail) {
    std::string text = unescape_newlines(raw_detail);
    std::vector<std::string> paths;
    size_t pos = 0;
    while ((pos = text.find("[img:", pos)) != std::string::npos) {
        size_t end = text.find("]", pos + 5);
        if (end != std::string::npos) {
            std::string p = text.substr(pos + 5, end - (pos + 5));
            if (!p.empty()) {
                paths.push_back(std::move(p));
            }
            pos = end + 1;
        } else {
            break;
        }
    }
    return paths;
}

inline std::string strip_image_tags(const std::string& raw_detail) {
    std::string text = unescape_newlines(raw_detail);
    std::string res;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t tag = text.find("[img:", pos);
        if (tag != std::string::npos) {
            res.append(text, pos, tag - pos);
            size_t end = text.find("]", tag + 5);
            if (end != std::string::npos) {
                pos = end + 1;
                while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n')) ++pos;
            } else {
                res.append(text, tag, std::string::npos);
                break;
            }
        } else {
            res.append(text, pos, std::string::npos);
            break;
        }
    }
    return res;
}

inline std::vector<SubTask> parse_subtasks(const std::string& raw_detail) {
    std::string detail = unescape_newlines(raw_detail);
    std::vector<SubTask> res;
    std::istringstream ss(detail);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
            ++start;
        }
        if (start >= line.size()) continue;
        std::string s = line.substr(start);

        SubTask st;
        if (s.size() >= 4 && (s.substr(0, 4) == "[x] " || s.substr(0, 4) == "[X] ")) {
            st.done = true;
            s = s.substr(4);
        } else if (s.size() >= 4 && s.substr(0, 4) == "[ ] ") {
            st.done = false;
            s = s.substr(4);
        } else if (s.size() >= 3 && (s.substr(0, 3) == "[x]" || s.substr(0, 3) == "[X]")) {
            st.done = true;
            s = s.substr(3);
        } else if (s.size() >= 3 && s.substr(0, 3) == "[ ]") {
            st.done = false;
            s = s.substr(3);
        } else if (s.size() >= 2 && (s.substr(0, 2) == "- " || s.substr(0, 2) == "* ")) {
            st.done = false;
            s = s.substr(2);
        } else {
            st.done = false;
        }

        // Check if line contains [img:<path>]
        size_t img_pos = s.find("[img:");
        if (img_pos != std::string::npos) {
            size_t img_end = s.find("]", img_pos + 5);
            if (img_end != std::string::npos) {
                st.image_path = s.substr(img_pos + 5, img_end - (img_pos + 5));
                std::string t1 = s.substr(0, img_pos);
                std::string t2 = (img_end + 1 < s.size()) ? s.substr(img_end + 1) : "";
                std::string combined = t1 + " " + t2;
                while (!combined.empty() && (combined.front() == ' ' || combined.front() == '\t')) combined.erase(0, 1);
                while (!combined.empty() && (combined.back() == ' ' || combined.back() == '\t')) combined.pop_back();
                st.title = std::move(combined);
            } else {
                st.title = s;
            }
        } else {
            st.title = s;
        }

        while (!st.title.empty() && (st.title.back() == ' ' || st.title.back() == '\t')) {
            st.title.pop_back();
        }
        size_t ts = 0;
        while (ts < st.title.size() && (st.title[ts] == ' ' || st.title[ts] == '\t')) {
            ++ts;
        }
        if (ts > 0) st.title = st.title.substr(ts);

        if (!st.title.empty() || !st.image_path.empty()) {
            res.push_back(std::move(st));
        }
    }
    return res;
}

inline std::string format_subtasks(const std::vector<SubTask>& subtasks) {
    std::string res;
    for (size_t i = 0; i < subtasks.size(); ++i) {
        if (i > 0) res += "\\n";
        res += subtasks[i].done ? "[x] " : "[ ] ";
        if (!subtasks[i].title.empty()) {
            res += subtasks[i].title;
            if (!subtasks[i].image_path.empty()) res += " ";
        }
        if (!subtasks[i].image_path.empty()) {
            res += "[img:" + subtasks[i].image_path + "]";
        }
    }
    return res;
}

inline std::string format_hm(time_t t) {
    std::tm tm_val{};
    localtime_safe(&t, &tm_val);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm_val.tm_hour, tm_val.tm_min);
    return buf;
}

inline std::string format_slot(time_t t, time_t now) {
    std::tm tm_t{}, tm_n{};
    localtime_safe(&t, &tm_t);
    localtime_safe(&now, &tm_n);
    if (tm_t.tm_year == tm_n.tm_year && tm_t.tm_yday == tm_n.tm_yday) {
        return format_hm(t);
    }
    static const char* mons[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%d %s · %02d:%02d",
                  tm_t.tm_mday, mons[tm_t.tm_mon], tm_t.tm_hour, tm_t.tm_min);
    return buf;
}

inline std::string format_period_label(const std::tm& sel, StatScope scope) {
    static const char* mons[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buf[32];
    if (scope == StatScope::Day || scope == StatScope::Range) {
        std::snprintf(buf, sizeof(buf), "%d %s %d",
                      sel.tm_mday, mons[sel.tm_mon], sel.tm_year + 1900);
    } else if (scope == StatScope::Month) {
        std::snprintf(buf, sizeof(buf), "%s %d",
                      mons[sel.tm_mon], sel.tm_year + 1900);
    } else {
        std::snprintf(buf, sizeof(buf), "%d", sel.tm_year + 1900);
    }
    return buf;
}

inline std::string format_duration_delta(long long secs, bool earlier) {
    if (secs < 0) secs = 0;
    long long h = secs / 3600;
    long long m = (secs % 3600) / 60;
    long long s = secs % 60;

    std::string out;
    if (h > 0) out += std::to_string(h) + (h == 1 ? " hour " : " hours ");
    if (m > 0 || h > 0) out += std::to_string(m) + (m == 1 ? " minute " : " minutes ");
    out += std::to_string(s) + (s == 1 ? " second " : " seconds ");
    out += earlier ? "earlier" : "after deadline";
    return out;
}

inline time_t day_start(const std::tm& d) {
    std::tm tm_val = d;
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;
    tm_val.tm_isdst = -1;
    return std::mktime(&tm_val);
}

inline time_t day_end(const std::tm& d) {
    std::tm tm_val = d;
    tm_val.tm_hour = 23;
    tm_val.tm_min = 59;
    tm_val.tm_sec = 59;
    tm_val.tm_isdst = -1;
    return std::mktime(&tm_val);
}

inline time_t with_date(time_t t, int year, int month_1_12, int day) {
    std::tm tm_val{};
    localtime_safe(&t, &tm_val);
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month_1_12 - 1;
    tm_val.tm_mday = day;
    tm_val.tm_sec = 0;
    return std::mktime(&tm_val);
}

inline time_t with_hm(time_t t, int hour, int minute) {
    std::tm tm_val{};
    localtime_safe(&t, &tm_val);
    tm_val.tm_hour = hour;
    tm_val.tm_min = minute;
    tm_val.tm_sec = 0;
    return std::mktime(&tm_val);
}

inline std::string sanitize(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return s;
}

inline bool in_scope(time_t ref, const std::tm& sel, StatScope scope) {
    std::tm tm_r{};
    localtime_safe(&ref, &tm_r);
    if (tm_r.tm_year != sel.tm_year) return false;
    if (scope == StatScope::Year) return true;
    if (tm_r.tm_mon != sel.tm_mon) return false;
    if (scope == StatScope::Month) return true;
    return tm_r.tm_mday == sel.tm_mday;
}

class Store {
public:
    std::vector<Task> tasks;
    long next_id = 1;

    explicit Store(std::filesystem::path db_path = get_default_data_dir() / "tasks.db")
        : db_path_(std::move(db_path)) {}

    const std::filesystem::path& db_path() const { return db_path_; }

    void load() {
        tasks.clear();
        std::ifstream in(db_path_.string());
        if (!in) return;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::vector<std::string> cols;
            std::istringstream ss(line);
            std::string part;
            while (std::getline(ss, part, '\t')) cols.push_back(part);
            if (cols.size() < 7) continue;
            try {
                Task t;
                t.id = std::stol(cols[0]);
                int st = std::stoi(cols[1]);
                t.status = (st == 1) ? Status::Achieved : (st == 2 ? Status::Trash : Status::Active);
                t.notified = (std::stoi(cols[2]) != 0);
                t.start_at = static_cast<time_t>(std::stoll(cols[3]));
                t.deadline_at = static_cast<time_t>(std::stoll(cols[4]));
                t.expires_at = static_cast<time_t>(std::stoll(cols[5]));
                if (cols.size() >= 10) {
                    t.completed_at = static_cast<time_t>(std::stoll(cols[6]));
                    t.title = cols[7];
                    t.warned_5m = (std::stoi(cols[8]) != 0);
                    t.detail = cols[9];
                    t.subtasks = parse_subtasks(t.detail);
                } else if (cols.size() >= 9) {
                    t.completed_at = static_cast<time_t>(std::stoll(cols[6]));
                    t.title = cols[7];
                    t.warned_5m = (std::stoi(cols[8]) != 0);
                } else if (cols.size() >= 8) {
                    t.completed_at = static_cast<time_t>(std::stoll(cols[6]));
                    t.title = cols[7];
                    t.warned_5m = (t.deadline_at - t.start_at <= 300) || (t.deadline_at <= std::time(nullptr));
                } else {
                    t.completed_at = 0;
                    t.title = cols[6];
                    t.warned_5m = (t.deadline_at - t.start_at <= 300) || (t.deadline_at <= std::time(nullptr));
                }
                next_id = std::max(next_id, t.id + 1);
                tasks.push_back(std::move(t));
            } catch (...) {
                continue;
            }
        }
    }

    void save() const {
        std::error_code ec;
        std::filesystem::create_directories(db_path_.parent_path(), ec);
        std::ofstream out(db_path_.string(), std::ios::trunc);
        if (!out) return;
        for (const auto& t : tasks) {
            out << t.id << '\t'
                << static_cast<int>(t.status) << '\t'
                << (t.notified ? 1 : 0) << '\t'
                << static_cast<long long>(t.start_at) << '\t'
                << static_cast<long long>(t.deadline_at) << '\t'
                << static_cast<long long>(t.expires_at) << '\t'
                << static_cast<long long>(t.completed_at) << '\t'
                << sanitize(t.title) << '\t'
                << (t.warned_5m ? 1 : 0) << '\t'
                << sanitize(t.subtasks.empty() ? t.detail : format_subtasks(t.subtasks)) << '\n';
        }
    }

    const Task& add(const std::string& title, time_t start_at, time_t deadline_at, time_t expires_at,
                    const std::string& detail = "") {
        Task t;
        t.id = next_id++;
        t.title = sanitize(title);
        t.detail = sanitize(detail);
        t.subtasks = parse_subtasks(detail);
        t.start_at = start_at;
        t.deadline_at = deadline_at;
        t.expires_at = expires_at;
        t.completed_at = 0;
        t.status = Status::Active;
        t.notified = false;
        t.warned_5m = (deadline_at - start_at <= 300);
        tasks.push_back(std::move(t));
        save();
        return tasks.back();
    }

    void set_status(long id, Status st) {
        for (auto& t : tasks) {
            if (t.id == id) {
                t.status = st;
                t.completed_at = (st == Status::Achieved) ? std::time(nullptr) : 0;
                if (st == Status::Achieved) {
                    for (auto& sub : t.subtasks) {
                        sub.done = true;
                    }
                    if (!t.subtasks.empty()) {
                        t.detail = format_subtasks(t.subtasks);
                    }
                }
                save();
                return;
            }
        }
    }

    bool set_subtask_done(long task_id, size_t sub_idx, bool done) {
        for (auto& t : tasks) {
            if (t.id == task_id) {
                if (sub_idx < t.subtasks.size()) {
                    t.subtasks[sub_idx].done = done;
                    t.detail = format_subtasks(t.subtasks);
                    bool all_done = true;
                    for (const auto& sub : t.subtasks) {
                        if (!sub.done) {
                            all_done = false;
                            break;
                        }
                    }
                    if (all_done) {
                        t.status = Status::Achieved;
                        t.completed_at = std::time(nullptr);
                        save();
                        return true; // triggered main task completion
                    } else if (t.status == Status::Achieved) {
                        // A subtask was unchecked from an achieved task
                        t.status = Status::Active;
                        t.completed_at = 0;
                    }
                    save();
                }
                return false;
            }
        }
        return false;
    }

    void remove_permanently(long id) {
        tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
                                   [id](const Task& t) { return t.id == id; }),
                    tasks.end());
        save();
    }

    int count(Status st) const {
        int n = 0;
        for (const auto& t : tasks)
            if (t.status == st) ++n;
        return n;
    }

    StatSummary compute_stats(const std::tm& sel, StatScope scope) const {
        return collect_stats([&](time_t ref) { return in_scope(ref, sel, scope); });
    }

    StatSummary compute_stats_range(time_t from_t, time_t to_t) const {
        return collect_stats([=](time_t ref) { return ref >= from_t && ref <= to_t; });
    }

    std::vector<NotifItem> tick(time_t now, bool& changed) {
        std::vector<NotifItem> to_notify;
        changed = false;
        for (auto& t : tasks) {
            if (t.status != Status::Active) continue;
            if (now >= t.expires_at) {
                t.status = Status::Trash;
                changed = true;
            } else {
                if (now >= t.start_at && !t.notified) {
                    t.notified = true;
                    changed = true;
                    if (now < t.deadline_at) {
                        to_notify.push_back({t.id, NotifKind::Ready});
                    }
                }

                const bool can_warn_5m = (t.deadline_at - t.start_at > 300);
                if (can_warn_5m && now >= t.start_at && now >= t.deadline_at - 300 && now < t.deadline_at && !t.warned_5m) {
                    t.warned_5m = true;
                    changed = true;
                    to_notify.push_back({t.id, NotifKind::Deadline5m});
                } else if ((!can_warn_5m || now >= t.deadline_at) && !t.warned_5m) {
                    t.warned_5m = true;
                    changed = true;
                }
            }
        }
        if (changed) save();
        return to_notify;
    }

    const Task* find(long id) const {
        for (const auto& t : tasks)
            if (t.id == id) return &t;
        return nullptr;
    }

private:
    template <typename Pred>
    StatSummary collect_stats(Pred pred) const {
        StatSummary sum;
        for (const auto& t : tasks) {
            if (t.status == Status::Active) continue;
            time_t ref = (t.status == Status::Achieved && t.completed_at > 0)
                ? t.completed_at
                : t.deadline_at;
            if (!pred(ref)) continue;

            if (t.status == Status::Achieved) {
                time_t comp = (t.completed_at > 0) ? t.completed_at : t.deadline_at;
                if (comp <= t.deadline_at) {
                    ++sum.on_time;
                    long long diff = static_cast<long long>(t.deadline_at - comp);
                    sum.victims.push_back({
                        t.title, Outcome::OnTime, format_duration_delta(diff, true)});
                } else {
                    ++sum.late;
                    long long diff = static_cast<long long>(comp - t.deadline_at);
                    sum.victims.push_back({
                        t.title, Outcome::Late, format_duration_delta(diff, false)});
                }
            } else {
                ++sum.failed;
                sum.victims.push_back({t.title, Outcome::Failed, "Unfinished"});
            }
        }
        return sum;
    }

    std::filesystem::path db_path_;
};

} // namespace tk
