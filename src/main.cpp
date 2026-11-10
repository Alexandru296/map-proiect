// Wires the HTTP routes of the common contract.
// Add the routes required by your assigned theme below.

#include <chrono>
#include <iostream>
#include <string>
#include <cctype>
#include<optional>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "app.hpp"

int main() {
    using namespace map_project;

    const auto started_at = std::chrono::steady_clock::now();
    Store store;
    httplib::Server server;

    server.Get("/health", [&](const httplib::Request&, httplib::Response& response) {
        response.set_content(HealthBody(started_at).dump(), "application/json; charset=utf-8");
    });

    server.Get("/version", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(VersionBody().dump(), "application/json; charset=utf-8");
    });

    server.Post("/reset", [&](const httplib::Request&, httplib::Response& response) {
        store.Reset();
        response.status = 204;
    });

    server.Get("/", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(HomePage(), "text/html; charset=utf-8");
    });
    // add a new contact to the store
    server.Post("/contacts", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            
            // validate required fields
            if (!body.contains("name") || !body.contains("email") || !body.contains("phone") || !body.contains("category")) {
                res.status = 400;
                res.set_content(ErrorBody("validation", "missing required fields").dump(), "application/json; charset=utf-8");
                return;
            }
            std::string name = body["name"];
            std::string email = body["email"];
            std::string phone = body["phone"];
            std::string category = body["category"];
            
            // validate name length
            if (name.empty() || name.length() > 50) {
                res.status = 400;
                res.set_content(ErrorBody("validation", "invalid name length").dump(), "application/json; charset=utf-8");
                return;
            }
            // validate category
            if (category != "family" && category != "friends" && category != "work" && category != "other") {
                res.status = 400;
                res.set_content(ErrorBody("validation", "invalid category").dump(), "application/json; charset=utf-8");
                return;
            }

            // validate phone format: must be 10 digits, optionally starting with '+'
            bool valid_phone = true;
            int digits = 0;
            size_t start = (phone.length() > 0 && phone[0] == '+') ? 1 : 0;
            for (size_t i = start; i < phone.length(); i++) {
                if (!std::isdigit(static_cast<unsigned char>(phone[i]))) valid_phone = false;
                else digits++;
            }
            if (!valid_phone || digits != 10 || phone.length() != (start + 10)) {
                res.status = 400;
                res.set_content(ErrorBody("validation", "invalid phone format").dump(), "application/json; charset=utf-8");
                return;
            }
            // validate email format
            size_t at_pos = email.find('@');
            size_t last_at_pos = email.rfind('@');
            bool email_valid = (at_pos != std::string::npos && at_pos == last_at_pos && at_pos > 0);
            if (email_valid) {
                size_t dot_pos = email.find('.', at_pos);
                if (dot_pos == std::string::npos || dot_pos == at_pos + 1) {
                    email_valid = false;
                }
            }
            if (!email_valid) {
                res.status = 400;
                res.set_content(ErrorBody("validation", "invalid email format").dump(), "application/json; charset=utf-8");
                return;
            }
            // check if email already exists
            if (store.EmailExists(email)) {
                res.status = 409;
                res.set_content(ErrorBody("conflict", "email already exists").dump(), "application/json; charset=utf-8");
                return;
            }
            // adding the cotact
            Contact c = store.AddContact(name, email, phone, category);
            res.status = 201;
            res.set_content(c.ToJson().dump(), "application/json; charset=utf-8");

        } 
            catch (const std::exception& e) {
            res.status = 400;
            res.set_content(ErrorBody("validation", "invalid JSON payload").dump(), "application/json; charset=utf-8");
        }
    });
     // GET /contacts - list contacts, optionally filtered by category and searched by name
    server.Get("/contacts", [&](const httplib::Request& req, httplib::Response& res) {
        std::optional<std::string> category_filter = std::nullopt;
        std::optional<std::string> query_filter = std::nullopt;

        if (req.has_param("category")) category_filter = req.get_param_value("category");
        if (req.has_param("q")) query_filter = req.get_param_value("q");

        auto contacts = store.GetContacts(category_filter, query_filter);
        nlohmann::json j_array = nlohmann::json::array();
        for (const auto& c : contacts) {
            j_array.push_back(c.ToJson());
        }
        res.set_content(j_array.dump(), "application/json; charset=utf-8");
    });
    
    // GET /contacts/{id} - return one contact
    server.Get(R"(/contacts/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto contact = store.GetContactById(id);
        if (contact) {
            res.set_content(contact->ToJson().dump(), "application/json; charset=utf-8");
        } else {
            res.status = 404;
            res.set_content(ErrorBody("not_found", "contact not found").dump(), "application/json; charset=utf-8");
        }
    });

    // delete one contact
    server.Delete(R"(/contacts/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        if (store.DeleteContact(id)) {
            res.status = 204;
        } else {
            res.status = 404;
            res.set_content(ErrorBody("not_found", "contact not found").dump(), "application/json; charset=utf-8");
        }
    });
     // count contacts per category
    server.Get("/stats", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(store.GetStats().dump(), "application/json; charset=utf-8");
    });
    
    
    server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if (response.status == 404 && response.body.empty()) {
            response.set_content(ErrorBody("not_found", "route does not exist").dump(),
                                 "application/json; charset=utf-8");
        }
    });

    std::cout << kAppName << " " << kAppVersion << " starting on port 8080" << std::endl;
    if (!server.listen("0.0.0.0", 8080)) {
        std::cerr << "failed to bind port 8080" << std::endl;
        return 1;
    }
    return 0;
}
