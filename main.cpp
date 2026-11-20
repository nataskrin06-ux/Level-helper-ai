#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace lh {

constexpr int SPIKE = 8;
constexpr int BLOCK = 1;

struct Obj { int id; int gx; int gy; };
struct Piece { const char* name; int width; std::vector<Obj> objs; };

// Заготовки: готові шматки рівня з запасом місця для стрибка.
static std::vector<Piece> const& pieces() {
	static std::vector<Piece> p = {
		{"single spike", 1, {{SPIKE,0,0}}},
		{"double spike", 2, {{SPIKE,0,0},{SPIKE,1,0}}},
		{"triple spike", 3, {{SPIKE,0,0},{SPIKE,1,0},{SPIKE,2,0}}},
		{"block platform + spike", 6, {{BLOCK,0,0},{BLOCK,1,0},{BLOCK,2,0},{SPIKE,5,0}}},
		{"stairs", 6, {{BLOCK,0,0},{BLOCK,1,0},{SPIKE,2,0},{SPIKE,3,0},
		               {BLOCK,4,0},{BLOCK,4,1},{BLOCK,5,0},{BLOCK,5,1}}},
	};
	return p;
}

struct Params {
	int count = 8;
	int spacing = 9;
	unsigned seed = 1;
	std::string name = "Helper Level";
};

static Params parse(std::string text) {
	Params p;
	std::string t = text;
	std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
	if (t.find("easy") != std::string::npos || t.find("легк") != std::string::npos) p.spacing = 12;
	if (t.find("hard") != std::string::npos || t.find("склад") != std::string::npos) p.spacing = 7;
	if (t.find("long") != std::string::npos || t.find("довг") != std::string::npos) p.count = 16;
	if (t.find("short") != std::string::npos || t.find("коротк") != std::string::npos) p.count = 5;
	// перше число в запиті = кількість шматків
	for (size_t i = 0; i < t.size(); ++i) {
		if (std::isdigit(static_cast<unsigned char>(t[i]))) {
			int n = 0;
			while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) { n = n * 10 + (t[i] - '0'); ++i; if (n > 100) break; }
			p.count = std::clamp(n, 1, 40);
			break;
		}
	}
	unsigned h = 2166136261u;
	for (unsigned char c : text) { h ^= c; h *= 16777619u; }
	p.seed = h;
	if (!text.empty()) p.name = "Helper: " + text.substr(0, 20);
	return p;
}

static std::string header() {
	return "kS38,1_60_2_160_3_70_11_255_12_255_13_255_4_-1_6_1000_7_1_15_1_18_0_8_1|"
	       "1_120_2_80_3_40_11_255_12_255_13_255_4_-1_6_1001_7_1_15_1_18_0_8_1|"
	       "1_120_2_80_3_40_11_255_12_255_13_255_4_-1_6_1009_7_1_15_1_18_0_8_1|"
	       "1_255_2_255_3_255_11_255_12_255_13_255_4_-1_6_1002_5_1_7_1_15_1_18_0_8_1|"
	       "1_0_2_255_3_255_11_255_12_255_13_255_4_-1_6_1005_5_1_7_1_15_1_18_0_8_1|"
	       "1_0_2_255_3_255_11_255_12_255_13_255_4_-1_6_1006_5_1_7_1_15_1_18_0_8_1|"
	       ",kA13,0,kA15,0,kA16,0,kA14,,kA6,0,kA7,0,kA17,0,kA18,0,kS39,0,kA2,0,kA3,0,kA8,0,kA4,0,kA9,0,kA10,0,kA11,0;";
}

// Збирає рядок рівня. plan (необов'язково) отримує назви шматків і позиції для перегляду.
static std::string build(Params const& p, std::vector<std::string>* plan) {
	std::string out = header();
	unsigned s = p.seed;
	auto rnd = [&]() { s = s * 1664525u + 1013904223u; return s >> 8; };
	int x = 12;
	for (int i = 0; i < p.count; ++i) {
		auto const& pc = pieces()[rnd() % pieces().size()];
		if (plan) plan->push_back(std::string(pc.name) + " @ " + std::to_string(x));
		for (auto const& o : pc.objs) {
			out += "1," + std::to_string(o.id) + ",2," + std::to_string((x + o.gx) * 30 + 15)
			     + ",3," + std::to_string(o.gy * 30 + 15) + ";";
		}
		x += pc.width + p.spacing;
	}
	// кінцева платформа
	for (int k = 0; k < 3; ++k)
		out += "1,1,2," + std::to_string((x + k) * 30 + 15) + ",3,15;";
	if (plan) plan->push_back("finish platform @ " + std::to_string(x));
	return out;
}

static void makeLevel(Params const& p) {
	auto raw = build(p, nullptr);
	auto level = GameLevelManager::get()->createNewLevel();
	level->m_levelName = p.name;
	level->m_levelString = ZipUtils::compressString(raw, false, 0);
	FLAlertLayer::create("Level Helper", "Рівень створено. Відкрий «Створені».", "OK")->show();
}

} // namespace lh

class HelperPopup : public Popup<> {
protected:
	TextInput* m_input = nullptr;

	bool setup() override {
		this->setTitle("Level Helper");
		m_input = TextInput::create(280.f, "easy level, 10 pieces", "chatFont.fnt");
		m_input->setPosition({m_size.width / 2, m_size.height / 2 + 20.f});
		m_mainLayer->addChild(m_input);

		auto spr = ButtonSprite::create("Preview");
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(HelperPopup::onPreview));
		m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, {0.f, 30.f});
		return true;
	}

	void onPreview(CCObject*) {
		auto p = lh::parse(m_input->getString());
		std::vector<std::string> plan;
		lh::build(p, &plan);
		std::string text;
		for (auto const& line : plan) text += line + "\n";
		createQuickPopup("Plan", text, "Stop", "Create",
			[p](FLAlertLayer*, bool create) { if (create) lh::makeLevel(p); });
	}

public:
	static HelperPopup* create() {
		auto ret = new HelperPopup();
		if (ret->initAnchored(320.f, 200.f)) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}
};

class $modify(HelperMenu, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		auto menu = CCMenu::create();
		auto btn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Helper"), this, menu_selector(HelperMenu::onHelper));
		menu->addChild(btn);
		auto win = CCDirector::get()->getWinSize();
		menu->setPosition({60.f, win.height - 25.f});
		this->addChild(menu);
		return true;
	}

	void onHelper(CCObject*) { HelperPopup::create()->show(); }
};
