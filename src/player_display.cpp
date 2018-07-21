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

class player_window
{
    private:
        const std::string title;

        virtual void print_impl( int selected_line ) = 0;

    protected:
        catacurses::window w_this;
        catacurses::window w_info;
        const player &p;

        ~player_window() {}

    public:
        virtual int values() = 0;

        void set_windows( catacurses::window w_this, catacurses::window w_info ) {
            this->w_this = w_this;
            this->w_info = w_info;
        }

        void print( int selected_line = -1 ) {
            bool selected = selected_line != -1;
            nc_color color = selected ? h_light_gray : c_light_gray;

            mvwprintz( w_this, 0, 0, color, std::string( 26, ' ' ) );
            center_print( w_this, 0, color, title );

            print_impl( selected_line );

            wrefresh( w_this );
            if( selected ) {
                wrefresh( w_info );
            }
        }

        player_window( const player &player, const std::string title )
            : w_this(), w_info(), title( title ), p( player ) {}
};

class stats_window : public player_window
{
    private:
        void display_stat( const char *name, int cur, int max, int line_n ) {
            nc_color cstatus;
            if( cur <= 0 ) {
                cstatus = c_dark_gray;
            } else if( cur < max / 2 ) {
                cstatus = c_red;
            } else if( cur < max ) {
                cstatus = c_light_red;
            } else if( cur == max ) {
                cstatus = c_white;
            } else if( cur < max * 1.5 ) {
                cstatus = c_light_green;
            } else {
                cstatus = c_green;
            }

            mvwprintz( w_this, line_n, 1, c_light_gray, name );
            mvwprintz( w_this, line_n, 18, cstatus, "%2d", cur );
            mvwprintz( w_this, line_n, 21, c_light_gray, "(%2d)", max );
        }

        virtual void print_impl( int selected_line ) override {
            if( selected_line == -1 ) {
                display_stat( _( "Strength:" ), p.str_cur, p.str_max, 2 );
                display_stat( _( "Dexterity:" ), p.dex_cur, p.dex_max, 3 );
                display_stat( _( "Intelligence:" ), p.int_cur, p.int_max, 4 );
                display_stat( _( "Perception:" ), p.per_cur, p.per_max, 5 );
            } else {
                if( selected_line == 0 ) {
                    // Display information on player strength in appropriate window
                    mvwprintz( w_this, 2, 1, h_light_gray, _( "Strength:" ) );
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta,
                                    _( "Strength affects your melee damage, the amount of weight you can carry, your total HP, "
                                       "your resistance to many diseases, and the effectiveness of actions which require brute force." ) );
                    mvwprintz( w_info, 3, 1, c_magenta, _( "Base HP:" ) );
                    mvwprintz( w_info, 3, 22, c_magenta, "%3d", p.hp_max[1] );
                    if( get_option<std::string>( "USE_METRIC_WEIGHTS" ) == "kg" ) {
                        mvwprintz( w_info, 4, 1, c_magenta, _( "Carry weight(kg):" ) );
                    } else {
                        mvwprintz( w_info, 4, 1, c_magenta, _( "Carry weight(lbs):" ) );
                    }
                    mvwprintz( w_info, 4, 21, c_magenta, "%4.1f", convert_weight( p.weight_capacity() ) );
                    mvwprintz( w_info, 5, 1, c_magenta, _( "Melee damage:" ) );
                    mvwprintz( w_info, 5, 22, c_magenta, "%3.1f", p.bonus_damage( false ) );
                } else if( selected_line == 1 ) {
                    // Display information on player dexterity in appropriate window
                    mvwprintz( w_this, 3, 1, h_light_gray, _( "Dexterity:" ) );
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta,
                                    _( "Dexterity affects your chance to hit in melee combat, helps you steady your "
                                       "gun for ranged combat, and enhances many actions that require finesse." ) );
                    mvwprintz( w_info, 3, 1, c_magenta, _( "Melee to-hit bonus:" ) );
                    mvwprintz( w_info, 3, 38, c_magenta, "%+.1lf", p.get_hit_base() );
                    mvwprintz( w_info, 4, 1, c_magenta, _( "Ranged penalty:" ) );
                    mvwprintz( w_info, 4, 38, c_magenta, "%+3d", -( abs( p.ranged_dex_mod() ) ) );
                    mvwprintz( w_info, 5, 1, c_magenta, _( "Throwing penalty per target's dodge:" ) );
                    mvwprintz( w_info, 5, 38, c_magenta, "%+3d", p.throw_dispersion_per_dodge( false ) );
                } else if( selected_line == 2 ) {
                    // Display information on player intelligence in appropriate window
                    mvwprintz( w_this, 4, 1, h_light_gray, _( "Intelligence:" ) );
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta,
                                    _( "Intelligence is less important in most situations, but it is vital for more complex tasks like "
                                       "electronics crafting.  It also affects how much skill you can pick up from reading a book." ) );
                    mvwprintz( w_info, 3, 1, c_magenta, _( "Read times:" ) );
                    mvwprintz( w_info, 3, 21, c_magenta, "%3d%%", p.read_speed( false ) );
                    mvwprintz( w_info, 4, 1, c_magenta, _( "Skill rust:" ) );
                    mvwprintz( w_info, 4, 22, c_magenta, "%2d%%", p.rust_rate( false ) );
                    mvwprintz( w_info, 5, 1, c_magenta, _( "Crafting bonus:" ) );
                    mvwprintz( w_info, 5, 22, c_magenta, "%2d%%", p.get_int() );
                } else if( selected_line == 3 ) {
                    // Display information on player perception in appropriate window
                    mvwprintz( w_this, 5, 1, h_light_gray, _( "Perception:" ) );
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta,
                                    _( "Perception is the most important stat for ranged combat.  It's also used for "
                                       "detecting traps and other things of interest." ) );
                    mvwprintz( w_info, 4, 1, c_magenta, _( "Trap detection level:" ) );
                    mvwprintz( w_info, 4, 23, c_magenta, "%2d", p.get_per() );
                    if( p.ranged_per_mod() > 0 ) {
                        mvwprintz( w_info, 5, 1, c_magenta, _( "Aiming penalty:" ) );
                        mvwprintz( w_info, 5, 21, c_magenta, "%+4d", -p.ranged_per_mod() );
                    }
                }
            }
        }

    public:
        virtual int values() override {
            return 4;
        }

        stats_window( const player &player ) : player_window( player, _( "STATS" ) ) {}
};

const skill_id skill_swimming( "swimming" );

class encumberance_window : public player_window
{
    private:
        item *selected_clothing;

        // Rescale temperature value to one that the player sees
        int temperature_print_rescaling( int temp ) {
            return ( temp / 100.0 ) * 2 - 100;
        }

        std::string swim_cost_text( int moves ) {
            return string_format( ngettext( "Swimming costs %+d movement point. ",
                                            "Swimming costs %+d movement points. ",
                                            moves ),
                                  moves );
        }

        std::string run_cost_text( int moves ) {
            return string_format( ngettext( "Running costs %+d movement point. ",
                                            "Running costs %+d movement points. ",
                                            moves ),
                                  moves );
        }

        std::string reload_cost_text( int moves ) {
            return string_format( ngettext( "Reloading costs %+d movement point. ",
                                            "Reloading costs %+d movement points. ",
                                            moves ),
                                  moves );
        }

        std::string melee_cost_text( int moves ) {
            return string_format( ngettext( "Melee and thrown attacks cost %+d movement point. ",
                                            "Melee and thrown attacks cost %+d movement points. ",
                                            moves ),
                                  moves );
        }

        std::string dodge_skill_text( double mod ) {
            return string_format( _( "Dodge skill %+.1f. " ), mod );
        }

        int get_encumbrance( const player &p, body_part bp, bool combine ) {
            // Body parts that can't combine with anything shouldn't print double values on combine
            // This shouldn't happen, but handle this, just in case
            bool combines_with_other = ( int )bp_aiOther[bp] != bp;
            return p.encumb( bp ) * ( ( combine && combines_with_other ) ? 2 : 1 );
        }

        std::string get_encumbrance_description( const player &p, body_part bp, bool combine ) {
            std::string s;

            const int eff_encumbrance = get_encumbrance( p, bp, combine );

            switch( bp ) {
                case bp_torso: {
                    const int melee_roll_pen = std::max( -eff_encumbrance, -80 );
                    s += string_format( _( "Melee attack rolls %+d%%; " ), melee_roll_pen );
                    s += dodge_skill_text( -( eff_encumbrance / 10 ) );
                    s += swim_cost_text( ( eff_encumbrance / 10 ) * ( 80 - p.get_skill_level( skill_swimming ) * 3 ) );
                    s += melee_cost_text( eff_encumbrance );
                    break;
                }
                case bp_head:
                    s += _( "Head encumbrance has no effect; it simply limits how much you can put on." );
                    break;
                case bp_eyes:
                    s += string_format( _( "Perception %+d when checking traps or firing ranged weapons;\n"
                                           "Dispersion %+d when throwing items." ),
                                        -( eff_encumbrance / 10 ),
                                        eff_encumbrance * 10 );
                    break;
                case bp_mouth:
                    s += _( "Covering your mouth will make it more difficult to breathe and catch your breath." );
                    break;
                case bp_arm_l:
                case bp_arm_r:
                    s += _( "Arm encumbrance affects stamina cost of melee attacks and accuracy with ranged weapons." );
                    break;
                case bp_hand_l:
                case bp_hand_r:
                    s += _( "Reduces the speed at which you can handle or manipulate items\n" );
                    s += reload_cost_text( ( eff_encumbrance / 10 ) * 15 );
                    s += string_format( _( "Dexterity %+.1f when throwing items;\n" ), -( eff_encumbrance / 10.0f ) );
                    s += melee_cost_text( eff_encumbrance / 2 );
                    s += "\n";
                    s += string_format( _( "Reduces aim speed of guns by %.1f." ), p.aim_speed_encumbrance_modifier() );
                    break;
                case bp_leg_l:
                case bp_leg_r:
                    s += run_cost_text( int( eff_encumbrance * 0.15 ) );
                    s += swim_cost_text( ( eff_encumbrance / 10 ) * ( 50 - p.get_skill_level(
                                             skill_swimming ) * 2 ) / 2 );
                    s += dodge_skill_text( -eff_encumbrance / 10.0 / 4.0 );
                    break;
                case bp_foot_l:
                case bp_foot_r:
                    s += run_cost_text( int( eff_encumbrance * 0.25 ) );
                    break;
                case num_bp:
                    break;
            }

            return s;
        }

    public:
        bool combined_here;

        bool should_combine_bps( const player &p, size_t l, size_t r ) {
            const auto enc_data = p.get_encumbrance();
            return enc_data[l] == enc_data[r] &&
                   temperature_print_rescaling( p.temp_conv[l] ) == temperature_print_rescaling( p.temp_conv[r] );
        }

    private:
        void print_encumbrance( int line = -1 ) {
            const int height = getmaxy( w_this );
            int orig_line = line;

            // fill a set with the indices of the body parts to display
            line = std::max( 0, line );
            std::set<int> parts;
            // check and optionally enqueue line+0, -1, +1, -2, +2, ...
            int off = 0; // offset from line
            int skip[2] = {}; // how far to skip on next neg/pos jump
            do {
                if( !skip[off > 0] && line + off >= 0 && line + off < num_bp ) { // line+off is in bounds
                    parts.insert( line + off );
                    if( line + off != ( int )bp_aiOther[line + off] &&
                        should_combine_bps( p, line + off, bp_aiOther[line + off] ) ) { // part of a pair
                        skip[( int )bp_aiOther[line + off] > line + off] = 1; // skip the next candidate in this direction
                    }
                } else {
                    skip[off > 0] = 0;
                }
                if( off < 0 ) {
                    off = -off;
                } else {
                    off = -off - 1;
                }
            } while( off > -num_bp && ( int )parts.size() < height - 1 );

            std::string out;
            /*** I chose to instead only display X+Y instead of X+Y=Z. More room was needed ***
            *** for displaying triple digit encumbrance, due to new encumbrance system.    ***
            *** If the player wants to see the total without having to do them maths, the  ***
            *** armor layers ui shows everything they want :-) -Davek                      ***/
            int row = 1;
            const auto enc_data = p.get_encumbrance();
            for( auto bp : parts ) {
                const encumbrance_data &e = enc_data[bp];
                bool highlighted = ( selected_clothing == nullptr ) ? false :
                                   ( selected_clothing->covers( static_cast<body_part>( bp ) ) );
                bool combine = should_combine_bps( p, bp, bp_aiOther[bp] );
                out.clear();
                // limb, and possible color highlighting
                // @todo: utf8 aware printf would be nice... this works well enough for now
                out = body_part_name_as_heading( all_body_parts[bp], combine ? 2 : 1 );

                int len = 7 - utf8_width( out );
                switch( sgn( len ) ) {
                    case -1:
                        out = utf8_truncate( out, 7 );
                        break;
                    case 1:
                        out = out + std::string( len, ' ' );
                        break;
                }

                // Two different highlighting schemes, highlight if the line is selected as per line being set.
                // Make the text green if this part is covered by the passed in item.
                nc_color limb_color = ( orig_line == bp ) ?
                                      ( highlighted ? h_green : h_light_gray ) :
                                      ( highlighted ? c_green : c_light_gray );
                mvwprintz( w_this, row, 1, limb_color, out.c_str() );
                // accumulated encumbrance from clothing, plus extra encumbrance from layering
                wprintz( w_this, encumb_color( e.encumbrance ), string_format( "%3d",
                         e.armor_encumbrance ).c_str() );
                // separator in low toned color
                wprintz( w_this, c_light_gray, "+" );
                // take into account the new encumbrance system for layers
                wprintz( w_this, encumb_color( e.encumbrance ), string_format( "%-3d",
                         e.layer_penalty ).c_str() );
                // print warmth, tethered to right hand side of the window
                out = string_format( "(% 3d)", temperature_print_rescaling( p.temp_conv[bp] ) );
                mvwprintz( w_this, row, getmaxx( w_this ) - 6, p.bodytemp_color( bp ), out.c_str() );
                row++;
            }

            if( off > -num_bp ) { // not every body part fit in the window
                //TODO: account for skipped paired body parts in scrollbar math
                draw_scrollbar( w_this, std::max( orig_line, 0 ), height - 1, num_bp, 1 );
            }
        }

        virtual void print_impl( int selected_line ) override {
            print_encumbrance( selected_line );

            if( selected_line != -1 ) {
                werase( w_info );
                std::string s;

                body_part bp = selected_line <= 11 ? all_body_parts[selected_line] : num_bp;
                combined_here = ( bp_aiOther[selected_line] == selected_line + 1 ||
                                  bp_aiOther[selected_line] == selected_line - 1 ) && // first of a pair
                                should_combine_bps( p, selected_line, bp_aiOther[selected_line] );
                s += get_encumbrance_description( p, bp, combined_here );
                fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta, s );
            }
        }

    public:
        virtual int values() override {
            return -1;
        }

        encumberance_window( const player &player, item *selected_clothing = nullptr )
            : player_window( player, _( "ENCUMBRANCE AND WARMTH" ) ), selected_clothing( selected_clothing ) {
        }
};

void player::print_encumbrance( const catacurses::window &win, int line,
                                item *selected_clothing ) const
{
    encumberance_window encumberance( *this, selected_clothing );
    encumberance.set_windows( win, catacurses::window() );
    encumberance.print();
}

class traits_window : public player_window
{
    private:
        std::vector<trait_id> traitslist;

        virtual void print_impl( int selected_line ) override {
            unsigned trait_win_size_y = getmaxy( w_this ) - 1;

            if( selected_line == -1 ) {
                std::sort( traitslist.begin(), traitslist.end(), trait_display_sort );
                for( size_t i = 0; i < traitslist.size() && i < trait_win_size_y; i++ ) {
                    const auto &mdata = traitslist[i].obj();
                    const auto color = mdata.get_display_color();
                    trim_and_print( w_this, int( i ) + 1, 1, getmaxx( w_this ) - 1, color, mdata.name );
                }
            } else {
                size_t min, max;
                if( selected_line <= ( trait_win_size_y - 1 ) / 2 ) {
                    min = 0;
                    max = trait_win_size_y;
                    if( traitslist.size() < max ) {
                        max = traitslist.size();
                    }
                } else if( selected_line >= traitslist.size() - ( trait_win_size_y + 1 ) / 2 ) {
                    min = ( traitslist.size() < trait_win_size_y ? 0 : traitslist.size() - trait_win_size_y );
                    max = traitslist.size();
                } else {
                    min = selected_line - ( trait_win_size_y - 1 ) / 2;
                    max = selected_line + trait_win_size_y / 2 + 1;
                    if( traitslist.size() < max ) {
                        max = traitslist.size();
                    }
                }

                for( size_t i = min; i < max; i++ ) {
                    const auto &mdata = traitslist[i].obj();
                    const auto color = mdata.get_display_color();
                    trim_and_print( w_this, int( 1 + i - min ), 1, getmaxx( w_this ) - 1,
                                    i == selected_line ? hilite( color ) : color, mdata.name );
                }
                if( selected_line < traitslist.size() ) {
                    const auto &mdata = traitslist[selected_line].obj();
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta, string_format(
                                        "<color_%s>%s</color>: %s", string_from_color( mdata.get_display_color() ),
                                        mdata.name, traitslist[selected_line]->description ) );
                }
            }
        }

    public:
        virtual int values() override {
            return traitslist.size();
        }

        traits_window( const player &player )
            : player_window( player, _( "TRAITS" ) ), traitslist( p.get_mutations() ) {
        }
};

class effects_window : public player_window
{
    private:
        std::vector<std::string> effect_name;
        std::vector<std::string> effect_text;

    public:
        virtual void print_impl( int selected_line ) override {
            unsigned effect_win_size_y = getmaxy( w_this ) - 1;

            if( selected_line == -1 ) {
                for( size_t i = 0; i < effect_name.size() && i < effect_win_size_y; i++ ) {
                    trim_and_print( w_this, int( i ) + 1, 0, getmaxx( w_this ) - 1, c_light_gray,
                                    effect_name[i] );
                }
            } else {
                size_t half_y = effect_win_size_y / 2;
                size_t min, max;
                if( selected_line <= half_y ) {
                    min = 0;
                    max = effect_win_size_y;
                    if( effect_name.size() < max ) {
                        max = effect_name.size();
                    }
                } else if( selected_line >= effect_name.size() - half_y ) {
                    min = ( effect_name.size() < effect_win_size_y ? 0 : effect_name.size() - effect_win_size_y );
                    max = effect_name.size();
                } else {
                    min = selected_line - half_y;
                    max = selected_line - half_y + effect_win_size_y;
                    if( effect_name.size() < max ) {
                        max = effect_name.size();
                    }
                }

                for( size_t i = min; i < max; i++ ) {
                    trim_and_print( w_this, int( 1 + i - min ), 0, getmaxx( w_this ) - 1,
                                    i == selected_line ? h_light_gray : c_light_gray, effect_name[i] );
                }
                if( selected_line < effect_text.size() ) {
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta, effect_text[selected_line] );
                }
            }
        }

        virtual int values() override {
            return effect_name.size();
        }

        effects_window( const player &player, const effects_map &effects_map )
            : player_window( player, _( "EFFECTS" ) ) {
            std::string tmp = "";
            for( auto &elem : effects_map ) {
                for( auto &_effect_it : elem.second ) {
                    tmp = _effect_it.second.disp_name();
                    if( !tmp.empty() ) {
                        effect_name.push_back( tmp );
                        effect_text.push_back( _effect_it.second.disp_desc() );
                    }
                }
            }
            if( p.get_perceived_pain() > 0 ) {
                effect_name.push_back( _( "Pain" ) );
                const auto ppen = p.get_pain_penalty();
                std::stringstream pain_text;
                if( ppen.strength > 0 ) {
                    pain_text << _( "Strength" ) << " -" << ppen.strength << "   ";
                }
                if( ppen.dexterity > 0 ) {
                    pain_text << _( "Dexterity" ) << " -" << ppen.dexterity << "   ";
                }
                if( ppen.intelligence > 0 ) {
                    pain_text << _( "Intelligence" ) << " -" << ppen.intelligence << "   ";
                }
                if( ppen.perception > 0 ) {
                    pain_text << _( "Perception" ) << " -" << ppen.perception << "   ";
                }
                if( ppen.speed > 0 ) {
                    pain_text << _( "Speed" ) << " -" << ppen.speed << "%   ";
                }
                effect_text.push_back( pain_text.str() );
            }

            if( ( p.has_trait( trait_id( "TROGLO" ) ) && g->is_in_sunlight( p.pos() ) &&
                  g->weather == WEATHER_SUNNY ) ||
                ( p.has_trait( trait_id( "TROGLO2" ) ) && g->is_in_sunlight( p.pos() ) &&
                  g->weather != WEATHER_SUNNY ) ) {
                effect_name.push_back( _( "In Sunlight" ) );
                effect_text.push_back( _( "The sunlight irritates you.\n\
Strength - 1;    Dexterity - 1;    Intelligence - 1;    Perception - 1" ) );
            } else if( p.has_trait( trait_id( "TROGLO2" ) ) && g->is_in_sunlight( p.pos() ) ) {
                effect_name.push_back( _( "In Sunlight" ) );
                effect_text.push_back( _( "The sunlight irritates you badly.\n\
Strength - 2;    Dexterity - 2;    Intelligence - 2;    Perception - 2" ) );
            } else if( p.has_trait( trait_id( "TROGLO3" ) ) && g->is_in_sunlight( p.pos() ) ) {
                effect_name.push_back( _( "In Sunlight" ) );
                effect_text.push_back( _( "The sunlight irritates you terribly.\n\
Strength - 4;    Dexterity - 4;    Intelligence - 4;    Perception - 4" ) );
            }

            for( auto &elem : p.addictions ) {
                if( elem.sated < 0_turns && elem.intensity >= MIN_ADDICTION_LEVEL ) {
                    effect_name.push_back( addiction_name( elem ) );
                    effect_text.push_back( addiction_text( elem ) );
                }
            }
        }
};

class skills_window : public player_window
{
    private:
        std::vector<const Skill *> skillslist;

    public:
        const Skill *selectedSkill;

        virtual void print_impl( int selected_line ) override {
            unsigned skill_win_size_y = getmaxy( w_this ) - 1;

            if( selected_line == -1 ) {
                unsigned line = 1;

                for( auto &elem : skillslist ) {
                    const SkillLevel &level = p.get_skill_level_object( elem->ident() );

                    // Default to not training and not rusting
                    nc_color text_color = c_blue;
                    bool not_capped = level.can_train();
                    bool training = level.isTraining();
                    bool rusting = level.isRusting();

                    if( training && rusting ) {
                        text_color = c_light_red;
                    } else if( training && not_capped ) {
                        text_color = c_light_blue;
                    } else if( rusting ) {
                        text_color = c_red;
                    } else if( !not_capped ) {
                        text_color = c_white;
                    }

                    int level_num = level.level();
                    int exercise = level.exercise();

                    // TODO: this skill list here is used in other places as well. Useless redundancy and
                    // dependency. Maybe change it into a flag of the skill that indicates it's a skill used
                    // by the bionic?
                    static const std::array<skill_id, 5> cqb_skills = { {
                            skill_id( "melee" ), skill_id( "unarmed" ), skill_id( "cutting" ),
                            skill_id( "bashing" ), skill_id( "stabbing" ),
                        }
                    };
                    if( p.has_active_bionic( bionic_id( "bio_cqb" ) ) &&
                        std::find( cqb_skills.begin(), cqb_skills.end(), elem->ident() ) != cqb_skills.end() ) {
                        level_num = 5;
                        exercise = 0;
                        text_color = c_yellow;
                    }

                    if( line < skill_win_size_y + 1 ) {
                        mvwprintz( w_this, line, 1, text_color, "%s:", ( elem )->name().c_str() );

                        if( ( elem )->ident() == skill_id( "dodge" ) ) {
                            mvwprintz( w_this, line, 15, text_color, "%-.1f/%-2d(%2d%%)",
                                       p.get_dodge(), level_num, exercise < 0 ? 0 : exercise );
                        } else {
                            mvwprintz( w_this, line, 19, text_color, "%-2d(%2d%%)", level_num,
                                       ( exercise <  0 ? 0 : exercise ) );
                        }

                        line++;
                    }
                }
            } else {
                size_t half_y = skill_win_size_y / 2;
                size_t min, max;
                if( selected_line <= half_y ) {
                    min = 0;
                    max = skill_win_size_y;
                    if( skillslist.size() < max ) {
                        max = skillslist.size();
                    }
                } else if( selected_line >= skillslist.size() - half_y ) {
                    min = ( skillslist.size() < size_t( skill_win_size_y ) ? 0 : skillslist.size() - skill_win_size_y );
                    max = skillslist.size();
                } else {
                    min = selected_line - half_y;
                    max = selected_line - half_y + skill_win_size_y;
                    if( skillslist.size() < max ) {
                        max = skillslist.size();
                    }
                }

                selectedSkill = NULL;

                for( size_t i = min; i < max; i++ ) {
                    const Skill *aSkill = skillslist[i];
                    const SkillLevel &level = p.get_skill_level_object( aSkill->ident() );

                    const bool can_train = level.can_train();
                    const bool training = level.isTraining();
                    const bool rusting = level.isRusting();
                    const int exercise = level.exercise();

                    nc_color cstatus;
                    if( i == selected_line ) {
                        selectedSkill = aSkill;
                        if( !can_train ) {
                            cstatus = rusting ? h_light_red : h_white;
                        } else if( exercise >= 100 ) {
                            cstatus = training ? h_pink : h_magenta;
                        } else if( rusting ) {
                            cstatus = training ? h_light_red : h_red;
                        } else {
                            cstatus = training ? h_light_blue : h_blue;
                        }
                    } else {
                        if( rusting ) {
                            cstatus = training ? c_light_red : c_red;
                        } else if( !can_train ) {
                            cstatus = c_white;
                        } else {
                            cstatus = training ? c_light_blue : c_blue;
                        }
                    }
                    mvwprintz( w_this, int( 1 + i - min ), 1, c_light_gray, "                         " );
                    mvwprintz( w_this, int( 1 + i - min ), 1, cstatus, "%s:", aSkill->name().c_str() );

                    if( aSkill->ident() == skill_id( "dodge" ) ) {
                        mvwprintz( w_this, int( 1 + i - min ), 15, cstatus, "%-.1f/%-2d(%2d%%)",
                                   p.get_dodge(), level.level(), exercise < 0 ? 0 : exercise );
                    } else {
                        mvwprintz( w_this, int( 1 + i - min ), 19, cstatus, "%-2d(%2d%%)", level.level(),
                                   ( exercise <  0 ? 0 : exercise ) );
                    }
                }

                draw_scrollbar( w_this, selected_line, skill_win_size_y, int( skillslist.size() ), 1 );
                wrefresh( w_this );

                werase( w_info );

                if( selected_line < skillslist.size() ) {
                    fold_and_print( w_info, 0, 1, FULL_SCREEN_WIDTH - 2, c_magenta, selectedSkill->description() );
                }
            }
        }

        virtual int values() override {
            return skillslist.size();
        }

        skills_window( const player &player )
            : player_window( player, _( "SKILLS" ) ) {
            skillslist = Skill::get_skills_sorted_by( [&]( Skill const & a, Skill const & b ) {
                int const level_a = p.get_skill_level_object( a.ident() ).exercised_level();
                int const level_b = p.get_skill_level_object( b.ident() ).exercised_level();
                return level_a > level_b || ( level_a == level_b && a.name() < b.name() );
            } );
        }
};

class speed_window : public player_window
{
    private:
        const effects_map &effects;

    public:
        virtual void print_impl( int ) override {
            mvwprintz( w_this, 1, 1, c_light_gray, _( "Base Move Cost:" ) );
            mvwprintz( w_this, 2, 1, c_light_gray, _( "Current Speed:" ) );
            int newmoves = p.get_speed();
            int pen = 0;
            unsigned line = 3;
            if( p.weight_carried() > p.weight_capacity() ) {
                pen = 25 * ( p.weight_carried() - p.weight_capacity() ) / ( p.weight_capacity() );
                mvwprintz( w_this, line, 1, c_red, _( "Overburdened        -%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }
            pen = p.get_pain_penalty().speed;
            if( pen >= 1 ) {
                mvwprintz( w_this, line, 1, c_red, _( "Pain                -%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }
            if( p.get_thirst() > 40 ) {
                pen = abs( p.thirst_speed_penalty( p.get_thirst() ) );
                mvwprintz( w_this, line, 1, c_red, _( "Thirst              -%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }
            if( p.get_hunger() > 100 ) {
                pen = abs( p.hunger_speed_penalty( p.get_hunger() ) );
                mvwprintz( w_this, line, 1, c_red, _( "Hunger              -%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }
            if( p.has_trait( trait_id( "SUNLIGHT_DEPENDENT" ) ) && !g->is_in_sunlight( p.pos() ) ) {
                pen = ( g->light_level( p.posz() ) >= 12 ? 5 : 10 );
                mvwprintz( w_this, line, 1, c_red, _( "Out of Sunlight     -%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }
            if( p.has_trait( trait_id( "COLDBLOOD4" ) ) && g->get_temperature( g->u.pos() ) > 65 ) {
                pen = ( g->get_temperature( g->u.pos() ) - 65 ) / 2;
                mvwprintz( w_this, line, 1, c_green, _( "Cold-Blooded        +%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }
            if( ( p.has_trait( trait_id( "COLDBLOOD" ) ) || p.has_trait( trait_id( "COLDBLOOD2" ) ) ||
                  p.has_trait( trait_id( "COLDBLOOD3" ) ) || p.has_trait( trait_id( "COLDBLOOD4" ) ) ) &&
                g->get_temperature( g->u.pos() ) < 65 ) {
                if( p.has_trait( trait_id( "COLDBLOOD3" ) ) || p.has_trait( trait_id( "COLDBLOOD4" ) ) ) {
                    pen = ( 65 - g->get_temperature( g->u.pos() ) ) / 2;
                } else if( p.has_trait( trait_id( "COLDBLOOD2" ) ) ) {
                    pen = ( 65 - g->get_temperature( g->u.pos() ) ) / 3;
                } else {
                    pen = ( 65 - g->get_temperature( g->u.pos() ) ) / 5;
                }
                mvwprintz( w_this, line, 1, c_red, _( "Cold-Blooded        -%s%d%%" ),
                           ( pen < 10 ? " " : "" ), pen );
                line++;
            }

            std::map<std::string, int> speed_effects;
            std::string dis_text = "";
            for( auto &elem : effects ) {
                for( auto &_effect_it : elem.second ) {
                    auto &it = _effect_it.second;
                    bool reduced = p.resists_effect( it );
                    int move_adjust = it.get_mod( "SPEED", reduced );
                    if( move_adjust != 0 ) {
                        dis_text = it.get_speed_name();
                        speed_effects[dis_text] += move_adjust;
                    }
                }
            }

            for( auto &speed_effect : speed_effects ) {
                nc_color col = ( speed_effect.second > 0 ? c_green : c_red );
                mvwprintz( w_this, line, 1, col, "%s", _( speed_effect.first.c_str() ) );
                mvwprintz( w_this, line, 21, col, ( speed_effect.second > 0 ? "+" : "-" ) );
                mvwprintz( w_this, line, ( abs( speed_effect.second ) >= 10 ? 22 : 23 ), col, "%d%%",
                           abs( speed_effect.second ) );
                line++;
            }

            int quick_bonus = int( newmoves - ( newmoves / 1.1 ) );
            int bio_speed_bonus = quick_bonus;
            if( p.has_trait( trait_id( "QUICK" ) ) && p.has_bionic( bionic_id( "bio_speed" ) ) ) {
                bio_speed_bonus = int( newmoves / 1.1 - ( newmoves / 1.1 / 1.1 ) );
                std::swap( quick_bonus, bio_speed_bonus );
            }
            if( p.has_trait( trait_id( "QUICK" ) ) ) {
                mvwprintz( w_this, line, 1, c_green, _( "Quick               +%s%d%%" ),
                           ( quick_bonus < 10 ? " " : "" ), quick_bonus );
                line++;
            }
            if( p.has_bionic( bionic_id( "bio_speed" ) ) ) {
                mvwprintz( w_this, line, 1, c_green, _( "Bionic Speed        +%s%d%%" ),
                           ( bio_speed_bonus < 10 ? " " : "" ), bio_speed_bonus );
            }

            int runcost = p.run_cost( 100 );
            nc_color col = ( runcost <= 100 ? c_green : c_red );
            mvwprintz( w_this, 1, ( runcost >= 100 ? 21 : ( runcost  < 10 ? 23 : 22 ) ), col,
                       "%d", runcost );
            col = ( newmoves >= 100 ? c_green : c_red );
            mvwprintz( w_this, 2, ( newmoves >= 100 ? 21 : ( newmoves < 10 ? 23 : 22 ) ), col,
                       "%d", newmoves );
        }

        virtual int values() override {
            return -1;
        }

        speed_window( const player &player, const effects_map &effects_map )
            : player_window( player, _( "SPEED" ) ), effects( effects_map ) { }
};

void player::disp_info()
{
    unsigned line;

    unsigned maxy = unsigned( TERMY );

    effects_window effects( *this, *effects );
    traits_window traits( *this );
    skills_window skills( *this );

    unsigned effect_win_size_y = 1 + unsigned( effects.values() );
    unsigned trait_win_size_y = 1 + unsigned( traits.values() );
    unsigned skill_win_size_y = 1 + unsigned( skills.values() );
    unsigned info_win_size_y = 6;

    unsigned infooffsetytop = 11;
    unsigned infooffsetybottom = infooffsetytop + 1 + info_win_size_y;

    if( trait_win_size_y + infooffsetybottom > maxy ) {
        trait_win_size_y = maxy - infooffsetybottom;
    }

    if( skill_win_size_y + infooffsetybottom > maxy ) {
        skill_win_size_y = maxy - infooffsetybottom;
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
    catacurses::window w_stats   = catacurses::newwin( 9, 26,  1 + VIEW_OFFSET_Y,  0 + VIEW_OFFSET_X );
    catacurses::window w_traits  = catacurses::newwin( trait_win_size_y, 26,
                                   infooffsetybottom + VIEW_OFFSET_Y, 27 + VIEW_OFFSET_X );
    catacurses::window w_encumb  = catacurses::newwin( 9, 26,  1 + VIEW_OFFSET_Y, 27 + VIEW_OFFSET_X );
    catacurses::window w_effects = catacurses::newwin( effect_win_size_y, 26,
                                   infooffsetybottom + VIEW_OFFSET_Y, 54 + VIEW_OFFSET_X );
    catacurses::window w_speed   = catacurses::newwin( 9, 26,  1 + VIEW_OFFSET_Y, 54 + VIEW_OFFSET_X );
    catacurses::window w_skills  = catacurses::newwin( skill_win_size_y, 26,
                                   infooffsetybottom + VIEW_OFFSET_Y, 0 + VIEW_OFFSET_X );
    catacurses::window w_info    = catacurses::newwin( info_win_size_y, FULL_SCREEN_WIDTH,
                                   infooffsetytop + VIEW_OFFSET_Y, 0 + VIEW_OFFSET_X );

    unsigned upper_info_border = 10;
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
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "CONFIRM", _( "Toggle skill training" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    std::string action;

    std::string help_msg = string_format( _( "Press %s for help." ),
                                          ctxt.get_desc( "HELP_KEYBINDINGS" ).c_str() );
    mvwprintz( w_tip, 0, FULL_SCREEN_WIDTH - utf8_width( help_msg ), c_light_red, help_msg.c_str() );
    help_msg.clear();
    wrefresh( w_tip );

    stats_window stats( *this );
    stats.set_windows( w_stats, w_info );
    stats.print();

    encumberance_window encumberance( *this );
    encumberance.set_windows( w_encumb, w_info );
    encumberance.print();

    traits.set_windows( w_traits, w_info );
    traits.print();

    effects.set_windows( w_effects, w_info );
    effects.print();

    skills.set_windows( w_skills, w_info );
    skills.print();

    speed_window speed( *this, *this->effects );
    speed.set_windows( w_speed, w_info );
    speed.print();

    catacurses::refresh();

    int curtab = 1;
    size_t min, max;
    line = 0;
    bool done = false;
    size_t half_y = 0;

    // Initial printing is DONE.  Now we give the player a chance to scroll around
    // and "hover" over different items for more info.
    do {
        werase( w_info );
        switch( curtab ) {
            case 1:
                stats.print( line );

                action = ctxt.handle_input();
                if( action == "DOWN" ) {
                    line++;
                    if( line == 4 ) {
                        line = 0;
                    }
                } else if( action == "UP" ) {
                    if( line == 0 ) {
                        line = 3;
                    } else {
                        line--;
                    }
                } else if( action == "NEXT_TAB" ) {
                    stats.print();
                    line = 0;
                    curtab++;
                } else if( action == "QUIT" ) {
                    done = true;
                }
                mvwprintz( w_stats, 2, 1, c_light_gray, _( "Strength:" ) );
                mvwprintz( w_stats, 3, 1, c_light_gray, _( "Dexterity:" ) );
                mvwprintz( w_stats, 4, 1, c_light_gray, _( "Intelligence:" ) );
                mvwprintz( w_stats, 5, 1, c_light_gray, _( "Perception:" ) );
                wrefresh( w_stats );
                break;
            case 2:
                encumberance.print( line );

                action = ctxt.handle_input();
                if( action == "DOWN" ) {
                    if( line < num_bp - 1 ) {
                        if( encumberance.combined_here ) {
                            line += ( line < num_bp - 2 ) ? 2 : 0; // skip a line if we aren't at the last pair
                        } else {
                            line++; // unpaired or unequal
                        }
                    }
                } else if( action == "UP" ) {
                    if( line > 0 ) {
                        if( bp_aiOther[line] == line - 1 && // second of a pair
                            encumberance.should_combine_bps( *this, line, bp_aiOther[line] ) ) {
                            line -= ( line > 1 ) ? 2 : 0; // skip a line if we aren't at the first pair
                        } else {
                            line--; // unpaired or unequal
                        }
                    }
                } else if( action == "NEXT_TAB" ) {
                    encumberance.print();
                    line = 0;
                    curtab++;
                } else if( action == "QUIT" ) {
                    done = true;
                }
                break;
            case 4:
                traits.print( line );

                action = ctxt.handle_input();
                if( action == "DOWN" ) {
                    if( line < traits.values() - 1 ) {
                        line++;
                    }
                    break;
                } else if( action == "UP" ) {
                    if( line > 0 ) {
                        line--;
                    }
                } else if( action == "NEXT_TAB" ) {
                    traits.print();
                    line = 0;
                    curtab++;
                } else if( action == "QUIT" ) {
                    done = true;
                }
                break;

            case 5:
                effects.print( line );

                action = ctxt.handle_input();
                if( action == "DOWN" ) {
                    if( line < effects.values() - 1 ) {
                        line++;
                    }
                    break;
                } else if( action == "UP" ) {
                    if( line > 0 ) {
                        line--;
                    }
                } else if( action == "NEXT_TAB" ) {
                    effects.print();
                    line = 0;
                    curtab = 1;
                } else if( action == "QUIT" ) {
                    done = true;
                }
                break;

            case 3:
                skills.print( line );

                action = ctxt.handle_input();
                if( action == "DOWN" ) {
                    if( size_t( line ) < skills.values() - 1 ) {
                        line++;
                    }
                } else if( action == "UP" ) {
                    if( line > 0 ) {
                        line--;
                    }
                } else if( action == "NEXT_TAB" ) {
                    skills.print();
                    line = 0;
                    curtab++;
                } else if( action == "CONFIRM" ) {
                    get_skill_level_object( skills.selectedSkill->ident() ).toggleTraining();
                } else if( action == "QUIT" ) {
                    done = true;
                }
        }
    } while( !done );

    g->refresh_all();
}
