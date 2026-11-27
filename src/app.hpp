// Starter skeleton for the MAP project.
// Pure logic lives here so it can be unit tested without starting a server.
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace map_project {

inline constexpr const char* kAppName = "map-project";
inline constexpr const char* kAppVersion = "0.1.0";
// Represents a contact in the address book.
struct Contact {
    int id;
    std::string name;
    std::string email;
    std::string phone;
    std::string category;

    nlohmann::json ToJson() const {
        return {
            {"id", id},
            {"name", name},
            {"email", email},
            {"phone", phone},
            {"category", category}
        };
    }
};


inline std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return str;
}
/// Maximum length of a contact name, in characters (parameter L of the theme).
inline constexpr std::size_t kMaxNameLength = 50;

/// Counts characters, not bytes, in a UTF-8 string.
/// Letters such as "ș" or "ă" take two bytes in UTF-8 but count as one character.
inline std::size_t Utf8Length(const std::string& text) {
    std::size_t count = 0;
    for (unsigned char byte : text) {
        // continuation bytes look like 10xxxxxx; every other byte starts a new character
        if ((byte & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

/// A name is valid when it has between 1 and kMaxNameLength characters.
inline bool IsValidNameLength(const std::string& name) {
    const std::size_t length = Utf8Length(name);
    return length >= 1 && length <= kMaxNameLength;
}

/// Reads an environment variable, falling back when it is missing or empty.
inline std::string Env(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return (value == nullptr || *value == '\0') ? fallback : std::string(value);
}

/// Holds the application data in memory.
/// Everything is lost when the container restarts. Add your theme's structures here.
class Store {
public:
    void Reset() {
        std::lock_guard<std::mutex> guard(mutex_);
        next_id_ = 1;
        contacts_.clear();
    }

    int NextId() {
        std::lock_guard<std::mutex> guard(mutex_);
        return next_id_;
    }

    int TakeNextId() {
        std::lock_guard<std::mutex> guard(mutex_);
        return next_id_++;
    }
    //checks if an email already exists in the contacts list (case-insensitive)
    bool EmailExists(const std::string& email) {
        std::lock_guard<std::mutex> guard(mutex_);
        std::string target = ToLower(email);
        for (const auto& c : contacts_) {
            if (ToLower(c.email) == target) {
                return true;
            }
        }
        return false;
    }
    // Adds a new contact to the store and returns it.
    Contact AddContact(const std::string& name, const std::string& email, const std::string& phone, const std::string& category) {
        std::lock_guard<std::mutex> guard(mutex_);
        Contact c{next_id_++, name, email, phone, category};
        contacts_.push_back(c);
        return c;
    }
    //looks for a contact by ID and returns it 
    std::optional<Contact> GetContactById(int id) {
        std::lock_guard<std::mutex> guard(mutex_);
        for (const auto& c : contacts_) {
            if (c.id == id) {
                return c;
            }
        }
        return std::nullopt;
    }
    //deletes a contact by ID 
    bool DeleteContact(int id) {
        std::lock_guard<std::mutex> guard(mutex_);
        auto it = std::remove_if(contacts_.begin(), contacts_.end(), [id](const Contact& c) {
            return c.id == id;
        });
        if (it != contacts_.end()) {
            contacts_.erase(it, contacts_.end());
            return true;
        }
        return false;
    }
    // Returns a list of contacts, optionally filtered by category and/or query (case-insensitive).
    std::vector<Contact> GetContacts(const std::optional<std::string>& category_filter, const std::optional<std::string>& query_filter) {
        std::lock_guard<std::mutex> guard(mutex_);
        std::vector<Contact> result;

        for (const auto& c : contacts_) {
            //filter by category
            if (category_filter.has_value() && !category_filter->empty()) {
                if (c.category != category_filter.value()) {
                    continue;
                }
            }
            // filter by query (case-insensitive)
            if (query_filter.has_value() && !query_filter->empty()) {
                std::string name_lower = ToLower(c.name);
                std::string q_lower = ToLower(query_filter.value());
                if (name_lower.find(q_lower) == std::string::npos) {
                    continue;
                }
            }

            result.push_back(c);
        }
    // sort the result by name and then by id
        std::sort(result.begin(), result.end(), [](const Contact& a, const Contact& b) {
            if (a.name != b.name) {
                return a.name < b.name;
            }
            return a.id < b.id;
        });

        return result;
    }
    /// Returns statistics about the contacts 
    nlohmann::json GetStats() {
        std::lock_guard<std::mutex> guard(mutex_);
        int total = static_cast<int>(contacts_.size());
        std::map<std::string, int> by_category = {
            {"family", 0},
            {"friends", 0},
            {"work", 0},
            {"other", 0}
        };

        for (const auto& c : contacts_) {
            by_category[c.category]++;
        }

        return {
            {"total", total},
            {"by_category", by_category}
        };
    }

private:
    std::mutex mutex_;
    int next_id_ = 1;
    std::vector<Contact> contacts_;
    // example: std::map<int, Contact> contacts_;
};

/// Seconds since the process started.
inline long UptimeSeconds(std::chrono::steady_clock::time_point started_at) {
    const auto elapsed = std::chrono::steady_clock::now() - started_at;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
}

inline nlohmann::json HealthBody(std::chrono::steady_clock::time_point started_at) {
    return {{"status", "ok"}, {"uptime_seconds", UptimeSeconds(started_at)}};
}

inline nlohmann::json VersionBody() {
    return {
        {"app", kAppName},
        {"version", kAppVersion},
        {"commit", Env("APP_COMMIT", "dev")},
        {"built_at", Env("APP_BUILT_AT", "unknown")},
    };
}

inline nlohmann::json ErrorBody(const std::string& code, const std::string& message) {
    return {{"error", code}, {"message", message}};
}

/// Decodes a standard Base64 string. Returns nothing if the input is not valid Base64.
inline std::optional<std::string> Base64Decode(const std::string& input) {
    static const std::string kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (input.size() % 4 != 0) {
        return std::nullopt;
    }
    std::string output;
    int buffer = 0;
    int bits = 0;
    size_t padding = 0;
    for (size_t i = 0; i < input.size(); i++) {
        const char ch = input[i];
        if (ch == '=') {
            // padding is allowed only in the last two positions
            if (i < input.size() - 2) return std::nullopt;
            padding++;
            continue;
        }
        if (padding > 0) return std::nullopt;  // no data after padding
        const auto pos = kAlphabet.find(ch);
        if (pos == std::string::npos) return std::nullopt;
        buffer = (buffer << 6) | static_cast<int>(pos);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return output;
}

/// Checks an HTTP Basic "Authorization" header against the expected credentials.
/// Access is always denied when the expected user or password is empty (not configured).
inline bool IsAuthorized(const std::string& header, const std::string& expected_user,
                         const std::string& expected_password) {
    if (expected_user.empty() || expected_password.empty()) {
        return false;
    }
    const std::string prefix = "Basic ";
    if (header.size() <= prefix.size() || header.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    const auto decoded = Base64Decode(header.substr(prefix.size()));
    if (!decoded) {
        return false;
    }
    // the user name ends at the first ':'; the password may contain ':'
    const auto colon = decoded->find(':');
    if (colon == std::string::npos) {
        return false;
    }
    return decoded->substr(0, colon) == expected_user &&
           decoded->substr(colon + 1) == expected_password;
}

inline std::string HomePage() {
    return std::string(R"(<!DOCTYPE html>
<html lang="ro"><head><meta charset="utf-8"><title>)") + kAppName + R"(</title></head>
<body>
<h1>)" + kAppName + R"(</h1>
<p>Autor: Costea Alexandru Mihai, grupa 1.1</p>
<p>Tema: 1</p>
<p>Versiune: )" + kAppVersion + ", commit " + Env("APP_COMMIT", "dev") + R"(</p>
</body></html>)";
}

}  // namespace map_project
