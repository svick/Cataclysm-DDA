#include "player.h"
#include "game.h"
#include "mutation.h"
#include "output.h"
#include "options.h"
#include "weather.h"
#include "string_formatter.h"
#include "units.h"
#include "profession.h"
#include "effect.h"
#include "input.h"
#include "addiction.h"
#include "skill.h"

#include <algorithm>

class player_window {
private:
    const std::string title;
    catacurses::window w_info;

    virtual void print_line(unsigned line, int y, bool selected) = 0;

protected:
    catacurses::window w_this;
    const player &p;

    void info_print_folded(const std::string &message)
    {
        fold_and_print(w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta, message);
    }

    template<typename ...Args>
    void info_print_label_value(int y, const std::string &label, int value_x, const std::string &value_format, Args &&... value_args)
    {
        mvwprintz(w_info, y, 1, c_magenta, label);
        mvwprintz(w_info, y, value_x, c_magenta, value_format.c_str(), std::forward<Args>(value_args)...);
    }

    ~player_window() {}

public:
    virtual int values() = 0;

    player_window(const player &player, const std::string title)
        : w_this(), w_info(), title(title), p(player) {}

    void set_windows(catacurses::window w_this, catacurses::window w_info)
    {
        this->w_this = w_this;
        this->w_info = w_info;
    }

    void print(int selected_line = - 1) 
    {
        bool selected = selected_line != -1;
        nc_color color = selected ? h_light_gray : c_light_gray;

        werase(w_this);
        mvwprintz(w_this, 0, 0, color, std::string(26, ' '));
        center_print(w_this, 0, color, title);

        int win_size_y = getmaxy(w_this) - 1;

        int min, max;

        int display_line = selected ? selected_line : 0;
        int half_y = win_size_y / 2;
        if (display_line <= half_y) {
            min = 0;
            max = std::min(win_size_y, values());
        }
        else if (display_line >= values() - half_y) {
            min = values() < win_size_y ? 0 : values() - win_size_y;
            max = values();
        }
        else {
            min = display_line - half_y;
            max = std::min(display_line - half_y + win_size_y, values());
        }

        for (int i = min; i < max; i++)
        {
            print_line(i, i - min + 1, i == selected_line);
        }

        if (values() > win_size_y)
            draw_scrollbar(w_this, min, win_size_y, values(), 1, 0, c_white, true);

        wrefresh(w_this);
        if (selected)
            wrefresh(w_info);
    }
};

class stats_window : public player_window
{
private:
    void display_stat(const char *name, int cur, int max, int line_n, bool selected) {
        nc_color cstatus;
        if (cur <= 0) {
            cstatus = c_dark_gray;
        }
        else if (cur < max / 2) {
            cstatus = c_red;
        }
        else if (cur < max) {
            cstatus = c_light_red;
        }
        else if (cur == max) {
            cstatus = c_white;
        }
        else if (cur < max * 1.5) {
            cstatus = c_light_green;
        }
        else {
            cstatus = c_green;
        }

        mvwprintz(w_this, line_n, 1, selected ? h_light_gray : c_light_gray, name);
        mvwprintz(w_this, line_n, 18, cstatus, "%2d", cur);
        mvwprintz(w_this, line_n, 21, c_light_gray, "(%2d)", max);
    }

    virtual void print_line(unsigned line, int y, bool selected) override
    {
        y++;

        if (line == 0) {
            // Display information on player strength in appropriate window
            display_stat(_("Strength"), p.str_cur, p.str_max, y, selected);

            if (selected)
            {
                info_print_folded( _("Strength affects your melee damage, the amount of weight you can carry, your total HP, "
                        "your resistance to many diseases, and the effectiveness of actions which require brute force."));
                info_print_label_value(3, _("Base HP:"), 23, "%3d", p.hp_max[1]);
                bool metric = get_option<std::string>("USE_METRIC_WEIGHTS") == "kg";
                info_print_label_value(4, metric ? _("Carry weight (kg):") : _("Carry weight (lbs):"), 21, "%5.1f", convert_weight(p.weight_capacity()));
                info_print_label_value(5, _("Melee damage:"), 23, "%3.1f", p.bonus_damage(false));
            }
        }
        else if (line == 1) {
            display_stat(_("Dexterity"), p.dex_cur, p.dex_max, y, selected);

            if (selected)
            {
                info_print_folded( _("Dexterity affects your chance to hit in melee combat, helps you steady your "
                        "gun for ranged combat, and enhances many actions that require finesse."));
                info_print_label_value(3, _("Melee to-hit bonus:"), 38, "%+4.1lf", p.get_hit_base());
                info_print_label_value(4, _("Ranged penalty:"), 39, "%+3d", -(abs(p.ranged_dex_mod())));
                info_print_label_value(5, _("Throwing penalty per target's dodge:"), 39, "%+3d", p.throw_dispersion_per_dodge(false));
            }
        }
        else if (line == 2) {
            display_stat(_("Intelligence"), p.int_cur, p.int_max, y, selected);

            if (selected)
            {
                info_print_folded( _("Intelligence is less important in most situations, but it is vital for more complex tasks like "
                        "electronics crafting.  It also affects how much skill you can pick up from reading a book."));
                info_print_label_value(3,_("Read times:"), 21, "%3d%%", p.read_speed(false));
                info_print_label_value(4,_("Skill rust:"), 22, "%2d%%", p.rust_rate(false));
                info_print_label_value(5, _("Crafting bonus:"), 22, "%2d%%", p.get_int());
            }
        }
        else if (line == 3) {
            display_stat(_("Perception"), p.per_cur, p.per_max, y, selected);

            if (selected)
            {
                info_print_folded( _("Perception is the most important stat for ranged combat.  It's also used for "
                        "detecting traps and other things of interest."));
                info_print_label_value(4, _("Trap detection level:"), 23, "%2d", p.get_per());
                if (p.ranged_per_mod() > 0) {
                    info_print_label_value(5, _("Aiming penalty:"), 21, "%+4d", -p.ranged_per_mod());
                }
            }
        }
    }

public:
    virtual int values() override
    {
        return 4;
    }

    stats_window(const player &player) : player_window(player, _("STATS")) {}
};

const skill_id skill_swimming("swimming");

class encumberance_window : public player_window
{
private:
    std::vector<body_part> parts;
    item *selected_clothing;

    // Rescale temperature value to one that the player sees
    static int temperature_print_rescaling(int temp)
    {
        return (temp / 100.0) * 2 - 100;
    }

    bool should_combine_bps(size_t l, size_t r)
    {
        const auto enc_data = p.get_encumbrance();
        return enc_data[l] == enc_data[r] &&
            temperature_print_rescaling(p.temp_conv[l]) == temperature_print_rescaling(p.temp_conv[r]);
    }

    static std::string swim_cost_text(int moves)
    {
        return string_format(ngettext("Swimming costs %+d movement point. ",
            "Swimming costs %+d movement points. ",
            moves),
            moves);
    }

    static std::string run_cost_text(int moves)
    {
        return string_format(ngettext("Running costs %+d movement point. ",
            "Running costs %+d movement points. ",
            moves),
            moves);
    }

    static std::string reload_cost_text(int moves)
    {
        return string_format(ngettext("Reloading costs %+d movement point. ",
            "Reloading costs %+d movement points. ",
            moves),
            moves);
    }

    static std::string melee_cost_text(int moves)
    {
        return string_format(ngettext("Melee and thrown attacks cost %+d movement point. ",
            "Melee and thrown attacks cost %+d movement points. ",
            moves),
            moves);
    }

    static std::string dodge_skill_text(double mod)
    {
        return string_format(_("Dodge skill %+.1f. "), mod);
    }

    static int get_encumbrance(const player &p, body_part bp, bool combine)
    {
        // Body parts that can't combine with anything shouldn't print double values on combine
        // This shouldn't happen, but handle this, just in case
        bool combines_with_other = (int)bp_aiOther[bp] != bp;
        return p.encumb(bp) * ((combine && combines_with_other) ? 2 : 1);
    }

    static std::string get_encumbrance_description(const player &p, body_part bp, bool combine)
    {
        std::string s;

        const int eff_encumbrance = get_encumbrance(p, bp, combine);

        switch (bp) {
        case bp_torso: {
            const int melee_roll_pen = std::max(-eff_encumbrance, -80);
            s += string_format(_("Melee attack rolls %+d%%; "), melee_roll_pen);
            s += dodge_skill_text(-(eff_encumbrance / 10));
            s += swim_cost_text((eff_encumbrance / 10) * (80 - p.get_skill_level(skill_swimming) * 3));
            s += melee_cost_text(eff_encumbrance);
            break;
        }
        case bp_head:
            s += _("Head encumbrance has no effect; it simply limits how much you can put on.");
            break;
        case bp_eyes:
            s += string_format(_("Perception %+d when checking traps or firing ranged weapons;\n"
                "Dispersion %+d when throwing items."),
                -(eff_encumbrance / 10),
                eff_encumbrance * 10);
            break;
        case bp_mouth:
            s += _("Covering your mouth will make it more difficult to breathe and catch your breath.");
            break;
        case bp_arm_l:
        case bp_arm_r:
            s += _("Arm encumbrance affects stamina cost of melee attacks and accuracy with ranged weapons.");
            break;
        case bp_hand_l:
        case bp_hand_r:
            s += _("Reduces the speed at which you can handle or manipulate items\n");
            s += reload_cost_text((eff_encumbrance / 10) * 15);
            s += string_format(_("Dexterity %+.1f when throwing items;\n"), -(eff_encumbrance / 10.0f));
            s += melee_cost_text(eff_encumbrance / 2);
            s += "\n";
            s += string_format(_("Reduces aim speed of guns by %.1f."), p.aim_speed_encumbrance_modifier());
            break;
        case bp_leg_l:
        case bp_leg_r:
            s += run_cost_text(int(eff_encumbrance * 0.15));
            s += swim_cost_text((eff_encumbrance / 10) * (50 - p.get_skill_level(
                skill_swimming) * 2) / 2);
            s += dodge_skill_text(-eff_encumbrance / 10.0 / 4.0);
            break;
        case bp_foot_l:
        case bp_foot_r:
            s += run_cost_text(int(eff_encumbrance * 0.25));
            break;
        case num_bp:
            break;
        }

        return s;
    }

    virtual void print_line(unsigned line, int y, bool selected) override
    {
        body_part bp = parts[line];
        const auto enc_data = p.get_encumbrance();
        const encumbrance_data &e = enc_data[bp];
        bool highlighted = (selected_clothing == nullptr) ? false :
            (selected_clothing->covers(all_body_parts[bp]));
        bool combine = should_combine_bps(bp, bp_aiOther[bp]);
        // limb, and possible color highlighting
        // @todo: utf8 aware printf would be nice... this works well enough for now
        auto out = body_part_name_as_heading(bp, combine ? 2 : 1);

        int len = 9 - utf8_width(out);
        switch (sgn(len)) {
        case -1:
            out = utf8_truncate(out, 9);
            break;
        case 1:
            out = out + std::string(len, ' ');
            break;
        }

        // Two different highlighting schemes, highlight if the line is selected as per line being set.
        // Make the text green if this part is covered by the passed in item.
        nc_color limb_color = selected ?
            (highlighted ? h_green : h_light_gray) :
            (highlighted ? c_green : c_light_gray);
        mvwprintz(w_this, y, 1, limb_color, out);
        // accumulated encumbrance from clothing, plus extra encumbrance from layering
        wprintz(w_this, encumb_color(e.encumbrance), string_format("%3d", e.armor_encumbrance));
        // separator in low toned color
        wprintz(w_this, c_light_gray, "+");
        // take into account the new encumbrance system for layers
        wprintz(w_this, encumb_color(e.encumbrance), string_format("%-3d", e.layer_penalty));
        // print warmth, tethered to right hand side of the window
        out = string_format("(% 3d)", temperature_print_rescaling(p.temp_conv[bp]));
        mvwprintz(w_this, y, getmaxx(w_this) - 6, p.bodytemp_color(bp), out);

        if (selected)
        {
            info_print_folded(get_encumbrance_description(p, bp, combine));
        }
    }

public:
    virtual int values() override
    {
        return parts.size();
    }

    encumberance_window(const player &player, item *selected_clothing = nullptr)
        : player_window(player, _("ENCUMBRANCE AND WARMTH")), selected_clothing(selected_clothing)
    {
        for (int bp = 0; bp < num_bp; bp++)
        {
            int other = bp_aiOther[bp];

            // always insert non-paired and first in pair; second in pair only when we're not combining
            if (other < bp && should_combine_bps(bp, other))
                continue;

            parts.push_back(body_part(bp));
        }
    }
};

void player::print_encumbrance(const catacurses::window &win, item *selected_clothing) const
{
    encumberance_window encumberance(*this, selected_clothing);
    encumberance.set_windows(win, catacurses::window());
    encumberance.print();
}

class traits_window : public player_window
{
private:
    std::vector<trait_id> traits;

    virtual void print_line(unsigned line, int y, bool selected) override
    {
        const auto &mdata = traits[line].obj();
        auto color = mdata.get_display_color();
        if (selected)
            color = hilite(color);
        trim_and_print(w_this, y, 1, getmaxx(w_this) - 1, color, mdata.name);

        if (selected)
        {
            info_print_folded(string_format(
                "<color_%s>%s</color>: %s", string_from_color(mdata.get_display_color()),
                mdata.name, traits[line]->description));
        }
    }

public:
    virtual int values() override
    {
        return traits.size();
    }

    traits_window(const player &player)
        : player_window(player, _("TRAITS")), traits(p.get_mutations())
    {
        std::sort(this->traits.begin(), this->traits.end(), trait_display_sort);
    }
};

class effects_window : public player_window
{
private:
    struct effect
    {
        std::string name;
        std::string text;
    };

    std::vector<effect> effects;

    virtual void print_line(unsigned line, int y, bool selected) override
    {
        trim_and_print(w_this, y, 1, getmaxx(w_this) - 1, selected ? h_light_gray : c_light_gray, effects[line].name);

        if (selected)
        {
            info_print_folded(effects[line].text);
        }
    }

public:
    virtual int values() override
    {
        return effects.size();
    }

    effects_window(const player &player, const effects_map &effects_map) : player_window(player, _("EFFECTS"))
    {
        for (auto &elem : effects_map) {
            for (auto &effect : elem.second) {
                std::string name = effect.second.disp_name();
                if (!name.empty()) {
                    effects.push_back({ name, effect.second.disp_desc() });
                }
            }
        }

        if (p.get_perceived_pain() > 0) {
            const auto ppen = p.get_pain_penalty();
            std::stringstream pain_text;
            if (ppen.strength > 0) {
                pain_text << _("Strength") << " -" << ppen.strength << "   ";
            }
            if (ppen.dexterity > 0) {
                pain_text << _("Dexterity") << " -" << ppen.dexterity << "   ";
            }
            if (ppen.intelligence > 0) {
                pain_text << _("Intelligence") << " -" << ppen.intelligence << "   ";
            }
            if (ppen.perception > 0) {
                pain_text << _("Perception") << " -" << ppen.perception << "   ";
            }
            if (ppen.speed > 0) {
                pain_text << _("Speed") << " -" << ppen.speed << "%   ";
            }
            effects.push_back({ _("Pain") , pain_text.str() });
        }

        if ((p.has_trait(trait_id("TROGLO")) && g->is_in_sunlight(p.pos()) &&
            g->weather == WEATHER_SUNNY) ||
            (p.has_trait(trait_id("TROGLO2")) && g->is_in_sunlight(p.pos()) &&
                g->weather != WEATHER_SUNNY)) {
            effects.push_back({ _("In Sunlight"), _("The sunlight irritates you.\n\
Strength - 1;    Dexterity - 1;    Intelligence - 1;    Perception - 1") });
        }
        else if (p.has_trait(trait_id("TROGLO2")) && g->is_in_sunlight(p.pos())) {
            effects.push_back({ _("In Sunlight"),_("The sunlight irritates you badly.\n\
Strength - 2;    Dexterity - 2;    Intelligence - 2;    Perception - 2") });
        }
        else if (p.has_trait(trait_id("TROGLO3")) && g->is_in_sunlight(p.pos())) {
            effects.push_back({ _("In Sunlight"), _("The sunlight irritates you terribly.\n\
Strength - 4;    Dexterity - 4;    Intelligence - 4;    Perception - 4") });
        }

        for (auto &elem : p.addictions) {
            if (elem.sated < 0_turns && elem.intensity >= MIN_ADDICTION_LEVEL) {
                effects.push_back({ addiction_name(elem), addiction_text(elem) });
            }
        }
    }
};

class skills_window : public player_window
{
private:
    std::vector<const Skill*> skills;

    virtual void print_line(unsigned line, int y, bool selected) override
    {
        auto skill = skills[line];
        const SkillLevel &level = p.get_skill_level_object(skill->ident());

        const bool can_train = level.can_train();
        const bool training = level.isTraining();
        const bool rusting = level.isRusting();
        int exercise = level.exercise();

        nc_color label_color;
        if (!can_train) {
            label_color = rusting ? c_light_red : c_white;
        }
        else if (exercise >= 100) {
            label_color = training ? c_pink : c_magenta;
        }
        else if (rusting) {
            label_color = training ? c_light_red : c_red;
        }
        else {
            label_color = training ? c_light_blue : c_blue;
        }

        int level_num = level.level();

        // TODO: this skill list here is used in other places as well. Useless redundancy and
        // dependency. Maybe change it into a flag of the skill that indicates it's a skill used
        // by the bionic?
        static const std::array<skill_id, 5> cqb_skills = { {
                skill_id("melee"), skill_id("unarmed"), skill_id("cutting"),
                skill_id("bashing"), skill_id("stabbing"),
            }
        };
        if (p.has_active_bionic(bionic_id("bio_cqb")) &&
            std::find(cqb_skills.begin(), cqb_skills.end(), skill->ident()) != cqb_skills.end()) {
            level_num = 5;
            exercise = 0;
            label_color = c_yellow;
        }

        nc_color value_color = label_color;

        if (selected) {
            std::string old_name = all_colors.get_name(label_color);
            label_color = hilite(label_color);
            std::string new_name = all_colors.get_name(label_color);
        }

        mvwprintz(w_this, y, 1, label_color, skill->name());

        if (skill->ident() == skill_id("dodge")) {
            mvwprintz(w_this, y, 14, value_color, "%4.1f/%-2d(%2d%%)",
                p.get_dodge(), level_num, exercise < 0 ? 0 : exercise);
        }
        else {
            mvwprintz(w_this, y, 19, value_color, "%-2d(%2d%%)", level_num,
                exercise < 0 ? 0 : exercise);
        }

        if (selected)
        {
            info_print_folded(skill->description());
        }
    }

public:
    virtual int values() override
    {
        return skills.size();
    }

    const Skill* get_skill(unsigned line)
    {
        return skills[line];
    }

    skills_window(const player &player)
        : player_window(player, _("SKILLS"))
    {
        skills = Skill::get_skills_sorted_by([&](Skill const & a, Skill const & b) {
            int const level_a = p.get_skill_level_object(a.ident()).exercised_level();
            int const level_b = p.get_skill_level_object(b.ident()).exercised_level();
            return level_a > level_b || (level_a == level_b && a.name() < b.name());
        });
    }
};

class speed_window : public player_window
{
private:
    struct modifier
    {
        std::string label;
        int value;
    };

    std::vector<modifier> modifiers;
    int runcost;
    int newmoves;

    virtual void print_line(unsigned line, int y, bool selected) override
    {
        if (line == 0 || line == 1) {
            std::string label;
            int value;
            nc_color col;

            if (line == 0)
            {
                label = _("Base Move Cost");
                value = runcost;
                col = runcost <= 100 ? c_green : c_red;
            }
            else
            {
                label = _("Current Speed");
                value = newmoves;
                col = newmoves >= 100 ? c_green : c_red;
            }

            mvwprintz(w_this, y, 1, c_light_gray, label);
            mvwprintz(w_this, y, 21, col, "%3d", value);
        }
        else
        {
            auto modifier = modifiers[line - 2];
            nc_color color = modifier.value > 0 ? c_green : c_red;
            mvwprintz(w_this, y, 1, color, modifier.label);
            mvwprintz(w_this, y, 21, color, "%c%2d%%", modifier.value > 0 ? '+' : '-', abs(modifier.value));
        }
    }

public:
    virtual int values() override
    {
        return modifiers.size() + 2;
    }

    void add_modifier(const std::string &label, int value)
    {
        if (value == 0)
            return;

        modifiers.push_back({ label, value });
    }

    speed_window(const player &player, const effects_map &effects_map) : player_window(player, _("SPEED"))
    {
        newmoves = p.get_speed();

        if (p.weight_carried() > p.weight_capacity()) {
            int pen = -25 * (p.weight_carried() - p.weight_capacity()) / (p.weight_capacity());
            add_modifier(_("Overburdened"), pen);
        }

        add_modifier(_("Pain"), -p.get_pain_penalty().speed);
        add_modifier(_("Thirst"), p.thirst_speed_penalty(p.get_thirst()));
        add_modifier(_("Hunger"), p.hunger_speed_penalty(p.get_hunger()));

        if (p.has_trait(trait_id("SUNLIGHT_DEPENDENT")) && !g->is_in_sunlight(p.pos())) {
            add_modifier(_("Out of Sunlight"), g->light_level(p.posz()) >= 12 ? -5 : -10);
        }
        if (p.has_trait(trait_id("COLDBLOOD4")) && g->get_temperature(g->u.pos()) > 65) {
            int pen = (g->get_temperature(g->u.pos()) - 65) / 2;
            add_modifier(_("Cold-Blooded"), pen);
        }
        if ((p.has_trait(trait_id("COLDBLOOD")) || p.has_trait(trait_id("COLDBLOOD2")) ||
            p.has_trait(trait_id("COLDBLOOD3")) || p.has_trait(trait_id("COLDBLOOD4"))) &&
            g->get_temperature(g->u.pos()) < 65) {
            int pen = 65 - g->get_temperature(g->u.pos());
            if (p.has_trait(trait_id("COLDBLOOD3")) || p.has_trait(trait_id("COLDBLOOD4"))) {
                pen = pen / 2;
            }
            else if (p.has_trait(trait_id("COLDBLOOD2"))) {
                pen = pen / 3;
            }
            else {
                pen = pen / 5;
            }
            add_modifier(_("Cold-Blooded"), -pen);
        }

        std::map<std::string, int> speed_effects;
        std::string dis_text = "";
        for (auto &elem : effects_map) {
            for (auto &_effect_it : elem.second) {
                auto &it = _effect_it.second;
                bool reduced = p.resists_effect(it);
                int move_adjust = it.get_mod("SPEED", reduced);
                if (move_adjust != 0) {
                    dis_text = it.get_speed_name();
                    speed_effects[dis_text] += move_adjust;
                }
            }
        }

        for (auto &speed_effect : speed_effects) {
            nc_color col = (speed_effect.second > 0 ? c_green : c_red);
            add_modifier(_(speed_effect.first.c_str()), speed_effect.second);
        }

        int quick_bonus = int(newmoves - (newmoves / 1.1));
        int bio_speed_bonus = quick_bonus;
        if (p.has_trait(trait_id("QUICK")) && p.has_bionic(bionic_id("bio_speed"))) {
            bio_speed_bonus = int(newmoves / 1.1 - (newmoves / 1.1 / 1.1));
            std::swap(quick_bonus, bio_speed_bonus);
        }
        if (p.has_trait(trait_id("QUICK"))) {
            add_modifier(_("Quick"), quick_bonus);
        }
        if (p.has_bionic(bionic_id("bio_speed"))) {
            add_modifier(_("Bionic Speed"), bio_speed_bonus);
        }

        runcost = p.run_cost(100);
    }
};

void player::disp_info()
{
    unsigned maxy = unsigned( TERMY );

    encumberance_window encumberance(*this);

    traits_window traits(*this);
    effects_window effects(*this, *this->effects);
    skills_window skills(*this);

    unsigned effect_win_size_y = 1 + unsigned(effects.values() );
    unsigned trait_win_size_y = 1 + unsigned( traits.values() );
    unsigned skill_win_size_y = 1 + unsigned( skills.values() );
    unsigned info_win_size_y = 6;

    unsigned infooffsetytop = 11;
    unsigned infooffsetybottom = infooffsetytop + 1 + info_win_size_y;

    if (effect_win_size_y + infooffsetybottom > maxy) {
        effect_win_size_y = maxy - infooffsetybottom;
    }

    if( trait_win_size_y + infooffsetybottom > maxy ) {
        trait_win_size_y = maxy - infooffsetybottom;
    }

    if( skill_win_size_y + infooffsetybottom > maxy ) {
        skill_win_size_y = maxy - infooffsetybottom;
    }

    // if encumberance would scroll, but we have free space, move info lower
    int free_y = maxy - (std::max({ effect_win_size_y, trait_win_size_y, skill_win_size_y }) + infooffsetybottom);
    if (free_y > 0 && encumberance.values() > 8)
    {
        infooffsetytop += std::min(free_y, encumberance.values() - 8);
        infooffsetybottom = infooffsetytop + 1 + info_win_size_y;
    }

    catacurses::window w_grid_top    = catacurses::newwin( infooffsetybottom, FULL_SCREEN_WIDTH + 1,
                                       VIEW_OFFSET_Y, VIEW_OFFSET_X );
    catacurses::window w_grid_skill  = catacurses::newwin( skill_win_size_y + 1, 27,
                                       infooffsetybottom + VIEW_OFFSET_Y, 0 + VIEW_OFFSET_X );
    catacurses::window w_grid_trait  = catacurses::newwin( trait_win_size_y + 1, 27,
                                       infooffsetybottom + VIEW_OFFSET_Y, 27 + VIEW_OFFSET_X );
    catacurses::window w_grid_effect = catacurses::newwin( effect_win_size_y + 1, 28,
                                       infooffsetybottom + VIEW_OFFSET_Y, 53 + VIEW_OFFSET_X );

    catacurses::window w_tip     = catacurses::newwin( 1, FULL_SCREEN_WIDTH,  VIEW_OFFSET_Y,
                                   0 + VIEW_OFFSET_X );
    catacurses::window w_stats   = catacurses::newwin( infooffsetytop - 2, 26,  1 + VIEW_OFFSET_Y,  0 + VIEW_OFFSET_X );
    catacurses::window w_traits  = catacurses::newwin( trait_win_size_y, 26,
                                   infooffsetybottom + VIEW_OFFSET_Y, 27 + VIEW_OFFSET_X );
    catacurses::window w_encumb  = catacurses::newwin( infooffsetytop - 2, 26,  1 + VIEW_OFFSET_Y, 27 + VIEW_OFFSET_X );
    catacurses::window w_effects = catacurses::newwin( effect_win_size_y, 26,
                                   infooffsetybottom + VIEW_OFFSET_Y, 54 + VIEW_OFFSET_X );
    catacurses::window w_speed   = catacurses::newwin( infooffsetytop - 2, 26,  1 + VIEW_OFFSET_Y, 54 + VIEW_OFFSET_X );
    catacurses::window w_skills  = catacurses::newwin( skill_win_size_y, 26,
                                   infooffsetybottom + VIEW_OFFSET_Y, 0 + VIEW_OFFSET_X );
    catacurses::window w_info    = catacurses::newwin( info_win_size_y, FULL_SCREEN_WIDTH,
                                   infooffsetytop + VIEW_OFFSET_Y, 0 + VIEW_OFFSET_X );

    unsigned upper_info_border = infooffsetytop - 1;
    unsigned lower_info_border = 1 + upper_info_border + info_win_size_y;
    for( unsigned i = 0; i < unsigned( FULL_SCREEN_WIDTH + 1 ); i++ ) {
        //Horizontal line top grid
        mvwputch( w_grid_top, upper_info_border, i, BORDER_COLOR, LINE_OXOX );
        mvwputch( w_grid_top, lower_info_border, i, BORDER_COLOR, LINE_OXOX );

        //Vertical line top grid
        if( i <= infooffsetybottom ) {
            mvwputch( w_grid_top, i, 26, BORDER_COLOR, LINE_XOXO );
            mvwputch( w_grid_top, i, 53, BORDER_COLOR, LINE_XOXO );
            mvwputch( w_grid_top, i, FULL_SCREEN_WIDTH, BORDER_COLOR, LINE_XOXO );
        }

        //Horizontal line skills
        if( i <= 26 ) {
            mvwputch( w_grid_skill, skill_win_size_y, i, BORDER_COLOR, LINE_OXOX );
        }

        //Vertical line skills
        if( i <= skill_win_size_y ) {
            mvwputch( w_grid_skill, i, 26, BORDER_COLOR, LINE_XOXO );
        }

        //Horizontal line traits
        if( i <= 26 ) {
            mvwputch( w_grid_trait, trait_win_size_y, i, BORDER_COLOR, LINE_OXOX );
        }

        //Vertical line traits
        if( i <= trait_win_size_y ) {
            mvwputch( w_grid_trait, i, 26, BORDER_COLOR, LINE_XOXO );
        }

        //Horizontal line effects
        if( i <= 27 ) {
            mvwputch( w_grid_effect, effect_win_size_y, i, BORDER_COLOR, LINE_OXOX );
        }

        //Vertical line effects
        if( i <= effect_win_size_y ) {
            mvwputch( w_grid_effect, i, 0, BORDER_COLOR, LINE_XOXO );
            mvwputch( w_grid_effect, i, 27, BORDER_COLOR, LINE_XOXO );
        }
    }

    //Intersections top grid
    mvwputch( w_grid_top, lower_info_border, 26, BORDER_COLOR, LINE_OXXX ); // T
    mvwputch( w_grid_top, lower_info_border, 53, BORDER_COLOR, LINE_OXXX ); // T
    mvwputch( w_grid_top, upper_info_border, 26, BORDER_COLOR, LINE_XXOX ); // _|_
    mvwputch( w_grid_top, upper_info_border, 53, BORDER_COLOR, LINE_XXOX ); // _|_
    mvwputch( w_grid_top, upper_info_border, FULL_SCREEN_WIDTH, BORDER_COLOR, LINE_XOXX ); // -|
    mvwputch( w_grid_top, lower_info_border, FULL_SCREEN_WIDTH, BORDER_COLOR, LINE_XOXX ); // -|
    wrefresh( w_grid_top );

    mvwputch( w_grid_skill, skill_win_size_y, 26, BORDER_COLOR, LINE_XOOX ); // _|

    if( skill_win_size_y > trait_win_size_y ) {
        mvwputch( w_grid_skill, trait_win_size_y, 26, BORDER_COLOR, LINE_XXXO );    // |-
    } else if( skill_win_size_y == trait_win_size_y ) {
        mvwputch( w_grid_skill, trait_win_size_y, 26, BORDER_COLOR, LINE_XXOX );    // _|_
    }

    mvwputch( w_grid_trait, trait_win_size_y, 26, BORDER_COLOR, LINE_XOOX ); // _|

    if( trait_win_size_y > effect_win_size_y ) {
        mvwputch( w_grid_trait, effect_win_size_y, 26, BORDER_COLOR, LINE_XXXO ); // |-
    } else if( trait_win_size_y == effect_win_size_y ) {
        mvwputch( w_grid_trait, effect_win_size_y, 26, BORDER_COLOR, LINE_XXOX ); // _|_
    } else if( trait_win_size_y < effect_win_size_y ) {
        mvwputch( w_grid_trait, trait_win_size_y, 26, BORDER_COLOR, LINE_XOXX ); // -|
        mvwputch( w_grid_trait, effect_win_size_y, 26, BORDER_COLOR, LINE_XXOO ); // |_
    }

    mvwputch( w_grid_effect, effect_win_size_y, 0, BORDER_COLOR, LINE_XXOO ); // |_
    mvwputch( w_grid_effect, effect_win_size_y, 27, BORDER_COLOR, LINE_XOOX ); // _|

    wrefresh( w_grid_skill );
    wrefresh( w_grid_effect );
    wrefresh( w_grid_trait );

    //-1 for header
    trait_win_size_y--;
    skill_win_size_y--;
    effect_win_size_y--;

    // Print name and header
    // Post-humanity trumps your pre-Cataclysm life.
    if( crossed_threshold() ) {
        std::string race;
        for( auto &mut : my_mutations ) {
            const auto &mdata = mut.first.obj();
            if( mdata.threshold ) {
                race = mdata.name;
                break;
            }
        }
        //~ player info window: 1s - name, 2s - gender, 3s - Prof or Mutation name
        mvwprintw( w_tip, 0, 0, _( "%1$s | %2$s | %3$s" ), name.c_str(),
                   male ? _( "Male" ) : _( "Female" ), race.c_str() );
    } else if( prof == NULL || prof == prof->generic() ) {
        // Regular person. Nothing interesting.
        //~ player info window: 1s - name, 2s - gender, '|' - field separator.
        mvwprintw( w_tip, 0, 0, _( "%1$s | %2$s" ), name.c_str(),
                   male ? _( "Male" ) : _( "Female" ) );
    } else {
        mvwprintw( w_tip, 0, 0, _( "%1$s | %2$s | %3$s" ), name.c_str(),
                   male ? _( "Male" ) : _( "Female" ), prof->gender_appropriate_name( male ).c_str() );
    }

    input_context ctxt( "PLAYER_INFO" );
    ctxt.register_updown();
    ctxt.register_action( "NEXT_TAB", _( "Cycle to next category" ) );
    ctxt.register_action( "PREV_TAB", _( "Cycle to previous category" ) );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM", _( "Toggle skill training" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    std::string action;

    std::string help_msg = string_format( _( "Press %s for help." ),
                                          ctxt.get_desc( "HELP_KEYBINDINGS" ).c_str() );
    mvwprintz( w_tip, 0, FULL_SCREEN_WIDTH - utf8_width( help_msg ), c_light_red, help_msg.c_str() );
    help_msg.clear();
    wrefresh( w_tip );

    stats_window stats(*this);
    stats.set_windows(w_stats, w_info);
    stats.print();

    encumberance.set_windows(w_encumb, w_info);
    encumberance.print();

    traits.set_windows(w_traits, w_info);
    traits.print();

    effects.set_windows(w_effects, w_info);
    effects.print();

    skills.set_windows(w_skills, w_info);
    skills.print();

    speed_window speed(*this, *this->effects);
    speed.set_windows(w_speed, w_info);
    speed.print();

    catacurses::refresh();

    int curtab = 0;
    unsigned line = 0;
    bool done = false;

    const std::vector<player_window*> categories { &stats, &encumberance, &skills, &traits, &effects };

    // Initial printing is DONE.  Now we give the player a chance to scroll around
    // and "hover" over different items for more info.
    do {
        werase( w_info );

        auto current_window = categories[curtab];

        current_window->print(line);

        action = ctxt.handle_input();
        if (action == "DOWN") {
            line++;
            if (line == current_window->values()) {
                line = 0;
            }
        }
        else if (action == "UP") {
            if (line == 0) {
                line = current_window->values() - 1;
            }
            else {
                line--;
            }
        }
        else if (action == "NEXT_TAB" || action == "PREV_TAB") {
            line = 0;
            if (action == "NEXT_TAB") {
                curtab++;
                if (curtab == categories.size())
                    curtab = 0;
            } else {
                if (curtab == 0)
                    curtab = categories.size();
                else
                    curtab--;
            }
            current_window->print();
        }
        else if (action == "CONFIRM" && current_window == &skills) {
            get_skill_level_object(skills.get_skill(line)->ident()).toggleTraining();
        }
         else if (action == "QUIT") {
                    done = true;
        }
    } while( !done );

    g->refresh_all();
}
