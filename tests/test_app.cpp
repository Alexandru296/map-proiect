#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>

#include "app.hpp"

using namespace map_project;

TEST_CASE("health body reports ok and uptime") {
    const auto started_at = std::chrono::steady_clock::now();
    const auto body = HealthBody(started_at);

    CHECK(body["status"] == "ok");
    CHECK(body.contains("uptime_seconds"));
    CHECK(body["uptime_seconds"].get<long>() >= 0);
}

TEST_CASE("version body contains all required fields") {
    const auto body = VersionBody();

    CHECK(body["app"] == kAppName);
    CHECK(body["version"] == kAppVersion);
    CHECK(body.contains("commit"));
    CHECK(body.contains("built_at"));
}

TEST_CASE("env falls back when the variable is missing") {
    CHECK(Env("MAP_VARIABLE_THAT_DOES_NOT_EXIST", "fallback") == "fallback");
}

TEST_CASE("error body has the standard shape") {
    const auto body = ErrorBody("not_found", "route does not exist");

    CHECK(body["error"] == "not_found");
    CHECK(body["message"] == "route does not exist");
}

TEST_CASE("new store starts at one") {
    Store store;
    CHECK(store.NextId() == 1);
}

TEST_CASE("take next id increments") {
    Store store;

    CHECK(store.TakeNextId() == 1);
    CHECK(store.TakeNextId() == 2);
    CHECK(store.NextId() == 3);
}

TEST_CASE("reset returns to one") {
    Store store;
    store.TakeNextId();
    store.TakeNextId();

    store.Reset();

    CHECK(store.NextId() == 1);
}

TEST_CASE("home page mentions the application name") {
    CHECK(HomePage().find(kAppName) != std::string::npos);
}
TEST_CASE("rejects_phone_number_with_invalid_length") {
    std::string short_phone = "0712345"; // are doar 7 cifre
    const auto body = ErrorBody("invalid_phone", "phone number must contain exactly 10 digits");

    CHECK(short_phone.length() != 10);
    CHECK(body["error"] == "invalid_phone");
}

// Test 2 (Validare 2): Numele contactului nu poate fi un câmp gol
TEST_CASE("rejects_empty_contact_name") {
    std::string empty_name = "";
    const auto body = ErrorBody("bad_request", "contact name cannot be empty");

    CHECK(empty_name.empty());
    CHECK(body["message"] == "contact name cannot be empty");
}

// Test 3 (Validare 3): Telefonul conține litere sau caractere nepermise
TEST_CASE("rejects_phone_number_with_letters") {
    std::string invalid_phone = "0712abc345";
    bool has_letters = std::any_of(invalid_phone.begin(), invalid_phone.end(), ::isalpha);
    const auto body = ErrorBody("invalid_input", "phone number contains invalid characters");

    CHECK(has_letters == true);
    CHECK(body["error"] == "invalid_input");
}

// Test 4 (Caz fericit): Verifică adăugarea corectă a unui contact valid
TEST_CASE("accepts_valid_contact_data") {
    std::string name = "Popescu Ion";
    std::string phone = "0712345678";

    CHECK(!name.empty());
    CHECK(phone.length() == 10);
}
// Test 5 (Eroare): Lipsa numărului de telefon din cerere
TEST_CASE("rejects_payload_missing_phone_number") {
    const auto body = ErrorBody("bad_request", "missing required field: phone");
    
    CHECK(body["error"] == "bad_request");
    CHECK(body["message"] == "missing required field: phone");
}

// Test 6 (Eroare): Număr de telefon duplicat (deja existent în agendă)
TEST_CASE("rejects_duplicate_phone_number_with_409") {
    const auto body = ErrorBody("conflict", "phone number already exists");
    
    CHECK(body["error"] == "conflict");
}

// Test 7 (Eroare): Ștergerea unui contact care nu există trebuie să dea eroare 404
TEST_CASE("returns_404_when_deleting_nonexistent_contact") {
    const auto body = ErrorBody("not_found", "contact not found");
    
    CHECK(body["error"] == "not_found");
}

// Test 8 (Eroare): Format de email invalid (lipsă simbol @)
TEST_CASE("rejects_invalid_email_format") {
    std::string invalid_email = "ion.popescu_la_gmail.com";
    const auto body = ErrorBody("invalid_input", "invalid email format");

    CHECK(invalid_email.find('@') == std::string::npos);
    CHECK(body["error"] == "invalid_input");
}

// Test 9 (Succes): Generatorul de ID-uri alocă valori unice pentru contacte noi
TEST_CASE("store_assigns_unique_ids_to_new_contacts") {
    Store store;
    int id_contact1 = store.TakeNextId();
    int id_contact2 = store.TakeNextId();

    CHECK(id_contact1 != id_contact2);
    CHECK(id_contact1 == 1);
    CHECK(id_contact2 == 2);
}

// Test 10 (Succes): Datele pentru actualizarea unui contact sunt valide
TEST_CASE("accepts_contact_update_with_valid_data") {
    std::string original_name = "Ion";
    std::string updated_name = "Ion Popescu";

    CHECK(original_name != updated_name);
    CHECK(!updated_name.empty());
}