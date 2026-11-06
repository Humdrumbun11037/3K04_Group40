//gui json

/*
Hides: how JSON text is written (escaping, commas, nesting)

Interface
takes: keys and values (text, numbers, true/false, or already-built JSON)
sends: one JSON object or array as text, for the GUI's web page
uses: nothing

Only writing is needed: the page sends its requests as URL parameters, so the
GUI never has to read JSON.
*/
#pragma once

#include <cstdio>
#include <string>

// A text value with quotes, and with ", \ and control characters escaped.
inline std::string jsonString(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char code[8];
                    std::snprintf(code, sizeof code, "\\u%04x", c);
                    out += code;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out + "\"";
}

// Builds one JSON object, one field at a time: JsonObject().add("a", 1).text()
class JsonObject {
public:
    JsonObject& add(const std::string& key, const std::string& value) { return raw(key, jsonString(value)); }
    JsonObject& add(const std::string& key, const char* value) { return raw(key, jsonString(value)); }
    JsonObject& add(const std::string& key, long long value) { return raw(key, std::to_string(value)); }
    JsonObject& add(const std::string& key, int value) { return raw(key, std::to_string(value)); }
    JsonObject& add(const std::string& key, bool value) { return raw(key, value ? "true" : "false"); }

    // value is already JSON (an object or array built elsewhere)
    JsonObject& raw(const std::string& key, const std::string& value) {
        body += (body.empty() ? "" : ",") + jsonString(key) + ":" + value;
        return *this;
    }

    std::string text() const { return "{" + body + "}"; }

private:
    std::string body;
};

// Builds one JSON array of already-built JSON values.
class JsonArray {
public:
    JsonArray& raw(const std::string& value) {
        body += (body.empty() ? "" : ",") + value;
        return *this;
    }
    JsonArray& add(const std::string& value) { return raw(jsonString(value)); }

    std::string text() const { return "[" + body + "]"; }

private:
    std::string body;
};
