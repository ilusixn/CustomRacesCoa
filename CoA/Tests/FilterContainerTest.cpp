#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    struct Item {
        uint32_t id = 0;
        std::string name;
        uint32_t group = 0;
        uint32_t order = 0;
    };

    class Box : public CoA::Filter::Container<Item> {
    public:
        std::vector<Item> items;
        int populated = 0;

    protected:
        std::vector<Item> DoPopulate() override {
            ++this->populated;
            return this->items;
        }

        bool TokenMatch(char type, int32_t value, const Item& e) override {
            return type == 'i' && static_cast<uint32_t>(value) == e.id;
        }

        bool ArgMatch(uint32_t a, const Item& e) override {
            return a == e.group || a == e.id + 100;
        }

        uint32_t ArgGroup(uint32_t a) override {
            return a >= 100 ? 1 : 0;
        }

        bool TextMatch(const std::string& lower, const Item& e) override {
            return lower.empty() || CoA::Filter::Lower(e.name).find(lower) != std::string::npos;
        }

        std::string FilterKey(const Item& e) override {
            std::string k;
            CoA::Filter::AppendU32(k, e.order);
            return k;
        }
    };

    std::vector<uint32_t> Ids(const Box& b) {
        std::vector<uint32_t> v;

        for (const Item& e : b.Filtered()) {
            v.push_back(e.id);
        }

        return v;
    }

    Box Make() {
        Box b;
        b.items = { { 1, "Fire Bolt", 7, 3 }, { 2, "Frost Bolt", 7, 1 }, { 3, "Holy Light", 8, 2 } };
        return b;
    }
}

// "$<type><digits>" words become tokens; a word with a non-digit tail is dropped without a token; the rest is trimmed
// and its double spaces collapsed.
COA_TEST(FilterContainer_ParseTokens) {
    std::string text = "  fire $i12  $xab bolt  ";
    const std::vector<CoA::Filter::Token> t = CoA::Filter::ParseTokens(text);
    CHECK(t.size() == 1);
    CHECK(t.size() == 1 && t[0].type == 'i' && t[0].value == 12);
    CHECK(text == "fire bolt");

    std::string big = "$i99999999999";
    const std::vector<CoA::Filter::Token> o = CoA::Filter::ParseTokens(big);
    CHECK(o.size() == 1 && o[0].value == 0);   // overflow reads as 0
}

// Text (case-insensitive) and tokens narrow the list; FilterKey orders it.
COA_TEST(FilterContainer_TextTokensAndOrder) {
    Box b = Make();
    b.ApplyFilter("BOLT");
    CHECK((Ids(b) == std::vector<uint32_t>{ 2, 1 }));
    b.ApplyFilter("bolt $i1");
    CHECK((Ids(b) == std::vector<uint32_t>{ 1 }));
    b.ApplyFilter("");
    CHECK((Ids(b) == std::vector<uint32_t>{ 2, 3, 1 }));
    CHECK(b.populated == 1);
}

// Argument groups: each group needs one matching argument; group 0 fails on its first miss.
COA_TEST(FilterContainer_ArgumentGroups) {
    Box b = Make();
    b.ApplyFilter("", { 7 }, {});
    CHECK((Ids(b) == std::vector<uint32_t>{ 2, 1 }));
    b.ApplyFilter("", { 7, 8 }, {});   // both in group 0: the first miss fails
    CHECK((Ids(b) == std::vector<uint32_t>{ 2, 1 }));
    b.ApplyFilter("", { 101, 103 }, {});   // group 1: any match keeps
    CHECK((Ids(b) == std::vector<uint32_t>{ 3, 1 }));
}

// Reapply uses the text after token extraction, so a token in the last filter is dropped.
COA_TEST(FilterContainer_ReapplyDropsTokens) {
    Box b = Make();
    b.ApplyFilter("bolt $i1");
    b.Reapply();
    CHECK((Ids(b) == std::vector<uint32_t>{ 2, 1 }));
}

// Reset forgets the entries; the next filter populates again.
COA_TEST(FilterContainer_ResetRepopulates) {
    Box b = Make();
    b.ApplyFilter("");
    b.Reset();
    CHECK(b.Size() == 0);
    b.items.push_back({ 4, "Bolt", 9, 0 });
    b.ApplyFilter("bolt");
    CHECK(b.populated == 2);
    CHECK((Ids(b) == std::vector<uint32_t>{ 4, 2, 1 }));
}
