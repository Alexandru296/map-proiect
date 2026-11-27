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
// ---------- own tests: contact book ----------

// helper that adds a contact with a valid phone number
static Contact AddTestContact(Store& store, const std::string& name, const std::string& email,
                              const std::string& category) {
    return store.AddContact(name, email, "0722334455", category);
}

TEST_CASE("added contacts get ids starting at one") {
    Store store;
    const auto first = AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    const auto second = AddTestContact(store, "Ion Popescu", "ion@example.com", "family");

    CHECK(first.id == 1);
    CHECK(second.id == 2);
}

TEST_CASE("added contact keeps all fields as written") {
    Store store;
    const auto c = store.AddContact("Ana Pop", "Ana@Example.com", "+0722334455", "friends");

    CHECK(c.name == "Ana Pop");
    CHECK(c.email == "Ana@Example.com");
    CHECK(c.phone == "+0722334455");
    CHECK(c.category == "friends");
}

TEST_CASE("contact json contains all fields") {
    Store store;
    const auto json = AddTestContact(store, "Ana Pop", "ana@example.com", "work").ToJson();

    CHECK(json["id"] == 1);
    CHECK(json["name"] == "Ana Pop");
    CHECK(json["email"] == "ana@example.com");
    CHECK(json["phone"] == "0722334455");
    CHECK(json["category"] == "work");
}

TEST_CASE("duplicate email is detected regardless of case") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");

    CHECK(store.EmailExists("ana@example.com"));
    CHECK(store.EmailExists("Ana@Example.COM"));
}

TEST_CASE("unknown email is not reported as duplicate") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");

    CHECK_FALSE(store.EmailExists("maria@example.com"));
}

TEST_CASE("get contact by id finds an existing contact") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");

    const auto found = store.GetContactById(1);
    REQUIRE(found.has_value());
    CHECK(found->name == "Ana Pop");
}

TEST_CASE("get contact by id returns nothing for a missing id") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");

    CHECK_FALSE(store.GetContactById(999).has_value());
}

TEST_CASE("delete removes an existing contact") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");

    CHECK(store.DeleteContact(1));
    CHECK_FALSE(store.GetContactById(1).has_value());
}

TEST_CASE("delete returns false for a missing contact") {
    Store store;

    CHECK_FALSE(store.DeleteContact(42));
}

TEST_CASE("ids are not reused after delete") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    store.DeleteContact(1);

    const auto next = AddTestContact(store, "Ion Popescu", "ion@example.com", "family");
    CHECK(next.id == 2);
}

TEST_CASE("list is sorted by name, then by id") {
    Store store;
    AddTestContact(store, "Maria", "maria@example.com", "work");  // id 1
    AddTestContact(store, "Ana", "ana1@example.com", "work");     // id 2
    AddTestContact(store, "Ana", "ana2@example.com", "family");   // id 3

    const auto list = store.GetContacts(std::nullopt, std::nullopt);
    REQUIRE(list.size() == 3);
    CHECK(list[0].id == 2);
    CHECK(list[1].id == 3);
    CHECK(list[2].id == 1);
}

TEST_CASE("list can be filtered by category") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    AddTestContact(store, "Ion Popescu", "ion@example.com", "family");

    const auto list = store.GetContacts(std::string("family"), std::nullopt);
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "Ion Popescu");
}

TEST_CASE("search by name ignores case") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    AddTestContact(store, "Ion Popescu", "ion@example.com", "family");

    const auto list = store.GetContacts(std::nullopt, std::string("ANA"));
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "Ana Pop");
}

TEST_CASE("category and search are applied together") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    AddTestContact(store, "Ana Ionescu", "ana.i@example.com", "friends");
    AddTestContact(store, "Ion Popescu", "ion@example.com", "work");

    const auto list = store.GetContacts(std::string("work"), std::string("ana"));
    REQUIRE(list.size() == 1);
    CHECK(list[0].name == "Ana Pop");
}

TEST_CASE("stats count contacts per category") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    AddTestContact(store, "Ion Popescu", "ion@example.com", "work");
    AddTestContact(store, "Maria", "maria@example.com", "family");

    const auto stats = store.GetStats();
    CHECK(stats["total"] == 3);
    CHECK(stats["by_category"]["work"] == 2);
    CHECK(stats["by_category"]["family"] == 1);
    CHECK(stats["by_category"]["friends"] == 0);
    CHECK(stats["by_category"]["other"] == 0);
}

TEST_CASE("reset clears contacts and restarts ids") {
    Store store;
    AddTestContact(store, "Ana Pop", "ana@example.com", "work");
    AddTestContact(store, "Ion Popescu", "ion@example.com", "family");

    store.Reset();

    CHECK(store.GetContacts(std::nullopt, std::nullopt).empty());
    CHECK(store.GetStats()["total"] == 0);
    CHECK(AddTestContact(store, "Maria", "maria@example.com", "other").id == 1);
}

// ---------- own tests: admin authentication ----------

TEST_CASE("base64 decodes a valid value") {
    // "YWRtaW46c2VjcmV0" is the Base64 form of "admin:secret"
    const auto decoded = Base64Decode("YWRtaW46c2VjcmV0");
    REQUIRE(decoded.has_value());
    CHECK(*decoded == "admin:secret");
}

TEST_CASE("base64 rejects invalid input") {
    CHECK_FALSE(Base64Decode("abc").has_value());   // wrong length
    CHECK_FALSE(Base64Decode("a$cd").has_value());  // invalid character
}

TEST_CASE("valid basic credentials are accepted") {
    CHECK(IsAuthorized("Basic YWRtaW46c2VjcmV0", "admin", "secret"));
}

TEST_CASE("wrong password is rejected") {
    // "YWRtaW46d3Jvbmc=" is "admin:wrong"
    CHECK_FALSE(IsAuthorized("Basic YWRtaW46d3Jvbmc=", "admin", "secret"));
}

TEST_CASE("missing or malformed authorization header is rejected") {
    CHECK_FALSE(IsAuthorized("", "admin", "secret"));
    CHECK_FALSE(IsAuthorized("Bearer YWRtaW46c2VjcmV0", "admin", "secret"));
    CHECK_FALSE(IsAuthorized("Basic !!!!", "admin", "secret"));
}

TEST_CASE("access is denied when credentials are not configured") {
    CHECK_FALSE(IsAuthorized("Basic YWRtaW46c2VjcmV0", "", ""));
}

TEST_CASE("password may contain a colon") {
    // "YWRtaW46c2U6Y3JldA==" is "admin:se:cret"
    CHECK(IsAuthorized("Basic YWRtaW46c2U6Y3JldA==", "admin", "se:cret"));
}

// ---------- own tests: name length ----------

TEST_CASE("utf8 length counts characters, not bytes") {
    CHECK(Utf8Length("Ana") == 3);
    // "\xC8\x98" is the letter "Ș" in UTF-8: two bytes, one character
    CHECK(Utf8Length("\xC8\x98tefan") == 6);
}

TEST_CASE("name of exactly 50 characters is accepted") {
    CHECK(IsValidNameLength(std::string(50, 'a')));
}

TEST_CASE("name of 51 characters is rejected") {
    CHECK_FALSE(IsValidNameLength(std::string(51, 'a')));
}

TEST_CASE("empty name is rejected") {
    CHECK_FALSE(IsValidNameLength(""));
}

TEST_CASE("name of 50 characters with a diacritic is accepted") {
    // 49 letters plus "ș" ("\xC8\x99"): 50 characters, but 51 bytes
    const std::string name = std::string(49, 'a') + "\xC8\x99";
    CHECK(name.size() == 51);
    CHECK(IsValidNameLength(name));
}

// ---------- own tests: email format ----------

TEST_CASE("valid email is accepted") {
    CHECK(IsValidEmail("ana@example.com"));
    CHECK(IsValidEmail("ana.pop@mail.example.ro"));
}

TEST_CASE("email without @ is rejected") {
    CHECK_FALSE(IsValidEmail("ana.example.com"));
}

TEST_CASE("email with two @ is rejected") {
    CHECK_FALSE(IsValidEmail("ana@pop@example.com"));
}

TEST_CASE("email with nothing before @ is rejected") {
    CHECK_FALSE(IsValidEmail("@example.com"));
}

TEST_CASE("email without a dot after @ is rejected") {
    CHECK_FALSE(IsValidEmail("ana@examplecom"));
    // a dot before '@' does not count
    CHECK_FALSE(IsValidEmail("ana.pop@example"));
}
